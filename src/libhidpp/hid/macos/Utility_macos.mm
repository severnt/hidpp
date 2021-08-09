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

extern "C" {
    #include <stdbool.h>
}

// Convert Cpp -> Cocoa

void Utility_macos::stringToIOString(std::string string, io_string_t &ioString) {
    strcpy(ioString, string.c_str());
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

    const char *cString = CFStringGetCStringPtr(cfString, kCFStringEncodingUTF8);

    std::string result(cString);

    return result;
}

HID::ReportDescriptor Utility_macos::CFDataToByteVector(CFDataRef cfData) {

    const uint8_t *bytes = CFDataGetBytePtr(cfData);
    CFIndex length = CFDataGetLength(cfData);

    std::vector<uint8_t> byteVector;
    byteVector.assign(bytes, bytes + length);

    return byteVector;
}

// Convenience wrappers IOHIDDeviceGetProperty

long Utility_macos::IOHIDDeviceGetIntProperty(IOHIDDeviceRef device, CFStringRef key) {
    return CFNumberToInt((CFNumberRef)IOHIDDeviceGetProperty(device, key));
}

std::string Utility_macos::IOHIDDeviceGetStringProperty(IOHIDDeviceRef device, CFStringRef key) {
    return CFStringToString((CFStringRef)IOHIDDeviceGetProperty(device, key));    
}

HID::ReportDescriptor Utility_macos::IOHIDDeviceGetReportDescriptor(IOHIDDeviceRef device) {
    CFTypeRef value = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDReportDescriptorKey));
    return CFDataToByteVector((CFDataRef)value); // typedef std::vector<uint8_t> ReportDescriptor
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

void Utility_macos::stopListeningToInputReports(IOHIDDeviceRef device, CFRunLoopRef runLoop) {

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
}

// Other

double Utility_macos::timestamp() {
    // Src: https://stackoverflow.com/a/45465312/10601702
    // Returns seconds since January 1st 1970

    auto now = std::chrono::system_clock::now();
    auto timeSinceEpoch = std::chrono::duration<double>(now.time_since_epoch()); // Epoch is usually January 1st 1970
    return timeSinceEpoch.count();
}
