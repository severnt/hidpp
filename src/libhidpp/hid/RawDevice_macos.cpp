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
    bool readIsBlocking;
    bool preventNextRead;
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
    IOReturn ior;

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
    if (service == 0) { // Idk what this returns when it can't find a matching service

    }

    // Create IOHIDDevice from service
    IOHIDDeviceRef device = IOHIDDeviceCreate(kCFAllocatorDefault, service);

    // Store device
    _p->iohidDevice = device;

    // Open device
    ior = IOHIDDeviceOpen(_p->iohidDevice, kIOHIDOptionsTypeNone); // Necessary to change the state of the device
    if (ior != kIOReturnSuccess) {
        // TODO: Throw and error or something
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
}

RawDevice::RawDevice(const RawDevice &other) : _p(std::make_unique<PrivateImpl>()),
                                               _vendor_id(other._vendor_id), _product_id(other._product_id),
                                               _name(other._name),
                                               _report_desc(other._report_desc)
{
    // Copy constructor
    
    // Copy values from `other` to `this`

    _p->iohidDevice = other._p->iohidDevice;
    // ^ As far as I understand, iohidDevices can't be copied, so we're just assigning it.
    _p->maxInputReportSize = other._p->maxInputReportSize;
    _p->maxOutputReportSize = other._p->maxOutputReportSize;
    _p->lastInputReportLength = 0;
    // ^ This is only used by readReport() to communicated with its callback. Doesn't make sense to be copied.
    _p->inputReportRunLoop = nullptr; 
    // ^ runLoops are per thread and can't be copied.
    _p->preventNextRead = false;
    // ^ This could be copied but that probably doesn't make sense.
}

RawDevice::RawDevice(RawDevice &&other) : _p(std::make_unique<PrivateImpl>()),
                                          _vendor_id(other._vendor_id), _product_id(other._product_id),
                                          _name(std::move(other._name)),
                                          _report_desc(std::move(other._report_desc))
{
    // Move constructor
    // How to write move constructor: https://stackoverflow.com/a/43387612/10601702

    // Assign values from `other` to `this`

    _p->iohidDevice = other._p->iohidDevice;
    _p->maxInputReportSize = other._p->maxInputReportSize;
    _p->maxOutputReportSize = other._p->maxOutputReportSize;
    _p->lastInputReportLength = 0;
    _p->inputReportRunLoop = nullptr;
    _p->preventNextRead = false;

    // Set `other` pointers to null

    other._p->iohidDevice = nullptr;
    other._p->maxInputReportSize = 0;
    other._p->maxOutputReportSize = 0;
    other._p->lastInputReportLength = 0;
    other._p->inputReportRunLoop = nullptr;
    other._p->preventNextRead = false;

}

// Destructor

RawDevice::~RawDevice(){
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

    // Convert timeout to seconds instead of milliseconds
    double timeoutSeconds = timeout / 1000.0;

    // Setup reportBuffer
    CFIndex reportBufferSize = _p->maxInputReportSize;
    uint8_t reportBuffer[reportBufferSize];
    memset(reportBuffer, 0, reportBufferSize); // Init with 0s

    // Init lastInputReportLength
    //  We override this in the reportCallback. If it hasn't been overriden once we exit this function, we know that the read has failed.
    //  Should only be used by this function. It's only a global variable so that the inputReportCallback can access it.
    _p->lastInputReportLength = -1;

    // Setup report callback
    //  IOHIDDeviceGetReportWithCallback has a built-in timeout and allows you to specify IOHIDReportType, 
    //      but Apple docs say it should only be used for feature reports. So we're using 
    //      IOHIDDeviceRegisterInputReportCallback instead.
    IOHIDDeviceRegisterInputReportCallback(
        _p->iohidDevice, 
        reportBuffer, // This will get filled when an inputReport occurs
        _p->maxInputReportSize,
        [] (void *context, IOReturn result, void *sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength) {

            RawDevice *thisss = static_cast<RawDevice *>(context); //  Get `this` from context
            //  ^ We can't capture `this` or anything else, because then the enclosing lambda wouldn't decay to a pure c function

            thisss->_p->lastInputReportLength = reportLength;
            CFRunLoopStop(thisss->_p->inputReportRunLoop); 
            // ^ Aka CFRunLoopGetCurrent() because this callback is driven by that runLoop
        }, 
        this // Pass `this` to context
    );

    // Start runLoop

    // Store current runLoop
    this->_p->inputReportRunLoop = CFRunLoopGetCurrent();

    // Add IOHIDDevice to runLoop.
    //  Async callbacks for this IOHIDDevice will be delivered to this runLoop
    //  We need to call this before CFRunLoopRun, because if the runLoop has nothing to do, it'll immediately exit when we try to run it.
    IOHIDDeviceScheduleWithRunLoop(_p->iohidDevice, _p->inputReportRunLoop, kCFRunLoopCommonModes);

    // Start runLoop
    //  Calling this blocks this thread and until the runLoop exits.
    //  We only exit the runLoop if one of these happens
    //      - Device sends input report
    //      - Timeout happens
    //      - interruptRead() is called
    //  If interruptRead has been called before this point, that will have 
    //      set preventNextRead = true. In that case we wont enter the input-listening runLoop, and not block and return immediately.

    if (!_p->preventNextRead) {
        _p->readIsBlocking = true; // Should only be mutated right here.
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeoutSeconds, false);
        _p->readIsBlocking = false;
    }
    // Reset preventNextRead
    _p->preventNextRead = false; // Should this be at the end of the function to prevent race conditions?

    // Tear down runLoop after it exits 

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

    if (_p->lastInputReportLength == -1) { // Reading has timed out or was interrupted

        return 0;

    } else { // Reading was successful

        // Write result to the `report` argument and return length
        size_t assignLength = std::min<size_t>(report.size(), _p->lastInputReportLength);
        report.assign(reportBuffer, reportBuffer + assignLength);
        return assignLength;
    }
}

void RawDevice::interruptRead() {

    if (_p->readIsBlocking) { 
        // readReport() is currently blocking and waiting for a report 
        //      -> Stop it from waiting and return immediately
        CFRunLoopStop(_p->inputReportRunLoop);
    } else {
        // readReport() is not currently blocking 
        //      -> Make it return immediately the next time it wants to block and wait for input.
        //  This is the expected behaviour according to https://github.com/cvuchener/hidpp/issues/17#issuecomment-896821785
        _p->preventNextRead = true;
    }
}