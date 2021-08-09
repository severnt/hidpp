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
    IOHIDDeviceRef iohidDevice;

    CFIndex maxInputReportSize;
    CFIndex maxOutputReportSize;
    CFRunLoopRef inputReportRunLoop;

    std::mutex inputReportMutex;
    std::condition_variable inputReportBlocker;
    bool waitingForInputReport; 
    //  ^ Using primitive method for blocking thread instead of inputReportBlocker for debugging. Remove this once inputReportBlocker works.

    std::thread runLoopThread;
};

// Private constructor

RawDevice::RawDevice() 
    : _p(std::make_unique<PrivateImpl>())
{

}

// Public constructors

RawDevice::RawDevice(const std::string &path) : _p(std::make_unique<PrivateImpl>()) {
    // Construct device from path

    // Declare vars
    kern_return_t kr;

    // Convert path to IOKit
    io_string_t ioPath;
    Utility_macos::stringToIOString(path, ioPath);

    // Get registryEntry from path
    const io_registry_entry_t registryEntry = IORegistryEntryFromPath(kIOMasterPortDefault, ioPath);

    // Get service from registryEntry
    uint64_t entryID;
    kr = IORegistryEntryGetRegistryEntryID(registryEntry, &entryID);
    if (kr != KERN_SUCCESS) {
        // TODO: Throw an error or something
    }
    CFDictionaryRef matchDict = IORegistryEntryIDMatching(entryID);
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, matchDict);
    // ^ TODO: Check that the service is valid

    // Create IOHIDDevice from service
    IOHIDDeviceRef device = IOHIDDeviceCreate(kCFAllocatorDefault, service);

    // Store device
    _p->iohidDevice = device;

    // Open device
    IOReturn ior = IOHIDDeviceOpen(_p->iohidDevice, kIOHIDOptionsTypeNone); // Necessary to change the state of the device
    if (ior != kIOReturnSuccess) {
        // TODO: Throw and error or something
    }

    // Store IOHIDDevice in self
    _p->iohidDevice = device;

    // Fill out public member variables
    _vendor_id = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDVendorIDKey));
    _product_id = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDProductIDKey));
    _name = Utility_macos::IOHIDDeviceGetStringProperty(device, CFSTR(kIOHIDProductKey));
    _report_desc = Utility_macos::IOHIDDeviceGetReportDescriptor(device);

    // Fill out private member variables
    _p->maxInputReportSize = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDMaxInputReportSizeKey));
    _p->maxOutputReportSize = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDMaxOutputReportSizeKey));

    // TODO: Release stuff
}

RawDevice::RawDevice(const RawDevice &other) : _p(std::make_unique<PrivateImpl>()),
                                               _vendor_id(other._vendor_id), _product_id(other._product_id),
                                               _name(other._name),
                                               _report_desc(other._report_desc)
{
    // I don't know what this is supposed to to
}

RawDevice::RawDevice(RawDevice &&other) : // What's the difference between this constructor and the one above?

                                          _p(std::make_unique<PrivateImpl>()),
                                          _vendor_id(other._vendor_id), _product_id(other._product_id),
                                          _name(std::move(other._name)),
                                          _report_desc(std::move(other._report_desc))
{
    // I don't know what this is supposed to to
}

// Destructor

RawDevice::~RawDevice(){
    Utility_macos::stopListeningToInputReports(_p->iohidDevice, _p->inputReportRunLoop);
    IOHIDDeviceClose(_p->iohidDevice, kIOHIDOptionsTypeNone); // Not sure if necessary
    CFRelease(_p->iohidDevice); // Not sure if necessary
}

// Interface

// writeReport
//  See https://developer.apple.com/library/archive/technotes/tn2187/_index.html
//      for info on how to use IOHID input/output report functions and more.

int RawDevice::writeReport(const std::vector<uint8_t> &report)
{

    // Guard report size
    if (report.size() > _p->maxOutputReportSize) {
        // TODO: Return error
    }

    // Gather args for report sending
    IOHIDDeviceRef device = _p->iohidDevice;
    IOHIDReportType reportType = kIOHIDReportTypeOutput; // Not sure if correct
    CFIndex reportID = report[0];
    const uint8_t *rawReport = report.data();
    CFIndex reportLength = report.size();

    // Send report
    IOReturn r = IOHIDDeviceSetReport(device, reportType, reportID, rawReport, reportLength);

    // Return error code
    if (r != kIOReturnSuccess) {
        // TODO: Return some meaningful error code
    }

    // Return success
    return 0;
}

// readReport

