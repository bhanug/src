/*	$OpenBSD: uscanner.c,v 1.4 2001/10/31 04:24:45 nate Exp $ */
/*	$NetBSD: uscanner.c,v 1.18 2001/10/11 12:05:10 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology
 * and Nick Hibma (n_hibma@qubesoft.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#endif
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/usbdevs.h>

#ifdef USCANNER_DEBUG
#define DPRINTF(x)	if (uscannerdebug) logprintf x
#define DPRINTFN(n,x)	if (uscannerdebug>(n)) logprintf x
int	uscannerdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/* Table of scanners that may work with this driver. */
static const struct scanner_id {
	uint16_t	vendor;
	uint16_t	product;
} scanner_ids [] = {
	/* Acer Peripherals */
	{ USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_320U },
	{ USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_640U },
	{ USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_620U },
	{ USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_C310U },

	/* AGFA */
	{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCAN1212U },
	{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCAN1212U2 },
	{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANTOUCH },

	/* Kye */
	{ USB_VENDOR_KYE, USB_PRODUCT_KYE_VIVIDPRO },

	/* HP */
	{ USB_VENDOR_HP, USB_PRODUCT_HP_3300C },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_3400CSE },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_4100C },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_4200C },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_S20 },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_5200C },
#if 0
	{ USB_VENDOR_HP, USB_PRODUCT_HP_5300C },
#endif
	{ USB_VENDOR_HP, USB_PRODUCT_HP_6200C },
	{ USB_VENDOR_HP, USB_PRODUCT_HP_6300C },

	/* Avision */
	{ USB_VENDOR_AVISION, USB_PRODUCT_AVISION_1200U },

#if 0
	/* Microtek */
	{ USB_VENDOR_SCANLOGIC, USB_PRODUCT_SCANLOGIC_336CX },
	{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_X6U },
	{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_336CX },
	{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_336CX2 },
	{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_C6 },
	{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6USL },
	{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6USL2 },
	{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6UL },
#endif

	/* Mustek */
	{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200CU },
	{ USB_VENDOR_NATIONAL, USB_PRODUCT_NATIONAL_BEARPAW },
	{ USB_VENDOR_NATIONAL, USB_PRODUCT_MUSTEK_600CU },
	{ USB_VENDOR_NATIONAL, USB_PRODUCT_MUSTEK_1200USB },
	{ USB_VENDOR_NATIONAL, USB_PRODUCT_MUSTEK_1200UB },

	/* Primax */
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2X300 },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E300 },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2300 },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E3002 },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_9600 },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_600U },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_19200 },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_1200U },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G600 },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_636I },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2600 },
	{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E600 },

	/* Epson */
	{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_636 },
	{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_610 },
	{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1200 },
	{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1240 },
	{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1600 },
	{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1640 },
	{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_640U },
	{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1650 },

	/* UMAX */
	{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA1220U },
	{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA1236U },
	{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA2000U },
	{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA2200U },
	{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA3400 },

	/* Visioneer */
	{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_5300 },
	{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_7600 },
	{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_6100 },
	{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_6200 },
	{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_8100 },
	{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_8600 },

 	/* Canon */
	{ USB_VENDOR_CANON, USB_PRODUCT_CANON_N656U },

	{ 0, 0 }
};

#define	USCANNER_BUFFERSIZE	1024

struct uscanner_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	usbd_pipe_handle	sc_bulkin_pipe;
	int			sc_bulkin;
	usbd_xfer_handle	sc_bulkin_xfer;
	void 			*sc_bulkin_buffer;
	int			sc_bulkin_bufferlen;
	int			sc_bulkin_datalen;

	usbd_pipe_handle	sc_bulkout_pipe;
	int			sc_bulkout;
	usbd_xfer_handle	sc_bulkout_xfer;
	void 			*sc_bulkout_buffer;
	int			sc_bulkout_bufferlen;
	int			sc_bulkout_datalen;

	u_char			sc_state;
