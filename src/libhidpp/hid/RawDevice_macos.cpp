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

extern "C"
{
    // #include <Foundation/Foundation.h>
}

using namespace HID;

// PrivateImpl
//  Why do we use the PrivateImpl struct instead of private member variables?

struct RawDevice::PrivateImpl
{
};

// Private constructor

RawDevice::RawDevice() : _p(std::make_unique<PrivateImpl>())
{
}

// Public constructors

RawDevice::RawDevice(const std::string &path) : _p(std::make_unique<PrivateImpl>())
{
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
}

// Interface

int RawDevice::writeReport(const std::vector<uint8_t> &report)
{
    return 0;
}

int RawDevice::readReport(std::vector<uint8_t> &report, int timeout)
{
    return 0;
}

void RawDevice::interruptRead()
{
}
