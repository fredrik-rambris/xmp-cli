/* Extended Module Player
 * Copyright (C) 1996-2000 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 *
 * $Id: openbsd.c,v 1.1 2001-06-02 20:25:55 cmatsuoka Exp $
 */

/* This should work for OpenBSD */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xmpi.h"
#include "driver.h"
#include "mixer.h"

static int audio_fd;

static int init (struct xmp_control *);
static int setaudio (struct xmp_control *);
static void bufdump (int);
static void shutdown (void);

static void dummy () { }

static char *help[] = {
    "gain=val", "Audio output gain (0 to 255)",
    "buffer=val", "Audio buffer size (default is 32768)",
    NULL
};

struct xmp_drv_info drv_openbsd = {
    "openbsd",		/* driver ID */
    "OpenBSD PCM audio",	/* driver description */
    help,		/* help */
    init,		/* init */
    shutdown,		/* shutdown */
    xmp_smix_numvoices,	/* numvoices */
    dummy,		/* voicepos */
    xmp_smix_echoback,	/* echoback */
    dummy,		/* setpatch */
    xmp_smix_setvol,	/* setvol */
    dummy,		/* setnote */
    xmp_smix_setpan,	/* setpan */
    dummy,		/* setbend */
    xmp_smix_seteffect,	/* seteffect */
    dummy,		/* starttimer */
    dummy,		/* stctlimer */
    dummy,		/* reset */
    bufdump,		/* bufdump */
    dummy,		/* bufwipe */
    dummy,		/* clearmem */
    dummy,		/* sync */
    xmp_smix_writepatch,/* writepatch */
    xmp_smix_getmsg,	/* getmsg */
    NULL
};


static int setaudio (struct xmp_control *ctl)
{
    audio_info_t ainfo;
    int gain = 128;
    int bsize = 32 * 1024;
    char *token;
    char **parm = ctl->parm;

    parm_init ();
    chkparm1 ("gain", gain = atoi (token));
    chkparm1 ("buffer", bsize = atoi (token));
    parm_end ();

    if (gain < AUDIO_MIN_GAIN)
	gain = AUDIO_MIN_GAIN;
    if (gain > AUDIO_MAX_GAIN)
	gain = AUDIO_MAX_GAIN;

    AUDIO_INITINFO (&ainfo);

    ainfo.play.sample_rate = ctl->freq;
    ainfo.play.channels = ctl->outfmt & XMP_FMT_MONO ? 1 : 2;
    ainfo.play.precision = ctl->resol;
    ainfo.play.encoding = ctl->resol > 8 ?
	AUDIO_ENCODING_SLINEAR : AUDIO_ENCODING_ULINEAR;
    ainfo.play.gain = gain;
    ainfo.play.buffer_size = bsize;

    if (ioctl (audio_fd, AUDIO_SETINFO, &ainfo) == -1) {
	close (audio_fd);
	return XMP_ERR_DINIT;
    }

    drv_openbsd.description = "OpenBSD PCM audio";
    return XMP_OK;
}


static int init (struct xmp_control *ctl)
{
    if ((audio_fd = open ("/dev/sound", O_WRONLY)) == -1)
	return XMP_ERR_DINIT;

    if (setaudio (ctl) != XMP_OK)
	return XMP_ERR_DINIT;

    return xmp_smix_on (ctl);
}


/* Build and write one tick (one PAL frame or 1/50 s in standard vblank
 * timed mods) of audio data to the output device.
 */
static void bufdump (int i)
{
    int j;
    void *b;

    /* Doesn't work if EINTR -- reported by Ruda Moura <ruda@helllabs.org> */
    /* for (; i -= write (audio_fd, xmp_smix_buffer (), i); ); */

    b = xmp_smix_buffer ();
    while (i) {
	if ((j = write (audio_fd, b, i)) > 0) {
	    i -= j;
	    (char *)b += j;
	} else
	    break;
    };
}


static void shutdown ()
{
    xmp_smix_off ();
    close (audio_fd);
}