#define USCANNER_OPEN		0x01	/* opened */

	int			sc_refcnt;
	u_char			sc_dying;
};

#if defined(__NetBSD__) || defined(__OpenBSD__)
cdev_decl(uscanner);
#elif defined(__FreeBSD__)
d_open_t  uscanneropen;
d_close_t uscannerclose;
d_read_t  uscannerread;
d_write_t uscannerwrite;
d_poll_t  uscannerpoll;

#define USCANNER_CDEV_MAJOR	156

Static struct cdevsw uscanner_cdevsw = {
	/* open */	uscanneropen,
	/* close */	uscannerclose,
	/* read */	uscannerread,
	/* write */	uscannerwrite,
	/* ioctl */	noioctl,
	/* poll */	uscannerpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"uscanner",
	/* maj */	USCANNER_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};
#endif

Static int uscanner_do_read(struct uscanner_softc *, struct uio *, int);
Static int uscanner_do_write(struct uscanner_softc *, struct uio *, int);
Static void uscanner_do_close(struct uscanner_softc *);

#define USCANNERUNIT(n) (minor(n))

USB_DECLARE_DRIVER(uscanner);

USB_MATCH(uscanner)
{
	USB_MATCH_START(uscanner, uaa);
	int i;

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	for (i = 0; scanner_ids[i].vendor != 0; i++) {
		if (scanner_ids[i].vendor == uaa->vendor &&
		    scanner_ids[i].product == uaa->product) {
			return (UMATCH_VENDOR_PRODUCT);
		}
	}

	return (UMATCH_NONE);
}

USB_ATTACH(uscanner)
{
	USB_ATTACH_START(uscanner, sc, uaa);
	usb_interface_descriptor_t *id = 0;
	usb_endpoint_descriptor_t *ed, *ed_bulkin = NULL, *ed_bulkout = NULL;
	char devinfo[1024];
	int i;
	usbd_status err;

	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfo);

	sc->sc_udev = uaa->device;

	err = usbd_set_config_no(uaa->device, 1, 1); /* XXX */
	if (err) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	/* XXX We only check the first interface */
	err = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface);
	if (!err && sc->sc_iface)
	    id = usbd_get_interface_descriptor(sc->sc_iface);
	if (err || id == 0) {
		printf("%s: could not get interface descriptor, err=%d,id=%p\n",
		       USBDEVNAME(sc->sc_dev), err, id);
		USB_ATTACH_ERROR_RETURN;
	}

	/* Find the two first bulk endpoints */
	for (i = 0 ; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == 0) {
			printf("%s: could not read endpoint descriptor\n",
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			ed_bulkin = ed;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
		        ed_bulkout = ed;
		}

		if (ed_bulkin && ed_bulkout)	/* found all we need */
			break;
	}

	/* Verify that we goething sensible */
	if (ed_bulkin == NULL || ed_bulkout == NULL) {
		printf("%s: bulk-in and/or bulk-out endpoint not found\n",
			USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_bulkin = ed_bulkin->bEndpointAddress;
	sc->sc_bulkout = ed_bulkout->bEndpointAddress;

#ifdef __FreeBSD__
	/* the main device, ctrl endpoint */
	make_dev(&uscanner_cdevsw, USBDEVUNIT(sc->sc_dev),
		UID_ROOT, GID_OPERATOR, 0644, "%s", USBDEVNAME(sc->sc_dev));
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	USB_ATTACH_SUCCESS_RETURN;
}

int
uscanneropen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct uscanner_softc *sc;
	int unit = USCANNERUNIT(dev);
	usbd_status err;

	USB_GET_SC_OPEN(uscanner, unit, sc);

 	DPRINTFN(5, ("uscanneropen: flag=%d, mode=%d, unit=%d\n", 
		     flag, mode, unit));

	if (sc->sc_dying)
		return (ENXIO);

	if (sc->sc_state & USCANNER_OPEN)
		return (EBUSY);

	sc->sc_state |= USCANNER_OPEN;

	sc->sc_bulkin_buffer = malloc(USCANNER_BUFFERSIZE, M_USBDEV, M_WAITOK);
	sc->sc_bulkout_buffer = malloc(USCANNER_BUFFERSIZE, M_USBDEV, M_WAITOK);
	/* No need to check buffers for NULL since we have WAITOK */

	sc->sc_bulkin_bufferlen = USCANNER_BUFFERSIZE;
	sc->sc_bulkout_bufferlen = USCANNER_BUFFERSIZE;

	/* We have decided on which endpoints to use, now open the pipes */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin,
				USBD_EXCLUSIVE_USE, &sc->sc_bulkin_pipe);
	if (err) {
		printf("%s: cannot open bulk-in pipe (addr %d)\n",
			USBDEVNAME(sc->sc_dev), sc->sc_bulkin);
		uscanner_do_close(sc);
		return (EIO);
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout,
				USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
	if (err) {
		printf("%s: cannot open bulk-out pipe (addr %d)\n",
			USBDEVNAME(sc->sc_dev), sc->sc_bulkout);
		uscanner_do_close(sc);
		return (EIO);
	}

	sc->sc_bulkin_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_bulkin_xfer == NULL) {
		uscanner_do_close(sc);
		return (ENOMEM);
	}
	sc->sc_bulkout_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_bulkout_xfer == NULL) {
		uscanner_do_close(sc);
		return (ENOMEM);
	}

	return (0);	/* success */
}

