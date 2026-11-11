/* rm_tags.h -- ENH: stand-ins for Street Rod 2 until it has remastered pictures (see rm_pack.h). */
#ifndef SR2_RM_TAGS_H
#define SR2_RM_TAGS_H

#include "types.h"

typedef u32 RmTag;
static const bool rm_tags_active = false;
static inline int rm_tag_slot(RmTag t) { return (int)(t >> 18 & 0x1FF); }
static inline int rm_tag_u(RmTag t)    { return (int)(t >> 9 & 0x1FF); }
static inline int rm_tag_v(RmTag t)    { return (int)(t & 0x1FF); }
static inline RmTag rm_mtag(u32 lin, int bit) { (void)lin; (void)bit; return 0; }
static inline RmTag rm_vtag(u16 off, int bit) { (void)off; (void)bit; return 0; }

#endif
