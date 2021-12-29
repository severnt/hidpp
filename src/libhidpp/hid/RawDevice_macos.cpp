/*
 * Copyright 2021 Clément Vuchener
 * Created by Noah Nuebling
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "RawDevice.h"

#include <misc/Log.h>

#include <stdexcept>
#include <locale>
#include <codecvt>
#include <array>
#include <map>
#include <set>
#include <cstring>
#include <cassert>
#include <thread>
#include <chrono>
#include <atomic>
#include "macos/Utility_macos.h"

using namespace HID;

extern "C" { // This needs to be declared after `using namespace HID` for some reason.
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDDevice.h>
}

// PrivateImpl
//  Why do we use the PrivateImpl struct instead of private member variables or namespace-less variable?

struct RawDevice::PrivateImpl
{

    // Attributes

    IOHIDDeviceRef iohidDevice;
    CFIndex maxInputReportSize;
    CFIndex maxOutputReportSize;

    dispatch_queue_t inputQueue;

    static void nullifyValues(RawDevice *dev) {
        dev->_p->iohidDevice = nullptr;
        dev->_p->maxInputReportSize = 0;
        dev->_p->maxOutputReportSize = 0;
        dev->_p->inputQueue = nullptr;

        dev->_p->initState(dev);
    }

    // State

    CFRunLoopRef inputReportRunLoop;
    std::vector<uint8_t> lastInputReport; // Can't make this atomic, use explicit mutexes to protect this instead

    std::atomic<bool> ignoreNextRead;
    std::atomic<bool> waitingForInput; 
    std::atomic<double> lastInputReportTime; 
    std::atomic<bool> waitingForInputWasInterrupted; 
    std::atomic<bool> inputRunLoopIsRunning;

    // Concurrency
    std::mutex mutexLock;
    std::condition_variable stopWaitingForInput;
    std::condition_variable inputRunLoopStarted;

    static void initState(RawDevice *dev) {

        dev->_p->inputReportRunLoop = nullptr;
        dev->_p->lastInputReport = std::vector<uint8_t>();

        dev->_p->ignoreNextRead = false;
        dev->_p->waitingForInput = false;
        dev->_p->lastInputReportTime = -1; 
        dev->_p->waitingForInputWasInterrupted = false;
        dev->_p->inputRunLoopIsRunning = false;
    }

    // Dispatch queue config

    static dispatch_queue_attr_t getDispatchQueueAttrs() {
        return dispatch_queue_attr_make_with_qos_class(NULL, QOS_CLASS_USER_INITIATED, -1);
    }
    static std::string getDispatchQueueLabel(RawDevice *dev) {
        char queueLabel[1000];
        sprintf(queueLabel, "com.cvuchener.hidpp.input-reports.%s", Utility_macos::IOHIDDeviceGetDebugIdentifier(dev->_p->iohidDevice));
        return std::string(queueLabel);
    }

    // Read input reports
    //  Should be active throughout the lifetime of this object

    static void stopReadThread(RawDevice *dev) {
        CFRunLoopStop(dev->_p->inputReportRunLoop);
    }
    static void readThread(RawDevice *dev) {

        // Does this:
        // - Read input reports 
        // - Store result into lastInputReport
        // - Send notifications that there is new report (via std::condition_variable)
        // Discussion:
        // - This function is blocking. The thread that calls it becomes the read thread until stopReadThread() is called from another thread.

        // Convenience
        RawDevice::PrivateImpl *_p = dev->_p.get();

        // Setup reportBuffer
        CFIndex reportBufferSize = _p->maxInputReportSize;
        uint8_t *reportBuffer = (uint8_t *) malloc(reportBufferSize * sizeof(uint8_t));
        memset(reportBuffer, 0, reportBufferSize); // Init with 0s

        // Setup report callback
        //  IOHIDDeviceGetReportWithCallback has a built-in timeout and might make for more straight forward code
        //      but Apple docs say it should only be used for feature reports. So we're using 
        //      IOHIDDeviceRegisterInputReportCallback instead.
        IOHIDDeviceRegisterInputReportCallback(
            _p->iohidDevice, 
            reportBuffer, // This will get filled when an inputReport occurs
            _p->maxInputReportSize,
            [] (void *context, IOReturn result, void *sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength) {
                
                //  Get dev from context
                //  ^ We can't capture `dev` or anything else, because then the enclosing lambda wouldn't decay to a pure c function
                RawDevice *devvv = static_cast<RawDevice *>(context);

                // Debug

                Log::debug() << "Received input from device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(devvv->_p->iohidDevice) << std::endl;

                // Lock
                std::unique_lock lock(devvv->_p->mutexLock);

                // Store new report
                devvv->_p->lastInputReport.assign(report, report + reportLength);
                devvv->_p->lastInputReportTime = Utility_macos::timestamp();

                // Notify waiting thread
                devvv->_p->stopWaitingForInput.notify_one(); // We assume that there's only one waiting thread

            }, 
            dev // Pass `dev` to context
        );

        // Start runLoop

        // Store current runLoop        
        _p->inputReportRunLoop = CFRunLoopGetCurrent();

        // Add IOHIDDevice to runLoop.
        //  Async callbacks for this IOHIDDevice will be delivered to this runLoop
        //  We need to call this before CFRunLoopRun, because if the runLoop has nothing to do, it'll immediately exit when we try to run it.
        IOHIDDeviceScheduleWithRunLoop(_p->iohidDevice, _p->inputReportRunLoop, kCFRunLoopCommonModes);

        // Debug

        Log::debug() << "Starting inputRunLoop on device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

        // Params

        CFRunLoopMode runLoopMode = kCFRunLoopDefaultMode;

        // Observer

        CFRunLoopObserverContext ctx = {
            .version = 0,
            .info = dev,
            .retain = NULL,
            .release = NULL,
            .copyDescription = NULL,
        };

        CFRunLoopObserverRef observer = CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopEntry | kCFRunLoopExit, false, 0, 
            [](CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info){
                RawDevice *devvv = (RawDevice *)info;
                if (activity == kCFRunLoopEntry) {
                    devvv->_p->inputRunLoopIsRunning = true;
                    devvv->_p->inputRunLoopStarted.notify_one();
                } else {
                    devvv->_p->inputRunLoopIsRunning = false; // Not sure if useful
                }
            }, 
            &ctx
        );

        CFRunLoopAddObserver(_p->inputReportRunLoop, observer, runLoopMode);

        // Start runLoop
        //  Calling this blocks this thread and until the runLoop exits.
        //  We only exit the runLoop if one of these happens
        //      - Device sends input report
        //      - Timeout happens
        //      - interruptRead() is called
        //
        // See HIDAPI https://github.com/libusb/hidapi/blob/master/mac/hid.c for reference

        while (true) {

            // Run runLoop

            CFRunLoopRunResult runLoopResult = CFRunLoopRunInMode(runLoopMode, 1000 /*sec*/, false); 

            // Analyze runLoop exit reason

            std::string runLoopResultString;
            if (runLoopResult == kCFRunLoopRunFinished) {
                runLoopResultString = "Finished";
            } else if (runLoopResult == kCFRunLoopRunHandledSource) {
                runLoopResultString = "HandledSource";
            } else if (runLoopResult == kCFRunLoopRunStopped) {
                runLoopResultString = "Stopped";
            } else if (runLoopResult == kCFRunLoopRunTimedOut) {
                runLoopResultString = "TimedOut";
            } else {
                runLoopResultString = "UnknownResult";
            }
            Log::debug() << "inputReportRunLoop exited with result: " << runLoopResultString << std::endl;

            // Exit condition

            if (runLoopResult == kCFRunLoopRunFinished // Not sure if this makes sense. I've never seen the result be `finished`
                || runLoopResult == kCFRunLoopRunStopped) { 
                break;
            }

            Log::debug() << "Restarting runloop" << std::endl;
        }

        // Free reportBuffer
        free(reportBuffer);

        // Tear down runLoop 
        //  Edit: disabling for now since it accesses _p which can lead to crash when RunLoopStop() is called from the deconstructor
        return;

        Log::debug() << "Tearing down runloop" << std::endl;

        // Unregister input report callback
        //  This is probably unnecessary
        uint8_t nullReportBuffer[0];
        CFIndex nullReportLength = 0;
        IOHIDDeviceRegisterInputReportCallback(_p->iohidDevice, nullReportBuffer, nullReportLength, NULL, NULL); 
        //  ^ Passing NULL for the callback unregisters the previous callback.
        //      Not sure if redundant when already calling IOHIDDeviceUnscheduleFromRunLoop.

        // Remove device from runLoop
        //  This is probably unnecessary since we're already stopping the runLoop via CFRunLoopStop()
        IOHIDDeviceUnscheduleFromRunLoop(_p->iohidDevice, _p->inputReportRunLoop, kCFRunLoopCommonModes);
    }

};


