/* This file is the part of the Lightweight USB device Stack for STM32 microcontrollers
 *
 * Copyright ©2016 Dmitry Filimonchuk <dmitrystu[at]gmail[dot]com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32.h"
#include "usb.h"
#include "usb_cdc.h"
#include "usb_hid.h"
#include "hid_usage_desktop.h"
#include "hid_usage_button.h"
#include <stdio.h>

#define CDC_EP0_SIZE    0x08
#define CDC_RXD_EP      0x01
#define CDC_TXD_EP      0x81
#define CDC_DATA_SZ     64


/* Declaration of the report descriptor */
struct cdc_config {
    struct usb_config_descriptor        config;
    struct usb_interface_descriptor     data;
    struct usb_endpoint_descriptor      data_eprx;
    struct usb_endpoint_descriptor      data_eptx;
} __attribute__((packed));


/* Device descriptor */
static const struct usb_device_descriptor device_desc = {
    .bLength            = sizeof(struct usb_device_descriptor),
    .bDescriptorType    = USB_DTYPE_DEVICE,
    .bcdUSB             = VERSION_BCD(2,0,0),
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = CDC_EP0_SIZE,
    .idVendor           = 0x0403,
    .idProduct          = 0x6010,
    .bcdDevice          = VERSION_BCD(1,0,0),
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = INTSERIALNO_DESCRIPTOR,
    .bNumConfigurations = 1,
};

/* Device configuration descriptor */
static const struct cdc_config config_desc = {
    .config = {
        .bLength                = sizeof(struct usb_config_descriptor),
        .bDescriptorType        = USB_DTYPE_CONFIGURATION,
        .wTotalLength           = sizeof(struct cdc_config),
        .bNumInterfaces         = 1,
        .bConfigurationValue    = 1,
        .iConfiguration         = NO_DESCRIPTOR,
        .bmAttributes           = USB_CFG_ATTR_RESERVED,
        .bMaxPower              = USB_CFG_POWER_MA(500),
    },
    .data = {
        .bLength                = sizeof(struct usb_interface_descriptor),
        .bDescriptorType        = USB_DTYPE_INTERFACE,
        .bInterfaceNumber       = 0,
        .bAlternateSetting      = 0,
        .bNumEndpoints          = 2,
        .bInterfaceClass        = USB_CLASS_VENDOR,
        .bInterfaceSubClass     = USB_SUBCLASS_VENDOR,
        .bInterfaceProtocol     = USB_PROTO_VENDOR,
        .iInterface             = NO_DESCRIPTOR,
    },
    .data_eprx = {
        .bLength                = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType        = USB_DTYPE_ENDPOINT,
        .bEndpointAddress       = CDC_RXD_EP,
        .bmAttributes           = USB_EPTYPE_BULK,
        .wMaxPacketSize         = CDC_DATA_SZ,
        .bInterval              = 0x00,
    },
    .data_eptx = {
        .bLength                = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType        = USB_DTYPE_ENDPOINT,
        .bEndpointAddress       = CDC_TXD_EP,
        .bmAttributes           = USB_EPTYPE_BULK,
        .wMaxPacketSize         = CDC_DATA_SZ,
        .bInterval              = 0x00,
    },
};

static const struct usb_string_descriptor lang_desc     = USB_ARRAY_DESC(USB_LANGID_ENG_US);
static const struct usb_string_descriptor manuf_desc_en = USB_STRING_DESC("Kumsong Tractor Factory");
static const struct usb_string_descriptor prod_desc_en  = USB_STRING_DESC("js2232");
static const struct usb_string_descriptor *const dtable[] = {
    &lang_desc,
    &manuf_desc_en,
    &prod_desc_en,
};

usbd_device udev;
uint32_t	ubuf[0x20];
uint8_t     fifo[0x200];
uint32_t    fpos = 0;

static usbd_respond cdc_getdesc (usbd_ctlreq *req, void **address, uint16_t *length) {
    const uint8_t dtype = req->wValue >> 8;
    const uint8_t dnumber = req->wValue & 0xFF;
    const void* desc;
    uint16_t len = 0;
    switch (dtype) {
    case USB_DTYPE_DEVICE:
        desc = &device_desc;
        break;
    case USB_DTYPE_CONFIGURATION:
        desc = &config_desc;
        len = sizeof(config_desc);
        break;
    case USB_DTYPE_STRING:
        if (dnumber < 3) {
            desc = dtable[dnumber];
        } else {
            return usbd_fail;
        }
        break;
    default:
        return usbd_fail;
    }
    if (len == 0) {
        len = ((struct usb_header_descriptor*)desc)->bLength;
    }
    *address = (void*)desc;
    *length = len;
    return usbd_ack;
}