int RawDevice::readReport(std::vector<uint8_t> &report, int timeout) {

    // Inflate timeout for debugging
    timeout *= 20;

    // Convert timeout to seconds (instead of milliseconds)
    double timeoutSeconds = timeout / 1000.0;

    // Get runLoop on a new thread
    //  We want to block the current thread to wait for the report to be read. 
    //  But that will block the runLoop if the runLoop belongs to the current thread, which will prevent the inputReportCallback from firing.
    //      (I think so at least)
    //  So we need the runLoop to belong to a different thread.
    _p->inputReportRunLoop = NULL;
    CFRunLoopTimerRef runLoopTimer = CFRunLoopTimerCreate(kCFAllocatorDefault, DBL_MAX, DBL_MAX, 0, 0, NULL, NULL);
    bool runLoopThreadIsDone = false;
    _p->runLoopThread = std::thread([this, runLoopTimer, &runLoopThreadIsDone]() {
        // Get current runLoop
        this->_p->inputReportRunLoop = CFRunLoopGetCurrent();
        // Add timer so the runLoop doesn't immediately exit when we run it
        CFRunLoopAddTimer(this->_p->inputReportRunLoop, runLoopTimer, kCFRunLoopCommonModes);

        // Signal
        runLoopThreadIsDone = true;

        // Run the runLoop for this thread. 
        //  Calling this seems to block this thread and prevent runLoopThreadIsDone = true from being called
        //   So CFRunLoopRun should be called last, but then there will be race conditions, because runLoopThreadIsDone will be set to true before it's actually done.
        CFRunLoopRun();

        // CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
        // ^ Alternative to CFRunLoopRun(). Does one pass of the loop and then return immediately.
        //      This will allow runLoopThreadIsDone = true to run.
        //      I hope this will "kick off" the runLoop, and then hopefully it will start running again automatically when IOHIDDeviceScheduleWithRunLoop is called?
        //      -> Doesn't seem to work. According to the debugger, this thread (and with it its runLoop) don't seem to exist anymore when we reach IOHIDDeviceScheduleWithRunLoop, making IOHIDDeviceScheduleWithRunLoop crash.
        //          But how can that be - even though the runLoopTimer is still scheduled to fire at that point and even though we still hold a reference to the runLoopThread object?
    });

    // Wait this thread until runLoop is obtained

    //  runLoopThread.join() doesn't work for some reason
    while (!runLoopThreadIsDone) { } 
    // ^ Doesn't work properly because runLoopThreadIsDone = true; is never called or called too early (see above)

    // while (_p->inputReportRunLoop == NULL
    //     || !CFRunLoopIsWaiting(_p->inputReportRunLoop)) { }
    // ^ Alternative method for checking if runLoopThreadIsDone. Not sure if it works.

    // Schedule device with runloop
    //  The inputReportCallback for this device will be driven by this runLoop
    IOHIDDeviceScheduleWithRunLoop(_p->iohidDevice, _p->inputReportRunLoop, kCFRunLoopCommonModes);

    // Allocate buffers
    uint8_t reportBuffer[_p->maxInputReportSize];
    memset(reportBuffer, 0, _p->maxInputReportSize); // Init with 0s
    CFIndex reportBufferLength = -1;

    // Init primitive method for waiting for input report

    _p->waitingForInputReport = true;

    // Get report
    //  IOHIDDeviceGetReportWithCallback has a built-in timeout and allows you to specify IOHIDReportType, 
    //      but Apple docs say it should only be used for feature reports. So we're using 
    //      IOHIDDeviceRegisterInputReportCallback instead.
    IOHIDDeviceRegisterInputReportCallback(
        _p->iohidDevice, 
        reportBuffer, 
        reportBufferLength, 
        [] (void *context, IOReturn result, void *sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength) {

            RawDevice *thisss = static_cast<RawDevice *>(context); //  Get `this` from context
            //  ^ We can't capture `this`, because then the enclosing lambda wouldn't decay to a pure c function
            thisss->_p->inputReportBlocker.notify_all(); // Report was received -> stop waiting for report
            thisss->_p->waitingForInputReport = false;
        }, 
        this // Pass `this` to context
    );

    // Remove timer from runLoop
    //  We only added this timer so the runLoop wouldn't exit immediately after running it.
    //  Now that we've added the inputReportCallback to the runLoop, we don't need the timer anymore.
    //  (The runLoop will exit when it has nothing to do anymore)
    CFRunLoopRemoveTimer(_p->inputReportRunLoop, runLoopTimer, kCFRunLoopCommonModes);

    // Wake up runLoop. Not sure if necessary.
    CFRunLoopWakeUp(_p->inputReportRunLoop);

    // Loop-based waiting
    //  Use a more primitive method of waiting for input to help debugging. Remove once lock-based waiting works
    double startOfWait = Utility_macos::timestamp();
    
    while (true) {
        if (!_p->waitingForInputReport) { 
            break; 
        };
        if (0 <= timeout) { // Only time out if `timeout` is non-negative
            double now = Utility_macos::timestamp();
            if ((now - startOfWait) > timeoutSeconds) { 
                break; 
            }
        }
    }

    // Lock-based waiting

    // Create lock for waiting
    // std::unique_lock<std::mutex> lock(_p->inputReportMutex); // I think this also locks the lock. Lock needs to be locked for inputReportBlocker to work.
    // lock.lock();

    // Wait until on of these happens
    //  - Device sends input report
    //  - Timeout happens
    //  - interruptRead() is called

    // if (timeout < 0) { // Negative `timeout` means no timeout
    //     _p->inputReportBlocker.wait(lock);
    // } else {
    //     std::chrono::milliseconds cppTimeout(timeout);
    //     _p->inputReportBlocker.wait_for(lock, cppTimeout);
    // }

    // Stop listening for reports
    Utility_macos::stopListeningToInputReports(_p->iohidDevice, _p->inputReportRunLoop);

    if (reportBufferLength == -1) { // Reading has timed out or was interrupted

        return 0;

        // TODO: Return some meaningful error code or something

    } else { // Reading was successful

        // Write result to the `report` argument
        int assignLength = report.size() < reportBufferLength ? report.size() : reportBufferLength;
        report.assign(reportBuffer, reportBuffer + assignLength);
        // Return
        return reportBufferLength;
    }
}

void RawDevice::interruptRead() {
    _p->inputReportBlocker.notify_all(); // Stop waiting for report
    _p->waitingForInputReport = false;
}