// Private constructor

RawDevice::RawDevice() 
    : _p(std::make_unique<PrivateImpl>())
{

}

// Copy constructor

RawDevice::RawDevice(const RawDevice &other) : _p(std::make_unique<PrivateImpl>()),
                                               _vendor_id(other._vendor_id), _product_id(other._product_id),
                                               _name(other._name),
                                               _report_desc(other._report_desc)
{

    // Don't use this
    //  I don't think it makes sense for RawDevice to be copied, since it's a wrapper for a real physical device that only exists once.
    std::__throw_bad_function_call();
    
    // Lock
    std::unique_lock lock1(_p->mutexLock);
    std::unique_lock lock2(other._p->mutexLock); // Probably not necessary

    // Copy attributes from `other` to `this`

    io_service_t service = IOHIDDeviceGetService(other._p->iohidDevice);
    _p->iohidDevice = IOHIDDeviceCreate(kCFAllocatorDefault, service);
    // ^ Copy iohidDevice. I'm not sure this way of copying works
    _p->maxInputReportSize = other._p->maxInputReportSize;
    _p->maxOutputReportSize = other._p->maxOutputReportSize;
    _p->inputQueue = dispatch_queue_create(_p->getDispatchQueueLabel(this).c_str(), _p->getDispatchQueueAttrs());

    // Reset state
    _p->initState(this);
}