static usbd_respond cdc_control(usbd_device *dev, usbd_ctlreq *req, usbd_rqc_callback *callback) {
    // printf("ctrl: ty: 0x%02x rq: 0x%02x v: 0x%04x\n", req->bmRequestType, req->bRequest, req->wValue);
    if (((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) == (USB_REQ_INTERFACE | USB_REQ_CLASS)
        && req->wIndex == 0 ) {
        switch (req->bRequest) {
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}

#pragma GCC push_options
#pragma GCC optimize (2)
void invert_buf_align32(uint8_t *buf, uint32_t len) {
    __builtin_assume_aligned(buf, 4);
    uint32_t *p32 = (uint32_t *)buf;
    __builtin_assume_aligned(p32, 4);
    while (len >= 4) {
        *p32 = *p32 ^ 0xffffffff;
        ++p32;
        len -= sizeof(*p32);
    }
}
#pragma GCC pop_options


static uint8_t rx_buf[1024] __attribute__((aligned (1024)));
static uint8_t tx_buf[1024] __attribute__((aligned (1024)));
volatile static int need_read = 1;
volatile static int got_read = 0;
volatile static int need_write = 0;
volatile static int got_write = 0;


static void xfer_cb(usbd_device *dev, uint8_t event, uint8_t ep) {
    int res;
    // slow down with this works
    // printf("evt: %u ep: 0x%02x rx: %02x %02x tx: %02x %02x\n", event, ep, rx_buf[0], rx_buf[1], tx_buf[0], tx_buf[1]);
    msleep(10);
    if (event != usbd_evt_eptx) {
        memcpy(tx_buf, rx_buf, CDC_DATA_SZ);
        invert_buf_align32(tx_buf, CDC_DATA_SZ);
        got_read = 1;
        need_read = 0;
        got_write = 0;
        need_write = 1;
        // printf("xfer_cb writing\n");
        // res = usbd_ep_write(dev, CDC_TXD_EP, loopback_buf, CDC_DATA_SZ);
        // printf("res: %d\n", res);
    } else {
        got_write = 1;
        need_write = 0;
        got_read = 0;
        need_read = 1;
    }
}

static usbd_respond cdc_setconf (usbd_device *dev, uint8_t cfg) {
    int res = 0;
    printf("setconf: 0x%02x\n", cfg);
    switch (cfg) {
    case 0:
        /* deconfiguring device */
        usbd_ep_deconfig(dev, CDC_TXD_EP);
        usbd_ep_deconfig(dev, CDC_RXD_EP);
        usbd_reg_endpoint(dev, CDC_RXD_EP, 0);
        usbd_reg_endpoint(dev, CDC_TXD_EP, 0);
        return usbd_ack;
    case 1:
        /* configuring device */
        usbd_ep_config(dev, CDC_RXD_EP, USB_EPTYPE_BULK /* | USB_EPTYPE_DBLBUF */, CDC_DATA_SZ);
        usbd_ep_config(dev, CDC_TXD_EP, USB_EPTYPE_BULK /* | USB_EPTYPE_DBLBUF */, CDC_DATA_SZ);
        usbd_reg_endpoint(dev, CDC_RXD_EP, xfer_cb);
        usbd_reg_endpoint(dev, CDC_TXD_EP, xfer_cb);
        printf("config done\n");
        // printf("config writing\n");
        // usbd_ep_write(dev, CDC_TXD_EP, 0, 0);
        // printf("config reading\n");
        // res = usbd_ep_read(dev, CDC_RXD_EP, loopback_buf, CDC_DATA_SZ);
        // printf("res: %d\n", res);
        // printf("config reading 2\n");
        // res = usbd_ep_read(dev, CDC_RXD_EP, loopback_buf, CDC_DATA_SZ);
        // printf("res: %d\n", res);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static void cdc_init_usbd(void) {
    usbd_init(&udev, &usbd_hw, CDC_EP0_SIZE, ubuf, sizeof(ubuf));
    usbd_reg_config(&udev, cdc_setconf);
    usbd_reg_control(&udev, cdc_control);
    usbd_reg_descr(&udev, cdc_getdesc);
}

int main(void) {
    cdc_init_usbd();
    usbd_enable(&udev, true);
    usbd_connect(&udev, true);
    int res;
    while(1) {
        if (need_read) {
            res = usbd_ep_read(&udev, CDC_RXD_EP, rx_buf, CDC_DATA_SZ);
            if (res > 0) {
                need_read = 0;
                // printf("loop read: %d buf: %02x %02x\n", res, loopback_buf[0], loopback_buf[1]);
                // got_read = 1;
            }
        }
        usbd_poll(&udev);
        if (need_write) {
            res = usbd_ep_write(&udev, CDC_TXD_EP, tx_buf, CDC_DATA_SZ);
            if (res > 0) {
                need_write = 0;
                // printf("loop read: %d buf: %02x %02x\n", res, loopback_buf[0], loopback_buf[1]);
                // got_write = 1;
            }
        }
    }
    return 0;
}