int
uscannerclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct uscanner_softc *sc;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	DPRINTFN(5, ("uscannerclose: flag=%d, mode=%d, unit=%d\n",
		     flag, mode, USCANNERUNIT(dev)));

#ifdef DIAGNOSTIC
	if (!(sc->sc_state & USCANNER_OPEN)) {
		printf("uscannerclose: not open\n");
		return (EINVAL);
	}
#endif

	uscanner_do_close(sc);

	return (0);
}

void
uscanner_do_close(struct uscanner_softc *sc)
{
	if (sc->sc_bulkin_xfer) {
		usbd_free_xfer(sc->sc_bulkin_xfer);
		sc->sc_bulkin_xfer = NULL;
	}
	if (sc->sc_bulkout_xfer) {
		usbd_free_xfer(sc->sc_bulkout_xfer);
		sc->sc_bulkout_xfer = NULL;
	}

	if (sc->sc_bulkin_pipe) {
		usbd_abort_pipe(sc->sc_bulkin_pipe);
		usbd_close_pipe(sc->sc_bulkin_pipe);
		sc->sc_bulkin_pipe = NULL;
	}
	if (sc->sc_bulkout_pipe) {
		usbd_abort_pipe(sc->sc_bulkout_pipe);
		usbd_close_pipe(sc->sc_bulkout_pipe);
		sc->sc_bulkout_pipe = NULL;
	}

	if (sc->sc_bulkin_buffer) {
		free(sc->sc_bulkin_buffer, M_USBDEV);
		sc->sc_bulkin_buffer = NULL;
	}
	if (sc->sc_bulkout_buffer) {
		free(sc->sc_bulkout_buffer, M_USBDEV);
		sc->sc_bulkout_buffer = NULL;
	}

	sc->sc_state &= ~USCANNER_OPEN;
}

