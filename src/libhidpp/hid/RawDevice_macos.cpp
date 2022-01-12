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
#include <IOKit/hid/IOHIDDevicePlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOCFPlugIn.h>
}

// Linux pre-process defintions
//  Copied from https://github.com/1kc/librazermacos/blob/2ca85a2bcdbf5e4b3938b95754d06b96de382bd9/src/include/razercommon.h#L12
#define HID_REQ_GET_REPORT 0x01
#define HID_REQ_SET_REPORT 0x09

#define USB_TYPE_CLASS (0x01 << 5)
#define USB_RECIP_INTERFACE 0x01
#define USB_DIR_OUT 0
#define USB_DIR_IN  0x80

// Pimpl

struct RawDevice::PrivateImpl {

    // Attributes

    IOHIDDeviceRef iohidDevice;
    IOHIDDeviceInterface122 **deviceInterface;

    CFIndex maxInputReportSize;
    CFIndex maxOutputReportSize;

    static void nullifyValues(RawDevice *dev) {
        dev->_p->iohidDevice = nullptr;
        dev->_p->maxInputReportSize = 0;
        dev->_p->maxOutputReportSize = 0;

        dev->_p->initState(dev);
    }

    // State

    std::atomic<bool> ignoreNextRead;
    std::atomic<bool> waitingForInput;

    static void initState(RawDevice *dev) {

        dev->_p->ignoreNextRead = false;
        dev->_p->waitingForInput = false;
    }

    // Concurrency

    std::mutex mutexLock;

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

    // Close IOHIDDevice
    IOHIDDeviceClose(_p->iohidDevice, kIOHIDOptionsTypeNone); // Not sure if necessary
    CFRelease(_p->iohidDevice); // Not sure if necessary
}

// Main constructor

