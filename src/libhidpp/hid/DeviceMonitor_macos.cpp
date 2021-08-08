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

#include "DeviceMonitor.h"

#include <misc/Log.h>

#include <string>
#include "macos/Utility_macos.h"

extern "C" {
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/IOKitLib.h>
}

using namespace HID;

// Private

struct DeviceMonitor::PrivateImpl
{
	IOHIDManagerRef manager;
	CFRunLoopRef managerRunLoop;
};

// Constructor & Destructor

DeviceMonitor::DeviceMonitor ():
	_p (std::make_unique<PrivateImpl> ())
{

	// Create manager
	_p->manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

	// Match all devices
	IOHIDManagerSetDeviceMatching(_p->manager, NULL);

	// Setup device matching callback
	IOHIDManagerRegisterDeviceMatchingCallback(
		_p->manager, 
		[] (void *context, IOReturn result, void *sender, IOHIDDeviceRef device) {
			
			// Get path
			const char *path = Utility_macos::IOHIDDeviceGetPath(device);
			// Add device
			addDevice(path);
		}, 
		NULL
	);

	// Setup device removal callback
	IOHIDManagerRegisterDeviceRemovalCallback(
		_p->manager, 
		[] (void *context, IOReturn result, void *sender, IOHIDDeviceRef device) {
			
			// Get path
			const char *path = Utility_macos::IOHIDDeviceGetPath(device);
			// Add device
			removeDevice(path);
		}, 
		NULL
	);

	// Callbacks won't be active until IOHIDManagerScheduleWithRunLoop() is called
}

DeviceMonitor::~DeviceMonitor () {

	IOHIDManagerUnscheduleFromRunLoop(_p->manager); // Not sure if necessary
	CFRelease(_p->manager); // Not sure if necessary
}

// Interface

void DeviceMonitor::enumerate () {

	// Get c array of devices attached to the manager

	CFSetRef devices = IOHIDManagerCopyDevices(_p->manager);

	CFIndex count = CFSetGetCount(devices);
	IOHIDDeviceRef deviceArray[count];

	CFSetGetValues(devices, (const void **)deviceArray);

	// Add all found devices

	for (int i = 0; i < count; ++i) {

		// Get device
		IOHIDDeviceRef device = deviceArray[i];
		// Get path
		const char *path = Utility_macos::IOHIDDeviceGetPath(device);
		// Add
		addDevice(path);
	}
}

void DeviceMonitor::run () {

	// Start monitor
	_p->managerRunLoop = CFRunLoopGetCurrent();
	IOHIDManagerScheduleWithRunLoop(_p->manager, _p->managerRunLoop, kCFRunLoopCommonModes); 
	// 	^ Matching and removal callbacks defined in constructor will now be active

	// Call addDevice() on all currently attached devices
	enumerate();
}

void DeviceMonitor::stop () {
	// Stop monitor
	IOHIDManagerUnscheduleFromRunLoop(_p->manager, _p->managerRunLoop, kCFRunLoopCommonModes); 
	// 	^ Matching and removal callbacks defined in constructor deactivate
	//		Storing managerRunLoop in _p because I think it might be good if the runloop used for scheduling and unscheduling is the same. Not sure though.
}
