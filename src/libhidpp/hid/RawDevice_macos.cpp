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
    CFIndex lastInputReportLength;

    CFRunLoopRef inputReportRunLoop;

    std::mutex inputReportMutex;
    // std::condition_variable inputReportBlocker;
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

RawDevice::RawDevice(RawDevice &&other) : _p(std::make_unique<PrivateImpl>()),
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
    IOHIDReportType reportType = kIOHIDReportTypeOutput;
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
    timeout *= 10;

    // Convert timeout to seconds instead of milliseconds
    double timeoutSeconds = timeout / 1000.0;

    // Get runLoop

    bool runLoopIsSetUp = false;

    if (this->_p->inputReportRunLoop != NULL) {

        // There's already a running runLoop for this device.
        //  This probably means that another attempt to readReport's is already in progress
        //  Not sure what to do in this case

        Log::warning () << "Requested input report with a runLoop already running" << std::endl;

        runLoopIsSetUp = true;

    } else {

        // Setup runLoop on a new thread
        //  We want to block the current function to wait for the inputReport to be read. 
        //  But that will also block the runLoop if the runLoop belongs to the current thread, which will prevent the inputReportCallback from firing.
        //  So we need the runLoop to belong to a different thread.

        _p->runLoopThread = std::thread([this, &runLoopIsSetUp]() {

            // Get runLoop
            this->_p->inputReportRunLoop = CFRunLoopGetCurrent();;

            // Add IOHIDDevice to runLoop.
            //  Async callbacks for this IOHIDDevice will be delivered to this runLoop
            //  We need to call this before CFRunLoopRun, because if the runLoop has nothing to do, it'll immediately exit when we try to run it.
            IOHIDDeviceScheduleWithRunLoop(_p->iohidDevice, _p->inputReportRunLoop, kCFRunLoopCommonModes);

            // Signal
            runLoopIsSetUp = true;

            // Start runLoop
            //  Calling this blocks this thread until the runLoop exits.
            CFRunLoopRun();

            // Delete runLoop
            //  By setting to NULL after the runLoop exits, we can see whether or not there's a running runLoop for this device from other places.
            this->_p->inputReportRunLoop = NULL;
        });

        // TODO: You should always either detach or join a thread
        // _p->runLoopThread.detach();
    }

    // Wait for runLoop setup

    // Loop-based waiting
    while (!runLoopIsSetUp) { } 
    // ^ This is not optimal because:
    //      1. It will unblock slightly before CFRunLoopRun() is called. Probably not an issue though, hidapi also does it like this.
    //      2. It's looping, not using a callback, which might be bad for performance.
    //      A better solution would be using a thread lock and to observe when the runLoop starts running with a callback.

    // Lock-based waiting
    //  TODO

    // Setup reportBuffer
    CFIndex reportBufferSize = _p->maxInputReportSize;
    uint8_t reportBuffer[reportBufferSize];
    memset(reportBuffer, 0, reportBufferSize); // Init with 0s

    // Init lastInputReportLength
    //  We override this in the reportCallback. If it hasn't been overriden once we exit this function, we know that the read has failed.
    //  Should only be used by this function.
    _p->lastInputReportLength = -1;

    // Init primitive method for waiting for input report
    _p->waitingForInputReport = true;

    // Get report
    //  IOHIDDeviceGetReportWithCallback has a built-in timeout and allows you to specify IOHIDReportType, 
    //      but Apple docs say it should only be used for feature reports. So we're using 
    //      IOHIDDeviceRegisterInputReportCallback instead.
    IOHIDDeviceRegisterInputReportCallback(
        _p->iohidDevice, 
        reportBuffer, 
        _p->maxInputReportSize,
        [] (void *context, IOReturn result, void *sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength) {

            RawDevice *thisss = static_cast<RawDevice *>(context); //  Get `this` from context
            //  ^ We can't capture `this`, because then the enclosing lambda wouldn't decay to a pure c function
            // thisss->_p->inputReportBlocker.notify_all(); // Report was received -> stop waiting for report
            thisss->_p->waitingForInputReport = false;
            thisss->_p->lastInputReportLength = reportLength;
        }, 
        this // Pass `this` to context
    );

    // Wake up runLoop. Not sure if necessary.
    CFRunLoopWakeUp(_p->inputReportRunLoop);

    // Wait for input report

    // Loop-based waiting
    //  Using a simpler method of waiting for input to help debugging. Move to lock-based waiting once everything else works.

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

    if (_p->lastInputReportLength == -1) { // Reading has timed out or was interrupted

        return 0;

        // TODO: Return some meaningful error code or something

    } else { // Reading was successful

        // Write result to the `report` argument
        int assignLength = report.size() < _p->lastInputReportLength ? report.size() : _p->lastInputReportLength;
        report.assign(reportBuffer, reportBuffer + assignLength);
        // Return
        return reportBufferSize;
    }
}

void RawDevice::interruptRead() {
    // _p->inputReportBlocker.notify_all(); // Stop waiting for report
    _p->waitingForInputReport = false;
}