/*
    Copyright (C) 2023  Tiernan8r
    Fork of origin simple-scan at https://gitlab.gnome.org/GNOME/simple-scan

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "screensaver.h"
#include <glibmm-2.68/glibmm.h>
#include <giomm-2.68/giomm/dbusproxy.h>

// [DBus (name = "org.freedesktop.ScreenSaver")]
IFreedesktopScreensaver IFreedesktopScreensaver::get_proxy(void) // throws IOError
{
    return Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::SESSION, "", "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", NULL);
};
