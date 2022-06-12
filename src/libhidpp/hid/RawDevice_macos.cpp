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

// Pimpl

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
    std::atomic<bool> inputRunLoopDidStart;

    static void initState(RawDevice *dev) {

        dev->_p->inputReportRunLoop = nullptr;
        dev->_p->lastInputReport = std::vector<uint8_t>();

        dev->_p->ignoreNextRead = false;
        dev->_p->waitingForInput = false;
        dev->_p->lastInputReportTime = -1; 
        dev->_p->waitingForInputWasInterrupted = false;
        dev->_p->inputRunLoopDidStart = false;
    }

    // Concurrency

    std::mutex generalLock;
    std::condition_variable shouldStopWaitingForInputSignal;
    std::condition_variable inputRunLoopDidStartSignal;

    // Dispatch queue config

    static dispatch_queue_attr_t getInputReportQueueAttrs() {
        return dispatch_queue_attr_make_with_qos_class(NULL, QOS_CLASS_USER_INITIATED, -1);
    }
    static std::string getInputReportQueueLabel(RawDevice *dev) {
        const char *prefix = "com.cvuchener.hidpp.input-reports.";
        std::string debugID = Utility_macos::IOHIDDeviceGetDebugIdentifier(dev->_p->iohidDevice);
        char queueLabel[strlen(prefix) + strlen(debugID.c_str())];
        sprintf(queueLabel, "%s%s", prefix, debugID.c_str());
        return std::string(queueLabel);
    }

    // Reset input buffer

    static void deleteInputBuffer(RawDevice *dev) {
        dev->_p->lastInputReport = std::vector<uint8_t>();
        dev->_p->lastInputReportTime = 0;
    }

    // Read input reports
    //  Read thread should be active throughout the lifetime of this RawDevice

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

                // Lock
                std::unique_lock lock(devvv->_p->generalLock);

                // Debug
                Log::debug() << "Received input from device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(devvv->_p->iohidDevice) << std::endl;

                // Store new report
                devvv->_p->lastInputReport.assign(report, report + reportLength);
                devvv->_p->lastInputReportTime = Utility_macos::timestamp();

                // Notify waiting thread
                devvv->_p->shouldStopWaitingForInputSignal.notify_one(); // We assume that there's only one waiting thread

            }, 
            dev // Pass `dev` to context
        );

        // Start runLoop

        // Params

        CFRunLoopMode runLoopMode = kCFRunLoopDefaultMode;

        // Store current runLoop      

        _p->inputReportRunLoop = CFRunLoopGetCurrent();

        // Add IOHIDDevice to runLoop.

        //  Async callbacks for this IOHIDDevice will be delivered to this runLoop
        //  We need to call this before CFRunLoopRun, because if the runLoop has nothing to do, it'll immediately exit when we try to run it.
        IOHIDDeviceScheduleWithRunLoop(_p->iohidDevice, _p->inputReportRunLoop, runLoopMode);

        // Debug
        
        Log::debug() << "Starting inputRunLoop on device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

        // Observer

        CFRunLoopObserverContext ctx = {
            .version = 0,
            .info = dev,
            .retain = NULL,
            .release = NULL,
            .copyDescription = NULL,
        };

        CFRunLoopObserverRef observer = CFRunLoopObserverCreate(
            kCFAllocatorDefault, 
            kCFRunLoopEntry | kCFRunLoopExit, 
            false, 
            0, 
            [](CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info){
                RawDevice *devvv = (RawDevice *)info;

                if (activity == kCFRunLoopEntry) {
                    devvv->_p->inputRunLoopDidStart = true;
                    devvv->_p->inputRunLoopDidStartSignal.notify_one();
                } else {
                    devvv->_p->inputRunLoopDidStart = false; // Not sure if useful
                }
            }, 
            &ctx
        );

        CFRunLoopAddObserver(_p->inputReportRunLoop, observer, runLoopMode);

        // Start runLoop
        //  Calling this blocks this thread and until the runLoop exits.
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
    std::unique_lock lock1(_p->generalLock);
    std::unique_lock lock2(other._p->generalLock); // Probably not necessary

    // Copy attributes from `other` to `this`

    io_service_t service = IOHIDDeviceGetService(other._p->iohidDevice);
    _p->iohidDevice = IOHIDDeviceCreate(kCFAllocatorDefault, service);
    // ^ Copy iohidDevice. I'm not sure this way of copying works
    _p->maxInputReportSize = other._p->maxInputReportSize;
    _p->maxOutputReportSize = other._p->maxOutputReportSize;
    _p->inputQueue = dispatch_queue_create(_p->getInputReportQueueLabel(this).c_str(), _p->getInputReportQueueAttrs());

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
    std::unique_lock lock1(_p->generalLock);
    std::unique_lock lock2(other._p->generalLock); // Probably not necessary

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
    std::unique_lock lock(_p->generalLock);

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

    // Lock
    //  This only unlocks once the readThread has set up its runLoop
    std::unique_lock lock(_p->generalLock);

    // Init pimpl
    _p->initState(this);

    // Declare vars
    kern_return_t kr;
    IOReturn ior;

    // Convert path to registryEntryId (int64_t)

    // uint64_t entryID = std::stoll(path);
    uint64_t entryID = std::strtoull(path.c_str() + strlen(Utility_macos::pathPrefix.c_str()), NULL, 10);

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
    _p->inputQueue = dispatch_queue_create(_p->getInputReportQueueLabel(this).c_str(), _p->getInputReportQueueAttrs());

    // Start listening to input on queue
    dispatch_async_f(_p->inputQueue, this, [](void *context) {
        RawDevice *thisss = (RawDevice *)context;
        thisss->_p->readThread(thisss);
    });

    // Wait until inputReportRunLoop Started
    while (!_p->inputRunLoopDidStart) {
        _p->inputRunLoopDidStartSignal.wait(lock);
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
    std::unique_lock lock(_p->generalLock);

    // Debug
    Log::debug() << "writeReport called on " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

    // Reset input buffer
    //  So we don't read after this expecting a response, but getting some old value from the buffer
    _p->deleteInputBuffer(this);

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

    // Lock
    std::unique_lock lock(_p->generalLock);

    // Debug
    Log::debug() << "readReport called on " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

    // Interrupt read
    if (_p->ignoreNextRead) {

        // Reset input buffer
        //  So we don't later return the value that's queued up now. Not sure if necessary.
        _p->deleteInputBuffer(this);

        // Reset ignoreNextRead flag
        _p->ignoreNextRead = false;

        // Return invalid
        return 0;
    }

    // Define constant
    double lookbackThreshold = 1 / 100.0; // If a report occured no more than `lookbackThreshold` seconds ago, then we use it.

    // Preprocess timeout
    double timeoutSeconds;
    if (timeout < 0) {
        // Disable timeout if negative
        timeoutSeconds = INFINITY;
    } else {
        // Convert timeout to seconds instead of milliseconds
        timeoutSeconds = timeout / 1000.0;
    }

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

        // Last received report is still fresh enough. Return that instead of waiting.
        Log::debug() << "Recent event already queued up for device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;
    } else {
        // Wait for next input report

        // Init loop state
        std::cv_status timeoutStatus = std::cv_status::no_timeout;
        auto timeoutTime = std::chrono::system_clock::now() + std::chrono::duration<double, std::ratio<1>>(timeoutSeconds); // Point in time until which to wait for input

        // Wait for report in a loop
        while (true) {
            
            // Debug
            Log::debug() << "Wait for device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

            // Wait
            _p->waitingForInput = true; // Should only be mutated right here.
            timeoutStatus = _p->shouldStopWaitingForInputSignal.wait_until(lock, timeoutTime);
            _p->waitingForInput = false;

            // Check state
            bool newEventReceived = _p->lastInputReportTime > lastInputReportTimeBeforeWaiting;
            bool timedOut = timeoutStatus == std::cv_status::timeout ? true : false; // ? Possible race condition if the wait is interrupted due to spurious wakeup but then the timeoutTime is exceeded before we reach here. But this is very unlikely and doesn't have bad consequences.
            bool interrupted = _p->waitingForInputWasInterrupted; 

            // Update state
            _p->waitingForInputWasInterrupted = false;

            // Stop waiting
            if (newEventReceived || timedOut || interrupted) {

                // Debug state
                if (newEventReceived + timedOut + interrupted > 1) {
                    Log::warning() << "Waiting for inputReport was stopped with WEIRD state: newReport: " << newEventReceived << " timedOut: " << timedOut << " interrupted: " << interrupted << std::endl;
                } else {
                    Log::debug() << "Waiting for inputReport stopped with state: newReport: " << newEventReceived << " timedOut: " << timedOut << " interrupted: " << interrupted << std::endl;
                }

                // Break loop
                break;
            }
        }
    }

    // Get return values

    int returnValue;

    if ((Utility_macos::timestamp() - _p->lastInputReportTime) <= lookbackThreshold) { 
        // ^ Reading was successful. Not sure if this is the best way to check if reading was successful. It might be more robust than using the `newEventReceived` flag.
        //  Edit: Getting a new timestamp here might lead to race condition if lastInputReport was determined to be "fresh enough" above but now is not "fresh enough" anymore. 

        // Write result to the `report` argument and return length
        report = _p->lastInputReport;
        returnValue = report.size();

    } else { // Reading was interrupted or timedOut
        returnValue = 0;
    }

    // Reset input buffer
    //  Not sure if necessary. Theoretically the `lookbackTreshold` should be enough. Edit: Why would `lookbackTreshold` be enough? Coudn't there be a problem where we read the same report twice, if we don't do this?
    _p->deleteInputBuffer(this);

    // Return
    return returnValue;
}

void RawDevice::interruptRead() {

    std::unique_lock lock(_p->generalLock);

    // Debug
    Log::debug() << "interruptRead called on " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

    // Do stuff

    if (_p->waitingForInput) {

        // Stop readReport() from waiting, if it's waiting
        _p->waitingForInputWasInterrupted = true;
        _p->shouldStopWaitingForInputSignal.notify_one();

    } else {
        _p->ignoreNextRead = true;
        // ^ If readReport() is not currently waiting, we ignore the next call to readReport()
        //      This is the expected behaviour according to https://github.com/cvuchener/hidpp/issues/17#issuecomment-896821785
    }
}