Static int
uscanner_do_read(sc, uio, flag)
	struct uscanner_softc *sc;
	struct uio *uio;
	int flag;
{
	u_int32_t n, tn;
	usbd_status err;
	int error = 0;

	DPRINTFN(5, ("%s: uscannerread\n", USBDEVNAME(sc->sc_dev)));

	if (sc->sc_dying)
		return (EIO);

	while ((n = min(sc->sc_bulkin_bufferlen, uio->uio_resid)) != 0) {
		DPRINTFN(1, ("uscannerread: start transfer %d bytes\n",n));
		tn = n;

		err = usbd_bulk_transfer(
			sc->sc_bulkin_xfer, sc->sc_bulkin_pipe,
			USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT,
			sc->sc_bulkin_buffer, &tn,
			"uscannerrb");
		if (err) {
			if (err == USBD_INTERRUPTED)
				error = EINTR;
			else if (err == USBD_TIMEOUT)
				error = ETIMEDOUT;
			else
				error = EIO;
			break;
		}
		DPRINTFN(1, ("uscannerread: got %d bytes\n", tn));
		error = uiomove(sc->sc_bulkin_buffer, tn, uio);
		if (error || tn < n)
			break;
	}

	return (error);
}

int
uscannerread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct uscanner_softc *sc;
	int error;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uscanner_do_read(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));

	return (error);
}

Static int
uscanner_do_write(sc, uio, flag)
	struct uscanner_softc *sc;
	struct uio *uio;
	int flag;
{
	u_int32_t n;
	int error = 0;
	usbd_status err;

	DPRINTFN(5, ("%s: uscanner_do_write\n", USBDEVNAME(sc->sc_dev)));

	if (sc->sc_dying)
		return (EIO);

	while ((n = min(sc->sc_bulkout_bufferlen, uio->uio_resid)) != 0) {
		error = uiomove(sc->sc_bulkout_buffer, n, uio);
		if (error)
			break;
		DPRINTFN(1, ("uscanner_do_write: transfer %d bytes\n", n));
		err = usbd_bulk_transfer(
			sc->sc_bulkout_xfer, sc->sc_bulkout_pipe,
			0, USBD_NO_TIMEOUT,
			sc->sc_bulkout_buffer, &n,
			"uscannerwb");
		if (err) {
			if (err == USBD_INTERRUPTED)
				error = EINTR;
			else
				error = EIO;
			break;
		}
	}

	return (error);
}

int
uscannerwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct uscanner_softc *sc;
	int error;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uscanner_do_write(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));
	return (error);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uscanner_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct uscanner_softc *sc = (struct uscanner_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}
#endif

USB_DETACH(uscanner)
{
	USB_DETACH_START(uscanner, sc);
	int s;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int maj, mn;
#elif defined(__FreeBSD__)
	dev_t dev;
	struct vnode *vp;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	DPRINTF(("uscanner_detach: sc=%p flags=%d\n", sc, flags));
#elif defined(__FreeBSD__)
	DPRINTF(("uscanner_detach: sc=%p\n", sc));
#endif

	sc->sc_dying = 1;

	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
	if (sc->sc_bulkin_pipe)
		usbd_abort_pipe(sc->sc_bulkin_pipe);
	if (sc->sc_bulkout_pipe)
		usbd_abort_pipe(sc->sc_bulkout_pipe);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == uscanneropen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit * USB_MAX_ENDPOINTS;
	vdevgone(maj, mn, mn + USB_MAX_ENDPOINTS - 1, VCHR);
#elif defined(__FreeBSD__)
	/* destroy the device for the control endpoint */
	dev = makedev(USCANNER_CDEV_MAJOR, USBDEVUNIT(sc->sc_dev));
	vp = SLIST_FIRST(&dev->si_hlist);
	if (vp)
		VOP_REVOKE(vp, REVOKEALL);
	destroy_dev(dev);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (0);
}

int
uscannerpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	struct uscanner_softc *sc;
	int revents = 0;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	if (sc->sc_dying)
		return (EIO);

	/* 
	 * We have no easy way of determining if a read will
	 * yield any data or a write will happen.
	 * Pretend they will.
	 */
	revents |= events & 
		   (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);

	return (revents);
}

int
uscannerioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	return (EINVAL);
}

#if defined(__FreeBSD__)
DRIVER_MODULE(uscanner, uhub, uscanner_driver, uscanner_devclass, usbd_driver_load, 0);
#endif
