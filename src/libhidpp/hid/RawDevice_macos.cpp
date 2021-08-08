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
#include "macos/macOSUtility.h"

extern "C" {
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDDevice.h>
};

using namespace HID;

// PrivateImpl
//  Why do we use the PrivateImpl struct instead of private member variables?

struct RawDevice::PrivateImpl
{
    IOHIDDeviceRef iohidDevice;

    // readReport helper variables
    struct readResult {
        uint8_t *   report;
        CFIndex     reportLength;
    };
    bool readHasTimedOut;
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
    macOSUtility::stringToIOString(path, ioPath);

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

    // Store IOHIDDevice in self

    _p->iohidDevice = device;

    // Fill up other member variables

    _vendor_id = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey));
    _product_id = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey));
    _name = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
    _report_desc = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDReportDescriptorKey));

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
    IOHIDDeviceClose(_p->iohidDevice); // Don't think this is necessary
}

// Interface

// writeReport

int RawDevice::writeReport(const std::vector<uint8_t> &report)
{

    IOReturn r = IOHIDDeviceSetReport(_p->iohidDevice, kIOHIDReportTypeOutput, report[0], report.data(), report.size())
    // ^ I'm not sure about these parameters at all

    if (r != kIOReturnSuccess) {
        // TODO: Return some meaningful error code
    }

    return 0;
}

// readReport

int RawDevice::readReport(std::vector<uint8_t> &report, int timeout)
{

    // Convert from timeout arg to IOHID-compatible timeoutCF

    CFTimeInterval timeoutCF;
    if timeout < 0 {
        timeoutCF = DBL_MAX; // A negative timeout means no time out. This should be close enough.
    } else {
        timeoutCF = timeout / 1000.0 // timeoutCF is in seconds, whereas timeout is in ms.
    }

    IOReturn r = IOHIDDeviceGetReportWithCallback(_p->iohidDevice, kIOHIDReportTypeInput, report[0], report.data(), report.size(), timeoutCF, readCallback, NULL);
    // ^ Not sure about these parameters

    iohiddevicereportcallba

    if (r != kIOReturnSuccess) {
        // TODO: Return some meaningful error code
    }

    return 0;
}

void RawDevice::interruptRead()
{
}

// readReport helper functions

void RawDevice::readCallback(   void * _Nullable        context,
                                IOReturn                result, 
                                void * _Nullable        sender,
                                IOHIDReportType         type, 
                                uint32_t                reportID,
                                uint8_t *               report, 
                                CFIndex                 reportLength) {
    
    if (!_p->readHasTimedOut) {
        _p->readResult->report = report;
        _p->readResult->reportLength = reportLength;
    }
}