// Move constructor

RawDevice::RawDevice(RawDevice &&other) : _p(std::make_unique<PrivateImpl>()),
                                          _vendor_id(other._vendor_id), _product_id(other._product_id),
                                          _name(std::move(other._name)),
                                          _report_desc(std::move(other._report_desc))
{
    // How to write move constructor: https://stackoverflow.com/a/43387612/10601702

    // Lock
    std::unique_lock lock1(_p->mutexLock);
    std::unique_lock lock2(other._p->mutexLock); // Probably not necessary

    // Assign values from `other` to `this` (without copying them)

    _p->iohidDevice = other._p->iohidDevice;
    _p->maxInputReportSize = other._p->maxInputReportSize;
    _p->maxOutputReportSize = other._p->maxOutputReportSize;
    _p->inputQueue = other._p->inputQueue;

    // Init state
    _p->initState(this);

    // Delete values in `other` 
    //  (so that it can't manipulate values in `this` through dangling references)
    other._p->nullifyValues(&other);
}

// Destructor

RawDevice::~RawDevice(){

    // Lock
    std::unique_lock lock(_p->mutexLock);

    // Debug
    Log::debug() << "Destroying device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl; 

    // Stop inputQueue
    _p->stopReadThread(this);

    // Close IOHIDDevice
    IOHIDDeviceClose(_p->iohidDevice, kIOHIDOptionsTypeNone); // Not sure if necessary
    CFRelease(_p->iohidDevice); // Not sure if necessary
}

// Main constructor

