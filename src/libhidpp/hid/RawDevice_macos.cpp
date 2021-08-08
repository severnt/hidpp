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
//  Why do we use the PrivateImpl struct instead of private member variables?

struct RawDevice::PrivateImpl
{
    IOHIDDeviceRef iohidDevice;

    CFIndex maxInputReportSize;
    CFIndex maxOutputReportSize;
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
    // _p->fd = ::dup (other._p->fd);
    // if (-1 == _p->fd) {
    // 	throw std::system_error (errno, std::system_category (), "dup");
    // }
    // if (-1 == ::pipe (_p->pipe)) {
    // 	int err = errno;
    // 	::close (_p->fd);
    // 	throw std::system_error (err, std::system_category (), "pipe");
    // }
}

RawDevice::RawDevice(RawDevice &&other) : // What's the difference between this constructor and the one above?

                                          _p(std::make_unique<PrivateImpl>()),
                                          _vendor_id(other._vendor_id), _product_id(other._product_id),
                                          _name(std::move(other._name)),
                                          _report_desc(std::move(other._report_desc))
{
    // _p->fd = other._p->fd;
    // _p->pipe[0] = other._p->pipe[0];
    // _p->pipe[1] = other._p->pipe[1];
    // other._p->fd = other._p->pipe[0] = other._p->pipe[1] = -1;
}

// Destructor

RawDevice::~RawDevice()
{
    IOHIDDeviceClose(_p->iohidDevice, kIOHIDOptionsTypeNone);
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

int RawDevice::readReport(std::vector<uint8_t> &report, int timeout)
{

    // Convert timeout
    CFTimeInterval timeoutCF;
    if (timeout < 0) {
        timeoutCF = DBL_MAX; // A negative timeout means no time out. This should be close enough.
    } else {
        timeoutCF = timeout / 1000.0; // timeoutCF is in s, timeout is in ms.
    }

    // Schedule device with runloop
    //  Necessary for asynch APIs to work
    // IOHIDDeviceScheduleWithRunLoop(_p->iohidDevice, RunLoopGet)

    // // Init report values

    // bool readHasTimedOut = false;

    // // Allocate buffers

    // uint8_t reportBuffer[1024];
    // CFIndex reportLengthBuffer;

    // // Query report

    // IOReturn r = IOHIDDeviceGetReportWithCallback(_p->iohidDevice, kIOHIDReportTypeInput, report[0], &reportBuffer, &reportLengthBuffer, timeoutCF, NULL, NULL);
    // // ^ Not sure about these parameters
    // // This function is blocking.
    // // Apple docs say this functions should only be used for feature reports.

    // // TODO: Check if timed out

    // if (r != kIOReturnSuccess) {
    //     // TODO: Return some meaningful error code
    // }

    // // Return

    // report = std::vector<uint8_t>(&reportBuffer[0], &reportBuffer[0] + reportLengthBuffer);
    // return reportLengthBuffer;
}

void RawDevice::interruptRead()
{
}
