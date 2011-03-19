/*
 * Copyright (c) 2011 Darren Hague & Eric Brandt
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef WIN32
#include <QString>
#include <QDebug>

#include "LibUsb.h"

#ifdef GC_HAVE_LIBUSB // only include if windows and have libusb installed


LibUsb::LibUsb()
{
    intf = NULL;
    readBufIndex = 0;
    readBufSize = 0;

    usb_set_debug(255);

   // Initialize the library.
    usb_init();

    // Find all busses.
    usb_find_busses();

    // Find all connected devices.
    usb_find_devices();
}

int LibUsb::open()
{
     // Search busses & devices for USB2 ANT+ stick
     device = OpenAntStick();

    if (device == NULL)
        return -1;

    int rc = usb_clear_halt(device, writeEndpoint);
    if (rc < 0)
        qDebug()<<"usb_clear_halt writeEndpoint Error: "<< usb_strerror();

    rc = usb_clear_halt(device, readEndpoint);
    if (rc < 0)
        qDebug()<<"usb_clear_halt readEndpoint Error: "<< usb_strerror();

    return rc;
}

void LibUsb::close()
{
    if (device) {
        usb_release_interface(device, 0);
        usb_close(device);
    }

    device = NULL;
}

int LibUsb::read(char *buf, int bytes)
{
    // The USB2 stick really doesn't like you reading 1 byte when more are available
    //  so we need a buffered reader

    int bufRemain = readBufSize - readBufIndex;

    // Can we entirely satisfy the request from the buffer?
    if (bufRemain > bytes)
    {
        // Yes, so do it
        memcpy(buf, readBuf+readBufIndex, bytes);
        readBufIndex += bytes;
        return bytes;
    }

    // No, so partially satisfy by emptying the buffer, then refill the buffer for the rest
    memcpy(buf, readBuf+readBufIndex, bufRemain);
    readBufSize = 0;
    readBufIndex = 0;

    int rc = usb_bulk_read(device, readEndpoint, readBuf, 64, 1000);

    if (rc < 0)
    {
        qDebug()<<"usb_bulk_read Error reading: "<< usb_strerror();
        return rc;
    }

    readBufSize = rc;

    int bytesToGo = bytes - bufRemain;
    if (bytesToGo < readBufSize)
    {
        // If we have enough bytes in the buffer, return them
        memcpy(buf+bufRemain, readBuf, bytesToGo);
        readBufIndex += bytesToGo;
        rc = bytes;
    } else {
        // Otherwise, just return what we can
        memcpy(buf+bufRemain, readBuf, readBufSize);
        rc = bufRemain + readBufSize;
        readBufSize = 0;
        readBufIndex = 0;
    }

    return rc;
}

int LibUsb::write(char *buf, int bytes)
{
    int rc = usb_interrupt_write(device, writeEndpoint, buf, bytes, 1000);

    if (rc < 0)
    {
        qDebug()<<"usb_interrupt_write Error writing: "<< usb_strerror();
    }

    return rc;
}

struct usb_dev_handle* LibUsb::OpenAntStick()
{
    struct usb_bus* bus;
    struct usb_device* dev;
    struct usb_dev_handle* udev;

    for (bus = usb_get_busses(); bus; bus = bus->next)
    {
        for (dev = bus->devices; dev; dev = dev->next)
        {
            if (dev->descriptor.idVendor == GARMIN_USB2_VID && dev->descriptor.idProduct == GARMIN_USB2_PID)
            {
                qDebug() << "Found a Garmin USB2 ANT+ stick";

                if ((udev = usb_open(dev)))
                {
                    if (dev->descriptor.bNumConfigurations)
                    {
                        if ((intf = usb_find_interface(&dev->config[0])) != NULL)
                        {
                            int rc = usb_set_configuration(udev, 1);
                            if (rc < 0)
                                qDebug()<<"usb_set_configuration Error: "<< usb_strerror();
                            rc = usb_claim_interface(udev, 0);
                            if (rc < 0)
                                qDebug()<<"usb_claim_interface Error: "<< usb_strerror();
                            rc = usb_set_altinterface(udev, 0);
                            if (rc < 0)
                                qDebug()<<"usb_set_altinterface Error: "<< usb_strerror();
                            return udev;
                        }
                    }

                    usb_close(udev);
                }
            }
        }
    }
    return NULL;
}

struct usb_interface_descriptor* LibUsb::usb_find_interface(struct usb_config_descriptor* config_descriptor)
{
    struct usb_interface_descriptor* intf;

    readEndpoint = -1;
    writeEndpoint = -1;

    if (!config_descriptor)
        return NULL;

    if (!config_descriptor->bNumInterfaces)
        return NULL;

    if (!config_descriptor->interface[0].num_altsetting)
        return NULL;

    intf = &config_descriptor->interface[0].altsetting[0];

    if (intf->bNumEndpoints != 2)
        return NULL;

    for (int i = 0 ; i < 2; i++)
    {
        if (intf->endpoint[i].bEndpointAddress & USB_ENDPOINT_DIR_MASK)
            readEndpoint = intf->endpoint[i].bEndpointAddress;
        else
            writeEndpoint = intf->endpoint[i].bEndpointAddress;
    }

    if (readEndpoint < 0 || writeEndpoint < 0)
        return NULL;

    return intf;
}
#else

// if we don't have libusb use stubs
LibUsb::LibUsb() {}

int LibUsb::open()
{
    return -1;
}

void LibUsb::close()
{
}

int LibUsb::read(char *, int)
{
    return -1;
}

int LibUsb::write(char *, int)
{
    return -1;
}

#endif // Have LIBUSB
#endif // WIN32