RawDevice::RawDevice(const std::string &path) : _p(std::make_unique<PrivateImpl>()) {
    // Construct device from path

    // Init pimpl
    _p->initState(this);

    // Lock
    //  This only unlocks once the readThread has set up its runLoop
    std::unique_lock lock(_p->mutexLock);

    // Declare vars
    kern_return_t kr;
    IOReturn ior;

    // Convert path to registryEntryId (int64_t)
    int64_t entryID = std::stoll(path);

    CFDictionaryRef matchDict = IORegistryEntryIDMatching(entryID);
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, matchDict);
    if (service == 0) { // Idk what this returns when it can't find a matching service

    }

    // Create IOHIDDevice from service
    IOHIDDeviceRef device = IOHIDDeviceCreate(kCFAllocatorDefault, service);

    // Store device
    _p->iohidDevice = device;

    // Open device
    ior = IOHIDDeviceOpen(_p->iohidDevice, kIOHIDOptionsTypeNone); // Necessary to change the state of the device
    if (ior != kIOReturnSuccess) {
        Log::warning() << "Opening the device \"" << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << "\" failed with error code " << ior << std::endl;
        
    } else {
        // Log::info().printf("Opening the device \"%s\" succeded", Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice));
        // ^ printf() function on Log doesn't work properly here for some reason. Shows up in vscode debug console but not in Terminal, even with the -vdebug option. Logging with the << syntax instead of printf works though. In Utility_macos the printf function works for some reason. TODO: Ask Clément about this.
        Log::info() << "Opening the device \"" << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << "\" succeded" << std::endl;
    }

    // Store IOHIDDevice in self
    _p->iohidDevice = device;

    // Fill out public member variables

    _vendor_id = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDVendorIDKey));
    _product_id = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDProductIDKey));
    _name = Utility_macos::IOHIDDeviceGetStringProperty(device, CFSTR(kIOHIDProductKey));

    try { // Try-except copied from the Linux implementation
        _report_desc = Utility_macos::IOHIDDeviceGetReportDescriptor(device);
        logReportDescriptor();
    } catch (std::exception &e) {
        Log::error () << "Invalid report descriptor: " << e.what () << std::endl;
    }

    // Fill out private member variables
    _p->maxInputReportSize = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDMaxInputReportSizeKey));
    _p->maxOutputReportSize = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDMaxOutputReportSizeKey));

    // Create dispatch queue
    dispatch_queue_attr_t attrs = _p->getDispatchQueueAttrs();
    std::string queueLabel = _p->getDispatchQueueLabel(this);
    _p->inputQueue = dispatch_queue_create(queueLabel.c_str(), attrs);

    // Start listening to input on queue
    dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 0.0), _p->inputQueue, this, [](void *context) {
        RawDevice *thisss = (RawDevice *)context;
        thisss->_p->readThread(thisss);
    });

    // Wait until inputReportRunLoop Started
    while (!_p->inputRunLoopIsRunning) {
        _p->inputRunLoopStarted.wait(lock);
    }

    // Debug
    Log::debug() << "Constructed device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;
}

// Interface

// writeReport
//  See https://developer.apple.com/library/archive/technotes/tn2187/_index.html
//      for info on how to use IOHID input/output report functions and more.

int RawDevice::writeReport(const std::vector<uint8_t> &report)
{

    // Lock
    std::unique_lock lock(_p->mutexLock);

    Log::debug() << "writeReport called on " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

    // Guard report size
    if (report.size() > _p->maxOutputReportSize) {
        // TODO: Return meaningful error
        return 1;
    }

    // Gather args for report sending
    IOHIDDeviceRef device = _p->iohidDevice;
    IOHIDReportType reportType = kIOHIDReportTypeOutput;
    CFIndex reportID = report[0];
    const uint8_t *rawReport = report.data();
    CFIndex reportLength = report.size();

    // Send report
    IOReturn r = IOHIDDeviceSetReport(device, reportType, reportID, rawReport, reportLength);

    // Return error code
    if (r != kIOReturnSuccess) {
        // TODO: Return some meaningful error code
        return r;
    }

    // Return success
    return 0;
}

// readReport

