/* Extended Module Player
 * Copyright (C) 1996-1999 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"

struct amd_instrument {
    uint8 name[23];		/* Instrument name */
    uint8 reg[11];		/* Adlib registers */
} PACKED;


struct amd_file_header {
    uint8 name[24];		/* ASCIIZ song name */
    uint8 author[24];		/* ASCIIZ author name */
    struct amd_instrument ins[26];	/* Instruments */
    uint8 len;			/* Song length */
    uint8 pat;			/* Index of last pattern */
    uint8 order[128];		/* Orders */
    uint8 magic[9];		/* 3c 6f ef 51 55 ee 52 6f 52 */
    uint8 version;		/* 0x10=normal module, 0x11=packed */
} PACKED;

static int reg_xlat[] = { 0, 5, 1, 6, 2, 7, 3, 8, 4, 9, 10 };


int amd_load (FILE * f)
{
    int r, i, j, tmode = 1;
    struct amd_file_header afh;
    struct xxm_event *event;
    char regs[11];
    uint16 w;
    uint8 b;

    LOAD_INIT ();

    fread (&afh, 1, sizeof (afh), f);
    if (strncmp ((char *) afh.magic, "<o", 2) ||
	strncmp ((char *) afh.magic + 6, "RoR", 3)) {
	return -1;
    }

    xxh->chn = 9;
    xxh->bpm = 125;
    xxh->tpo = 6;
    xxh->len = afh.len;
    xxh->pat = afh.pat + 1;
    xxh->ins = 26;
    xxh->smp = 0;
    memcpy (xxo, afh.order, xxh->len);

    strcpy (xmp_ctl->type, "Amusic");
    strncpy (xmp_ctl->name, afh.name, 24);
    strncpy (author_name, afh.author, 24);

    MODULE_INFO ();

    if (V (0))
	report ("Instruments    : %d ", xxh->ins);

    INSTRUMENT_INIT ();

    /* Load instruments */
    for (i = 0; i < xxh->ins; i++) {
	xxi[i] = calloc (sizeof (struct xxm_instrument), 1);
	strncpy ((char *) xxih[i].name, afh.ins[i].name, 23);
	str_adj ((char *) xxih[i].name);
	xxih[i].nsm = 1;
	xxi[i][0].vol = 0x40;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	xxi[i][0].xpo = -1;

	for (j = 0; j < 11; j++)
	    regs[j] = afh.ins[i].reg[reg_xlat[j]];

	if (V (1)) {
	    report ("\n[%2X] %-23.23s ", i, xxih[i].name);
	    if (regs[0] | regs[1] | regs[2] | regs[3] | regs[4] | regs[5] | regs[6]
		| regs[7] | regs[8] | regs[9] | regs[10]) {
		for (j = 0; j < 11; j++)
		    report ("%02x ", (uint8) regs[j]);
	    }
	}
	if (V (0) == 1)
	    report (".");
	xmp_drv_loadpatch (f, xxi[i][0].sid, 0, 0, NULL, regs);
    }
    if (V (0))
	report ("\n");

    if (!afh.version) {
	report (
	    "Aborting: Unpacked modules not supported. Please contact the authors.\n");
	return -1;
    }
    if (V (0))
	report ("Stored patterns: %d ", xxh->pat);
    xxp = calloc (sizeof (struct xxm_pattern *), xxh->pat + 1);

    for (i = 0; i < xxh->pat; i++) {
	PATTERN_ALLOC (i);
	for (j = 0; j < 9; j++) {
	    fread (&w, 1, 2, f);
	    L_ENDIAN16 (w);
	    xxp[i]->info[j].index = w;
	    if (w > xxh->trk)
		xxh->trk = w;
	}
	xxp[i]->rows = 64;
	if (V (0))
	    report (".");
    }
    xxh->trk++;

    fread (&w, 1, 2, f);
    if (V (0))
	report ("\nStored tracks  : %d ", w);
    xxt = calloc (sizeof (struct xxm_track *), xxh->trk);
    xxh->trk = w;

    for (i = 0; i < xxh->trk; i++) {
	fread (&w, 1, 2, f);
	xxt[w] = calloc (sizeof (struct xxm_track) +
	    sizeof (struct xxm_event) * 64, 1);
	xxt[w]->rows = 64;
	for (r = 0; r < 64; r++) {
	    event = &xxt[w]->event[r];
	    fread (&b, 1, 1, f);	/* Effect parameter */
	    if (b & 0x80) {
		r += (b & 0x7f) - 1;
		continue;
	    }
	    event->fxp = b;
	    fread (&b, 1, 1, f);	/* Instrument + effect type */
	    event->ins = MSN (b);
	    switch (b = LSN (b)) {
	    case 0:		/* Arpeggio */
		break;
	    case 4:		/* Set volume */
		b = FX_VOLSET;
		break;
	    case 1:		/* Slide up */
	    case 2:		/* Slide down */
	    case 3:		/* Modulator/carrier intensity */
	    case 8:		/* Tone portamento */
	    case 9:		/* Tremolo/vibrato */
		event->fxp = b = 0;
		break;
	    case 5:		/* Pattern jump */
		b = FX_JUMP;
		break;
	    case 6:		/* Pattern break */
		b = FX_BREAK;
		break;
	    case 7:		/* Speed */
		if (!event->fxp)
		    tmode = 3;
		if (event->fxp > 31) {
		    event->fxp = b = 0;
		    break;
		}
		event->fxp *= tmode;
		b = FX_TEMPO;
		break;
	    }
	    event->fxt = b;
	    fread (&b, 1, 1, f);	/* Note + octave + instrument */
	    event->ins |= (b & 1) << 4;
	    if ((event->note = MSN (b)))
		event->note += (1 + ((b & 0xe) >> 1)) * 12;
	}
	if (V (0) && !(i % 9))
	    report (".");
    }
    if (V (0))
	report ("\n");

    for (i = 0; i < xxh->chn; i++) {
	xxc[i].pan = 0x80;
	xxc[i].flg = XXM_CHANNEL_FM;
    }
    return 0;
}
