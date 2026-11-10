/* rm_dump.h -- the remaster's picture dump (--dump-pictures). */
#ifndef RM_DUMP_H
#define RM_DUMP_H

#include "types.h"

/* Every picture of LIB1 and LIB2 in game_dir as <out>/<sha1>.bmp (8-bit, ega_palette order) plus
 * <out>/<sha1>.mask.bmp for masked pictures (255 = transparent, 0 = opaque) and one line per picture in
 * <out>/pictures.jsonl. Needs the program image loaded (mem_load_exe: the unpacker's run tables).
 * Returns the number of pictures, -1 on error (message on stderr). */
int rm_dump_pictures(const char *game_dir, const char *out);

/* Race-view shape `type` of DS_shape_table (most detailed size, as the enhanced view draws it) as a picture:
 * *w x *h indices into px (max 256 x 256), mask 1 = no span, its origin (*x0, *y0) relative to the shape's
 * anchor, and the key (SHA-1 of "SHAPE", the type and the spans). Needs the program image. false: no shape. */
bool rm_shape_picture(int type, u8 *px, u8 *mask, int *w, int *h, int *x0, int *y0, char sha1[41]);
#define RM_SHAPE_TYPES 20

/* SHA-1 of n bytes as 40 hex digits and a NUL. */
void rm_sha1_hex(const unsigned char *p, unsigned long n, char hex[41]);

#endif