int RawDevice::readReport(std::vector<uint8_t> &report, int timeout) {

    // Debug
    timeout = 5000;

    // Lock
    std::unique_lock lock(_p->mutexLock);

    // Debug
    Log::debug() << "readReport called on " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

    // Define constant
    double lookbackThreshold = 1 / 100.0; // If a report occured no more than `lookbackThreshold` seconds ago, then we use it.

    // Convert timeout to seconds instead of milliseconds
    double timeoutSeconds = timeout / 1000.0;

    // Wait for input
    //  Block this thread until the next inputReport is issued
    //  We only stop waiting if one of these happens
    //      - Device sends input report
    //      - Timeout happens
    //      - interruptRead() is called
    //
    //  Use _p->lastInputReport instead of waiting - if it's been less than "lookbackThreshold" since the lastInputReport occured
    //      The hidpp library expects this function to only receive inputReports that are issued after we start reading. But sometimes starting to read takes a fraction too long making us miss events.
    //      I think every mouse movement creates input reports - can that lead to problems?

    double lastInputReportTimeBeforeWaiting = _p->lastInputReportTime;

    if ((Utility_macos::timestamp() - lastInputReportTimeBeforeWaiting) <= lookbackThreshold) {
        // Last receive report is still fresh enough. Return that instead of waiting.
        Log::debug() << "Recent event already queued up for device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;
    } else {
        // Wait for next input report

        // Init loop state
        std::cv_status timeoutStatus = std::cv_status::no_timeout;
        auto timeoutTime = std::chrono::system_clock::now() + std::chrono::duration<double, std::ratio<1>>(timeoutSeconds); // Point in time until which to wait for input

        // Wait for report in a loop
        while (true) {
            
            // Check state
            //  We check state first (instead of waiting first) to minimize race cond where a new _p->lastInputReport comes in before we reach here but after deciding that we wait for the next report. Should probably use a mutex instead. If this race cond occurs, we miss a report.
            bool newEventReceived = _p->lastInputReportTime > lastInputReportTimeBeforeWaiting;
            bool timedOut = timeoutStatus == std::cv_status::timeout ? true : false; // ? Possible race condition if the wait is interrupted due to spurious wakeup but then the timeoutTime is exceeded before we reach here. But this is very unlikely and doesn't have bad consequences.
            bool interrupted = _p->waitingForInputWasInterrupted; 

            // Update state
            _p->waitingForInputWasInterrupted = false;

            // Stop waiting
            if (newEventReceived || timedOut || interrupted) {

                // Debug state
                if (newEventReceived + timedOut + interrupted > 1) {
                    Log::warning() << "Waiting for inputReport was interrupted with WEIRD state: newReport: " << newEventReceived << " timedOut: " << timedOut << " interrupted: " << interrupted << std::endl;
                } else {
                    Log::debug() << "Waiting for inputReport stopped with state: newReport: " << newEventReceived << " timedOut: " << timedOut << " interrupted: " << interrupted << std::endl;
                }

                // Break loop
                break;
            }

            // Debug
            Log::debug() << "Wait for device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

            // Ignore read
            //  Theres a race condition if ignoreNextRead is set to true after this func checks for _p->ignoreNextRead (its set in interruptRead()) but before this func starts wait for input. But this is super unlikely and doesn't have bad consequences.
            if (_p->ignoreNextRead) {
                _p->ignoreNextRead = false;
                return 0;
            }
            // Wait
            _p->waitingForInput = true; // Should only be mutated right here.
            timeoutStatus = _p->stopWaitingForInput.wait_until(lock, timeoutTime); // Maybe we should use an extra lock here that just synchronizes readReport() and the readThread()? Might be faster and prevent deadlocks. (In case there are any deadlocks)
            _p->waitingForInput = false;
        }
    }

    // Get return values

    int returnValue;

    if ((Utility_macos::timestamp() - _p->lastInputReportTime) <= lookbackThreshold) { // Reading was successful

        // Write result to the `report` argument and return length
        report = _p->lastInputReport;
        returnValue = report.size();

    } else { // Reading was interrupted or timedOut
        returnValue = 0;
    }

    // Reset return values
    //  Not sure if necessary. Theoretically the `lookbackTreshold` should be enough
    _p->lastInputReport = std::vector<uint8_t>();
    _p->lastInputReportTime = 0;

    // Return
    return returnValue;
}

void RawDevice::interruptRead() {

    // Maybe we should use _p->mutexLock here? Make sure there are no race conditions!
    //  Edit: I've thought about it and there are some minor race conds but they aren't a problem I think.

    // Debug
    Log::debug() << "interruptRead called on " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

    // Do stuff

    if (_p->waitingForInput) {

    // Stop readReport() from waiting, if it's waiting

        _p->waitingForInputWasInterrupted = true;
        _p->stopWaitingForInput.notify_one();
    } else {
        // If readReport() is not currently waiting, ignore the next call to readReport()
        //  This is the expected behaviour according to https://github.com/cvuchener/hidpp/issues/17#issuecomment-896821785

        _p->ignoreNextRead = true;
    }
}