/*
 * Copyright 2015 Clément Vuchener
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

#include <cstdio>
#include <cstring>

extern "C" {
#include <libudev.h>
}

#include <misc/SysCallError.h>
#include <misc/Log.h>
#include <hidpp/Device.h>
#include <hidpp10/Error.h>

#include "common/common.h"
#include "common/Option.h"
#include "common/CommonOptions.h"

int main (int argc, char *argv[])
{
	std::vector<Option> options = {
		VerboseOption (),
	};
	Option help = HelpOption (argv[0], "", &options);
	options.push_back (help);

	int first_arg;
	if (!Option::processOptions (argc, argv, options, first_arg))
		return EXIT_FAILURE;

	if (argc-first_arg != 0) {
		fprintf (stderr, "%s", getUsage (argv[0], "", &options).c_str ());
		return EXIT_FAILURE;
	}

	int ret;

	struct udev *ctx = udev_new ();
	if (!ctx) {
		fprintf (stderr, "Failed to create udev context\n");
		return EXIT_FAILURE;
	}

	struct udev_enumerate *enumerator = udev_enumerate_new (ctx);
	if (!enumerator) {
		fprintf (stderr, "Failed to create udev enumerator\n");
		return EXIT_FAILURE;
	}
	if (0 != (ret = udev_enumerate_add_match_subsystem (enumerator, "hidraw"))) {
		fprintf (stderr, "Failed to add match: %s\n", strerror (ret));
		return EXIT_FAILURE;
	}
	if (0 != (ret = udev_enumerate_scan_devices (enumerator))) {
		fprintf (stderr, "Failed to scan devices: %s\n", strerror (ret));
		return EXIT_FAILURE;
	}

	struct udev_list_entry *current;
	udev_list_entry_foreach (current, udev_enumerate_get_list_entry (enumerator)) {
		struct udev_device *device = udev_device_new_from_syspath (ctx, udev_list_entry_get_name (current));
		const char *hidraw_node = udev_device_get_devnode (device);
		try {
			HIDPP::Device dev (hidraw_node);
			unsigned int major, minor;
			dev.getProtocolVersion (major, minor);
			printf ("%s: %s (%04hx:%04hx) HID++ %d.%d\n",
				hidraw_node, dev.name ().c_str (),
				dev.vendorID (), dev.productID (),
				major, minor);
			for (HIDPP::DeviceIndex index: {
					HIDPP::WirelessDevice1,
					HIDPP::WirelessDevice2,
					HIDPP::WirelessDevice3,
					HIDPP::WirelessDevice4,
					HIDPP::WirelessDevice5,
					HIDPP::WirelessDevice6 }) {
				try {
					HIDPP::Device wldev (hidraw_node, index);
					wldev.getProtocolVersion (major, minor);
					printf ("%s (wireless device %d): %s (%04hx) HID++ %d.%d\n",
						hidraw_node, index, wldev.name ().c_str (),
						wldev.productID (),
						major, minor);
				}
				catch (HIDPP10::Error e) {
					if (e.errorCode () != HIDPP10::Error::UnknownDevice) {
						Log::printf (Log::Error,
							     "Error while querying %s wireless device %d: %s\n",
							     hidraw_node, index, e.what ());
					}
				}
			}

		}
		catch (HIDPP::Device::NoHIDPPReportException e) {
		}
		catch (SysCallError e) {
			Log::printf (Log::Warning, "Failed to open %s: %s\n", hidraw_node, e.what ());
		}
		udev_device_unref (device);
	}

	udev_enumerate_unref (enumerator);
	udev_unref (ctx);

	return 0;
}

