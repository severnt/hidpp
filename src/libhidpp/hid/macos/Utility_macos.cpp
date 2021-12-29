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


#include "Utility_macos.h"
#include <chrono>
#include "../../misc/Log.h"

extern "C" {
    #include <stdbool.h>
}

// Convert Cpp -> Cocoa

void Utility_macos::stringToIOString(std::string string, io_string_t &ioString) {
    strcpy(ioString, string.c_str());
}

CFStringRef Utility_macos::stringToCFString(std::string string) {
    return CFStringCreateWithCString(kCFAllocatorDefault, string.c_str(), kCFStringEncodingUTF8);
}

// Convert Cocoa -> Cpp

long Utility_macos::CFNumberToInt(CFNumberRef cfNumber) {

    long result;

    CFNumberType numberType = CFNumberGetType(cfNumber);
    bool success = CFNumberGetValue(cfNumber, numberType, &result);

    if (!success) {
        // TODO: Throw error or something
    }

    return result;
}

std::string Utility_macos::CFStringToString(CFStringRef cfString) {


    CFIndex length = CFStringGetLength(cfString);
    char buffer[length];

    bool success = CFStringGetCString(cfString, buffer, length, kCFStringEncodingUTF8);

    if (!success) {
        // TODO: Throw error
    }

    return std::string(buffer);
}

std::vector<uint8_t> Utility_macos::CFDataToByteVector(CFDataRef cfData) {

    const uint8_t *bytes = CFDataGetBytePtr(cfData);
    CFIndex length = CFDataGetLength(cfData);

    std::vector<uint8_t> byteVector;
    byteVector.assign(bytes, bytes + length);

    return byteVector;
}

// Convenience wrappers IOHIDDeviceGetProperty

long Utility_macos::IOHIDDeviceGetIntProperty(IOHIDDeviceRef device, CFStringRef key) {

    CFNumberRef cfValue = (CFNumberRef)IOHIDDeviceGetProperty(device, key);

    if (cfValue != NULL) {
        return CFNumberToInt(cfValue);
    } else {
        // Log warning
        Log::warning().printf("Property \"%s\" was NULL on device \"%s\". Using 0 instead.\n", CFStringGetCStringPtr(key, kCFStringEncodingUTF8), IOHIDDeviceGetDebugIdentifier(device));
        // Return
        return 0;
    }
}

std::string Utility_macos::IOHIDDeviceGetStringProperty(IOHIDDeviceRef device, CFStringRef key) {
    CFStringRef cfValue = (CFStringRef)IOHIDDeviceGetProperty(device, key);
    if (cfValue != NULL) {
        return CFStringToString(cfValue);
    } else {
        // Log warning
        Log::warning().printf("Property \"%s\" was NULL on device \"%s\". Using empty string instead.\n", CFStringGetCStringPtr(key, kCFStringEncodingUTF8), IOHIDDeviceGetDebugIdentifier(device));
        // Return
        return "";
    }
}

HID::ReportDescriptor Utility_macos::IOHIDDeviceGetReportDescriptor(IOHIDDeviceRef device) {

    CFTypeRef cfValue = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDReportDescriptorKey));
    std::vector<uint8_t> byteVector;

    if (cfValue != NULL) {
        byteVector = CFDataToByteVector((CFDataRef)cfValue);
        return HID::ReportDescriptor::fromRawData(byteVector.data(), byteVector.size());
    } else {
        // Log warning
        Log::warning().printf("Report descriptor was NULL on device \"%s\". Using empty vector instead.\n", IOHIDDeviceGetDebugIdentifier(device));
        // We want to return an empty report descriptor here. TODO: Make sure `return HID::ReportDescriptor();` works for that.
        return HID::ReportDescriptor();
    }
}

// Other IOHIDDevice helpers

const char * Utility_macos::IOHIDDeviceGetPath(IOHIDDeviceRef device) {
    
    // return CFStringToString((CFStringRef)IOHIDDeviceGetProperty(device, CFSTR(kIOPathKey))).c_str(); 
    //  ^ Not sure if this would work

    // Get service
    io_service_t service = IOHIDDeviceGetService(device);
    // Get cfPath
    CFStringRef cfPath = IORegistryEntryCopyPath(service, kIOServicePlane);
    // Get path
    const char *path = CFStringGetCStringPtr(cfPath, kCFStringEncodingUTF8);

    return path;
}

void Utility_macos::stopListeningToInputReports(IOHIDDeviceRef device, CFRunLoopRef &runLoop) {
    // This function is unused. Only to be used as reference. Delete at some point.

    // Unregister input report callback
    uint8_t reportBuffer[0]; // Passing this instead of NULL to silence warnings
    CFIndex reportLength = 0;
    IOHIDDeviceRegisterInputReportCallback(device, reportBuffer, reportLength, NULL, NULL); 
    //  ^ Passing NULL for the callback unregisters the previous callback.
    //      Not sure if redundant when already calling IOHIDDeviceUnscheduleFromRunLoop.

    // Remove device from runLoop
    IOHIDDeviceUnscheduleFromRunLoop(device, runLoop, kCFRunLoopCommonModes);

    // If there is nothing else scheduled on the runLoop it should exit automatically after this.
    //  And if there's nothing else scheduled on the thread which the runLoop is running on, it should exit, as well.

    // Force-stop runLoop - probably unnecessary
    if (runLoop != NULL) {
        CFRunLoopStop(runLoop);
        runLoop = NULL;
    }
}

const char * Utility_macos::IOHIDDeviceGetUniqueIdentifier(IOHIDDeviceRef device) {
    // This is currently the device path. We should base this on the registryEntryID at some point, because of issues with long paths (see HIDAPI Github issues)

    return Utility_macos::IOHIDDeviceGetPath(device);

}
const char * Utility_macos::IOHIDDeviceGetDebugIdentifier(IOHIDDeviceRef device) {
    // Human readable identifier that doesn't clog up the debug output too much

    // Get service
    io_service_t service = IOHIDDeviceGetService(device);
    // Get id
    uint64_t id;
    IORegistryEntryGetRegistryEntryID(service, &id);
    // Convert to string
    //  Little cumbersom because id_str.c_str() will be deallocated when id_str goes out of scope
    std::string id_str_cpp = std::to_string(id);
    char *id_str = (char *) malloc(id_str_cpp.length() * sizeof(char));
    strcpy(id_str, id_str_cpp.c_str());
    // Return
    return id_str;
}

// Other

double Utility_macos::timestamp() {
    // Src: https://stackoverflow.com/a/45465312/10601702
    // Returns seconds since January 1st 1970

    auto now = std::chrono::system_clock::now();
    auto timeSinceEpoch = std::chrono::duration<double>(now.time_since_epoch()); // Epoch is usually January 1st 1970
    return timeSinceEpoch.count();
}
