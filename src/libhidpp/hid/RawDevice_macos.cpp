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
        // TODO: Log something and return
    }
    CFDictionaryRef matchDict = IORegistryEntryIDMatching(entryID);
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, matchDict);
    // ^ TODO: Check that the service is valid

    // Create IOHIDDevice from service
    IOHIDDeviceRef device = IOHIDDeviceCreate(kCFAllocatorDefault, service);

    // Open device
    IOHIDDeviceOpen(_p->iohidDevice, kIOHIDOptionsTypeNone); //  Necessary to change the state of the device

    // Store IOHIDDevice in self
    _p->iohidDevice = device;

    // Fill up public member variables
    _vendor_id = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDVendorIDKey));
    _product_id = Utility_macos::IOHIDDeviceGetIntProperty(device, CFSTR(kIOHIDProductIDKey));
    _name = Utility_macos::IOHIDDeviceGetStringProperty(device, CFSTR(kIOHIDProductKey));
    _report_desc = Utility_macos::IOHIDDeviceGetReportDescriptor(device);

    // Fill up private member variables
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

// Interface

// writeReport

int RawDevice::writeReport(const std::vector<uint8_t> &report)
{
    //  See https://developer.apple.com/library/archive/technotes/tn2187/_index.html 
    //      for info on how to use IOHID input report functions and more.

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

    // Schedule device with runloop
    //  Necessary for async API (callback) to work

    _p->inputReportRunLoop = CFRunLoopGetCurrent();
    IOHIDDeviceScheduleWithRunLoop(_p->iohidDevice, _p->inputReportRunLoop, kCFRunLoopCommonModes);

    // Allocate buffers

    uint8_t reportBuffer[_p->maxInputReportSize];
    CFIndex reportLength = -1;

    // Get report
    //  IOHIDDeviceGetReportWithCallback has a built in timeout, but Apple docs say it should only be used for feature reports. 
    //      So we're using IOHIDDeviceRegisterInputReportCallback instead.

    IOHIDDeviceRegisterInputReportCallback(
        _p->iohidDevice, 
        reportBuffer, 
        reportLength, 
        [] (void *context, IOReturn result, void *sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength) {

            RawDevice *thisss = static_cast<RawDevice *>(context); //  Get `this` from context
            //  ^ We can't capture `this`, because then this lambda wouldn't decay to a pure c function anymore
            thisss->_p->inputReportBlocker.notify_all(); // Report was received -> stop waiting for report
        }, 
        this // Pass `this` to context
    );

    // Wait until on of these happens
    //  - Device sends input report
    //  - Timeout happens
    //  - interruptRead() is called

    // Create lock
    std::unique_lock<std::mutex> lock(_p->inputReportMutex); // I think this also locks the lock. Lock needs to be locked for inputReportBlocker to work.

    if (timeout < 0) { // Negative `timeout` means no timeout
        _p->inputReportBlocker.wait(lock);
    } else {
        std::chrono::milliseconds cppTimeout(timeout);
        _p->inputReportBlocker.wait_for(lock, cppTimeout);
    }

    // Stop listening for reports
    IOHIDDeviceUnscheduleFromRunLoop(_p->iohidDevice, _p->inputReportRunLoop, kCFRunLoopCommonModes);

    // Check success
    if (reportLength == -1) { // The read has timed out or was interrupted

        // TODO: Return some meaningful error code or something
    }

    // Write result to the `report` argument
    report = std::vector<uint8_t>(reportBuffer, reportBuffer + reportLength);

    // Return
    return reportLength;
}

void RawDevice::interruptRead() {
    _p->inputReportBlocker.notify_all(); // Stop waiting for report
}

// Destructor

RawDevice::~RawDevice()
{
    IOHIDDeviceUnscheduleFromRunLoop(_p->iohidDevice, _p->inputReportRunLoop, kCFRunLoopCommonModes); // Not sure if necessary
    IOHIDDeviceClose(_p->iohidDevice, kIOHIDOptionsTypeNone); // Not sure if necessary
    CFRelease(_p->iohidDevice); // Not sure if necessary
}