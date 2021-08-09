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
#include <thread>

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
			// Get this
			DeviceMonitor *thisss = static_cast<DeviceMonitor *>(context);
			// Get path
			const char *path = Utility_macos::IOHIDDeviceGetPath(device);
			// Add device
			thisss->addDevice(path);
		}, 
		this
	);

	// Setup device removal callback
	IOHIDManagerRegisterDeviceRemovalCallback(
		_p->manager, 
		[] (void *context, IOReturn result, void *sender, IOHIDDeviceRef device) {
			// Get this
			DeviceMonitor *thisss = static_cast<DeviceMonitor *>(context);
			// Get path
			const char *path = Utility_macos::IOHIDDeviceGetPath(device);
			// Remove device
			thisss->removeDevice(path);
		}, 
		NULL
	);

	// Callbacks won't be active until IOHIDManagerScheduleWithRunLoop() is called
}

DeviceMonitor::~DeviceMonitor () {

	IOHIDManagerUnscheduleFromRunLoop(_p->manager, _p->managerRunLoop, kCFRunLoopCommonModes); // Not sure if necessary
	CFRelease(_p->manager); // Not sure if necessary
}

// Interface

void DeviceMonitor::enumerate () {

	// Get C array of devices attached to the manager

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

	// Call addDevice() on all currently attached devices
	enumerate();

	// Activate device attached/removed callback by scheduling manager with runLoop
	std::thread runLoopThread([this] () {

		// Store runLoop
		this->_p->managerRunLoop = CFRunLoopGetCurrent();

		// Associate manager with runLoop
		IOHIDManagerScheduleWithRunLoop(this->_p->manager, this->_p->managerRunLoop, kCFRunLoopCommonModes); 
		// 	^ Matching and removal callbacks defined in constructor will now be active

		// Run runLoop
		//	This will block the current thread. That's why we're executing this on a new thread "runLoopThread"
		CFRunLoopRun();

		// Set runLoop to NULL after it exits
		this->_p->managerRunLoop = NULL;
	});
}

void DeviceMonitor::stop () {

	// Stop monitor
	IOHIDManagerUnscheduleFromRunLoop(_p->manager, _p->managerRunLoop, kCFRunLoopCommonModes); 
	// 	^ Deactivates matching and removal callbacks defined in constructor

	// Make sure the runLoop stops
	// 	Probably unnecessary. The runLoop should stop automatically after we unschedule the manager, because it won't have anything to do.
	if (_p->managerRunLoop != NULL) { 
		CFRunLoopStop(_p->managerRunLoop);
	}
}