RawDevice::RawDevice(const std::string &path) : _p(std::make_unique<PrivateImpl>()) {
    // Construct device from path

    // Lock
    //  This only unlocks once the readThread has set up its runLoop
    std::unique_lock lock(_p->mutexLock);

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

    // Get USB Interface

    IOCFPlugInInterface **pluginInterface = NULL;
    SInt32 score = 0;
    kr = IOCreatePlugInInterfaceForService(service, kIOHIDDeviceTypeID, kIOCFPlugInInterfaceID, &pluginInterface, &score);
    if (kr != KERN_SUCCESS) {
        Log::error() << "Failed to get plugin interface for device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << " with error code " << kr << std::endl;
        
        CFStringRef path = IORegistryEntryCopyPath(service, kIOServicePlane);
        Log::debug() << "Service path: " << CFStringGetCStringPtr(path, kCFStringEncodingUTF8) << std::endl;
        CFRelease(path);
    } else {

        Log::error() << "Successfully obtained plugin interface for device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

        IOHIDDeviceInterface122 **deviceInterface = NULL;
        HRESULT hResult = (*pluginInterface)->QueryInterface(pluginInterface, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID122), (LPVOID *)&deviceInterface);
        if (hResult != 0 || deviceInterface == NULL) {
            Log::error() << "Failed to get USB interface for device " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;
        }
        (*pluginInterface)->Release(pluginInterface); /// Not needed after getting USBInterface

        _p->deviceInterface = deviceInterface;
        _p
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


    if (_p->deviceInterface == NULL) {
        return 1;
    }


    // Guard report size
    if (report.size() > _p->maxOutputReportSize) {
        // TODO: Return meaningful error
        return 1;
    }


    // (*_p->deviceInterface)->setReport(_p->deviceInterface, kIOHIDReportTypeOutput, report[0], report.)

    // CFIndex reportID = report[0]; /// Should we use this as wIndex? Razer driver just uses 0 as wIndex as far as I can tell.
    UInt16 requestLength = report.size();
    const uint8_t *requestData = report.data();

    IOUSBDevRequest request = { // Copied from https://github.com/1kc/librazermacos/blob/2ca85a2bcdbf5e4b3938b95754d06b96de382bd9/src/lib/razercommon.c#L20
        .bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT, /// This is from razer driver, but docs say valid values are kUSBStandard, kUSBClass or kUSBVendor.
        .bRequest = HID_REQ_SET_REPORT,
        .wValue = 0x300,
        .wIndex = 0x00,
        .wLength = requestLength, // Is this bits or bytes?
        .pData = (void *)requestData,
        .wLenDone = 0,
    };

    IOReturn r = (*_p->deviceInterface)->DeviceRequest(_p->deviceInterface, &request);

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
    std::unique_lock lock(_p->mutexLock);

    // Debug
    Log::debug() << "readReport called on " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;


    if (_p->deviceInterface == NULL) {
        return 0;   
    }


    // Setup reportBuffer
    UInt16 bufferSize = (UInt16)_p->maxInputReportSize;
    uint8_t buffer[bufferSize];

    // Setup request return
    IOReturn rt;
    UInt32 responseLength;

    // Send request and wait for response

    if (timeout >= 0) {
        
        UInt32 t = (UInt32)timeout;

        IOUSBDevRequestTO request = { // Copied from https://github.com/1kc/librazermacos/blob/2ca85a2bcdbf5e4b3938b95754d06b96de382bd9/src/lib/razercommon.c#L20
            .bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN, // This is from razer driver, but docs say valid values are kUSBStandard, kUSBClass or kUSBVendor.
            .bRequest = HID_REQ_GET_REPORT,
            .wValue = 0x300,
            .wIndex = 0x00,
            .wLength = bufferSize, // Is this in bits or bytes?
            .pData = &buffer,
            .wLenDone = 0,
            .noDataTimeout = t,
            .completionTimeout = UINT32_MAX,
        };

        if (_p->ignoreNextRead) { // Checking for `ignoreNextRead` as close as possible to the wait, to avoid race conditions
            _p->ignoreNextRead = false;
            return 0;
        }
        _p->waitingForInput = true;
        rt = (*_p->deviceInterface)->DeviceRequestTO(_p->deviceInterface, &request);
        _p->waitingForInput = false;
        responseLength = request.wLenDone;
        
    } else { // No timeout

        IOUSBDevRequest request = { 
            .bmRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN, // This is from razer driver, but docs say valid values are kUSBStandard, kUSBClass or kUSBVendor.
            .bRequest = HID_REQ_GET_REPORT,
            .wValue = 0x300,
            .wIndex = 0x00,
            .wLength = bufferSize,
            .pData = &buffer,
            .wLenDone = 0,
        };

        if (_p->ignoreNextRead) {
            _p->ignoreNextRead = false;
            return 0;
        }

        _p->waitingForInput = true;
        rt = (*_p->deviceInterface)->DeviceRequest(_p->deviceInterface, &request);
        _p->waitingForInput = false;
        responseLength = request.wLenDone;
    }

    // Return

    if (rt == kIOReturnSuccess) {
        
        report.assign(buffer, buffer + responseLength);
        return responseLength;

    } else if (rt == kIOReturnAborted) {
        // Reading has been interrupted
        return 0;
    } else if (rt == kIOReturnNoDevice || rt == kIOReturnNotOpen) {
        // This should never happen I think
        Log::error() << "readReport failed with code" << rt << std::endl;
        return 0;
    } else {
        // There are no other returns listed in the docs, so this should never happen
        Log::error() << "readReport failed with unknown code" << rt << std::endl;
        return 0;
    }
}

void RawDevice::interruptRead() {

    // Maybe we should use _p->mutexLock here? Make sure there are no race conditions!
    //  Edit: I've thought about it and there are some race conds but they unlikely and don't have bad consequences.

    // Debug
    Log::debug() << "interruptRead called on " << Utility_macos::IOHIDDeviceGetDebugIdentifier(_p->iohidDevice) << std::endl;

    // Do stuff

    if (_p->waitingForInput) {

        // Stop waiting
        IOReturn rt = (*_p->deviceInterface)->USBDeviceAbortPipeZero(_p->deviceInterface);

        /// Log errors
        if (rt != kIOReturnSuccess) {
            Log::error() << "interruptRead failed with code" << rt << std::endl;
        }
    } else {
        // If readReport() is not currently waiting, ignore the next call to readReport()
        //  This is the expected behaviour according to https://github.com/cvuchener/hidpp/issues/17#issuecomment-896821785

        _p->ignoreNextRead = true;
    }
}