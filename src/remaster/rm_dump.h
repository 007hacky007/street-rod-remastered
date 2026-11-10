/* rm_dump.h -- the remaster's picture dump (--dump-pictures). */
#ifndef RM_DUMP_H
#define RM_DUMP_H

/* Every picture of LIB1 and LIB2 in game_dir as <out>/<sha1>.bmp (8-bit, ega_palette order) plus
 * <out>/<sha1>.mask.bmp for masked pictures (255 = transparent, 0 = opaque) and one line per picture in
 * <out>/pictures.jsonl. Needs the program image loaded (mem_load_exe: the unpacker's run tables).
 * Returns the number of pictures, -1 on error (message on stderr). */
int rm_dump_pictures(const char *game_dir, const char *out);

/* SHA-1 of n bytes as 40 hex digits and a NUL. */
void rm_sha1_hex(const unsigned char *p, unsigned long n, char hex[41]);

#endif
