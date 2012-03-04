/*
 * The following offsets are derived from the V8 3.6.6.14 source.  A future
 * version of this helper will automatically generate this file based on the
 * debug metadata included in libv8.  See v8ustack.d for details on how these
 * values are used.
 */

#ifndef V8_CONSTANTS_H
#define	V8_CONSTANTS_H

#if defined(__i386)

/*
 * Frame pointer offsets
 */
#define	V8_OFF_FP_FUNC		(-0x8)
#define	V8_OFF_FP_CONTEXT	(-0x4)
#define	V8_OFF_FP_MARKER	(-0x8)

/*
 * Heap class->field offsets
 */
#define	V8_OFF_HEAP(off)	((off) - 1)

#define	V8_OFF_FUNC_SHARED	V8_OFF_HEAP(0x14)
#define	V8_OFF_SHARED_NAME	V8_OFF_HEAP(0x4)
#define	V8_OFF_SHARED_INFERRED	V8_OFF_HEAP(0x24)
#define	V8_OFF_SHARED_SCRIPT	V8_OFF_HEAP(0x1c)
#define	V8_OFF_SHARED_FUNTOK	V8_OFF_HEAP(0x4c)
#define	V8_OFF_SCRIPT_NAME	V8_OFF_HEAP(0x8)
#define	V8_OFF_SCRIPT_LENDS	V8_OFF_HEAP(0x28)
#define	V8_OFF_STR_LENGTH	V8_OFF_HEAP(0x4)
#define	V8_OFF_STR_CHARS	V8_OFF_HEAP(0xc)
#define	V8_OFF_CONSSTR_CAR	V8_OFF_HEAP(0xc)
#define	V8_OFF_CONSSTR_CDR	V8_OFF_HEAP(0x10)
#define	V8_OFF_EXTSTR_RSRC	V8_OFF_HEAP(0xc)
#define	V8_OFF_FA_SIZE		V8_OFF_HEAP(0x4)
#define	V8_OFF_FA_DATA		V8_OFF_HEAP(0x8)
#define	V8_OFF_HEAPOBJ_MAP	V8_OFF_HEAP(0x0)
#define	V8_OFF_MAP_ATTRS	V8_OFF_HEAP(0x8)

#define	NODE_OFF_EXTSTR_DATA	0x4

/*
 * Stack frame types
 */
#define	V8_FT_ENTRY		0x1
#define	V8_FT_ENTRYCONSTRUCT	0x2
#define	V8_FT_EXIT		0x3
#define	V8_FT_JAVASCRIPT	0x4
#define	V8_FT_OPTIMIZED		0x5
#define	V8_FT_INTERNAL		0x6
#define	V8_FT_CONSTRUCT		0x7
#define	V8_FT_ADAPTOR		0x8

/*
 * Instance types
 */
#define	V8_IT_FIXEDARRAY	0x9f
#define	V8_IT_CODE		0x81

/*
 * Identification masks and tags
 */
#define	V8_SmiTagMask		0x1
#define	V8_SmiTag		0x0
#define	V8_SmiValueShift	V8_SmiTagMask

#define	V8_HeapObjectTagMask	0x3
#define	V8_HeapObjectTag	0x1

#define	V8_IsNotStringMask	0x80
#define	V8_StringTag		0x0

#define	V8_StringEncodingMask	0x4
#define	V8_AsciiStringTag	0x4

#define	V8_StringRepresentationMask	0x3
#define	V8_SeqStringTag		0x0
#define	V8_ConsStringTag	0x1
#define	V8_ExternalStringTag	0x2

#else
#error "only i386 is supported for DTrace ustack helper"
#endif

#endif /* V8_CONSTANTS_H */
