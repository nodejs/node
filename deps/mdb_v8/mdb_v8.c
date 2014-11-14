/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * mdb(1M) module for debugging the V8 JavaScript engine.  This implementation
 * makes heavy use of metadata defined in the V8 binary for inspecting in-memory
 * structures.  Canned configurations can be manually loaded for V8 binaries
 * that predate this metadata.  See mdb_v8_cfg.c for details.
 *
 * NOTE: This dmod implementation (including this file and related headers and C
 * files) exist in both the Node and illumos source trees.  THESE SHOULD BE KEPT
 * IN SYNC.  The version in the Node tree is built directly into modern Node
 * binaries as part of the build process, and the version in the illumos source
 * tree is delivered with the OS for debugging Node binaries that predate
 * support for including the dmod directly in the binary.  Note too that these
 * files have different licenses to match their corresponding repositories.
 */

/*
 * We hard-code our MDB_API_VERSION to be 3 to allow this module to be
 * compiled on systems with higher version numbers, but still allow the
 * resulting binary object to be used on older systems.  (We do not make use
 * of functionality present in versions later than 3.)  This is particularly
 * important for mdb_v8 because (1) it's used in particular to debug
 * application-level software and (2) it has a history of rapid evolution.
 */
#define	MDB_API_VERSION		3

#include <sys/mdb_modapi.h>
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <libproc.h>
#include <sys/avl.h>
#include <alloca.h>

#include "v8dbg.h"
#include "v8cfg.h"

#define	offsetof(s, m)	((size_t)(&(((s *)0)->m)))

/*
 * The "v8_class" and "v8_field" structures describe the C++ classes used to
 * represent V8 heap objects.
 */
typedef struct v8_class {
	struct v8_class *v8c_next;	/* list linkage */
	struct v8_class *v8c_parent;	/* parent class (inheritance) */
	struct v8_field *v8c_fields;	/* array of class fields */
	size_t		v8c_start;	/* offset of first class field */
	size_t		v8c_end;	/* offset of first subclass field */
	char		v8c_name[64];	/* heap object class name */
} v8_class_t;

typedef struct v8_field {
	struct v8_field	*v8f_next;	/* list linkage */
	ssize_t		v8f_offset;	/* field offset */
	char 		v8f_name[64];	/* field name */
	boolean_t	v8f_isbyte;	/* 1-byte int field */
	boolean_t	v8f_isstr;	/* NUL-terminated string */
} v8_field_t;

/*
 * Similarly, the "v8_enum" structure describes an enum from V8.
 */
typedef struct {
	char 	v8e_name[64];
	uint_t	v8e_value;
} v8_enum_t;

/*
 * During configuration, the dmod updates these globals with the actual set of
 * classes, types, and frame types based on the debug metadata.
 */
static v8_class_t	*v8_classes;

static v8_enum_t	v8_types[128];
static int 		v8_next_type;

static v8_enum_t 	v8_frametypes[16];
static int 		v8_next_frametype;

static int		v8_warnings;
static int		v8_silent;

/*
 * The following constants describe offsets from the frame pointer that are used
 * to inspect each stack frame.  They're initialized from the debug metadata.
 */
static ssize_t	V8_OFF_FP_CONTEXT;
static ssize_t	V8_OFF_FP_MARKER;
static ssize_t	V8_OFF_FP_FUNCTION;
static ssize_t	V8_OFF_FP_ARGS;

/*
 * The following constants are used by macros defined in heap-dbg-common.h to
 * examine the types of various V8 heap objects.  In general, the macros should
 * be preferred to using the constants directly.  The values of these constants
 * are initialized from the debug metadata.
 */
static intptr_t	V8_FirstNonstringType;
static intptr_t	V8_IsNotStringMask;
static intptr_t	V8_StringTag;
static intptr_t	V8_NotStringTag;
static intptr_t	V8_StringEncodingMask;
static intptr_t	V8_TwoByteStringTag;
static intptr_t	V8_AsciiStringTag;
static intptr_t	V8_StringRepresentationMask;
static intptr_t	V8_SeqStringTag;
static intptr_t	V8_ConsStringTag;
static intptr_t	V8_SlicedStringTag;
static intptr_t	V8_ExternalStringTag;
static intptr_t	V8_FailureTag;
static intptr_t	V8_FailureTagMask;
static intptr_t	V8_HeapObjectTag;
static intptr_t	V8_HeapObjectTagMask;
static intptr_t	V8_SmiTag;
static intptr_t	V8_SmiTagMask;
static intptr_t	V8_SmiValueShift;
static intptr_t	V8_SmiShiftSize;
static intptr_t	V8_PointerSizeLog2;

static intptr_t	V8_ISSHARED_SHIFT;
static intptr_t	V8_DICT_SHIFT;
static intptr_t	V8_DICT_PREFIX_SIZE;
static intptr_t	V8_DICT_ENTRY_SIZE;
static intptr_t	V8_DICT_START_INDEX;
static intptr_t	V8_FIELDINDEX_MASK;
static intptr_t	V8_FIELDINDEX_SHIFT;
static intptr_t	V8_PROP_IDX_CONTENT;
static intptr_t	V8_PROP_IDX_FIRST;
static intptr_t	V8_PROP_TYPE_FIELD;
static intptr_t	V8_PROP_TYPE_MASK;
static intptr_t	V8_PROP_DESC_KEY;
static intptr_t	V8_PROP_DESC_DETAILS;
static intptr_t	V8_PROP_DESC_VALUE;
static intptr_t	V8_PROP_DESC_SIZE;
static intptr_t	V8_TRANSITIONS_IDX_DESC;

static intptr_t V8_TYPE_JSOBJECT = -1;
static intptr_t V8_TYPE_JSARRAY = -1;
static intptr_t V8_TYPE_JSFUNCTION = -1;
static intptr_t V8_TYPE_FIXEDARRAY = -1;

static intptr_t V8_ELEMENTS_KIND_SHIFT;
static intptr_t V8_ELEMENTS_KIND_BITCOUNT;
static intptr_t V8_ELEMENTS_FAST_ELEMENTS;
static intptr_t V8_ELEMENTS_FAST_HOLEY_ELEMENTS;
static intptr_t V8_ELEMENTS_DICTIONARY_ELEMENTS;

/*
 * Although we have this information in v8_classes, the following offsets are
 * defined explicitly because they're used directly in code below.
 */
static ssize_t V8_OFF_CODE_INSTRUCTION_SIZE;
static ssize_t V8_OFF_CODE_INSTRUCTION_START;
static ssize_t V8_OFF_CONSSTRING_FIRST;
static ssize_t V8_OFF_CONSSTRING_SECOND;
static ssize_t V8_OFF_EXTERNALSTRING_RESOURCE;
static ssize_t V8_OFF_FIXEDARRAY_DATA;
static ssize_t V8_OFF_FIXEDARRAY_LENGTH;
static ssize_t V8_OFF_HEAPNUMBER_VALUE;
static ssize_t V8_OFF_HEAPOBJECT_MAP;
static ssize_t V8_OFF_JSARRAY_LENGTH;
static ssize_t V8_OFF_JSDATE_VALUE;
static ssize_t V8_OFF_JSFUNCTION_SHARED;
static ssize_t V8_OFF_JSOBJECT_ELEMENTS;
static ssize_t V8_OFF_JSOBJECT_PROPERTIES;
static ssize_t V8_OFF_MAP_CONSTRUCTOR;
static ssize_t V8_OFF_MAP_INOBJECT_PROPERTIES;
static ssize_t V8_OFF_MAP_INSTANCE_ATTRIBUTES;
static ssize_t V8_OFF_MAP_INSTANCE_DESCRIPTORS;
static ssize_t V8_OFF_MAP_INSTANCE_SIZE;
static ssize_t V8_OFF_MAP_BIT_FIELD;
static ssize_t V8_OFF_MAP_BIT_FIELD2;
static ssize_t V8_OFF_MAP_BIT_FIELD3;
static ssize_t V8_OFF_MAP_TRANSITIONS;
static ssize_t V8_OFF_ODDBALL_TO_STRING;
static ssize_t V8_OFF_SCRIPT_LINE_ENDS;
static ssize_t V8_OFF_SCRIPT_NAME;
static ssize_t V8_OFF_SCRIPT_SOURCE;
static ssize_t V8_OFF_SEQASCIISTR_CHARS;
static ssize_t V8_OFF_SEQONEBYTESTR_CHARS;
static ssize_t V8_OFF_SEQTWOBYTESTR_CHARS;
static ssize_t V8_OFF_SHAREDFUNCTIONINFO_CODE;
static ssize_t V8_OFF_SHAREDFUNCTIONINFO_END_POSITION;
static ssize_t V8_OFF_SHAREDFUNCTIONINFO_FUNCTION_TOKEN_POSITION;
static ssize_t V8_OFF_SHAREDFUNCTIONINFO_INFERRED_NAME;
static ssize_t V8_OFF_SHAREDFUNCTIONINFO_LENGTH;
static ssize_t V8_OFF_SHAREDFUNCTIONINFO_SCRIPT;
static ssize_t V8_OFF_SHAREDFUNCTIONINFO_NAME;
static ssize_t V8_OFF_SLICEDSTRING_PARENT;
static ssize_t V8_OFF_SLICEDSTRING_OFFSET;
static ssize_t V8_OFF_STRING_LENGTH;

/* see node_string.h */
#define	NODE_OFF_EXTSTR_DATA		sizeof (uintptr_t)

#define	V8_CONSTANT_OPTIONAL		1
#define	V8_CONSTANT_HASFALLBACK		2
#define	V8_CONSTANT_REMOVED		4

#define	V8_CONSTANT_MAJORSHIFT		3
#define	V8_CONSTANT_MAJORMASK		((1 << 4) - 1)
#define	V8_CONSTANT_MAJOR(flags)	\
	(((flags) >> V8_CONSTANT_MAJORSHIFT) & V8_CONSTANT_MAJORMASK)

#define	V8_CONSTANT_MINORSHIFT		7
#define	V8_CONSTANT_MINORMASK		((1 << 9) - 1)
#define	V8_CONSTANT_MINOR(flags)	\
	(((flags) >> V8_CONSTANT_MINORSHIFT) & V8_CONSTANT_MINORMASK)

#define	V8_CONSTANT_FALLBACK(maj, min) \
	(V8_CONSTANT_OPTIONAL | V8_CONSTANT_HASFALLBACK | \
	((maj) << V8_CONSTANT_MAJORSHIFT) | ((min) << V8_CONSTANT_MINORSHIFT))

#define V8_CONSTANT_REMOVED_SINCE(maj, min) \
	(V8_CONSTANT_REMOVED | \
	((maj) << V8_CONSTANT_MAJORSHIFT) | ((min) << V8_CONSTANT_MINORSHIFT))

/*
 * Table of constants used directly by this file.
 */
typedef struct v8_constant {
	intptr_t	*v8c_valp;
	const char	*v8c_symbol;
	uint32_t	v8c_flags;
	intptr_t	v8c_fallback;
} v8_constant_t;

static v8_constant_t v8_constants[] = {
	{ &V8_OFF_FP_CONTEXT,		"v8dbg_off_fp_context"		},
	{ &V8_OFF_FP_FUNCTION,		"v8dbg_off_fp_function"		},
	{ &V8_OFF_FP_MARKER,		"v8dbg_off_fp_marker"		},
	{ &V8_OFF_FP_ARGS,		"v8dbg_off_fp_args"		},

	{ &V8_FirstNonstringType,	"v8dbg_FirstNonstringType"	},
	{ &V8_IsNotStringMask,		"v8dbg_IsNotStringMask"		},
	{ &V8_StringTag,		"v8dbg_StringTag"		},
	{ &V8_NotStringTag,		"v8dbg_NotStringTag"		},
	{ &V8_StringEncodingMask,	"v8dbg_StringEncodingMask"	},
	{ &V8_TwoByteStringTag,		"v8dbg_TwoByteStringTag"	},
	{ &V8_AsciiStringTag,		"v8dbg_AsciiStringTag"		},
	{ &V8_StringRepresentationMask,	"v8dbg_StringRepresentationMask" },
	{ &V8_SeqStringTag,		"v8dbg_SeqStringTag"		},
	{ &V8_ConsStringTag,		"v8dbg_ConsStringTag"		},
	{ &V8_SlicedStringTag,		"v8dbg_SlicedStringTag",
	    V8_CONSTANT_FALLBACK(0, 0), 0x3 },
	{ &V8_ExternalStringTag,	"v8dbg_ExternalStringTag"	},
	{ &V8_FailureTag,		"v8dbg_FailureTag",
		V8_CONSTANT_REMOVED_SINCE(3, 28) },
	{ &V8_FailureTagMask,		"v8dbg_FailureTagMask",
		V8_CONSTANT_REMOVED_SINCE(3, 28) },
	{ &V8_HeapObjectTag,		"v8dbg_HeapObjectTag"		},
	{ &V8_HeapObjectTagMask,	"v8dbg_HeapObjectTagMask"	},
	{ &V8_SmiTag,			"v8dbg_SmiTag"			},
	{ &V8_SmiTagMask,		"v8dbg_SmiTagMask"		},
	{ &V8_SmiValueShift,		"v8dbg_SmiValueShift"		},
	{ &V8_SmiShiftSize,		"v8dbg_SmiShiftSize",
#ifdef _LP64
	    V8_CONSTANT_FALLBACK(0, 0), 31 },
#else
	    V8_CONSTANT_FALLBACK(0, 0), 0 },
#endif
	{ &V8_PointerSizeLog2,		"v8dbg_PointerSizeLog2"		},

	{ &V8_DICT_SHIFT,		"v8dbg_bit_field3_dictionary_map_shift",
	    V8_CONSTANT_FALLBACK(3, 13), 24 },
	{ &V8_DICT_PREFIX_SIZE,		"v8dbg_dict_prefix_size",
	    V8_CONSTANT_FALLBACK(3, 11), 2 },
	{ &V8_DICT_ENTRY_SIZE,		"v8dbg_dict_entry_size",
	    V8_CONSTANT_FALLBACK(3, 11), 3 },
	{ &V8_DICT_START_INDEX,		"v8dbg_dict_start_index",
	    V8_CONSTANT_FALLBACK(3, 11), 3 },
	{ &V8_FIELDINDEX_MASK,		"v8dbg_fieldindex_mask",
	    V8_CONSTANT_FALLBACK(3, 26), 0x3ff00000 },
	{ &V8_FIELDINDEX_SHIFT,		"v8dbg_fieldindex_shift",
	    V8_CONSTANT_FALLBACK(3, 26), 20 },
	{ &V8_ISSHARED_SHIFT,		"v8dbg_isshared_shift",
	    V8_CONSTANT_FALLBACK(3, 11), 0 },
	{ &V8_PROP_IDX_FIRST,		"v8dbg_prop_idx_first"		},
	{ &V8_PROP_TYPE_FIELD,		"v8dbg_prop_type_field"     	},
	{ &V8_PROP_TYPE_MASK,		"v8dbg_prop_type_mask"		},
	{ &V8_PROP_IDX_CONTENT,		"v8dbg_prop_idx_content",
	    V8_CONSTANT_OPTIONAL },
	{ &V8_PROP_DESC_KEY,		"v8dbg_prop_desc_key",
	    V8_CONSTANT_FALLBACK(0, 0), 0 },
	{ &V8_PROP_DESC_DETAILS,	"v8dbg_prop_desc_details",
	    V8_CONSTANT_FALLBACK(0, 0), 1 },
	{ &V8_PROP_DESC_VALUE,		"v8dbg_prop_desc_value",
	    V8_CONSTANT_FALLBACK(0, 0), 2 },
	{ &V8_PROP_DESC_SIZE,		"v8dbg_prop_desc_size",
	    V8_CONSTANT_FALLBACK(0, 0), 3 },
	{ &V8_TRANSITIONS_IDX_DESC,	"v8dbg_transitions_idx_descriptors",
	    V8_CONSTANT_OPTIONAL },

	{ &V8_ELEMENTS_KIND_SHIFT,	"v8dbg_elements_kind_shift",
	    V8_CONSTANT_FALLBACK(0, 0), 3 },
	{ &V8_ELEMENTS_KIND_BITCOUNT,	"v8dbg_elements_kind_bitcount",
	    V8_CONSTANT_FALLBACK(0, 0), 5 },
	{ &V8_ELEMENTS_FAST_ELEMENTS,
	    "v8dbg_elements_fast_elements",
	    V8_CONSTANT_FALLBACK(0, 0), 2 },
	{ &V8_ELEMENTS_FAST_HOLEY_ELEMENTS,
	    "v8dbg_elements_fast_holey_elements",
	    V8_CONSTANT_FALLBACK(0, 0), 3 },
	{ &V8_ELEMENTS_DICTIONARY_ELEMENTS,
	    "v8dbg_elements_dictionary_elements",
	    V8_CONSTANT_FALLBACK(0, 0), 6 },
};

static int v8_nconstants = sizeof (v8_constants) / sizeof (v8_constants[0]);

typedef struct v8_offset {
	ssize_t		*v8o_valp;
	const char	*v8o_class;
	const char	*v8o_member;
	boolean_t	v8o_optional;
} v8_offset_t;

static v8_offset_t v8_offsets[] = {
	{ &V8_OFF_CODE_INSTRUCTION_SIZE,
	    "Code", "instruction_size" },
	{ &V8_OFF_CODE_INSTRUCTION_START,
	    "Code", "instruction_start" },
	{ &V8_OFF_CONSSTRING_FIRST,
	    "ConsString", "first" },
	{ &V8_OFF_CONSSTRING_SECOND,
	    "ConsString", "second" },
	{ &V8_OFF_EXTERNALSTRING_RESOURCE,
	    "ExternalString", "resource" },
	{ &V8_OFF_FIXEDARRAY_DATA,
	    "FixedArray", "data" },
	{ &V8_OFF_FIXEDARRAY_LENGTH,
	    "FixedArray", "length" },
	{ &V8_OFF_HEAPNUMBER_VALUE,
	    "HeapNumber", "value" },
	{ &V8_OFF_HEAPOBJECT_MAP,
	    "HeapObject", "map" },
	{ &V8_OFF_JSARRAY_LENGTH,
	    "JSArray", "length" },
	{ &V8_OFF_JSDATE_VALUE,
	    "JSDate", "value", B_TRUE },
	{ &V8_OFF_JSFUNCTION_SHARED,
	    "JSFunction", "shared" },
	{ &V8_OFF_JSOBJECT_ELEMENTS,
	    "JSObject", "elements" },
	{ &V8_OFF_JSOBJECT_PROPERTIES,
	    "JSObject", "properties" },
	{ &V8_OFF_MAP_CONSTRUCTOR,
	    "Map", "constructor" },
	{ &V8_OFF_MAP_INOBJECT_PROPERTIES,
	    "Map", "inobject_properties" },
	{ &V8_OFF_MAP_INSTANCE_ATTRIBUTES,
	    "Map", "instance_attributes" },
	{ &V8_OFF_MAP_INSTANCE_DESCRIPTORS,
	    "Map", "instance_descriptors", B_TRUE },
	{ &V8_OFF_MAP_TRANSITIONS,
	    "Map", "transitions", B_TRUE },
	{ &V8_OFF_MAP_INSTANCE_SIZE,
	    "Map", "instance_size" },
	{ &V8_OFF_MAP_BIT_FIELD2,
	    "Map", "bit_field2", B_TRUE },
	{ &V8_OFF_MAP_BIT_FIELD3,
	    "Map", "bit_field3", B_TRUE },
	{ &V8_OFF_ODDBALL_TO_STRING,
	    "Oddball", "to_string" },
	{ &V8_OFF_SCRIPT_LINE_ENDS,
	    "Script", "line_ends" },
	{ &V8_OFF_SCRIPT_NAME,
	    "Script", "name" },
	{ &V8_OFF_SCRIPT_SOURCE,
	    "Script", "source" },
	{ &V8_OFF_SEQASCIISTR_CHARS,
	    "SeqAsciiString", "chars", B_TRUE },
	{ &V8_OFF_SEQONEBYTESTR_CHARS,
	    "SeqOneByteString", "chars", B_TRUE },
	{ &V8_OFF_SEQTWOBYTESTR_CHARS,
	    "SeqTwoByteString", "chars", B_TRUE },
	{ &V8_OFF_SHAREDFUNCTIONINFO_CODE,
	    "SharedFunctionInfo", "code" },
	{ &V8_OFF_SHAREDFUNCTIONINFO_END_POSITION,
	    "SharedFunctionInfo", "end_position" },
	{ &V8_OFF_SHAREDFUNCTIONINFO_FUNCTION_TOKEN_POSITION,
	    "SharedFunctionInfo", "function_token_position" },
	{ &V8_OFF_SHAREDFUNCTIONINFO_INFERRED_NAME,
	    "SharedFunctionInfo", "inferred_name" },
	{ &V8_OFF_SHAREDFUNCTIONINFO_LENGTH,
	    "SharedFunctionInfo", "length" },
	{ &V8_OFF_SHAREDFUNCTIONINFO_NAME,
	    "SharedFunctionInfo", "name" },
	{ &V8_OFF_SHAREDFUNCTIONINFO_SCRIPT,
	    "SharedFunctionInfo", "script" },
	{ &V8_OFF_SLICEDSTRING_OFFSET,
	    "SlicedString", "offset" },
	{ &V8_OFF_SLICEDSTRING_PARENT,
	    "SlicedString", "parent", B_TRUE },
	{ &V8_OFF_STRING_LENGTH,
	    "String", "length" },
};

static int v8_noffsets = sizeof (v8_offsets) / sizeof (v8_offsets[0]);

static uintptr_t v8_major;
static uintptr_t v8_minor;
static uintptr_t v8_build;
static uintptr_t v8_patch;

static int autoconf_iter_symbol(mdb_symbol_t *, void *);
static v8_class_t *conf_class_findcreate(const char *);
static v8_field_t *conf_field_create(v8_class_t *, const char *, size_t);
static char *conf_next_part(char *, char *);
static int conf_update_parent(const char *);
static int conf_update_field(v8_cfg_t *, const char *);
static int conf_update_enum(v8_cfg_t *, const char *, const char *,
    v8_enum_t *);
static int conf_update_type(v8_cfg_t *, const char *);
static int conf_update_frametype(v8_cfg_t *, const char *);
static void conf_class_compute_offsets(v8_class_t *);

static int read_typebyte(uint8_t *, uintptr_t);
static int heap_offset(const char *, const char *, ssize_t *);
static int jsfunc_name(uintptr_t, char **, size_t *);

/*
 * When iterating properties, it's useful to keep track of what kinds of
 * properties were found.  This is useful for developers to identify objects of
 * different kinds in order to debug them.
 */
typedef enum {
	JPI_NONE = 0,

	/*
	 * Indicates how properties are stored in this object.  There can be
	 * both numeric properties and some of the other kinds.
	 */
	JPI_NUMERIC	= 0x01,	/* numeric-named properties in "elements" */
	JPI_DICT	= 0x02,	/* dictionary properties */
	JPI_INOBJECT	= 0x04,	/* properties stored inside object */
	JPI_PROPS	= 0x08,	/* "properties" array */

	/* error-like cases */
	JPI_SKIPPED   = 0x10,	/* some properties were skipped */
	JPI_BADLAYOUT = 0x20,	/* we didn't recognize the layout at all */

	/* fallback cases */
	JPI_HASTRANSITIONS	= 0x100, /* found a transitions array */
	JPI_HASCONTENT		= 0x200, /* found a separate content array */
} jspropinfo_t;

typedef struct jsobj_print {
	char **jsop_bufp;
	size_t *jsop_lenp;
	int jsop_indent;
	uint64_t jsop_depth;
	boolean_t jsop_printaddr;
	uintptr_t jsop_baseaddr;
	int jsop_nprops;
	const char *jsop_member;
	boolean_t jsop_found;
	boolean_t jsop_descended;
	jspropinfo_t jsop_propinfo;
} jsobj_print_t;

static int jsobj_print_number(uintptr_t, jsobj_print_t *);
static int jsobj_print_oddball(uintptr_t, jsobj_print_t *);
static int jsobj_print_jsobject(uintptr_t, jsobj_print_t *);
static int jsobj_print_jsarray(uintptr_t, jsobj_print_t *);
static int jsobj_print_jsfunction(uintptr_t, jsobj_print_t *);
static int jsobj_print_jsdate(uintptr_t, jsobj_print_t *);

/*
 * Returns 1 if the V8 version v8_major.v8.minor is strictly older than
 * the V8 version represented by "flags".
 * Returns 0 otherwise.
 */
static int
v8_version_older(uintptr_t v8_major, uintptr_t v8_minor, uint32_t flags) {
	return 	v8_major < V8_CONSTANT_MAJOR(flags) ||
	        (v8_major == V8_CONSTANT_MAJOR(flags) &&
	         v8_minor < V8_CONSTANT_MINOR(flags));
}

/*
 * Invoked when this dmod is initially loaded to load the set of classes, enums,
 * and other constants from the metadata in the target binary.
 */
static int
autoconfigure(v8_cfg_t *cfgp)
{
	v8_class_t *clp;
	v8_enum_t *ep;
	struct v8_constant *cnp;
	int ii;
	int failed = 0;

	assert(v8_classes == NULL);

	/*
	 * Iterate all global symbols looking for metadata.
	 */
	if (cfgp->v8cfg_iter(cfgp, autoconf_iter_symbol, cfgp) != 0) {
		mdb_warn("failed to autoconfigure V8 support\n");
		return (-1);
	}

	/*
	 * By now we've configured all of the classes so we can update the
	 * "start" and "end" fields in each class with information from its
	 * parent class.
	 */
	for (clp = v8_classes; clp != NULL; clp = clp->v8c_next) {
		if (clp->v8c_end != (size_t)-1)
			continue;

		conf_class_compute_offsets(clp);
	};

	/*
	 * Load various constants used directly in the module.
	 */
	for (ii = 0; ii < v8_nconstants; ii++) {
		cnp = &v8_constants[ii];

		if (cfgp->v8cfg_readsym(cfgp,
		    cnp->v8c_symbol, cnp->v8c_valp) != -1) {
			continue;
		}

		if (!(cnp->v8c_flags & V8_CONSTANT_OPTIONAL) &&
		    (!(cnp->v8c_flags & V8_CONSTANT_REMOVED) ||
		     v8_version_older(v8_major, v8_minor, cnp->v8c_flags))) {
			mdb_warn("failed to read \"%s\"", cnp->v8c_symbol);
			failed++;
			continue;
		}

		if (!(cnp->v8c_flags & V8_CONSTANT_HASFALLBACK) ||
		    v8_major < V8_CONSTANT_MAJOR(cnp->v8c_flags) ||
		    (v8_major == V8_CONSTANT_MAJOR(cnp->v8c_flags) &&
		    v8_minor < V8_CONSTANT_MINOR(cnp->v8c_flags))) {
			*cnp->v8c_valp = -1;
			continue;
		}

		/*
		 * We have a fallback -- and we know that the version satisfies
		 * the fallback's version constraints; use the fallback value.
		 */
		*cnp->v8c_valp = cnp->v8c_fallback;
	}

	/*
	 * Load type values for well-known classes that we use a lot.
	 */
	for (ep = v8_types; ep->v8e_name[0] != '\0'; ep++) {
		if (strcmp(ep->v8e_name, "JSObject") == 0)
			V8_TYPE_JSOBJECT = ep->v8e_value;

		if (strcmp(ep->v8e_name, "JSArray") == 0)
			V8_TYPE_JSARRAY = ep->v8e_value;

		if (strcmp(ep->v8e_name, "JSFunction") == 0)
			V8_TYPE_JSFUNCTION = ep->v8e_value;

		if (strcmp(ep->v8e_name, "FixedArray") == 0)
			V8_TYPE_FIXEDARRAY = ep->v8e_value;
	}

	if (V8_TYPE_JSOBJECT == -1) {
		mdb_warn("couldn't find JSObject type\n");
		failed++;
	}

	if (V8_TYPE_JSARRAY == -1) {
		mdb_warn("couldn't find JSArray type\n");
		failed++;
	}

	if (V8_TYPE_JSFUNCTION == -1) {
		mdb_warn("couldn't find JSFunction type\n");
		failed++;
	}

	if (V8_TYPE_FIXEDARRAY == -1) {
		mdb_warn("couldn't find FixedArray type\n");
		failed++;
	}

	/*
	 * Finally, load various class offsets.
	 */
	for (ii = 0; ii < v8_noffsets; ii++) {
		struct v8_offset *offp = &v8_offsets[ii];
		const char *klass = offp->v8o_class;

again:
		if (heap_offset(klass, offp->v8o_member, offp->v8o_valp) == 0)
			continue;

		if (strcmp(klass, "FixedArray") == 0) {
			/*
			 * The V8 included in node v0.6 uses a FixedArrayBase
			 * class to contain the "length" field, while the one
			 * in v0.4 has no such base class and stores the field
			 * directly in FixedArray; if we failed to derive
			 * the offset from FixedArray, try FixedArrayBase.
			 */
			klass = "FixedArrayBase";
			goto again;
		}

		if (offp->v8o_optional) {
			*offp->v8o_valp = -1;
			continue;
		}

		mdb_warn("couldn't find class \"%s\", field \"%s\"\n",
		    offp->v8o_class, offp->v8o_member);
		failed++;
	}

	if (!((V8_OFF_SEQASCIISTR_CHARS != -1) ^
	    (V8_OFF_SEQONEBYTESTR_CHARS != -1))) {
		mdb_warn("expected exactly one of SeqAsciiString and "
		    "SeqOneByteString to be defined\n");
		failed++;
	}

	if (V8_OFF_SEQONEBYTESTR_CHARS != -1)
		V8_OFF_SEQASCIISTR_CHARS = V8_OFF_SEQONEBYTESTR_CHARS;

	if (V8_OFF_SEQTWOBYTESTR_CHARS == -1)
		V8_OFF_SEQTWOBYTESTR_CHARS = V8_OFF_SEQASCIISTR_CHARS;

	if (V8_OFF_SLICEDSTRING_PARENT == -1)
		V8_OFF_SLICEDSTRING_PARENT = V8_OFF_SLICEDSTRING_OFFSET -
		    sizeof (uintptr_t);

	/*
	 * If we don't have bit_field/bit_field2 for Map, we know that they're
	 * the second and third byte of instance_attributes.
	 */
	if (V8_OFF_MAP_BIT_FIELD == -1)
		V8_OFF_MAP_BIT_FIELD = V8_OFF_MAP_INSTANCE_ATTRIBUTES + 2;

	if (V8_OFF_MAP_BIT_FIELD2 == -1)
		V8_OFF_MAP_BIT_FIELD2 = V8_OFF_MAP_INSTANCE_ATTRIBUTES + 3;

	return (failed ? -1 : 0);
}

/* ARGSUSED */
static int
autoconf_iter_symbol(mdb_symbol_t *symp, void *arg)
{
	v8_cfg_t *cfgp = arg;

	if (strncmp(symp->sym_name, "v8dbg_parent_",
	    sizeof ("v8dbg_parent_") - 1) == 0)
		return (conf_update_parent(symp->sym_name));

	if (strncmp(symp->sym_name, "v8dbg_class_",
	    sizeof ("v8dbg_class_") - 1) == 0)
		return (conf_update_field(cfgp, symp->sym_name));

	if (strncmp(symp->sym_name, "v8dbg_type_",
	    sizeof ("v8dbg_type_") - 1) == 0)
		return (conf_update_type(cfgp, symp->sym_name));

	if (strncmp(symp->sym_name, "v8dbg_frametype_",
	    sizeof ("v8dbg_frametype_") - 1) == 0)
		return (conf_update_frametype(cfgp, symp->sym_name));

	return (0);
}

/*
 * Extracts the next field of a string whose fields are separated by "__" (as
 * the V8 metadata symbols are).
 */
static char *
conf_next_part(char *buf, char *start)
{
	char *pp;

	if ((pp = strstr(start, "__")) == NULL) {
		mdb_warn("malformed symbol name: %s\n", buf);
		return (NULL);
	}

	*pp = '\0';
	return (pp + sizeof ("__") - 1);
}

static v8_class_t *
conf_class_findcreate(const char *name)
{
	v8_class_t *clp, *iclp, *prev = NULL;
	int cmp;

	for (iclp = v8_classes; iclp != NULL; iclp = iclp->v8c_next) {
		if ((cmp = strcmp(iclp->v8c_name, name)) == 0)
			return (iclp);

		if (cmp > 0)
			break;

		prev = iclp;
	}

	if ((clp = mdb_zalloc(sizeof (*clp), UM_NOSLEEP)) == NULL)
		return (NULL);

	(void) strlcpy(clp->v8c_name, name, sizeof (clp->v8c_name));
	clp->v8c_end = (size_t)-1;
	clp->v8c_next = iclp;

	if (prev != NULL) {
		prev->v8c_next = clp;
	} else {
		v8_classes = clp;
	}

	return (clp);
}

static v8_field_t *
conf_field_create(v8_class_t *clp, const char *name, size_t offset)
{
	v8_field_t *flp, *iflp;

	if ((flp = mdb_zalloc(sizeof (*flp), UM_NOSLEEP)) == NULL)
		return (NULL);

	(void) strlcpy(flp->v8f_name, name, sizeof (flp->v8f_name));
	flp->v8f_offset = offset;

	if (clp->v8c_fields == NULL || clp->v8c_fields->v8f_offset > offset) {
		flp->v8f_next = clp->v8c_fields;
		clp->v8c_fields = flp;
		return (flp);
	}

	for (iflp = clp->v8c_fields; iflp->v8f_next != NULL;
	    iflp = iflp->v8f_next) {
		if (iflp->v8f_next->v8f_offset > offset)
			break;
	}

	flp->v8f_next = iflp->v8f_next;
	iflp->v8f_next = flp;
	return (flp);
}

/*
 * Given a "v8dbg_parent_X__Y", symbol, update the parent of class X to class Y.
 * Note that neither class necessarily exists already.
 */
static int
conf_update_parent(const char *symbol)
{
	char *pp, *qq;
	char buf[128];
	v8_class_t *clp, *pclp;

	(void) strlcpy(buf, symbol, sizeof (buf));
	pp = buf + sizeof ("v8dbg_parent_") - 1;
	qq = conf_next_part(buf, pp);

	if (qq == NULL)
		return (-1);

	clp = conf_class_findcreate(pp);
	pclp = conf_class_findcreate(qq);

	if (clp == NULL || pclp == NULL) {
		mdb_warn("mdb_v8: out of memory\n");
		return (-1);
	}

	clp->v8c_parent = pclp;
	return (0);
}

/*
 * Given a "v8dbg_class_CLASS__FIELD__TYPE", symbol, save field "FIELD" into
 * class CLASS with the offset described by the symbol.  Note that CLASS does
 * not necessarily exist already.
 */
static int
conf_update_field(v8_cfg_t *cfgp, const char *symbol)
{
	v8_class_t *clp;
	v8_field_t *flp;
	intptr_t offset;
	char *pp, *qq, *tt;
	char buf[128];

	(void) strlcpy(buf, symbol, sizeof (buf));

	pp = buf + sizeof ("v8dbg_class_") - 1;
	qq = conf_next_part(buf, pp);

	if (qq == NULL || (tt = conf_next_part(buf, qq)) == NULL)
		return (-1);

	if (cfgp->v8cfg_readsym(cfgp, symbol, &offset) == -1) {
		mdb_warn("failed to read symbol \"%s\"", symbol);
		return (-1);
	}

	if ((clp = conf_class_findcreate(pp)) == NULL ||
	    (flp = conf_field_create(clp, qq, (size_t)offset)) == NULL)
		return (-1);

	if (strcmp(tt, "int") == 0)
		flp->v8f_isbyte = B_TRUE;

	if (strcmp(tt, "char") == 0)
		flp->v8f_isstr = B_TRUE;

	return (0);
}

static int
conf_update_enum(v8_cfg_t *cfgp, const char *symbol, const char *name,
    v8_enum_t *enp)
{
	intptr_t value;

	if (cfgp->v8cfg_readsym(cfgp, symbol, &value) == -1) {
		mdb_warn("failed to read symbol \"%s\"", symbol);
		return (-1);
	}

	enp->v8e_value = (int)value;
	(void) strlcpy(enp->v8e_name, name, sizeof (enp->v8e_name));
	return (0);
}

/*
 * Given a "v8dbg_type_TYPENAME" constant, save the type name in v8_types.  Note
 * that this enum has multiple integer values with the same string label.
 */
static int
conf_update_type(v8_cfg_t *cfgp, const char *symbol)
{
	char *klass;
	v8_enum_t *enp;
	char buf[128];

	if (v8_next_type > sizeof (v8_types) / sizeof (v8_types[0])) {
		mdb_warn("too many V8 types\n");
		return (-1);
	}

	(void) strlcpy(buf, symbol, sizeof (buf));

	klass = buf + sizeof ("v8dbg_type_") - 1;
	if (conf_next_part(buf, klass) == NULL)
		return (-1);

	enp = &v8_types[v8_next_type++];
	return (conf_update_enum(cfgp, symbol, klass, enp));
}

/*
 * Given a "v8dbg_frametype_TYPENAME" constant, save the frame type in
 * v8_frametypes.
 */
static int
conf_update_frametype(v8_cfg_t *cfgp, const char *symbol)
{
	const char *frametype;
	v8_enum_t *enp;

	if (v8_next_frametype >
	    sizeof (v8_frametypes) / sizeof (v8_frametypes[0])) {
		mdb_warn("too many V8 frame types\n");
		return (-1);
	}

	enp = &v8_frametypes[v8_next_frametype++];
	frametype = symbol + sizeof ("v8dbg_frametype_") - 1;
	return (conf_update_enum(cfgp, symbol, frametype, enp));
}

/*
 * Now that all classes have been loaded, update the "start" and "end" fields of
 * each class based on the values of its parent class.
 */
static void
conf_class_compute_offsets(v8_class_t *clp)
{
	v8_field_t *flp;

	assert(clp->v8c_start == 0);
	assert(clp->v8c_end == (size_t)-1);

	if (clp->v8c_parent != NULL) {
		if (clp->v8c_parent->v8c_end == (size_t)-1)
			conf_class_compute_offsets(clp->v8c_parent);

		clp->v8c_start = clp->v8c_parent->v8c_end;
	}

	if (clp->v8c_fields == NULL) {
		clp->v8c_end = clp->v8c_start;
		return;
	}

	for (flp = clp->v8c_fields; flp->v8f_next != NULL; flp = flp->v8f_next)
		;

	if (flp == NULL)
		clp->v8c_end = clp->v8c_start;
	else
		clp->v8c_end = flp->v8f_offset + sizeof (uintptr_t);
}

/*
 * Utility functions
 */
#define	JSSTR_NONE		0
#define	JSSTR_NUDE		JSSTR_NONE

#define	JSSTR_FLAGSHIFT		16
#define	JSSTR_VERBOSE		(0x1 << JSSTR_FLAGSHIFT)
#define	JSSTR_QUOTED		(0x2 << JSSTR_FLAGSHIFT)
#define	JSSTR_ISASCII		(0x4 << JSSTR_FLAGSHIFT)

#define	JSSTR_MAXDEPTH		512
#define	JSSTR_DEPTH(f)		((f) & ((1 << JSSTR_FLAGSHIFT) - 1))
#define	JSSTR_BUMPDEPTH(f)	((f) + 1)

static int jsstr_print(uintptr_t, uint_t, char **, size_t *);
static boolean_t jsobj_is_undefined(uintptr_t addr);
static boolean_t jsobj_is_hole(uintptr_t addr);

static const char *
enum_lookup_str(v8_enum_t *enums, int val, const char *dflt)
{
	v8_enum_t *ep;

	for (ep = enums; ep->v8e_name[0] != '\0'; ep++) {
		if (ep->v8e_value == val)
			return (ep->v8e_name);
	}

	return (dflt);
}

static void
enum_print(v8_enum_t *enums)
{
	v8_enum_t *itp;

	for (itp = enums; itp->v8e_name[0] != '\0'; itp++)
		mdb_printf("%-30s = 0x%02x\n", itp->v8e_name, itp->v8e_value);
}

/*
 * b[v]snprintf behave like [v]snprintf(3c), except that they update the buffer
 * and length arguments based on how much buffer space is used by the operation.
 * This makes it much easier to combine multiple calls in sequence without
 * worrying about buffer overflow.
 */
static size_t
bvsnprintf(char **bufp, size_t *buflenp, const char *format, va_list alist)
{
	size_t rv, len;

	if (*buflenp == 0)
		return (vsnprintf(NULL, 0, format, alist));

	rv = vsnprintf(*bufp, *buflenp, format, alist);

	len = MIN(rv, *buflenp);
	*buflenp -= len;
	*bufp += len;

	return (len);
}

static size_t
bsnprintf(char **bufp, size_t *buflenp, const char *format, ...)
{
	va_list alist;
	size_t rv;

	va_start(alist, format);
	rv = bvsnprintf(bufp, buflenp, format, alist);
	va_end(alist);

	return (rv);
}

static void
v8_warn(const char *format, ...)
{
	char buf[512];
	va_list alist;
	int len;

	if (!v8_warnings || v8_silent)
		return;

	va_start(alist, format);
	(void) vsnprintf(buf, sizeof (buf), format, alist);
	va_end(alist);

	/*
	 * This is made slightly annoying because we need to effectively
	 * preserve the original format string to allow for mdb to use the
	 * new-line at the end to indicate that strerror should be elided.
	 */
	if ((len = strlen(format)) > 0 && format[len - 1] == '\n') {
		buf[strlen(buf) - 1] = '\0';
		mdb_warn("%s\n", buf);
	} else {
		mdb_warn("%s", buf);
	}
}

static v8_field_t *
conf_field_lookup(const char *klass, const char *field)
{
	v8_class_t *clp;
	v8_field_t *flp;

	for (clp = v8_classes; clp != NULL; clp = clp->v8c_next) {
		if (strcmp(klass, clp->v8c_name) == 0)
			break;
	}

	if (clp == NULL)
		return (NULL);

	for (flp = clp->v8c_fields; flp != NULL; flp = flp->v8f_next) {
		if (strcmp(field, flp->v8f_name) == 0)
			break;
	}

	return (flp);
}

/*
 * Returns in "offp" the offset of field "field" in C++ class "klass".
 */
static int
heap_offset(const char *klass, const char *field, ssize_t *offp)
{
       v8_field_t *flp;

       flp = conf_field_lookup(klass, field);

	   if (flp == NULL)
			   return (-1);

	*offp = V8_OFF_HEAP(flp->v8f_offset);
	return (0);
}

/*
 * Assuming "addr" is an instance of the C++ heap class "klass", read into *valp
 * the pointer-sized value of field "field".
 */
static int
read_heap_ptr(uintptr_t *valp, uintptr_t addr, ssize_t off)
{
	if (mdb_vread(valp, sizeof (*valp), addr + off) == -1) {
		v8_warn("failed to read offset %d from %p", off, addr);
		return (-1);
	}

	return (0);
}

/*
 * Like read_heap_ptr, but assume the field is an SMI and store the actual value
 * into *valp rather than the encoded representation.
 */
static int
read_heap_smi(uintptr_t *valp, uintptr_t addr, ssize_t off)
{
	if (read_heap_ptr(valp, addr, off) != 0)
		return (-1);

	if (!V8_IS_SMI(*valp)) {
		v8_warn("expected SMI, got %p\n", *valp);
		return (-1);
	}

	*valp = V8_SMI_VALUE(*valp);

	return (0);
}

static int
read_heap_double(double *valp, uintptr_t addr, ssize_t off)
{
	if (mdb_vread(valp, sizeof (*valp), addr + off) == -1) {
		v8_warn("failed to read heap value at %p", addr + off);
		return (-1);
	}

	return (0);
}

/*
 * Assuming "addr" refers to a FixedArray, return a newly-allocated array
 * representing its contents.
 */
static int
read_heap_array(uintptr_t addr, uintptr_t **retp, size_t *lenp, int flags)
{
	uint8_t type;
	uintptr_t len;

	if (!V8_IS_HEAPOBJECT(addr))
		return (-1);

	if (read_typebyte(&type, addr) != 0)
		return (-1);

	if (type != V8_TYPE_FIXEDARRAY)
		return (-1);

	if (read_heap_smi(&len, addr, V8_OFF_FIXEDARRAY_LENGTH) != 0)
		return (-1);

	*lenp = len;

	if (len == 0) {
		*retp = NULL;
		return (0);
	}

	if ((*retp = mdb_zalloc(len * sizeof (uintptr_t), flags)) == NULL)
		return (-1);

	if (mdb_vread(*retp, len * sizeof (uintptr_t),
	    addr + V8_OFF_FIXEDARRAY_DATA) == -1) {
		if (!(flags & UM_GC))
			mdb_free(*retp, len * sizeof (uintptr_t));

		*retp = NULL;
		return (-1);
	}

	return (0);
}

static int
read_heap_byte(uint8_t *valp, uintptr_t addr, ssize_t off)
{
	if (mdb_vread(valp, sizeof (*valp), addr + off) == -1) {
		v8_warn("failed to read heap value at %p", addr + off);
		return (-1);
	}

	return (0);
}

/*
 * This is truly horrific.  Inside the V8 Script class are a number of
 * small-integer fields like the function_token_position (an offset into the
 * script's text where the "function" token appears).  For 32-bit processes, V8
 * stores these as a sequence of SMI fields, which we know how to interpret well
 * enough.  For 64-bit processes, "to avoid wasting space", they use a different
 * trick: each 8-byte word contains two integer fields.  The low word is
 * represented like an SMI: shifted left by one.  They don't bother shifting the
 * high word, since its low bit will never be looked at (since it's not
 * word-aligned).
 *
 * This function is used for cases where we would use read_heap_smi(), except
 * that this is one of those fields that might be encoded or might not be,
 * depending on whether the address is word-aligned.
 */
static int
read_heap_maybesmi(uintptr_t *valp, uintptr_t addr, ssize_t off)
{
#ifdef _LP64
	uint32_t readval;

	if (mdb_vread(&readval, sizeof (readval), addr + off) == -1) {
		*valp = -1;
		v8_warn("failed to read offset %d from %p", off, addr);
		return (-1);
	}

	/*
	 * If this was the low half-word, it needs to be shifted right.
	 */
	if ((addr + off) % sizeof (uintptr_t) == 0)
		readval >>= 1;

	*valp = (uintptr_t)readval;
	return (0);
#else
	return (read_heap_smi(valp, addr, off));
#endif
}

/*
 * Given a heap object, returns in *valp the byte describing the type of the
 * object.  This is shorthand for first retrieving the Map at the start of the
 * heap object and then retrieving the type byte from the Map object.
 */
static int
read_typebyte(uint8_t *valp, uintptr_t addr)
{
	uintptr_t mapaddr;
	ssize_t off = V8_OFF_HEAPOBJECT_MAP;

	if (mdb_vread(&mapaddr, sizeof (mapaddr), addr + off) == -1) {
		v8_warn("failed to read type of %p", addr);
		return (-1);
	}

	if (!V8_IS_HEAPOBJECT(mapaddr)) {
		v8_warn("object map is not a heap object\n");
		return (-1);
	}

	if (read_heap_byte(valp, mapaddr, V8_OFF_MAP_INSTANCE_ATTRIBUTES) == -1)
		return (-1);

	return (0);
}

/*
 * Given a heap object, returns in *valp the size of the object.  For
 * variable-size objects, returns an undefined value.
 */
static int
read_size(size_t *valp, uintptr_t addr)
{
	uintptr_t mapaddr;
	uint8_t size;

	if (read_heap_ptr(&mapaddr, addr, V8_OFF_HEAPOBJECT_MAP) != 0)
		return (-1);

	if (!V8_IS_HEAPOBJECT(mapaddr)) {
		v8_warn("heap object map is not itself a heap object\n");
		return (-1);
	}

	if (read_heap_byte(&size, mapaddr, V8_OFF_MAP_INSTANCE_SIZE) != 0)
		return (-1);

	*valp = size << V8_PointerSizeLog2;
	return (0);
}

/*
 * Assuming "addr" refers to a FixedArray that is implementing a
 * StringDictionary, iterate over its contents calling the specified function
 * with key and value.
 */
static int
read_heap_dict(uintptr_t addr,
    int (*func)(const char *, uintptr_t, void *), void *arg)
{
	uint8_t type;
	uintptr_t len;
	char buf[512];
	char *bufp;
	int rval = -1;
	uintptr_t *dict, ndict, i;

	if (read_heap_array(addr, &dict, &ndict, UM_SLEEP) != 0)
		return (-1);

	if (V8_DICT_ENTRY_SIZE < 2) {
		v8_warn("dictionary entry size (%d) is too small for a "
		    "key and value\n", V8_DICT_ENTRY_SIZE);
		goto out;
	}

	for (i = V8_DICT_START_INDEX + V8_DICT_PREFIX_SIZE; i < ndict;
	    i += V8_DICT_ENTRY_SIZE) {
		/*
		 * The layout here is key, value, details. (This is hardcoded
		 * in Dictionary<Shape, Key>::SetEntry().)
		 */
		if (jsobj_is_undefined(dict[i]))
			continue;

		if (V8_IS_SMI(dict[i])) {
			intptr_t val = V8_SMI_VALUE(dict[i]);
			(void) snprintf(buf, sizeof (buf), "%" PRIdPTR, val);
		} else {
			if (jsobj_is_hole(dict[i])) {
				/*
				 * In some cases, the key can (apparently) be a
				 * hole, in which case we skip over it.
				 */
				continue;
			}

			if (read_typebyte(&type, dict[i]) != 0)
				goto out;

			if (!V8_TYPE_STRING(type))
				goto out;

			bufp = buf;
			len = sizeof (buf);

			if (jsstr_print(dict[i], JSSTR_NUDE, &bufp, &len) != 0)
				goto out;
		}

		if (func(buf, dict[i + 1], arg) == -1)
			goto out;
	}

	rval = 0;
out:
	mdb_free(dict, ndict * sizeof (uintptr_t));

	return (rval);
}

/*
 * Given an object, returns in "buf" the name of the constructor function.  With
 * "verbose", prints the pointer to the JSFunction object.  Given anything else,
 * returns an error (and warns the user why).
 */
static int
obj_jsconstructor(uintptr_t addr, char **bufp, size_t *lenp, boolean_t verbose)
{
	uint8_t type;
	uintptr_t map, consfunc, funcinfop;
	const char *constype;

	if (!V8_IS_HEAPOBJECT(addr) ||
	    read_typebyte(&type, addr) != 0 ||
	    (type != V8_TYPE_JSOBJECT && type != V8_TYPE_JSARRAY)) {
		mdb_warn("%p is not a JSObject\n", addr);
		return (-1);
	}

	if (mdb_vread(&map, sizeof (map), addr + V8_OFF_HEAPOBJECT_MAP) == -1 ||
	    mdb_vread(&consfunc, sizeof (consfunc),
	    map + V8_OFF_MAP_CONSTRUCTOR) == -1) {
		mdb_warn("unable to read object map\n");
		return (-1);
	}

	if (read_typebyte(&type, consfunc) != 0)
		return (-1);

	constype = enum_lookup_str(v8_types, type, "");
	if (strcmp(constype, "Oddball") == 0) {
		jsobj_print_t jsop;
		bzero(&jsop, sizeof (jsop));
		jsop.jsop_bufp = bufp;
		jsop.jsop_lenp = lenp;
		return (jsobj_print_oddball(consfunc, &jsop));
	}

	if (strcmp(constype, "JSFunction") != 0) {
		mdb_warn("constructor: expected JSFunction, found %s\n",
		    constype);
		return (-1);
	}

	if (read_heap_ptr(&funcinfop, consfunc, V8_OFF_JSFUNCTION_SHARED) != 0)
		return (-1);

	if (jsfunc_name(funcinfop, bufp, lenp) != 0)
		return (-1);

	if (verbose)
		bsnprintf(bufp, lenp, " (JSFunction: %p)", consfunc);

	return (0);
}

/*
 * Returns in "buf" a description of the type of "addr" suitable for printing.
 */
static int
obj_jstype(uintptr_t addr, char **bufp, size_t *lenp, uint8_t *typep)
{
	uint8_t typebyte;
	uintptr_t strptr, map, consfunc, funcinfop;
	const char *typename;

	if (V8_IS_FAILURE(addr)) {
		if (typep)
			*typep = 0;
		(void) bsnprintf(bufp, lenp, "'Failure' object");
		return (0);
	}

	if (V8_IS_SMI(addr)) {
		if (typep)
			*typep = 0;
		(void) bsnprintf(bufp, lenp, "SMI: value = %d",
		    V8_SMI_VALUE(addr));
		return (0);
	}

	if (read_typebyte(&typebyte, addr) != 0)
		return (-1);

	if (typep)
		*typep = typebyte;

	typename = enum_lookup_str(v8_types, typebyte, "<unknown>");
	(void) bsnprintf(bufp, lenp, typename);

	if (strcmp(typename, "Oddball") == 0) {
		if (read_heap_ptr(&strptr, addr,
		    V8_OFF_ODDBALL_TO_STRING) != -1) {
			(void) bsnprintf(bufp, lenp, ": \"");
			(void) jsstr_print(strptr, JSSTR_NUDE, bufp, lenp);
			(void) bsnprintf(bufp, lenp, "\"");
		}
	}

	if (strcmp(typename, "JSObject") == 0 &&
	    mdb_vread(&map, sizeof (map), addr + V8_OFF_HEAPOBJECT_MAP) != -1 &&
	    mdb_vread(&consfunc, sizeof (consfunc),
	    map + V8_OFF_MAP_CONSTRUCTOR) != -1 &&
	    read_typebyte(&typebyte, consfunc) == 0 &&
	    strcmp(enum_lookup_str(v8_types, typebyte, ""),
	    "JSFunction") == 0 &&
	    mdb_vread(&funcinfop, sizeof (funcinfop),
	    consfunc + V8_OFF_JSFUNCTION_SHARED) != -1) {
		(void) bsnprintf(bufp, lenp, ": ");
		(void) jsfunc_name(funcinfop, bufp, lenp);
	}

	return (0);
}

/*
 * V8 allows implementers (like Node) to store pointer-sized values into
 * internal fields within V8 heap objects.  Implementors access these values by
 * 0-based index (e.g., SetInternalField(0, value)).  These values are stored as
 * an array directly after the last actual C++ field in the C++ object.
 *
 * Node uses internal fields to refer to handles.  For example, a socket's C++
 * HandleWrap object is typically stored as internal field 0 in the JavaScript
 * Socket object.  Similarly, the native-heap-allocated chunk of memory
 * associated with a Node Buffer is referenced by field 0 in the External array
 * pointed-to by the Node Buffer JSObject.
 */
static int
obj_v8internal(uintptr_t addr, uint_t idx, uintptr_t *valp)
{
	char *bufp;
	size_t len;
	ssize_t off;
	uint8_t type;

	v8_class_t *clp;
	char buf[256];

	bufp = buf;
	len = sizeof (buf);
	if (obj_jstype(addr, &bufp, &len, &type) != 0)
		return (DCMD_ERR);

	if (type == 0) {
		mdb_warn("%p: unsupported type\n", addr);
		return (DCMD_ERR);
	}

	for (clp = v8_classes; clp != NULL; clp = clp->v8c_next) {
		if (strcmp(buf, clp->v8c_name) == 0)
			break;
	}

	if (clp == NULL) {
		mdb_warn("%p: didn't find expected class\n", addr);
		return (DCMD_ERR);
	}

	off = clp->v8c_end + (idx * sizeof (uintptr_t)) - 1;
	if (read_heap_ptr(valp, addr, off) != 0) {
		mdb_warn("%p: failed to read from %p\n", addr, addr + off);
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*
 * Print out the fields of the given object that come from the given class.
 */
static int
obj_print_fields(uintptr_t baddr, v8_class_t *clp)
{
	v8_field_t *flp;
	uintptr_t addr, value;
	int rv;
	char *bufp;
	size_t len;
	uint8_t type;
	char buf[256];

	for (flp = clp->v8c_fields; flp != NULL; flp = flp->v8f_next) {
		bufp = buf;
		len = sizeof (buf);

		addr = baddr + V8_OFF_HEAP(flp->v8f_offset);

		if (flp->v8f_isstr) {
			if (mdb_readstr(buf, sizeof (buf), addr) == -1) {
				mdb_printf("%p %s (unreadable)\n",
				    addr, flp->v8f_name);
				continue;
			}

			mdb_printf("%p %s = \"%s\"\n",
			    addr, flp->v8f_name, buf);
			continue;
		}

		if (flp->v8f_isbyte) {
			uint8_t sv;
			if (mdb_vread(&sv, sizeof (sv), addr) == -1) {
				mdb_printf("%p %s (unreadable)\n",
				    addr, flp->v8f_name);
				continue;
			}

			mdb_printf("%p %s = 0x%x\n", addr, flp->v8f_name, sv);
			continue;
		}

		rv = mdb_vread((void *)&value, sizeof (value), addr);

		if (rv != sizeof (value) ||
		    obj_jstype(value, &bufp, &len, &type) != 0) {
			mdb_printf("%p %s (unreadable)\n", addr, flp->v8f_name);
			continue;
		}

		if (type != 0 && V8_TYPE_STRING(type)) {
			(void) bsnprintf(&bufp, &len, ": ");
			(void) jsstr_print(value, JSSTR_QUOTED, &bufp, &len);
		}

		mdb_printf("%p %s = %p (%s)\n", addr, flp->v8f_name, value,
		    buf);
	}

	return (DCMD_OK);
}

/*
 * Print out all fields of the given object, starting with the root of the class
 * hierarchy and working down the most specific type.
 */
static int
obj_print_class(uintptr_t addr, v8_class_t *clp)
{
	int rv = 0;

	/*
	 * If we have no fields, we just print a simple inheritance hierarchy.
	 * If we have fields but our parent doesn't, our header includes the
	 * inheritance hierarchy.
	 */
	if (clp->v8c_end == 0) {
		mdb_printf("%s ", clp->v8c_name);

		if (clp->v8c_parent != NULL) {
			mdb_printf("< ");
			(void) obj_print_class(addr, clp->v8c_parent);
		}

		return (0);
	}

	mdb_printf("%p %s", addr, clp->v8c_name);

	if (clp->v8c_start == 0 && clp->v8c_parent != NULL) {
		mdb_printf(" < ");
		(void) obj_print_class(addr, clp->v8c_parent);
	}

	mdb_printf(" {\n");
	(void) mdb_inc_indent(4);

	if (clp->v8c_start > 0 && clp->v8c_parent != NULL)
		rv = obj_print_class(addr, clp->v8c_parent);

	rv |= obj_print_fields(addr, clp);
	(void) mdb_dec_indent(4);
	mdb_printf("}\n");

	return (rv);
}

/*
 * Print the ASCII string for the given JS string, expanding ConsStrings and
 * ExternalStrings as needed.
 */
static int jsstr_print_seq(uintptr_t, uint_t, char **, size_t *, size_t,
    ssize_t);
static int jsstr_print_cons(uintptr_t, uint_t, char **, size_t *);
static int jsstr_print_sliced(uintptr_t, uint_t, char **, size_t *);
static int jsstr_print_external(uintptr_t, uint_t, char **, size_t *);

static int
jsstr_print(uintptr_t addr, uint_t flags, char **bufp, size_t *lenp)
{
	uint8_t typebyte;
	int err = 0;
	char *lbufp;
	size_t llen;
	char buf[64];
	boolean_t verbose = flags & JSSTR_VERBOSE ? B_TRUE : B_FALSE;

	if (read_typebyte(&typebyte, addr) != 0) {
		(void) bsnprintf(bufp, lenp, "<could not read type>");
		return (-1);
	}

	if (!V8_TYPE_STRING(typebyte)) {
		(void) bsnprintf(bufp, lenp, "<not a string>");
		return (-1);
	}

	if (verbose) {
		lbufp = buf;
		llen = sizeof (buf);
		(void) obj_jstype(addr, &lbufp, &llen, NULL);
		mdb_printf("%s\n", buf);
		(void) mdb_inc_indent(4);
	}

	if (JSSTR_DEPTH(flags) > JSSTR_MAXDEPTH) {
		(void) bsnprintf(bufp, lenp, "<maximum depth exceeded>");
		return (-1);
	}

	if (V8_STRENC_ASCII(typebyte))
		flags |= JSSTR_ISASCII;
	else
		flags &= ~JSSTR_ISASCII;

	flags = JSSTR_BUMPDEPTH(flags);

	if (V8_STRREP_SEQ(typebyte))
		err = jsstr_print_seq(addr, flags, bufp, lenp, 0, -1);
	else if (V8_STRREP_CONS(typebyte))
		err = jsstr_print_cons(addr, flags, bufp, lenp);
	else if (V8_STRREP_EXT(typebyte))
		err = jsstr_print_external(addr, flags, bufp, lenp);
	else if (V8_STRREP_SLICED(typebyte))
		err = jsstr_print_sliced(addr, flags, bufp, lenp);
	else {
		(void) bsnprintf(bufp, lenp, "<unknown string type>");
		err = -1;
	}

	if (verbose)
		(void) mdb_dec_indent(4);

	return (err);
}

static int
jsstr_print_seq(uintptr_t addr, uint_t flags, char **bufp, size_t *lenp,
    size_t sliceoffset, ssize_t slicelen)
{
	/*
	 * To allow the caller to allocate a very large buffer for strings,
	 * we'll allocate a buffer sized based on our input, making it at
	 * least enough space for our ellipsis and at most 256K.
	 */
	uintptr_t i, nreadoffset, blen, nstrbytes, nstrchrs;
	ssize_t nreadbytes;
	boolean_t verbose = flags & JSSTR_VERBOSE ? B_TRUE : B_FALSE;
	boolean_t quoted = flags & JSSTR_QUOTED ? B_TRUE : B_FALSE;
	char *buf;
	uint16_t chrval;

	if (read_heap_smi(&nstrchrs, addr, V8_OFF_STRING_LENGTH) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<string (failed to read length)>");
		return (-1);
	}

	if (slicelen != -1)
		nstrchrs = slicelen;

	blen = ((flags & JSSTR_ISASCII) != 0) ? *lenp : 2 * (*lenp);
	if ((blen = MIN(blen, 256 * 1024)) == 0)
		return (0);

	if ((flags & JSSTR_ISASCII) != 0) {
		nstrbytes = nstrchrs;
		nreadoffset = sliceoffset;
		nreadbytes = nstrbytes + sizeof ("\"\"") <= *lenp ?
		    nstrbytes : *lenp - sizeof ("\"\"[...]");
	} else {
		nstrbytes = 2 * nstrchrs;
		nreadoffset = 2 * sliceoffset;
		nreadbytes = nstrchrs + sizeof ("\"\"") <= *lenp ?
		    nstrbytes : 2 * (*lenp - sizeof ("\"\"[...]"));
	}

	if (nreadbytes < 0) {
		/*
		 * We don't even have the room to store the ellipsis; zero
		 * the buffer out and set the length to zero.
		 */
		*bufp = '\0';
		*lenp = 0;
		return (0);
	}

	if (verbose) {
		mdb_printf("length: %d chars (%d bytes), "
		    "will read %d bytes from offset %d\n",
		    nstrchrs, nstrbytes, nreadbytes, nreadoffset);
		mdb_printf("given buffer size: %d, internal buffer: %d\n",
		    *lenp, blen);
	}

	if (nstrbytes == 0) {
		(void) bsnprintf(bufp, lenp, "%s%s",
		    quoted ? "\"" : "", quoted ? "\"" : "");
		return (0);
	}

	buf = alloca(blen);
	buf[0] = '\0';

	if ((flags & JSSTR_ISASCII) != 0) {
		if (mdb_readstr(buf, nreadbytes + 1,
		    addr + V8_OFF_SEQASCIISTR_CHARS + nreadoffset) == -1) {
			v8_warn("failed to read SeqString data");
			return (-1);
		}

		if (nreadbytes != nstrbytes)
			(void) strlcat(buf, "[...]", blen);

		(void) bsnprintf(bufp, lenp, "%s%s%s",
		    quoted ? "\"" : "", buf, quoted ? "\"" : "");
	} else {
		if (mdb_readstr(buf, nreadbytes,
		    addr + V8_OFF_SEQTWOBYTESTR_CHARS + nreadoffset) == -1) {
			v8_warn("failed to read SeqTwoByteString data");
			return (-1);
		}

		(void) bsnprintf(bufp, lenp, "%s", quoted ? "\"" : "");
		for (i = 0; i < nreadbytes; i += 2) {
			/*LINTED*/
			chrval = *((uint16_t *)(buf + i));
			(void) bsnprintf(bufp, lenp, "%c",
			    (isascii(chrval) || chrval == 0) ?
			    (char)chrval : '?');
		}
		if (nreadbytes != nstrbytes)
			(void) bsnprintf(bufp, lenp, "[...]");
		(void) bsnprintf(bufp, lenp, "%s", quoted ? "\"" : "");
	}

	return (0);
}

static int
jsstr_print_cons(uintptr_t addr, uint_t flags, char **bufp, size_t *lenp)
{
	boolean_t verbose = flags & JSSTR_VERBOSE ? B_TRUE : B_FALSE;
	boolean_t quoted = flags & JSSTR_QUOTED ? B_TRUE : B_FALSE;
	uintptr_t ptr1, ptr2;

	if (read_heap_ptr(&ptr1, addr, V8_OFF_CONSSTRING_FIRST) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<cons string (failed to read first)>");
		return (-1);
	}

	if (read_heap_ptr(&ptr2, addr, V8_OFF_CONSSTRING_SECOND) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<cons string (failed to read second)>");
		return (-1);
	}

	if (verbose) {
		mdb_printf("ptr1: %p\n", ptr1);
		mdb_printf("ptr2: %p\n", ptr2);
	}

	if (quoted)
		(void) bsnprintf(bufp, lenp, "\"");

	flags = JSSTR_BUMPDEPTH(flags) & ~JSSTR_QUOTED;

	if (jsstr_print(ptr1, flags, bufp, lenp) != 0)
		return (-1);

	if (jsstr_print(ptr2, flags, bufp, lenp) != 0)
		return (-1);

	if (quoted)
		(void) bsnprintf(bufp, lenp, "\"");

	return (0);
}

static int
jsstr_print_sliced(uintptr_t addr, uint_t flags, char **bufp, size_t *lenp)
{
	uintptr_t parent, offset, length;
	uint8_t typebyte;
	boolean_t verbose = flags & JSSTR_VERBOSE ? B_TRUE : B_FALSE;
	boolean_t quoted = flags & JSSTR_QUOTED ? B_TRUE : B_FALSE;

	if (read_heap_ptr(&parent, addr, V8_OFF_SLICEDSTRING_PARENT) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<sliced string (failed to read parent)>");
		return (-1);
	}

	if (read_heap_smi(&offset, addr, V8_OFF_SLICEDSTRING_OFFSET) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<sliced string (failed to read offset)>");
		return (-1);
	}

	if (read_heap_smi(&length, addr, V8_OFF_STRING_LENGTH) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<sliced string (failed to read length)>");
		return (-1);
	}

	if (verbose)
		mdb_printf("parent: %p, offset = %d, length = %d\n",
		    parent, offset, length);

	if (read_typebyte(&typebyte, parent) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<sliced string (failed to read parent type)>");
		return (0);
	}

	if (!V8_STRREP_SEQ(typebyte)) {
		(void) bsnprintf(bufp, lenp,
		    "<sliced string (parent is not a sequential string)>");
		return (0);
	}

	if (quoted)
		(void) bsnprintf(bufp, lenp, "\"");

	flags = JSSTR_BUMPDEPTH(flags) & ~JSSTR_QUOTED;

	if (V8_STRENC_ASCII(typebyte))
		flags |= JSSTR_ISASCII;

	if (jsstr_print_seq(parent, flags, bufp, lenp, offset, length) != 0)
		return (-1);

	if (quoted)
		(void) bsnprintf(bufp, lenp, "\"");

	return (0);
}

static int
jsstr_print_external(uintptr_t addr, uint_t flags, char **bufp, size_t *lenp)
{
	uintptr_t ptr1, ptr2;
	size_t blen = *lenp + 1;
	char *buf;
	boolean_t quoted = flags & JSSTR_QUOTED ? B_TRUE : B_FALSE;
	int rval = -1;

	if ((flags & JSSTR_ISASCII) == 0) {
		(void) bsnprintf(bufp, lenp, "<external two-byte string>");
		return (0);
	}

	if (flags & JSSTR_VERBOSE)
		mdb_printf("assuming Node.js string\n");

	if (read_heap_ptr(&ptr1, addr, V8_OFF_EXTERNALSTRING_RESOURCE) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<external string (failed to read resource)>");
		return (-1);
	}

	if (mdb_vread(&ptr2, sizeof (ptr2),
	    ptr1 + NODE_OFF_EXTSTR_DATA) == -1) {
		(void) bsnprintf(bufp, lenp, "<external string (failed to "
		    "read node external pointer %p)>",
		    ptr1 + NODE_OFF_EXTSTR_DATA);
		return (-1);
	}

	buf = mdb_alloc(blen, UM_SLEEP);

	if (mdb_readstr(buf, blen, ptr2) == -1) {
		(void) bsnprintf(bufp, lenp, "<external string "
		    "(failed to read ExternalString data)>");
		goto out;
	}

	if (buf[0] != '\0' && !isascii(buf[0])) {
		(void) bsnprintf(bufp, lenp, "<external string "
		    "(failed to read ExternalString ascii data)>");
		goto out;
	}

	(void) bsnprintf(bufp, lenp, "%s%s%s",
	    quoted ? "\"" : "", buf, quoted ? "\"" : "");

	rval = 0;
out:
	mdb_free(buf, blen);

	return (rval);
}

/*
 * Returns true if the given address refers to the named oddball object (e.g.
 * "undefined").  Returns false on failure (since we shouldn't fail on the
 * actual "undefined" value).
 */
static boolean_t
jsobj_is_oddball(uintptr_t addr, char *oddball)
{
	uint8_t type;
	uintptr_t strptr;
	const char *typename;
	char buf[16];
	char *bufp = buf;
	size_t len = sizeof (buf);

	v8_silent++;

	if (read_typebyte(&type, addr) != 0) {
		v8_silent--;
		return (B_FALSE);
	}

	v8_silent--;
	typename = enum_lookup_str(v8_types, type, "<unknown>");
	if (strcmp(typename, "Oddball") != 0)
		return (B_FALSE);

	if (read_heap_ptr(&strptr, addr, V8_OFF_ODDBALL_TO_STRING) == -1)
		return (B_FALSE);

	if (jsstr_print(strptr, JSSTR_NUDE, &bufp, &len) != 0)
		return (B_FALSE);

	return (strcmp(buf, oddball) == 0);
}

static boolean_t
jsobj_is_undefined(uintptr_t addr)
{
	return (jsobj_is_oddball(addr, "undefined"));
}

static boolean_t
jsobj_is_hole(uintptr_t addr)
{
	return (jsobj_is_oddball(addr, "hole"));
}

/*
 * Iterate the properties of a JavaScript object "addr".
 *
 * Every heap object refers to a Map that describes how that heap object is laid
 * out.  The Map includes information like the constructor function used to
 * create the object, how many bytes each object uses, and how many properties
 * are stored inside the object.  (A single Map object can be shared by many
 * objects of the same general type, which is why this information is encoded by
 * reference rather than contained in each object.)
 *
 * V8 knows about lots of different kinds of properties:
 *
 *    o properties with numeric names (e.g., array elements)
 *    o dictionary properties
 *    o "fast" properties stored inside each object, much like a C struct
 *    o properties stored in the separate "properties" array
 *    o getters, setters, and other magic (not supported by this module)
 *
 * While property lookup in JavaScript involves traversing an object's prototype
 * chain, this module only iterates the properties local to the object itself.
 *
 *
 * Numeric properties
 *
 * Properties having numeric indexes are stored in the "elements" array attached
 * to each object.  Objects with numeric properties can also have other
 * properties.
 *
 *
 * Dictionary properties
 *
 * An object with dictionary properties is identified by one of the bits in
 * "bitfield3" in the object's Map.  For details on slow properties, see
 * read_heap_dict().
 *
 *
 * Other properties
 *
 * The Map object refers to an array of "instance descriptors".  This array has
 * a few metadata entries at the front, followed by groups of three entries for
 * each property.  In Node v0.10 and later, it looks roughly like this:
 *
 *               +--------------+         +----------------------+
 *               | JSObject     |    +--> | Map                  |
 *               +--------------|    |    +----------------------+
 *               | map          | ---+    | ...                  |
 *               | ...          |         | instance_descriptors | --+
 *  in-object    | [prop 0 val] |         | ...                  |   |
 *  properties   | [prop 1 val] |         +----------------------+   |
 *  (not for all | ...          |                                    |
 *  objects)     | [prop N val] |                                    |
 *               +--------------+                                    |
 *                 +------------------------------------------------+
 *                 |
 *                 +----> +------------------------------+
 *                        | FixedArray                   |
 *                        +------------------------------+
 *                        | ...                          |
 *                        | prop 0 "key" descriptor      |
 *                        | prop 0 "details" descriptor  |
 *                        | prop 0 "value" descriptor    |
 *                        | prop 1 "key" descriptor      |
 *                        | prop 1 "details" descriptor  |
 *                        | prop 1 "value" descriptor    |
 *                        | ...                          |
 *                        | prop N "key" descriptor      |
 *                        | prop N "details" descriptor  |
 *                        | prop N "value" descriptor    |
 *                        +------------------------------+
 *
 * In versions of Node prior to 0.10, there's an extra level of indirection.
 * The Map refers to a "transitions" array, which has an entry that points to
 * the instance descriptors.  In both cases, the descriptors look roughly the
 * same.
 *
 * Each property is described by three pointer-sized entries:
 *
 *    o key: a string denoting the name of the property
 *    o details: a bitfield describing attributes of this property
 *    o value: an integer describing where this property's value is stored
 *
 * "key" is straightforward: it's just the name of the property as the
 * JavaScript programmer knows it.
 *
 * In versions prior to Node 0.12, "value" is an integer.  If "value" is less
 * than the number of properties stored inside the object (which is also
 * recorded in the Map), then it denotes which of the in-object property value
 * slots (shown above inside the JSObject object) stores the value for this
 * property.  If "value" is greater than the number of properties stored inside
 * the object, then it denotes which index into the separate "properties" array
 * (a separate field in the JSObject, not shown above) contains the value for
 * this property.
 *
 * In Node 0.12, for properties that are stored inside the object, the offset is
 * obtained not using "value", but using a bitfield from the "details" part of
 * the descriptor.
 *
 * Terminology notes: it's important to keep straight the different senses of
 * "object" and "property" here.  We use "JavaScript objects" to refer to the
 * things that JavaScript programmers would call objects, including instances of
 * Object and Array and subclasses of those.  These are a subset of V8 heap
 * objects, since V8 uses its heap to manage lots of other objects that
 * JavaScript programmers don't think about.  This function iterates JavaScript
 * properties of these JavaScript objects, not internal properties of heap
 * objects in general.
 *
 * Relatedly, while JavaScript programmers frequently interchange the notions of
 * property names, property values, and property configurations (e.g., getters
 * and setters, read-only or not, hidden or not), these are all distinct in the
 * implementation of the VM, and "property" typically refers to the whole
 * configuration, which may include a way to get the property name and value.
 *
 * The canonical source of the information used here is the implementation of
 * property lookup in the V8 source code, currently in Object::GetProperty.
 */

static int
jsobj_properties(uintptr_t addr,
    int (*func)(const char *, uintptr_t, void *), void *arg,
    jspropinfo_t *propinfop)
{
	uintptr_t ptr, map, elements;
	uintptr_t *props = NULL, *descs = NULL, *content = NULL, *trans, *elts;
	size_t size, nprops, ndescs, ncontent, ntrans, len;
	ssize_t ii, rndescs;
	uint8_t type, ninprops;
	int rval = -1;
	size_t ps = sizeof (uintptr_t);
	ssize_t off;
	jspropinfo_t propinfo = JPI_NONE;

	/*
	 * First, check if the JSObject's "properties" field is a FixedArray.
	 * If not, then this is something we don't know how to deal with, and
	 * we'll just pass the caller a NULL value.
	 */
	if (mdb_vread(&ptr, ps, addr + V8_OFF_JSOBJECT_PROPERTIES) == -1)
		return (-1);

	if (read_typebyte(&type, ptr) != 0)
		return (-1);

	if (type != V8_TYPE_FIXEDARRAY) {
		char buf[256];
		(void) mdb_snprintf(buf, sizeof (buf), "<%s>",
		    enum_lookup_str(v8_types, type, "unknown"));
		if (propinfop != NULL)
			*propinfop = JPI_BADLAYOUT;
		return (func(buf, NULL, arg));
	}

	/*
	 * As described above, we need the Map to figure out how to iterate the
	 * properties for this object.
	 */
	if (mdb_vread(&map, ps, addr + V8_OFF_HEAPOBJECT_MAP) == -1)
		goto err;

	/*
	 * Check to see if our elements member is an array and non-zero; if
	 * so, it contains numerically-named properties.  Whether or not there
	 * are any numerically-named properties, there may be other kinds of
	 * properties.
	 */
	if (V8_ELEMENTS_KIND_SHIFT != -1 &&
	    read_heap_ptr(&elements, addr, V8_OFF_JSOBJECT_ELEMENTS) == 0 &&
	    read_heap_array(elements, &elts, &len, UM_SLEEP) == 0 && len != 0) {
		uint8_t bit_field2, kind;
		size_t sz = len * sizeof (uintptr_t);

		if (mdb_vread(&bit_field2, sizeof (bit_field2),
		    map + V8_OFF_MAP_BIT_FIELD2) == -1) {
			mdb_free(elts, sz);
			goto err;
		}

		kind = bit_field2 >> V8_ELEMENTS_KIND_SHIFT;
		kind &= (1 << V8_ELEMENTS_KIND_BITCOUNT) - 1;
		propinfo |= JPI_NUMERIC;

		if (kind == V8_ELEMENTS_FAST_ELEMENTS ||
		    kind == V8_ELEMENTS_FAST_HOLEY_ELEMENTS) {
			for (ii = 0; ii < len; ii++) {
				char name[10];

				if (kind == V8_ELEMENTS_FAST_HOLEY_ELEMENTS &&
				    jsobj_is_hole(elts[ii]))
					continue;

				snprintf(name, sizeof (name), "%" PRIdPTR, ii);

				if (func(name, elts[ii], arg) != 0) {
					mdb_free(elts, sz);
					goto err;
				}
			}
		} else if (kind == V8_ELEMENTS_DICTIONARY_ELEMENTS) {
			propinfo |= JPI_DICT;
			if (read_heap_dict(elements, func, arg) != 0) {
				mdb_free(elts, sz);
				goto err;
			}
		}

		mdb_free(elts, sz);
	}

	if (V8_DICT_SHIFT != -1) {
		v8_field_t *flp;
		uintptr_t bit_field3;

		/*
		 * If dictionary properties are supported (the V8_DICT_SHIFT
		 * offset is not -1), then bitfield 3 tells us if the properties
		 * for this object are stored in "properties" field of the
		 * object using a Dictionary representation.
		 *
		 * Versions of V8 prior to Node 0.12 treated bit_field3 as an
		 * SMI, so it was pointer-sized, and it has to be converted from
		 * an SMI before using it.  In 0.12, it's treated as a raw
		 * uint32_t, meaning it's always int-sized and it should not be
		 * converted.  We can tell which case we're in because the debug
		 * constant (v8dbg_class_map__bit_field3__TYPE) tells us whether
		 * the TYPE is "SMI" or "int".
		 */

		flp = conf_field_lookup("Map", "bit_field3");
		if (flp == NULL || flp->v8f_isbyte) {
			/*
			 * v8f_isbyte indicates the type is "int", so we're in
			 * the int-sized not-a-SMI world.
			 */
			unsigned int bf3_value;
			if (mdb_vread(&bf3_value, sizeof (bf3_value),
				map + V8_OFF_MAP_BIT_FIELD3) == -1)
				goto err;
			bit_field3 = (uintptr_t)bf3_value;
		} else {
			/* The metadata indicates this is an SMI. */
			if (mdb_vread(&bit_field3, sizeof (bit_field3),
				map + V8_OFF_MAP_BIT_FIELD3) == -1)
					goto err;
			bit_field3 = V8_SMI_VALUE(bit_field3);
		}

		if (bit_field3 & (1 << V8_DICT_SHIFT)) {
			propinfo |= JPI_DICT;
			if (propinfop != NULL)
				*propinfop = propinfo;
			return (read_heap_dict(ptr, func, arg));
		}
	} else if (V8_OFF_MAP_INSTANCE_DESCRIPTORS != -1) {
		uintptr_t bit_field3;

		if (mdb_vread(&bit_field3, sizeof (bit_field3),
		    map + V8_OFF_MAP_INSTANCE_DESCRIPTORS) == -1)
			goto err;

		if (V8_SMI_VALUE(bit_field3) == (1 << V8_ISSHARED_SHIFT)) {
			/*
			 * On versions of V8 prior to that used in 0.10,
			 * the instance descriptors were overloaded to also
			 * be bit_field3 -- and there was no way from that
			 * field to infer a dictionary type.  Because we
			 * can't determine if the map is actually the
			 * hash_table_map, we assume that if it's an object
			 * that has kIsShared set, that it is in fact a
			 * dictionary -- an assumption that is assuredly in
			 * error in some cases.
			 */
			propinfo |= JPI_DICT;
			if (propinfop != NULL)
				*propinfop = propinfo;
			return (read_heap_dict(ptr, func, arg));
		}
	}

	if (read_heap_array(ptr, &props, &nprops, UM_SLEEP) != 0)
		goto err;

	/*
	 * Check if we're looking at an older version of V8, where the instance
	 * descriptors are stored not directly in the Map, but in the
	 * "transitions" array that's stored in the Map.
	 */
	if (V8_OFF_MAP_INSTANCE_DESCRIPTORS == -1) {
		if (V8_OFF_MAP_TRANSITIONS == -1 ||
		    V8_TRANSITIONS_IDX_DESC == -1 ||
		    V8_PROP_IDX_CONTENT != -1) {
			mdb_warn("missing instance_descriptors, but did "
			    "not find expected transitions array metadata; "
			    "cannot read properties\n");
			goto err;
		}

		propinfo |= JPI_HASTRANSITIONS;
		off = V8_OFF_MAP_TRANSITIONS;
		if (mdb_vread(&ptr, ps, map + off) == -1)
			goto err;

		if (read_heap_array(ptr, &trans, &ntrans, UM_SLEEP) != 0)
			goto err;

		ptr = trans[V8_TRANSITIONS_IDX_DESC];
		mdb_free(trans, ntrans * sizeof (uintptr_t));
	} else {
		off = V8_OFF_MAP_INSTANCE_DESCRIPTORS;
		if (mdb_vread(&ptr, ps, map + off) == -1)
			goto err;
	}

	/*
	 * Either way, at this point "ptr" should refer to the descriptors
	 * array.
	 */
	if (read_heap_array(ptr, &descs, &ndescs, UM_SLEEP) != 0)
		goto err;

	/*
	 * For cases where property values are stored directly inside the object
	 * ("fast properties"), we need to know the whole size of the object and
	 * the number of properties in the object in order to calculate the
	 * correct offset for each property.
	 */
	if (read_size(&size, addr) != 0)
		size = 0;
	if (mdb_vread(&ninprops, ps,
	    map + V8_OFF_MAP_INOBJECT_PROPERTIES) == -1)
		goto err;

	if (V8_PROP_IDX_CONTENT == -1) {
		/*
		 * On node v0.8 and later, the content is not stored in a
		 * separate FixedArray, but rather with the descriptors.  The
		 * number of actual properties is the length of the array minus
		 * the first (non-property) elements divided by the number of
		 * elements per property.
		 */
		content = descs;
		ncontent = ndescs;
		rndescs = ndescs > V8_PROP_IDX_FIRST ?
		    (ndescs - V8_PROP_IDX_FIRST) / V8_PROP_DESC_SIZE : 0;
	} else {
		/*
		 * On older versions, the content is stored in a separate array,
		 * and there's one entry per property (rather than three).
		 */
		if (V8_PROP_IDX_CONTENT < ndescs &&
		    read_heap_array(descs[V8_PROP_IDX_CONTENT], &content,
		    &ncontent, UM_SLEEP) != 0)
			goto err;

		rndescs = ndescs - V8_PROP_IDX_FIRST;
		propinfo |= JPI_HASCONTENT;
	}

	/*
	 * At this point, we've read all the pieces we need to process the list
	 * of instance descriptors.
	 */
	for (ii = 0; ii < rndescs; ii++) {
		uintptr_t keyidx, validx, detidx, baseidx;
		char buf[1024];
		intptr_t val;
		size_t len = sizeof (buf);
		char *c = buf;

		if (V8_PROP_IDX_CONTENT != -1) {
			/*
			 * In node versions prior to v0.8, this was hardcoded
			 * in the V8 implementation, so we hardcode it here
			 * as well.
			 */
			keyidx = ii + V8_PROP_IDX_FIRST;
			validx = ii << 1;
			detidx = (ii << 1) + 1;
		} else {
			baseidx = V8_PROP_IDX_FIRST + (ii * V8_PROP_DESC_SIZE);
			keyidx = baseidx + V8_PROP_DESC_KEY;
			validx = baseidx + V8_PROP_DESC_VALUE;
			detidx = baseidx + V8_PROP_DESC_DETAILS;
		}

		/*
		 * Ignore cases where our understanding doesn't appear to match
		 * what's here.
		 */
		if (detidx >= ncontent) {
			propinfo |= JPI_SKIPPED;
			v8_warn("property descriptor %d: detidx (%d) "
			    "out of bounds for content array (length %d)\n",
			    ii, detidx, ncontent);
			continue;
		}

		/*
		 * We only process fields.  There are other entries here
		 * (notably: transitions) that we don't care about (and these
		 * are not errors).
		 */
		if (!V8_DESC_ISFIELD(content[detidx]))
			continue;

		if (keyidx >= ndescs) {
			propinfo |= JPI_SKIPPED;
			v8_warn("property descriptor %d: keyidx (%d) "
			    "out of bounds for descriptor array (length %d)\n",
			    ii, keyidx, ndescs);
			continue;
		}

		if (jsstr_print(descs[keyidx], JSSTR_NUDE, &c, &len) != 0) {
			propinfo |= JPI_SKIPPED;
			continue;
		}

		val = (intptr_t)content[validx];
		if (!V8_IS_SMI(val)) {
			propinfo |= JPI_SKIPPED;
			v8_warn("object %p: property descriptor %d: value "
			    "index is not an SMI: %p\n", addr, ii, val);
			continue;
		}

		/*
		 * The "value" part of each property descriptor tells us whether
		 * the property value is stored directly in the object or in the
		 * related "props" array.  See JSObject::RawFastPropertyAt() in
		 * the V8 source.
		 */
		val = V8_SMI_VALUE(val) - ninprops;
		if (val < 0) {
			uintptr_t propaddr;

			/*
			 * The property is stored directly inside the object.
			 * In Node 0.10, "val - ninprops" is the (negative)
			 * index of the property counted from the end of the
			 * object.  In that context, -1 refers to the last
			 * word in the object; -2 refers to the second-last
			 * word, and so on.
			 *
			 * In Node 0.12, we get the 0-based index from the
			 * first property inside the object by reading certain
			 * bits from the property descriptor details word.
			 * These constants are literal here because they're
			 * literal in the V8 source itself.
			 */
			if (v8_major > 3 || (v8_major == 3 && v8_minor >= 26)) {
				val = V8_PROP_FIELDINDEX(content[detidx]);
				propaddr = addr + V8_OFF_HEAP(
				    size - (ninprops - val) * ps);
			} else {
				propaddr = addr + V8_OFF_HEAP(size + val * ps);
			}

			if (mdb_vread(&ptr, sizeof (ptr), propaddr) == -1) {
				propinfo |= JPI_SKIPPED;
				v8_warn("object %p: failed to read in-object "
				    "property at %p", addr, propaddr);
				continue;
			}

			propinfo |= JPI_INOBJECT;
		} else {
			/*
			 * The property is in the separate "props" array.
			 */
			if (val >= nprops) {
				/*
				 * This can happen when properties are deleted.
				 * If this value isn't obviously corrupt, we'll
				 * just silently ignore it.
				 */
				if (val < rndescs)
					continue;

				propinfo |= JPI_SKIPPED;
				v8_warn("object %p: property descriptor %d: "
				    "value index value (%d) out of bounds "
				    "(%d)\n", addr, ii, val, nprops);
				goto err;
			}

			propinfo |= JPI_PROPS;
			ptr = props[val];
		}

		if (func(buf, ptr, arg) != 0)
			goto err;
	}

	rval = 0;
	if (propinfop != NULL)
		*propinfop = propinfo;

err:
	if (props != NULL)
		mdb_free(props, nprops * sizeof (uintptr_t));

	if (descs != NULL)
		mdb_free(descs, ndescs * sizeof (uintptr_t));

	if (content != NULL && V8_PROP_IDX_CONTENT != -1)
		mdb_free(content, ncontent * sizeof (uintptr_t));

	return (rval);
}

/*
 * Given the line endings table in "lendsp", computes the line number for the
 * given token position and print the result into "buf".  If "lendsp" is
 * undefined, prints the token position instead.
 */
static int
jsfunc_lineno(uintptr_t lendsp, uintptr_t tokpos,
    char *buf, size_t buflen, int *lineno)
{
	uintptr_t size, bufsz, lower, upper, ii = 0;
	uintptr_t *data;

	if (lineno != NULL)
		*lineno = -1;

	if (jsobj_is_undefined(lendsp)) {
		/*
		 * The token position is an SMI, but it comes in as its raw
		 * value so we can more easily compare it to values in the line
		 * endings table.  If we're just printing the position directly,
		 * we must convert it here, unless we're checking against the
		 * "-1" sentinel.
		 */
		if (tokpos == V8_VALUE_SMI(-1))
			mdb_snprintf(buf, buflen, "unknown position");
		else
			mdb_snprintf(buf, buflen, "position %d",
			    V8_SMI_VALUE(tokpos));

		if (lineno != NULL)
			*lineno = 0;

		return (0);
	}

	if (read_heap_smi(&size, lendsp, V8_OFF_FIXEDARRAY_LENGTH) != 0)
		return (-1);

	bufsz = size * sizeof (data[0]);

	if ((data = mdb_alloc(bufsz, UM_NOSLEEP)) == NULL) {
		v8_warn("failed to alloc %d bytes for FixedArray data", bufsz);
		return (-1);
	}

	if (mdb_vread(data, bufsz, lendsp + V8_OFF_FIXEDARRAY_DATA) != bufsz) {
		v8_warn("failed to read FixedArray data");
		mdb_free(data, bufsz);
		return (-1);
	}

	lower = 0;
	upper = size - 1;

	if (tokpos > data[upper]) {
		(void) strlcpy(buf, "position out of range", buflen);
		mdb_free(data, bufsz);

		if (lineno != NULL)
			*lineno = 0;

		return (0);
	}

	if (tokpos <= data[0]) {
		(void) strlcpy(buf, "line 1", buflen);
		mdb_free(data, bufsz);

		if (lineno != NULL)
			*lineno = 1;

		return (0);
	}

	while (upper >= 1) {
		ii = (lower + upper) >> 1;
		if (tokpos > data[ii])
			lower = ii + 1;
		else if (tokpos <= data[ii - 1])
			upper = ii - 1;
		else
			break;
	}

	if (lineno != NULL)
		*lineno = ii + 1;

	(void) mdb_snprintf(buf, buflen, "line %d", ii + 1);
	mdb_free(data, bufsz);
	return (0);
}

/*
 * Given a Script object, prints nlines on either side of lineno, with each
 * line prefixed by prefix (if non-NULL).
 */
static void
jsfunc_lines(uintptr_t scriptp,
    uintptr_t start, uintptr_t end, int nlines, char *prefix)
{
	uintptr_t src;
	char *buf, *bufp;
	size_t bufsz = 1024, len;
	int i, line, slop = 10;
	boolean_t newline = B_TRUE;
	int startline = -1, endline = -1;

	if (read_heap_ptr(&src, scriptp, V8_OFF_SCRIPT_SOURCE) != 0)
		return;

	for (;;) {
		if ((buf = mdb_zalloc(bufsz, UM_NOSLEEP)) == NULL) {
			mdb_warn("failed to allocate source code "
			    "buffer of size %d", bufsz);
			return;
		}

		bufp = buf;
		len = bufsz;

		if (jsstr_print(src, JSSTR_NUDE, &bufp, &len) != 0) {
			mdb_free(buf, bufsz);
			return;
		}

		if (len > slop)
			break;

		mdb_free(buf, bufsz);
		bufsz <<= 1;
	}

	if (end >= bufsz)
		return;

	/*
	 * First, take a pass to determine where our lines actually start.
	 */
	for (i = 0, line = 1; buf[i] != '\0'; i++) {
		if (buf[i] == '\n')
			line++;

		if (i == start)
			startline = line;

		if (i == end) {
			endline = line;
			break;
		}
	}

	if (startline == -1 || endline == -1) {
		mdb_warn("for script %p, could not determine startline/endline"
		    " (start %ld, end %ld, nlines %d)\n",
		    scriptp, start, end, nlines);
		mdb_free(buf, bufsz);
		return;
	}

	for (i = 0, line = 1; buf[i] != '\0'; i++) {
		if (buf[i] == '\n') {
			line++;
			newline = B_TRUE;
		}

		if (line < startline - nlines)
			continue;

		if (line > endline + nlines)
			break;

		mdb_printf("%c", buf[i]);

		if (newline) {
			if (line >= startline && line <= endline)
				mdb_printf("%<b>");

			if (prefix != NULL)
				mdb_printf(prefix, line);

			if (line >= startline && line <= endline)
				mdb_printf("%</b>");

			newline = B_FALSE;
		}
	}

	mdb_printf("\n");

	if (line == endline)
		mdb_printf("%</b>");

	mdb_free(buf, bufsz);
}

/*
 * Given a SharedFunctionInfo object, prints into bufp a name of the function
 * suitable for printing.  This function attempts to infer a name for anonymous
 * functions.
 */
static int
jsfunc_name(uintptr_t funcinfop, char **bufp, size_t *lenp)
{
	uintptr_t ptrp;
	char *bufs = *bufp;

	if (read_heap_ptr(&ptrp, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_NAME) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<function (failed to read SharedFunctionInfo)>");
		return (-1);
	}

	if (jsstr_print(ptrp, JSSTR_NUDE, bufp, lenp) != 0)
		return (-1);

	if (*bufp != bufs)
		return (0);

	if (read_heap_ptr(&ptrp, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_INFERRED_NAME) != 0) {
		(void) bsnprintf(bufp, lenp, "<anonymous>");
		return (0);
	}

	(void) bsnprintf(bufp, lenp, "<anonymous> (as ");
	bufs = *bufp;

	if (jsstr_print(ptrp, JSSTR_NUDE, bufp, lenp) != 0)
		return (-1);

	if (*bufp == bufs)
		(void) bsnprintf(bufp, lenp, "<anon>");

	(void) bsnprintf(bufp, lenp, ")");

	return (0);
}

/*
 * JavaScript-level object printing
 */

static int
jsobj_print(uintptr_t addr, jsobj_print_t *jsop)
{
	uint8_t type;
	const char *klass;
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;

	const struct {
		char *name;
		int (*func)(uintptr_t, jsobj_print_t *);
	} table[] = {
		{ "HeapNumber", jsobj_print_number },
		{ "Oddball", jsobj_print_oddball },
		{ "JSObject", jsobj_print_jsobject },
		{ "JSArray", jsobj_print_jsarray },
		{ "JSFunction", jsobj_print_jsfunction },
		{ "JSDate", jsobj_print_jsdate },
		{ NULL }
	}, *ent;

	if (jsop->jsop_baseaddr != NULL && jsop->jsop_member == NULL)
		(void) bsnprintf(bufp, lenp, "%p: ", jsop->jsop_baseaddr);

	if (jsop->jsop_printaddr && jsop->jsop_member == NULL)
		(void) bsnprintf(bufp, lenp, "%p: ", addr);

	if (V8_IS_SMI(addr)) {
		(void) bsnprintf(bufp, lenp, "%d", V8_SMI_VALUE(addr));
		return (0);
	}

	if (!V8_IS_HEAPOBJECT(addr)) {
		(void) bsnprintf(bufp, lenp, "<not a heap object>");
		return (-1);
	}

	if (read_typebyte(&type, addr) != 0) {
		(void) bsnprintf(bufp, lenp, "<couldn't read type>");
		return (-1);
	}

	if (V8_TYPE_STRING(type)) {
		if (jsstr_print(addr, JSSTR_QUOTED, bufp, lenp) == -1)
			return (-1);

		return (0);
	}

	klass = enum_lookup_str(v8_types, type, "<unknown>");

	for (ent = &table[0]; ent->name != NULL; ent++) {
		if (strcmp(klass, ent->name) == 0) {
			jsop->jsop_descended = B_TRUE;
			return (ent->func(addr, jsop));
		}
	}

	(void) bsnprintf(bufp, lenp,
	    "<unknown JavaScript object type \"%s\">", klass);
	return (-1);
}

static int
jsobj_print_number(uintptr_t addr, jsobj_print_t *jsop)
{
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;
	double numval;

	if (read_heap_double(&numval, addr, V8_OFF_HEAPNUMBER_VALUE) == -1)
		return (-1);

	if (numval == (long long)numval)
		(void) bsnprintf(bufp, lenp, "%lld", (long long)numval);
	else
		(void) bsnprintf(bufp, lenp, "%e", numval);

	return (0);
}

static int
jsobj_print_oddball(uintptr_t addr, jsobj_print_t *jsop)
{
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;
	uintptr_t strptr;

	if (read_heap_ptr(&strptr, addr, V8_OFF_ODDBALL_TO_STRING) != 0)
		return (-1);

	return (jsstr_print(strptr, JSSTR_NUDE, bufp, lenp));
}

static int
jsobj_print_prop(const char *desc, uintptr_t val, void *arg)
{
	jsobj_print_t *jsop = arg, descend;
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;

	(void) bsnprintf(bufp, lenp, "%s\n%*s\"%s\": ", jsop->jsop_nprops == 0 ?
	    "{" : "", jsop->jsop_indent + 4, "", desc);

	descend = *jsop;
	descend.jsop_depth--;
	descend.jsop_indent += 4;

	(void) jsobj_print(val, &descend);
	(void) bsnprintf(bufp, lenp, ",");

	jsop->jsop_nprops++;

	return (0);
}

static int
jsobj_print_prop_member(const char *desc, uintptr_t val, void *arg)
{
	jsobj_print_t *jsop = arg, descend;
	const char *member = jsop->jsop_member, *next = member;
	int rv;

	for (; *next != '\0' && *next != '.' && *next != '['; next++)
		continue;

	if (*member == '[') {
		mdb_warn("cannot use array indexing on an object\n");
		return (-1);
	}

	if (strncmp(member, desc, next - member) != 0)
		return (0);

	if (desc[next - member] != '\0')
		return (0);

	/*
	 * This property matches the desired member; descend.
	 */
	descend = *jsop;

	if (*next == '\0') {
		descend.jsop_member = NULL;
		descend.jsop_found = B_TRUE;
	} else {
		descend.jsop_member = *next == '.' ? next + 1 : next;
	}

	rv = jsobj_print(val, &descend);
	jsop->jsop_found = descend.jsop_found;

	return (rv);
}

static int
jsobj_print_jsobject(uintptr_t addr, jsobj_print_t *jsop)
{
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;

	if (jsop->jsop_member != NULL)
		return (jsobj_properties(addr, jsobj_print_prop_member,
		    jsop, &jsop->jsop_propinfo));

	if (jsop->jsop_depth == 0) {
		(void) bsnprintf(bufp, lenp, "[...]");
		return (0);
	}

	jsop->jsop_nprops = 0;

	if (jsobj_properties(addr, jsobj_print_prop, jsop,
	    &jsop->jsop_propinfo) != 0)
		return (-1);

	if (jsop->jsop_nprops > 0) {
		(void) bsnprintf(bufp, lenp, "\n%*s", jsop->jsop_indent, "");
	} else if (jsop->jsop_nprops == 0) {
		(void) bsnprintf(bufp, lenp, "{");
	} else {
		(void) bsnprintf(bufp, lenp, "{ /* unknown property */ ");
	}

	(void) bsnprintf(bufp, lenp, "}");

	return (0);
}

static int
jsobj_print_jsarray_member(uintptr_t addr, jsobj_print_t *jsop)
{
	uintptr_t *elts;
	jsobj_print_t descend;
	uintptr_t ptr;
	const char *member = jsop->jsop_member, *end, *p;
	size_t elt = 0, place = 1, len, rv;
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;

	if (read_heap_ptr(&ptr, addr, V8_OFF_JSOBJECT_ELEMENTS) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<array member (failed to read elements)>");
		return (-1);
	}

	if (read_heap_array(ptr, &elts, &len, UM_SLEEP | UM_GC) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<array member (failed to read array)>");
		return (-1);
	}

	if (*member != '[') {
		mdb_warn("expected bracketed array index; "
		    "found '%s'\n", member);
		return (-1);
	}

	if ((end = strchr(member, ']')) == NULL) {
		mdb_warn("missing array index terminator\n");
		return (-1);
	}

	/*
	 * We know where our array index ends; convert it to an integer
	 * by stepping through it from least significant digit to most.
	 */
	for (p = end - 1; p > member; p--) {
		if (*p < '0' || *p > '9') {
			mdb_warn("illegal array index at '%c'\n", *p);
			return (-1);
		}

		elt += (*p - '0') * place;
		place *= 10;
	}

	if (place == 1) {
		mdb_warn("missing array index\n");
		return (-1);
	}

	if (elt >= len) {
		mdb_warn("array index %d exceeds size of %d\n", elt, len);
		return (-1);
	}

	descend = *jsop;

	switch (*(++end)) {
	case '\0':
		descend.jsop_member = NULL;
		descend.jsop_found = B_TRUE;
		break;

	case '.':
		descend.jsop_member = end + 1;
		break;

	case '[':
		descend.jsop_member = end;
		break;

	default:
		mdb_warn("illegal character '%c' following "
		    "array index terminator\n", *end);
		return (-1);
	}

	rv = jsobj_print(elts[elt], &descend);
	jsop->jsop_found = descend.jsop_found;

	return (rv);
}

static int
jsobj_print_jsarray(uintptr_t addr, jsobj_print_t *jsop)
{
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;
	int indent = jsop->jsop_indent;
	jsobj_print_t descend;
	uintptr_t ptr;
	uintptr_t *elts;
	size_t ii, len;

	if (jsop->jsop_member != NULL)
		return (jsobj_print_jsarray_member(addr, jsop));

	if (jsop->jsop_depth == 0) {
		(void) bsnprintf(bufp, lenp, "[...]");
		return (0);
	}

	if (read_heap_ptr(&ptr, addr, V8_OFF_JSOBJECT_ELEMENTS) != 0) {
		(void) bsnprintf(bufp, lenp,
		    "<array (failed to read elements)>");
		return (-1);
	}

	if (read_heap_array(ptr, &elts, &len, UM_SLEEP | UM_GC) != 0) {
		(void) bsnprintf(bufp, lenp, "<array (failed to read array)>");
		return (-1);
	}

	if (len == 0) {
		(void) bsnprintf(bufp, lenp, "[]");
		return (0);
	}

	descend = *jsop;
	descend.jsop_depth--;
	descend.jsop_indent += 4;

	if (len == 1) {
		(void) bsnprintf(bufp, lenp, "[ ");
		(void) jsobj_print(elts[0], &descend);
		(void) bsnprintf(bufp, lenp, " ]");
		return (0);
	}

	(void) bsnprintf(bufp, lenp, "[\n");

	for (ii = 0; ii < len && *lenp > 0; ii++) {
		(void) bsnprintf(bufp, lenp, "%*s", indent + 4, "");
		(void) jsobj_print(elts[ii], &descend);
		(void) bsnprintf(bufp, lenp, ",\n");
	}

	(void) bsnprintf(bufp, lenp, "%*s", indent, "");
	(void) bsnprintf(bufp, lenp, "]");

	return (0);
}

static int
jsobj_print_jsfunction(uintptr_t addr, jsobj_print_t *jsop)
{
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;
	uintptr_t shared;

	if (read_heap_ptr(&shared, addr, V8_OFF_JSFUNCTION_SHARED) != 0)
		return (-1);

	(void) bsnprintf(bufp, lenp, "function ");
	return (jsfunc_name(shared, bufp, lenp) != 0);
}

static int
jsobj_print_jsdate(uintptr_t addr, jsobj_print_t *jsop)
{
	char **bufp = jsop->jsop_bufp;
	size_t *lenp = jsop->jsop_lenp;
	char buf[128];
	uintptr_t value;
	uint8_t type;
	double numval;

	if (V8_OFF_JSDATE_VALUE == -1) {
		(void) bsnprintf(bufp, lenp, "<JSDate>", buf);
		return (0);
	}

	if (read_heap_ptr(&value, addr, V8_OFF_JSDATE_VALUE) != 0) {
		(void) bsnprintf(bufp, lenp, "<JSDate (failed to read value)>");
		return (-1);
	}

	if (read_typebyte(&type, value) != 0) {
		(void) bsnprintf(bufp, lenp, "<JSDate (failed to read type)>");
		return (-1);
	}

	if (strcmp(enum_lookup_str(v8_types, type, ""), "HeapNumber") != 0)
		return (-1);

	if (read_heap_double(&numval, value, V8_OFF_HEAPNUMBER_VALUE) == -1) {
		(void) bsnprintf(bufp, lenp, "<JSDate (failed to read num)>");
		return (-1);
	}

	mdb_snprintf(buf, sizeof (buf), "%Y",
	    (time_t)((long long)numval / MILLISEC));
	(void) bsnprintf(bufp, lenp, "%lld (%s)", (long long)numval, buf);

	return (0);
}

/*
 * dcmd implementations
 */

/* ARGSUSED */
static int
dcmd_v8classes(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	v8_class_t *clp;

	for (clp = v8_classes; clp != NULL; clp = clp->v8c_next)
		mdb_printf("%s\n", clp->v8c_name);

	return (DCMD_OK);
}

static int
do_v8code(uintptr_t addr, boolean_t opt_d)
{
	uintptr_t instrlen;
	ssize_t instroff = V8_OFF_CODE_INSTRUCTION_START;

	if (read_heap_ptr(&instrlen, addr, V8_OFF_CODE_INSTRUCTION_SIZE) != 0)
		return (DCMD_ERR);

	mdb_printf("code: %p\n", addr);
	mdb_printf("instructions: [%p, %p)\n", addr + instroff,
	    addr + instroff + instrlen);

	if (!opt_d)
		return (DCMD_OK);

	mdb_set_dot(addr + instroff);

	do {
		(void) mdb_inc_indent(8); /* gets reset by mdb_eval() */

		/*
		 * This is absolutely awful. We want to disassemble the above
		 * range of instructions.  Because we don't know how many there
		 * are, we can't use "::dis".  We resort to evaluating "./i",
		 * but then we need to advance "." by the size of the
		 * instruction just printed.  The only way to do that is by
		 * printing out "+", but we don't want that to show up, so we
		 * redirect it to /dev/null.
		 */
		if (mdb_eval("/i") != 0 ||
		    mdb_eval("+=p ! cat > /dev/null") != 0) {
			(void) mdb_dec_indent(8);
			v8_warn("failed to disassemble at %p", mdb_get_dot());
			return (DCMD_ERR);
		}
	} while (mdb_get_dot() < addr + instroff + instrlen);

	(void) mdb_dec_indent(8);
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_v8code(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	boolean_t opt_d = B_FALSE;

	if (mdb_getopts(argc, argv, 'd', MDB_OPT_SETBITS, B_TRUE, &opt_d,
	    NULL) != argc)
		return (DCMD_USAGE);

	return (do_v8code(addr, opt_d));
}

/* ARGSUSED */
static int
dcmd_v8function(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint8_t type;
	uintptr_t funcinfop, scriptp, lendsp, tokpos, namep, codep;
	char *bufp;
	size_t len;
	boolean_t opt_d = B_FALSE;
	char buf[512];

	if (mdb_getopts(argc, argv, 'd', MDB_OPT_SETBITS, B_TRUE, &opt_d,
	    NULL) != argc)
		return (DCMD_USAGE);

	v8_warnings++;

	if (read_typebyte(&type, addr) != 0)
		goto err;

	if (strcmp(enum_lookup_str(v8_types, type, ""), "JSFunction") != 0) {
		v8_warn("%p is not an instance of JSFunction\n", addr);
		goto err;
	}

	if (read_heap_ptr(&funcinfop, addr, V8_OFF_JSFUNCTION_SHARED) != 0 ||
	    read_heap_maybesmi(&tokpos, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_FUNCTION_TOKEN_POSITION) != 0 ||
	    read_heap_ptr(&scriptp, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_SCRIPT) != 0 ||
	    read_heap_ptr(&namep, scriptp, V8_OFF_SCRIPT_NAME) != 0 ||
	    read_heap_ptr(&lendsp, scriptp, V8_OFF_SCRIPT_LINE_ENDS) != 0)
		goto err;

	/*
	 * The token position is normally a SMI, so read_heap_maybesmi() will
	 * interpret the value for us.  However, this code uses its SMI-encoded
	 * value, so convert it back here.
	 */
	tokpos = V8_VALUE_SMI(tokpos);

	bufp = buf;
	len = sizeof (buf);
	if (jsfunc_name(funcinfop, &bufp, &len) != 0)
		goto err;

	mdb_printf("%p: JSFunction: %s\n", addr, buf);

	bufp = buf;
	len = sizeof (buf);
	mdb_printf("defined at ");

	if (jsstr_print(namep, JSSTR_NUDE, &bufp, &len) == 0)
		mdb_printf("%s ", buf);

	if (jsfunc_lineno(lendsp, tokpos, buf, sizeof (buf), NULL) == 0)
		mdb_printf("%s", buf);

	mdb_printf("\n");

	if (read_heap_ptr(&codep,
	    funcinfop, V8_OFF_SHAREDFUNCTIONINFO_CODE) != 0)
		goto err;

	v8_warnings--;

	return (do_v8code(codep, opt_d));

err:
	v8_warnings--;
	return (DCMD_ERR);
}

/*
 * Access an internal field of a V8 object.
 */
/* ARGSUSED */
static int
dcmd_v8internal(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintptr_t idx;
	uintptr_t fieldaddr;

	if (mdb_getopts(argc, argv, NULL) != argc - 1 ||
	    argv[argc - 1].a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	idx = mdb_strtoull(argv[argc - 1].a_un.a_str);
	if (obj_v8internal(addr, idx, &fieldaddr) != 0)
		return (DCMD_ERR);

	mdb_printf("%p\n", fieldaddr);
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_v8frametypes(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	enum_print(v8_frametypes);
	return (DCMD_OK);
}

static void
dcmd_v8print_help(void)
{
	mdb_printf(
	    "Prints out \".\" (a V8 heap object) as an instance of its C++\n"
	    "class.  With no arguments, the appropriate class is detected\n"
	    "automatically.  The 'class' argument overrides this to print an\n"
	    "object as an instance of the given class.  The list of known\n"
	    "classes can be viewed with ::jsclasses.");
}

/* ARGSUSED */
static int
dcmd_v8print(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char *rqclass;
	v8_class_t *clp;
	char *bufp;
	size_t len;
	uint8_t type;
	char buf[256];

	if (argc < 1) {
		/*
		 * If no type was specified, determine it automatically.
		 */
		bufp = buf;
		len = sizeof (buf);
		if (obj_jstype(addr, &bufp, &len, &type) != 0)
			return (DCMD_ERR);

		if (type == 0) {
			/* For SMI or Failure, just print out the type. */
			mdb_printf("%s\n", buf);
			return (DCMD_OK);
		}

		if ((rqclass = enum_lookup_str(v8_types, type, NULL)) == NULL) {
			v8_warn("object has unknown type\n");
			return (DCMD_ERR);
		}
	} else {
		if (argv[0].a_type != MDB_TYPE_STRING)
			return (DCMD_USAGE);

		rqclass = argv[0].a_un.a_str;
	}

	for (clp = v8_classes; clp != NULL; clp = clp->v8c_next) {
		if (strcmp(rqclass, clp->v8c_name) == 0)
			break;
	}

	if (clp == NULL) {
		v8_warn("unknown class '%s'\n", rqclass);
		return (DCMD_USAGE);
	}

	return (obj_print_class(addr, clp));
}

/* ARGSUSED */
static int
dcmd_v8type(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	char buf[64];
	char *bufp = buf;
	size_t len = sizeof (buf);

	if (obj_jstype(addr, &bufp, &len, NULL) != 0)
		return (DCMD_ERR);

	mdb_printf("0x%p: %s\n", addr, buf);
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_v8types(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	enum_print(v8_types);
	return (DCMD_OK);
}

static int
load_current_context(uintptr_t *fpp, uintptr_t *raddrp)
{
	mdb_reg_t regfp, regip;

#ifdef __amd64
	if (mdb_getareg(1, "rbp", &regfp) != 0 ||
	    mdb_getareg(1, "rip", &regip) != 0) {
#else
#ifdef __i386
	if (mdb_getareg(1, "ebp", &regfp) != 0 ||
	    mdb_getareg(1, "eip", &regip) != 0) {
#else
#error Unrecognized microprocessor
#endif
#endif
		v8_warn("failed to load current context");
		return (-1);
	}

	if (fpp != NULL)
		*fpp = (uintptr_t)regfp;

	if (raddrp != NULL)
		*raddrp = (uintptr_t)regip;

	return (0);
}

typedef struct jsframe {
	boolean_t	jsf_showall;	/* show hidden frames and pointers */
	boolean_t	jsf_verbose;	/* show arguments and JS code */
	char		*jsf_func;	/* filter frames for named function */
	char		*jsf_prop;	/* filter arguments */
	uintptr_t	jsf_nlines;	/* lines of context (for verbose) */
	uint_t		jsf_nskipped;	/* skipped frames */
} jsframe_t;

static void
jsframe_skip(jsframe_t *jsf)
{
	jsf->jsf_nskipped++;
}

static void
jsframe_print_skipped(jsframe_t *jsf)
{
	if (jsf->jsf_nskipped == 1)
		mdb_printf("        (1 internal frame elided)\n");
	else if (jsf->jsf_nskipped > 1)
		mdb_printf("        (%d internal frames elided)\n",
		    jsf->jsf_nskipped);
	jsf->jsf_nskipped = 0;
}

static int
do_jsframe_special(uintptr_t fptr, uintptr_t raddr, jsframe_t *jsf)
{
	uint_t count;
	uintptr_t ftype;
	const char *ftypename;
	char *prop = jsf->jsf_prop;

	/*
	 * First see if this looks like a native frame rather than a JavaScript
	 * frame.  We check this by asking MDB to print the return address
	 * symbolically.  If that works, we assume this was NOT a V8 frame,
	 * since those are never in the symbol table.
	 */
	count = mdb_snprintf(NULL, 0, "%A", raddr);
	if (count > 1) {
		if (prop != NULL)
			return (0);

		jsframe_print_skipped(jsf);
		if (jsf->jsf_showall) {
			mdb_printf("%p %a\n", fptr, raddr);
		} else if (count <= 65) {
			mdb_printf("native: %a\n", raddr);
		} else {
			char buf[65];
			mdb_snprintf(buf, sizeof (buf), "%a", raddr);
			mdb_printf("native: %s...\n", buf);
		}
		return (0);
	}

	/*
	 * Figure out what kind of frame this is using the same algorithm as
	 * V8's ComputeType function.  First, look for an ArgumentsAdaptorFrame.
	 */
	if (mdb_vread(&ftype, sizeof (ftype), fptr + V8_OFF_FP_CONTEXT) != -1 &&
	    V8_IS_SMI(ftype) &&
	    (ftypename = enum_lookup_str(v8_frametypes, V8_SMI_VALUE(ftype),
	    NULL)) != NULL && strstr(ftypename, "ArgumentsAdaptor") != NULL) {
		if (prop != NULL)
			return (0);

		if (jsf->jsf_showall) {
			jsframe_print_skipped(jsf);
			mdb_printf("%p %a <%s>\n", fptr, raddr, ftypename);
		} else {
			jsframe_skip(jsf);
		}
		return (0);
	}

	/*
	 * Other special frame types are indicated by a marker.
	 */
	if (mdb_vread(&ftype, sizeof (ftype), fptr + V8_OFF_FP_MARKER) != -1 &&
	    V8_IS_SMI(ftype)) {
		if (prop != NULL)
			return (0);

		ftypename = enum_lookup_str(v8_frametypes, V8_SMI_VALUE(ftype),
		    NULL);

		if (jsf->jsf_showall && ftypename != NULL) {
			jsframe_print_skipped(jsf);
			mdb_printf("%p %a <%s>\n", fptr, raddr, ftypename);
		} else {
			jsframe_skip(jsf);
		}

		return (0);
	}

	return (-1);
}

static int
do_jsframe(uintptr_t fptr, uintptr_t raddr, jsframe_t *jsf)
{
	boolean_t showall = jsf->jsf_showall;
	boolean_t verbose = jsf->jsf_verbose;
	const char *func = jsf->jsf_func;
	const char *prop = jsf->jsf_prop;
	uintptr_t nlines = jsf->jsf_nlines;

	uintptr_t funcp, funcinfop, tokpos, endpos, scriptp, lendsp, ptrp;
	uintptr_t ii, nargs;
	const char *typename;
	char *bufp;
	size_t len;
	uint8_t type;
	char buf[256];
	int lineno;

	/*
	 * Check for non-JavaScript frames first.
	 */
	if (func == NULL && do_jsframe_special(fptr, raddr, jsf) == 0)
		return (DCMD_OK);

	/*
	 * At this point we assume we're looking at a JavaScript frame.  As with
	 * native frames, fish the address out of the parent frame.
	 */
	if (mdb_vread(&funcp, sizeof (funcp),
	    fptr + V8_OFF_FP_FUNCTION) == -1) {
		v8_warn("failed to read stack at %p",
		    fptr + V8_OFF_FP_FUNCTION);
		return (DCMD_ERR);
	}

	/*
	 * Check if this thing is really a JSFunction at all. For some frames,
	 * it's a Code object, presumably indicating some internal frame.
	 */
	if (read_typebyte(&type, funcp) != 0 ||
	    (typename = enum_lookup_str(v8_types, type, NULL)) == NULL) {
		if (func != NULL || prop != NULL)
			return (DCMD_OK);

		if (showall) {
			jsframe_print_skipped(jsf);
			mdb_printf("%p %a\n", fptr, raddr);
		} else {
			jsframe_skip(jsf);
		}
		return (DCMD_OK);
	}

	if (strcmp("Code", typename) == 0) {
		if (func != NULL || prop != NULL)
			return (DCMD_OK);

		if (showall) {
			jsframe_print_skipped(jsf);
			mdb_printf("%p %a internal (Code: %p)\n",
			    fptr, raddr, funcp);
		} else {
			jsframe_skip(jsf);
		}
		return (DCMD_OK);
	}

	if (strcmp("JSFunction", typename) != 0) {
		if (func != NULL || prop != NULL)
			return (DCMD_OK);

		if (showall) {
			jsframe_print_skipped(jsf);
			mdb_printf("%p %a unknown (%s: %p)",
			    fptr, raddr, typename, funcp);
		} else {
			jsframe_skip(jsf);
		}
		return (DCMD_OK);
	}

	if (read_heap_ptr(&funcinfop, funcp, V8_OFF_JSFUNCTION_SHARED) != 0)
		return (DCMD_ERR);

	bufp = buf;
	len = sizeof (buf);
	if (jsfunc_name(funcinfop, &bufp, &len) != 0)
		return (DCMD_ERR);

	if (func != NULL && strcmp(buf, func) != 0)
		return (DCMD_OK);

	if (prop == NULL) {
		jsframe_print_skipped(jsf);
		if (showall)
			mdb_printf("%p %a ", fptr, raddr);
		else
			mdb_printf("js:     ");
		mdb_printf("%s", buf);
		if (showall)
			mdb_printf(" (JSFunction: %p)\n", funcp);
		else
			mdb_printf("\n");
	}

	if (!verbose && prop == NULL)
		return (DCMD_OK);

	if (verbose)
		jsframe_print_skipped(jsf);

	/*
	 * Although the token position is technically an SMI, we're going to
	 * byte-compare it to other SMI values so we don't want decode it here.
	 */
	if (read_heap_maybesmi(&tokpos, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_FUNCTION_TOKEN_POSITION) != 0)
		return (DCMD_ERR);
	tokpos = V8_VALUE_SMI(tokpos);

	if (read_heap_ptr(&scriptp, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_SCRIPT) != 0)
		return (DCMD_ERR);

	if (read_heap_ptr(&ptrp, scriptp, V8_OFF_SCRIPT_NAME) != 0)
		return (DCMD_ERR);

	bufp = buf;
	len = sizeof (buf);
	(void) jsstr_print(ptrp, JSSTR_NUDE, &bufp, &len);

	if (prop != NULL && strcmp(prop, "file") == 0) {
		mdb_printf("%s\n", buf);
		return (DCMD_OK);
	}

	if (prop == NULL) {
		(void) mdb_inc_indent(10);
		mdb_printf("file: %s\n", buf);
	}

	if (read_heap_ptr(&lendsp, scriptp, V8_OFF_SCRIPT_LINE_ENDS) != 0)
		return (DCMD_ERR);

	(void) jsfunc_lineno(lendsp, tokpos, buf, sizeof (buf), &lineno);

	if (prop != NULL && strcmp(prop, "posn") == 0) {
		mdb_printf("%s\n", buf);
		return (DCMD_OK);
	}

	if (prop == NULL)
		mdb_printf("posn: %s\n", buf);

	if (read_heap_maybesmi(&nargs, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_LENGTH) == 0) {
		uintptr_t argptr;
		char arg[10];

		if (mdb_vread(&argptr, sizeof (argptr),
		    fptr + V8_OFF_FP_ARGS + nargs * sizeof (uintptr_t)) != -1 &&
		    argptr != NULL) {
			(void) snprintf(arg, sizeof (arg), "this");
			if (prop != NULL && strcmp(arg, prop) == 0) {
				mdb_printf("%p\n", argptr);
				return (DCMD_OK);
			}

			if (prop == NULL) {
				bufp = buf;
				len = sizeof (buf);
				(void) obj_jstype(argptr, &bufp, &len, NULL);

				mdb_printf("this: %p (%s)\n", argptr, buf);
			}
		}

		for (ii = 0; ii < nargs; ii++) {
			if (mdb_vread(&argptr, sizeof (argptr),
			    fptr + V8_OFF_FP_ARGS + (nargs - ii - 1) *
			    sizeof (uintptr_t)) == -1)
				continue;

			(void) snprintf(arg, sizeof (arg), "arg%" PRIuPTR,
			    ii + 1);

			if (prop != NULL) {
				if (strcmp(arg, prop) != 0)
					continue;

				mdb_printf("%p\n", argptr);
				return (DCMD_OK);
			}

			bufp = buf;
			len = sizeof (buf);
			(void) obj_jstype(argptr, &bufp, &len, NULL);

			mdb_printf("arg%d: %p (%s)\n", (ii + 1), argptr, buf);
		}
	}


	if (prop != NULL) {
		mdb_warn("unknown frame property '%s'\n", prop);
		return (DCMD_ERR);
	}

	if (nlines != 0 && read_heap_maybesmi(&endpos, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_END_POSITION) == 0) {
		jsfunc_lines(scriptp,
		    V8_SMI_VALUE(tokpos), endpos, nlines, "%5d ");
		mdb_printf("\n");
	}

	(void) mdb_dec_indent(10);

	return (DCMD_OK);
}

typedef struct findjsobjects_prop {
	struct findjsobjects_prop *fjsp_next;
	char fjsp_desc[1];
} findjsobjects_prop_t;

typedef struct findjsobjects_instance {
	uintptr_t fjsi_addr;
	struct findjsobjects_instance *fjsi_next;
} findjsobjects_instance_t;

typedef struct findjsobjects_obj {
	findjsobjects_prop_t *fjso_props;
	findjsobjects_prop_t *fjso_last;
	jspropinfo_t fjso_propinfo;
	size_t fjso_nprops;
	findjsobjects_instance_t fjso_instances;
	int fjso_ninstances;
	avl_node_t fjso_node;
	struct findjsobjects_obj *fjso_next;
	boolean_t fjso_malformed;
	char fjso_constructor[80];
} findjsobjects_obj_t;

typedef struct findjsobjects_func {
	findjsobjects_instance_t fjsf_instances;
	int fjsf_ninstances;
	avl_node_t fjsf_node;
	struct findjsobjects_func *fjsf_next;
	uintptr_t fjsf_shared;
	char fjsf_funcname[40];
	char fjsf_scriptname[80];
	char fjsf_location[20];
} findjsobjects_func_t;

typedef struct findjsobjects_stats {
	int fjss_heapobjs;
	int fjss_cached;
	int fjss_typereads;
	int fjss_jsobjs;
	int fjss_objects;
	int fjss_arrays;
	int fjss_uniques;
	int fjss_funcs;
	int fjss_funcs_skipped;
	int fjss_funcs_unique;
} findjsobjects_stats_t;

typedef struct findjsobjects_reference {
	uintptr_t fjsrf_addr;
	char *fjsrf_desc;
	size_t fjsrf_index;
	struct findjsobjects_reference *fjsrf_next;
} findjsobjects_reference_t;

typedef struct findjsobjects_referent {
	avl_node_t fjsr_node;
	uintptr_t fjsr_addr;
	findjsobjects_reference_t *fjsr_head;
	findjsobjects_reference_t *fjsr_tail;
	struct findjsobjects_referent *fjsr_next;
} findjsobjects_referent_t;

typedef struct findjsobjects_state {
	uintptr_t fjs_addr;
	uintptr_t fjs_size;
	boolean_t fjs_verbose;
	boolean_t fjs_brk;
	boolean_t fjs_allobjs;
	boolean_t fjs_initialized;
	boolean_t fjs_marking;
	boolean_t fjs_referred;
	avl_tree_t fjs_tree;
	avl_tree_t fjs_referents;
	avl_tree_t fjs_funcinfo;
	findjsobjects_referent_t *fjs_head;
	findjsobjects_referent_t *fjs_tail;
	findjsobjects_obj_t *fjs_current;
	findjsobjects_obj_t *fjs_objects;
	findjsobjects_func_t *fjs_funcs;
	findjsobjects_stats_t fjs_stats;
} findjsobjects_state_t;

findjsobjects_obj_t *
findjsobjects_alloc(uintptr_t addr)
{
	findjsobjects_obj_t *obj;

	obj = mdb_zalloc(sizeof (findjsobjects_obj_t), UM_SLEEP);
	obj->fjso_instances.fjsi_addr = addr;
	obj->fjso_ninstances = 1;

	return (obj);
}

void
findjsobjects_free(findjsobjects_obj_t *obj)
{
	findjsobjects_prop_t *prop, *next;

	for (prop = obj->fjso_props; prop != NULL; prop = next) {
		next = prop->fjsp_next;
		mdb_free(prop, sizeof (findjsobjects_prop_t) +
		    strlen(prop->fjsp_desc));
	}

	mdb_free(obj, sizeof (findjsobjects_obj_t));
}

int
findjsobjects_cmp(findjsobjects_obj_t *lhs, findjsobjects_obj_t *rhs)
{
	findjsobjects_prop_t *lprop, *rprop;
	int rv;

	lprop = lhs->fjso_props;
	rprop = rhs->fjso_props;

	while (lprop != NULL && rprop != NULL) {
		if ((rv = strcmp(lprop->fjsp_desc, rprop->fjsp_desc)) != 0)
			return (rv > 0 ? 1 : -1);

		lprop = lprop->fjsp_next;
		rprop = rprop->fjsp_next;
	}

	if (lprop != NULL)
		return (1);

	if (rprop != NULL)
		return (-1);

	if (lhs->fjso_nprops > rhs->fjso_nprops)
		return (1);

	if (lhs->fjso_nprops < rhs->fjso_nprops)
		return (-1);

	rv = strcmp(lhs->fjso_constructor, rhs->fjso_constructor);

	return (rv < 0 ? -1 : rv > 0 ? 1 : 0);
}

int
findjsobjects_cmp_funcinfo(findjsobjects_func_t *lhs,
    findjsobjects_func_t *rhs)
{
	int diff = lhs->fjsf_shared - rhs->fjsf_shared;
	return (diff < 0 ? -1 : diff > 0 ? 1 : 0);
}

int
findjsobjects_cmp_referents(findjsobjects_referent_t *lhs,
    findjsobjects_referent_t *rhs)
{
	if (lhs->fjsr_addr < rhs->fjsr_addr)
		return (-1);

	if (lhs->fjsr_addr > rhs->fjsr_addr)
		return (1);

	return (0);
}

int
findjsobjects_cmp_ninstances(const void *l, const void *r)
{
	findjsobjects_obj_t *lhs = *((findjsobjects_obj_t **)l);
	findjsobjects_obj_t *rhs = *((findjsobjects_obj_t **)r);
	size_t lprod = lhs->fjso_ninstances * lhs->fjso_nprops;
	size_t rprod = rhs->fjso_ninstances * rhs->fjso_nprops;

	if (lprod < rprod)
		return (-1);

	if (lprod > rprod)
		return (1);

	if (lhs->fjso_ninstances < rhs->fjso_ninstances)
		return (-1);

	if (lhs->fjso_ninstances > rhs->fjso_ninstances)
		return (1);

	if (lhs->fjso_nprops < rhs->fjso_nprops)
		return (-1);

	if (lhs->fjso_nprops > rhs->fjso_nprops)
		return (1);

	return (0);
}

/*ARGSUSED*/
int
findjsobjects_prop(const char *desc, uintptr_t val, void *arg)
{
	findjsobjects_state_t *fjs = arg;
	findjsobjects_obj_t *current = fjs->fjs_current;
	findjsobjects_prop_t *prop;

	if (desc == NULL)
		desc = "<unknown>";

	prop = mdb_zalloc(sizeof (findjsobjects_prop_t) +
	    strlen(desc), UM_SLEEP);

	strcpy(prop->fjsp_desc, desc);

	if (current->fjso_last != NULL) {
		current->fjso_last->fjsp_next = prop;
	} else {
		current->fjso_props = prop;
	}

	current->fjso_last = prop;
	current->fjso_nprops++;
	current->fjso_malformed =
	    val == NULL && current->fjso_nprops == 1 && desc[0] == '<';

	return (0);
}

static void
findjsobjects_constructor(findjsobjects_obj_t *obj)
{
	char *bufp = obj->fjso_constructor;
	size_t len = sizeof (obj->fjso_constructor);
	uintptr_t map, funcinfop;
	uintptr_t addr = obj->fjso_instances.fjsi_addr;
	uint8_t type;

	v8_silent++;

	if (read_heap_ptr(&map, addr, V8_OFF_HEAPOBJECT_MAP) != 0 ||
	    read_heap_ptr(&addr, map, V8_OFF_MAP_CONSTRUCTOR) != 0)
		goto out;

	if (read_typebyte(&type, addr) != 0)
		goto out;

	if (strcmp(enum_lookup_str(v8_types, type, ""), "JSFunction") != 0)
		goto out;

	if (read_heap_ptr(&funcinfop, addr, V8_OFF_JSFUNCTION_SHARED) != 0)
		goto out;

	if (jsfunc_name(funcinfop, &bufp, &len) != 0)
		goto out;
out:
	v8_silent--;
}

static void
findjsobjects_jsfunc(findjsobjects_state_t *fjs, uintptr_t addr)
{
	findjsobjects_func_t *func, *ofunc;
	findjsobjects_instance_t *inst;
	uintptr_t funcinfo, script, name;
	avl_index_t where;
	int err;
	char *bufp;
	size_t len;

	/*
	 * This may be somewhat expensive to do for all JSFunctions, but in most
	 * core files, there aren't that many.  We could defer some of this work
	 * until the user tries to print the function ::jsfunctions, but this
	 * step is useful to do early to filter out garbage data.
	 */

	v8_silent++;
	if (read_heap_ptr(&funcinfo, addr, V8_OFF_JSFUNCTION_SHARED) != 0 ||
	    read_heap_ptr(&script, funcinfo,
	    V8_OFF_SHAREDFUNCTIONINFO_SCRIPT) != 0 ||
	    read_heap_ptr(&name, script, V8_OFF_SCRIPT_NAME) != 0) {
		fjs->fjs_stats.fjss_funcs_skipped++;
		v8_silent--;
		return;
	}

	func = mdb_zalloc(sizeof (findjsobjects_func_t), UM_SLEEP);
	func->fjsf_ninstances = 1;
	func->fjsf_instances.fjsi_addr = addr;
	func->fjsf_shared = funcinfo;

	bufp = func->fjsf_funcname;
	len = sizeof (func->fjsf_funcname);
	err = jsfunc_name(funcinfo, &bufp, &len);

	bufp = func->fjsf_scriptname;
	len = sizeof (func->fjsf_scriptname);
	err |= jsstr_print(name, JSSTR_NUDE, &bufp, &len);

	v8_silent--;
	if (err != 0) {
		fjs->fjs_stats.fjss_funcs_skipped++;
		mdb_free(func, sizeof (findjsobjects_func_t));
		return;
	}

	fjs->fjs_stats.fjss_funcs++;
	ofunc = avl_find(&fjs->fjs_funcinfo, func, &where);
	if (ofunc == NULL) {
		avl_add(&fjs->fjs_funcinfo, func);
		func->fjsf_next = fjs->fjs_funcs;
		fjs->fjs_funcs = func;
		fjs->fjs_stats.fjss_funcs_unique++;
	} else {
		inst = mdb_alloc(sizeof (findjsobjects_instance_t), UM_SLEEP);
		inst->fjsi_addr = addr;
		inst->fjsi_next = ofunc->fjsf_instances.fjsi_next;
		ofunc->fjsf_instances.fjsi_next = inst;
		ofunc->fjsf_ninstances++;
		mdb_free(func, sizeof (findjsobjects_func_t));
	}
}

int
findjsobjects_range(findjsobjects_state_t *fjs, uintptr_t addr, uintptr_t size)
{
	uintptr_t limit;
	findjsobjects_stats_t *stats = &fjs->fjs_stats;
	uint8_t type;
	int jsobject = V8_TYPE_JSOBJECT, jsarray = V8_TYPE_JSARRAY;
	int jsfunction = V8_TYPE_JSFUNCTION;
	caddr_t range = mdb_alloc(size, UM_SLEEP);
	uintptr_t base = addr, mapaddr;

	if (mdb_vread(range, size, addr) == -1)
		return (0);

	for (limit = addr + size; addr < limit; addr++) {
		findjsobjects_instance_t *inst;
		findjsobjects_obj_t *obj;
		avl_index_t where;

		if (V8_IS_SMI(addr))
			continue;

		if (!V8_IS_HEAPOBJECT(addr))
			continue;

		stats->fjss_heapobjs++;

		mapaddr = *((uintptr_t *)((uintptr_t)range +
		    (addr - base) + V8_OFF_HEAPOBJECT_MAP));

		if (!V8_IS_HEAPOBJECT(mapaddr))
			continue;

		mapaddr += V8_OFF_MAP_INSTANCE_ATTRIBUTES;
		stats->fjss_typereads++;

		if (mapaddr >= base && mapaddr < base + size) {
			stats->fjss_cached++;

			type = *((uint8_t *)((uintptr_t)range +
			    (mapaddr - base)));
		} else {
			if (mdb_vread(&type, sizeof (uint8_t), mapaddr) == -1)
				continue;
		}

		if (type == jsfunction) {
			findjsobjects_jsfunc(fjs, addr);
			continue;
		}

		if (type != jsobject && type != jsarray)
			continue;

		stats->fjss_jsobjs++;

		fjs->fjs_current = findjsobjects_alloc(addr);

		if (type == jsobject) {
			if (jsobj_properties(addr,
			    findjsobjects_prop, fjs,
			    &fjs->fjs_current->fjso_propinfo) != 0) {
				findjsobjects_free(fjs->fjs_current);
				fjs->fjs_current = NULL;
				continue;
			}

			findjsobjects_constructor(fjs->fjs_current);
			stats->fjss_objects++;
		} else {
			uintptr_t ptr;
			size_t *nprops = &fjs->fjs_current->fjso_nprops;
			ssize_t len = V8_OFF_JSARRAY_LENGTH;
			ssize_t elems = V8_OFF_JSOBJECT_ELEMENTS;
			ssize_t flen = V8_OFF_FIXEDARRAY_LENGTH;
			uintptr_t nelems;
			uint8_t t;

			if (read_heap_smi(nprops, addr, len) != 0 ||
			    read_heap_ptr(&ptr, addr, elems) != 0 ||
			    !V8_IS_HEAPOBJECT(ptr) ||
			    read_typebyte(&t, ptr) != 0 ||
			    t != V8_TYPE_FIXEDARRAY ||
			    read_heap_smi(&nelems, ptr, flen) != 0 ||
			    nelems < *nprops) {
				findjsobjects_free(fjs->fjs_current);
				fjs->fjs_current = NULL;
				continue;
			}

			strcpy(fjs->fjs_current->fjso_constructor, "Array");
			stats->fjss_arrays++;
		}

		/*
		 * Now determine if we already have an object matching our
		 * properties.  If we don't, we'll add our new object; if we
		 * do we'll merely enqueue our instance.
		 */
		obj = avl_find(&fjs->fjs_tree, fjs->fjs_current, &where);

		if (obj == NULL) {
			avl_add(&fjs->fjs_tree, fjs->fjs_current);
			fjs->fjs_current->fjso_next = fjs->fjs_objects;
			fjs->fjs_objects = fjs->fjs_current;
			fjs->fjs_current = NULL;
			stats->fjss_uniques++;
			continue;
		}

		findjsobjects_free(fjs->fjs_current);
		fjs->fjs_current = NULL;

		inst = mdb_alloc(sizeof (findjsobjects_instance_t), UM_SLEEP);
		inst->fjsi_addr = addr;
		inst->fjsi_next = obj->fjso_instances.fjsi_next;
		obj->fjso_instances.fjsi_next = inst;
		obj->fjso_ninstances++;
	}

	mdb_free(range, size);

	return (0);
}

static int
findjsobjects_mapping(findjsobjects_state_t *fjs, const prmap_t *pmp,
    const char *name)
{
	if (name != NULL && !(fjs->fjs_brk && (pmp->pr_mflags & MA_BREAK)))
		return (0);

	if (fjs->fjs_addr != NULL && (fjs->fjs_addr < pmp->pr_vaddr ||
	    fjs->fjs_addr >= pmp->pr_vaddr + pmp->pr_size))
		return (0);

	return (findjsobjects_range(fjs, pmp->pr_vaddr, pmp->pr_size));
}

static void
findjsobjects_references_add(findjsobjects_state_t *fjs, uintptr_t val,
    const char *desc, size_t index)
{
	findjsobjects_referent_t search, *referent;
	findjsobjects_reference_t *reference;

	search.fjsr_addr = val;

	if ((referent = avl_find(&fjs->fjs_referents, &search, NULL)) == NULL)
		return;

	reference = mdb_zalloc(sizeof (*reference), UM_SLEEP | UM_GC);
	reference->fjsrf_addr = fjs->fjs_addr;

	if (desc != NULL) {
		reference->fjsrf_desc =
		    mdb_alloc(strlen(desc) + 1, UM_SLEEP | UM_GC);
		(void) strcpy(reference->fjsrf_desc, desc);
	} else {
		reference->fjsrf_index = index;
	}

	if (referent->fjsr_head == NULL) {
		referent->fjsr_head = reference;
	} else {
		referent->fjsr_tail->fjsrf_next = reference;
	}

	referent->fjsr_tail = reference;
}

static int
findjsobjects_references_prop(const char *desc, uintptr_t val, void *arg)
{
	findjsobjects_references_add(arg, val, desc, -1);

	return (0);
}

static void
findjsobjects_references_array(findjsobjects_state_t *fjs,
    findjsobjects_obj_t *obj)
{
	findjsobjects_instance_t *inst = &obj->fjso_instances;
	uintptr_t *elts;
	size_t i, len;

	for (; inst != NULL; inst = inst->fjsi_next) {
		uintptr_t addr = inst->fjsi_addr, ptr;

		if (read_heap_ptr(&ptr, addr, V8_OFF_JSOBJECT_ELEMENTS) != 0 ||
		    read_heap_array(ptr, &elts, &len, UM_SLEEP) != 0)
			continue;

		fjs->fjs_addr = addr;

		for (i = 0; i < len; i++)
			findjsobjects_references_add(fjs, elts[i], NULL, i);

		mdb_free(elts, len * sizeof (uintptr_t));
	}
}

static void
findjsobjects_referent(findjsobjects_state_t *fjs, uintptr_t addr)
{
	findjsobjects_referent_t search, *referent;

	search.fjsr_addr = addr;

	if (avl_find(&fjs->fjs_referents, &search, NULL) != NULL) {
		assert(fjs->fjs_marking);
		mdb_warn("%p is already marked; ignoring\n", addr);
		return;
	}

	referent = mdb_zalloc(sizeof (findjsobjects_referent_t), UM_SLEEP);
	referent->fjsr_addr = addr;

	avl_add(&fjs->fjs_referents, referent);

	if (fjs->fjs_tail != NULL) {
		fjs->fjs_tail->fjsr_next = referent;
	} else {
		fjs->fjs_head = referent;
	}

	fjs->fjs_tail = referent;

	if (fjs->fjs_marking)
		mdb_printf("findjsobjects: marked %p\n", addr);
}

static void
findjsobjects_references(findjsobjects_state_t *fjs)
{
	findjsobjects_reference_t *reference;
	findjsobjects_referent_t *referent;
	avl_tree_t *referents = &fjs->fjs_referents;
	findjsobjects_obj_t *obj;
	void *cookie = NULL;
	uintptr_t addr;

	fjs->fjs_referred = B_FALSE;

	v8_silent++;

	/*
	 * First traverse over all objects and arrays, looking for references
	 * to our designated referent(s).
	 */
	for (obj = fjs->fjs_objects; obj != NULL; obj = obj->fjso_next) {
		findjsobjects_instance_t *head = &obj->fjso_instances, *inst;

		if (obj->fjso_nprops != 0 && obj->fjso_props == NULL) {
			findjsobjects_references_array(fjs, obj);
			continue;
		}

		for (inst = head; inst != NULL; inst = inst->fjsi_next) {
			fjs->fjs_addr = inst->fjsi_addr;

			(void) jsobj_properties(inst->fjsi_addr,
			    findjsobjects_references_prop, fjs, NULL);
		}
	}

	v8_silent--;
	fjs->fjs_addr = NULL;

	/*
	 * Now go over our referent(s), reporting any references that we have
	 * accumulated.
	 */
	for (referent = fjs->fjs_head; referent != NULL;
	    referent = referent->fjsr_next) {
		addr = referent->fjsr_addr;

		if ((reference = referent->fjsr_head) == NULL) {
			mdb_printf("%p is not referred to by a "
			    "known object.\n", addr);
			continue;
		}

		for (; reference != NULL; reference = reference->fjsrf_next) {
			mdb_printf("%p referred to by %p",
			    addr, reference->fjsrf_addr);

			if (reference->fjsrf_desc == NULL) {
				mdb_printf("[%d]\n", reference->fjsrf_index);
			} else {
				mdb_printf(".%s\n", reference->fjsrf_desc);
			}
		}
	}

	/*
	 * Finally, destroy our referent nodes.
	 */
	while ((referent = avl_destroy_nodes(referents, &cookie)) != NULL)
		mdb_free(referent, sizeof (findjsobjects_referent_t));

	fjs->fjs_head = NULL;
	fjs->fjs_tail = NULL;
}

static findjsobjects_instance_t *
findjsobjects_instance(findjsobjects_state_t *fjs, uintptr_t addr,
    findjsobjects_instance_t **headp)
{
	findjsobjects_obj_t *obj;

	for (obj = fjs->fjs_objects; obj != NULL; obj = obj->fjso_next) {
		findjsobjects_instance_t *head = &obj->fjso_instances, *inst;

		for (inst = head; inst != NULL; inst = inst->fjsi_next) {
			if (inst->fjsi_addr == addr) {
				*headp = head;
				return (inst);
			}
		}
	}

	return (NULL);
}

/*ARGSUSED*/
static void
findjsobjects_match_all(findjsobjects_obj_t *obj, const char *ignored)
{
	mdb_printf("%p\n", obj->fjso_instances.fjsi_addr);
}

static void
findjsobjects_match_propname(findjsobjects_obj_t *obj, const char *propname)
{
	findjsobjects_prop_t *prop;

	for (prop = obj->fjso_props; prop != NULL; prop = prop->fjsp_next) {
		if (strcmp(prop->fjsp_desc, propname) == 0) {
			mdb_printf("%p\n", obj->fjso_instances.fjsi_addr);
			return;
		}
	}
}

static void
findjsobjects_match_constructor(findjsobjects_obj_t *obj,
    const char *constructor)
{
	if (strcmp(constructor, obj->fjso_constructor) == 0)
		mdb_printf("%p\n", obj->fjso_instances.fjsi_addr);
}

static void
findjsobjects_match_kind(findjsobjects_obj_t *obj, const char *propkind)
{
	jspropinfo_t p = obj->fjso_propinfo;

	if (((p & JPI_NUMERIC) != 0 && strstr(propkind, "numeric") != NULL) ||
	    ((p & JPI_DICT) != 0 && strstr(propkind, "dict") != NULL) ||
	    ((p & JPI_INOBJECT) != 0 && strstr(propkind, "inobject") != NULL) ||
	    ((p & JPI_PROPS) != 0 && strstr(propkind, "props") != NULL) ||
	    ((p & JPI_HASTRANSITIONS) != 0 &&
	    strstr(propkind, "transitions") != NULL) ||
	    ((p & JPI_HASCONTENT) != 0 &&
	    strstr(propkind, "content") != NULL) ||
	    ((p & JPI_SKIPPED) != 0 && strstr(propkind, "skipped") != NULL) ||
	    ((p & JPI_BADLAYOUT) != 0 &&
	    strstr(propkind, "badlayout") != NULL)) {
		mdb_printf("%p\n", obj->fjso_instances.fjsi_addr);
	}
}

static int
findjsobjects_match(findjsobjects_state_t *fjs, uintptr_t addr,
    uint_t flags, void (*func)(findjsobjects_obj_t *, const char *),
    const char *match)
{
	findjsobjects_obj_t *obj;

	if (!(flags & DCMD_ADDRSPEC)) {
		for (obj = fjs->fjs_objects; obj != NULL;
		    obj = obj->fjso_next) {
			if (obj->fjso_malformed && !fjs->fjs_allobjs)
				continue;

			func(obj, match);
		}

		return (DCMD_OK);
	}

	/*
	 * First, look for the specified address among the representative
	 * objects.
	 */
	for (obj = fjs->fjs_objects; obj != NULL; obj = obj->fjso_next) {
		if (obj->fjso_instances.fjsi_addr == addr) {
			func(obj, match);
			return (DCMD_OK);
		}
	}

	/*
	 * We didn't find it among the representative objects; iterate over
	 * all objects.
	 */
	for (obj = fjs->fjs_objects; obj != NULL; obj = obj->fjso_next) {
		findjsobjects_instance_t *head = &obj->fjso_instances, *inst;

		for (inst = head; inst != NULL; inst = inst->fjsi_next) {
			if (inst->fjsi_addr == addr) {
				func(obj, match);
				return (DCMD_OK);
			}
		}
	}

	mdb_warn("%p does not correspond to a known object\n", addr);
	return (DCMD_ERR);
}

static void
findjsobjects_print(findjsobjects_obj_t *obj)
{
	int col = 19 + (sizeof (uintptr_t) * 2) + strlen("..."), len;
	uintptr_t addr = obj->fjso_instances.fjsi_addr;
	findjsobjects_prop_t *prop;

	mdb_printf("%?p %8d %8d ",
	    addr, obj->fjso_ninstances, obj->fjso_nprops);

	if (obj->fjso_constructor[0] != '\0') {
		mdb_printf("%s%s", obj->fjso_constructor,
		    obj->fjso_props != NULL ? ": " : "");
		col += strlen(obj->fjso_constructor) + 2;
	}

	for (prop = obj->fjso_props; prop != NULL; prop = prop->fjsp_next) {
		if (col + (len = strlen(prop->fjsp_desc) + 2) < 80) {
			mdb_printf("%s%s", prop->fjsp_desc,
			    prop->fjsp_next != NULL ? ", " : "");
			col += len;
		} else {
			mdb_printf("...");
			break;
		}
	}

	mdb_printf("\n", col);
}

static void
dcmd_findjsobjects_help(void)
{
	mdb_printf("%s\n\n",
"Finds all JavaScript objects in the V8 heap via brute force iteration over\n"
"all mapped anonymous memory.  (This can take up to several minutes on large\n"
"dumps.)  The output consists of representative objects, the number of\n"
"instances of that object and the number of properties on the object --\n"
"followed by the constructor and first few properties of the objects.  Once\n"
"run, subsequent calls to ::findjsobjects use cached data.  If provided an\n"
"address (and in the absence of -r, described below), ::findjsobjects treats\n"
"the address as that of a representative object, and lists all instances of\n"
"that object (that is, all objects that have a matching property signature).");

	mdb_dec_indent(2);
	mdb_printf("%<b>OPTIONS%</b>\n");
	mdb_inc_indent(2);

	mdb_printf("%s\n",
"  -b       Include the heap denoted by the brk(2) (normally excluded)\n"
"  -c cons  Display representative objects with the specified constructor\n"
"  -p prop  Display representative objects that have the specified property\n"
"  -l       List all objects that match the representative object\n"
"  -m       Mark specified object for later reference determination via -r\n"
"  -r       Find references to the specified and/or marked object(s)\n"
"  -v       Provide verbose statistics\n");
}

static findjsobjects_state_t findjsobjects_state;

static int
findjsobjects_run(findjsobjects_state_t *fjs)
{
	struct ps_prochandle *Pr;
	findjsobjects_obj_t *obj;
	findjsobjects_stats_t *stats = &fjs->fjs_stats;

	if (!fjs->fjs_initialized) {
		avl_create(&fjs->fjs_tree,
		    (int(*)(const void *, const void *))findjsobjects_cmp,
		    sizeof (findjsobjects_obj_t),
		    offsetof(findjsobjects_obj_t, fjso_node));

		avl_create(&fjs->fjs_referents,
		    (int(*)(const void *, const void *))
		    findjsobjects_cmp_referents,
		    sizeof (findjsobjects_referent_t),
		    offsetof(findjsobjects_referent_t, fjsr_node));

		avl_create(&fjs->fjs_funcinfo,
		    (int(*)(const void *, const void*))
		    findjsobjects_cmp_funcinfo,
		    sizeof (findjsobjects_func_t),
		    offsetof(findjsobjects_func_t, fjsf_node));

		fjs->fjs_initialized = B_TRUE;
	}

	if (avl_is_empty(&fjs->fjs_tree)) {
		findjsobjects_obj_t **sorted;
		int nobjs, i;
		hrtime_t start = gethrtime();

		if (mdb_get_xdata("pshandle", &Pr, sizeof (Pr)) == -1) {
			mdb_warn("couldn't read pshandle xdata");
			return (-1);
		}

		v8_silent++;

		if (Pmapping_iter(Pr,
		    (proc_map_f *)findjsobjects_mapping, fjs) != 0) {
			v8_silent--;
			return (-1);
		}

		if ((nobjs = avl_numnodes(&fjs->fjs_tree)) != 0) {
			/*
			 * We have the objects -- now sort them.
			 */
			sorted = mdb_alloc(nobjs * sizeof (void *),
			    UM_SLEEP | UM_GC);

			for (obj = fjs->fjs_objects, i = 0; obj != NULL;
			    obj = obj->fjso_next, i++) {
				sorted[i] = obj;
			}

			qsort(sorted, avl_numnodes(&fjs->fjs_tree),
			    sizeof (void *), findjsobjects_cmp_ninstances);

			for (i = 1, fjs->fjs_objects = sorted[0];
			    i < nobjs; i++)
				sorted[i - 1]->fjso_next = sorted[i];

			sorted[nobjs - 1]->fjso_next = NULL;
		}

		v8_silent--;

		if (fjs->fjs_verbose) {
			const char *f = "findjsobjects: %30s => %d\n";
			int elapsed = (int)((gethrtime() - start) / NANOSEC);

			mdb_printf(f, "elapsed time (seconds)", elapsed);
			mdb_printf(f, "heap objects", stats->fjss_heapobjs);
			mdb_printf(f, "type reads", stats->fjss_typereads);
			mdb_printf(f, "cached reads", stats->fjss_cached);
			mdb_printf(f, "JavaScript objects", stats->fjss_jsobjs);
			mdb_printf(f, "processed objects", stats->fjss_objects);
			mdb_printf(f, "processed arrays", stats->fjss_arrays);
			mdb_printf(f, "unique objects", stats->fjss_uniques);
			mdb_printf(f, "functions found", stats->fjss_funcs);
			mdb_printf(f, "unique functions",
			    stats->fjss_funcs_unique);
			mdb_printf(f, "functions skipped",
			    stats->fjss_funcs_skipped);
		}
	}

	return (0);
}

static int
dcmd_findjsobjects(uintptr_t addr,
    uint_t flags, int argc, const mdb_arg_t *argv)
{
	findjsobjects_state_t *fjs = &findjsobjects_state;
	findjsobjects_obj_t *obj;
	boolean_t references = B_FALSE, listlike = B_FALSE;
	const char *propname = NULL;
	const char *constructor = NULL;
	const char *propkind = NULL;

	fjs->fjs_verbose = B_FALSE;
	fjs->fjs_brk = B_FALSE;
	fjs->fjs_marking = B_FALSE;
	fjs->fjs_allobjs = B_FALSE;

	if (mdb_getopts(argc, argv,
	    'a', MDB_OPT_SETBITS, B_TRUE, &fjs->fjs_allobjs,
	    'b', MDB_OPT_SETBITS, B_TRUE, &fjs->fjs_brk,
	    'c', MDB_OPT_STR, &constructor,
	    'k', MDB_OPT_STR, &propkind,
	    'l', MDB_OPT_SETBITS, B_TRUE, &listlike,
	    'm', MDB_OPT_SETBITS, B_TRUE, &fjs->fjs_marking,
	    'p', MDB_OPT_STR, &propname,
	    'r', MDB_OPT_SETBITS, B_TRUE, &references,
	    'v', MDB_OPT_SETBITS, B_TRUE, &fjs->fjs_verbose,
	    NULL) != argc)
		return (DCMD_USAGE);

	if (findjsobjects_run(fjs) != 0)
		return (DCMD_ERR);

	if (listlike && !(flags & DCMD_ADDRSPEC)) {
		if (propname != NULL || constructor != NULL ||
		    propkind != NULL) {
			char opt = propname != NULL ? 'p' :
			    propkind != NULL ? 'k' :'c';

			mdb_warn("cannot specify -l with -%c; instead, pipe "
			    "output of ::findjsobjects -%c to "
			    "::findjsobjects -l\n", opt, opt);
			return (DCMD_ERR);
		}

		return (findjsobjects_match(fjs, addr, flags,
		    findjsobjects_match_all, NULL));
	}

	if (propname != NULL) {
		if (constructor != NULL || propkind != NULL) {
			mdb_warn("cannot specify both a property name "
			    "and a %s\n", constructor != NULL ?
			    "constructor" : "property kind");
			return (DCMD_ERR);
		}

		return (findjsobjects_match(fjs, addr, flags,
		    findjsobjects_match_propname, propname));
	}

	if (constructor != NULL) {
		if (propkind != NULL) {
			mdb_warn("cannot specify both a constructor name "
			    "and a property kind\n");
			return (DCMD_ERR);
		}

		return (findjsobjects_match(fjs, addr, flags,
		    findjsobjects_match_constructor, constructor));
	}

	if (propkind != NULL) {
		return (findjsobjects_match(fjs, addr, flags,
		    findjsobjects_match_kind, propkind));
	}

	if (references && !(flags & DCMD_ADDRSPEC) &&
	    avl_is_empty(&fjs->fjs_referents)) {
		mdb_warn("must specify or mark an object to find references\n");
		return (DCMD_ERR);
	}

	if (fjs->fjs_marking && !(flags & DCMD_ADDRSPEC)) {
		mdb_warn("must specify an object to mark\n");
		return (DCMD_ERR);
	}

	if (references && fjs->fjs_marking) {
		mdb_warn("can't both mark an object and find its references\n");
		return (DCMD_ERR);
	}

	if (flags & DCMD_ADDRSPEC) {
		findjsobjects_instance_t *inst, *head;

		/*
		 * If we've been passed an address, it's to either list like
		 * objects (-l), mark an object (-m) or find references to the
		 * specified/marked objects (-r).  (Note that the absence of
		 * any of these options implies -l.)
		 */
		inst = findjsobjects_instance(fjs, addr, &head);

		if (inst == NULL) {
			mdb_warn("%p is not a valid object\n", addr);
			return (DCMD_ERR);
		}

		if (!references && !fjs->fjs_marking) {
			for (inst = head; inst != NULL; inst = inst->fjsi_next)
				mdb_printf("%p\n", inst->fjsi_addr);

			return (DCMD_OK);
		}

		if (!listlike) {
			findjsobjects_referent(fjs, inst->fjsi_addr);
		} else {
			for (inst = head; inst != NULL; inst = inst->fjsi_next)
				findjsobjects_referent(fjs, inst->fjsi_addr);
		}
	}

	if (references)
		findjsobjects_references(fjs);

	if (references || fjs->fjs_marking)
		return (DCMD_OK);

	mdb_printf("%?s %8s %8s %s\n", "OBJECT",
	    "#OBJECTS", "#PROPS", "CONSTRUCTOR: PROPS");

	for (obj = fjs->fjs_objects; obj != NULL; obj = obj->fjso_next) {
		if (obj->fjso_malformed && !fjs->fjs_allobjs)
			continue;

		findjsobjects_print(obj);
	}

	return (DCMD_OK);
}

/*
 * Given a Node Buffer object, print out details about it.  With "-a", just
 * print the address.
 */
/* ARGSUSED */
static int
dcmd_nodebuffer(uintptr_t addr, uint_t flags, int argc,
    const mdb_arg_t *argv)
{
	boolean_t opt_f = B_FALSE;
	char buf[80];
	char *bufp = buf;
	size_t len = sizeof (buf);
	uintptr_t elts, rawbuf;

	/*
	 * The undocumented "-f" option allows users to override constructor
	 * checks.
	 */
	if (mdb_getopts(argc, argv,
	    'f', MDB_OPT_SETBITS, B_TRUE, &opt_f, NULL) != argc)
		return (DCMD_USAGE);

	if (!opt_f) {
		if (obj_jsconstructor(addr, &bufp, &len, B_FALSE) != 0)
			return (DCMD_ERR);

		if (strcmp(buf, "Buffer") != 0) {
			mdb_warn("%p does not appear to be a buffer\n", addr);
			return (DCMD_ERR);
		}
	}

	if (read_heap_ptr(&elts, addr, V8_OFF_JSOBJECT_ELEMENTS) != 0)
		return (DCMD_ERR);

	if (obj_v8internal(elts, 0, &rawbuf) != 0)
		return (DCMD_ERR);

	mdb_printf("%p\n", rawbuf);
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_jsconstructor(uintptr_t addr, uint_t flags, int argc,
    const mdb_arg_t *argv)
{
	boolean_t opt_v = B_FALSE;
	char buf[80];
	char *bufp;
	size_t len = sizeof (buf);

	if (mdb_getopts(argc, argv, 'v', MDB_OPT_SETBITS, B_TRUE, &opt_v,
	    NULL) != argc)
		return (DCMD_USAGE);

	bufp = buf;
	if (obj_jsconstructor(addr, &bufp, &len, opt_v))
		return (DCMD_ERR);

	mdb_printf("%s\n", buf);
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_jsframe(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintptr_t fptr, raddr;
	boolean_t opt_i = B_FALSE;
	jsframe_t jsf;
	int rv;

	bzero(&jsf, sizeof (jsf));
	jsf.jsf_nlines = 5;

	if (mdb_getopts(argc, argv,
	    'a', MDB_OPT_SETBITS, B_TRUE, &jsf.jsf_showall,
	    'v', MDB_OPT_SETBITS, B_TRUE, &jsf.jsf_verbose,
	    'i', MDB_OPT_SETBITS, B_TRUE, &opt_i,
	    'f', MDB_OPT_STR, &jsf.jsf_func,
	    'n', MDB_OPT_UINTPTR, &jsf.jsf_nlines,
	    'p', MDB_OPT_STR, &jsf.jsf_prop, NULL) != argc)
		return (DCMD_USAGE);

	/*
	 * As with $C, we assume we are given a *pointer* to the frame pointer
	 * for a frame, rather than the actual frame pointer for the frame of
	 * interest. This is needed to show the instruction pointer, which is
	 * actually stored with the next frame.  For debugging, this can be
	 * overridden with the "-i" option (for "immediate").
	 */
	if (opt_i) {
		rv = do_jsframe(addr, 0, &jsf);
		if (rv == 0)
			jsframe_print_skipped(&jsf);
		return (rv);
	}

	if (mdb_vread(&raddr, sizeof (raddr),
	    addr + sizeof (uintptr_t)) == -1) {
		mdb_warn("failed to read return address from %p",
		    addr + sizeof (uintptr_t));
		return (DCMD_ERR);
	}

	if (mdb_vread(&fptr, sizeof (fptr), addr) == -1) {
		mdb_warn("failed to read frame pointer from %p", addr);
		return (DCMD_ERR);
	}

	if (fptr == NULL)
		return (DCMD_OK);

	rv = do_jsframe(fptr, raddr, &jsf);
	if (rv == 0)
		jsframe_print_skipped(&jsf);
	return (rv);
}

static void
jsobj_print_propinfo(jspropinfo_t propinfo)
{
	if (propinfo == JPI_NONE)
		return;

	mdb_printf("property kind: ");
	if ((propinfo & JPI_NUMERIC) != 0)
		mdb_printf("numeric-named ");
	if ((propinfo & JPI_DICT) != 0)
		mdb_printf("dictionary ");
	if ((propinfo & JPI_INOBJECT) != 0)
		mdb_printf("in-object ");
	if ((propinfo & JPI_PROPS) != 0)
		mdb_printf("\"properties\" array ");
	mdb_printf("\n");

	if ((propinfo & (JPI_HASTRANSITIONS | JPI_HASCONTENT)) != 0) {
		mdb_printf("fallbacks: ");
		if ((propinfo & JPI_HASTRANSITIONS) != 0)
			mdb_printf("transitions ");
		if ((propinfo & JPI_HASCONTENT) != 0)
			mdb_printf("content ");
		mdb_printf("\n");
	}

	if ((propinfo & JPI_SKIPPED) != 0)
		mdb_printf(
		    "some properties skipped due to unexpected layout\n");
	if ((propinfo & JPI_BADLAYOUT) != 0)
		mdb_printf("object has unexpected layout\n");
}

/* ARGSUSED */
static int
dcmd_jsprint(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	char *buf, *bufp;
	size_t bufsz = 262144, len = bufsz;
	jsobj_print_t jsop;
	boolean_t opt_b = B_FALSE;
	boolean_t opt_v = B_FALSE;
	int rv, i;

	bzero(&jsop, sizeof (jsop));
	jsop.jsop_depth = 2;
	jsop.jsop_printaddr = B_FALSE;

	i = mdb_getopts(argc, argv,
	    'a', MDB_OPT_SETBITS, B_TRUE, &jsop.jsop_printaddr,
	    'b', MDB_OPT_SETBITS, B_TRUE, &opt_b,
	    'd', MDB_OPT_UINT64, &jsop.jsop_depth,
	    'v', MDB_OPT_SETBITS, B_TRUE, &opt_v, NULL);

	if (opt_b)
		jsop.jsop_baseaddr = addr;

	do {
		if (i != argc) {
			const mdb_arg_t *member = &argv[i++];

			if (member->a_type != MDB_TYPE_STRING)
				return (DCMD_USAGE);

			jsop.jsop_member = member->a_un.a_str;
		}

		for (;;) {
			if ((buf = bufp =
			    mdb_zalloc(bufsz, UM_NOSLEEP)) == NULL)
				return (DCMD_ERR);

			jsop.jsop_bufp = &bufp;
			jsop.jsop_lenp = &len;

			rv = jsobj_print(addr, &jsop);

			if (len > 0)
				break;

			mdb_free(buf, bufsz);
			bufsz <<= 1;
			len = bufsz;
		}

		if (jsop.jsop_member == NULL && rv != 0) {
			if (!jsop.jsop_descended)
				mdb_warn("%s\n", buf);

			return (DCMD_ERR);
		}

		if (jsop.jsop_member && !jsop.jsop_found) {
			if (jsop.jsop_baseaddr)
				(void) mdb_printf("%p: ", jsop.jsop_baseaddr);

			(void) mdb_printf("undefined%s",
			    i < argc ? " " : "");
		} else {
			(void) mdb_printf("%s%s", buf, i < argc &&
			    !isspace(buf[strlen(buf) - 1]) ? " " : "");
		}

		mdb_free(buf, bufsz);
		jsop.jsop_found = B_FALSE;
		jsop.jsop_baseaddr = NULL;
	} while (i < argc);

	mdb_printf("\n");

	if (opt_v)
		jsobj_print_propinfo(jsop.jsop_propinfo);

	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_jssource(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	const char *typename;
	uintptr_t nlines = 5;
	uintptr_t funcinfop, scriptp, funcnamep;
	uintptr_t tokpos, endpos;
	uint8_t type;
	char buf[256];
	char *bufp = buf;
	size_t len = sizeof (buf);

	if (mdb_getopts(argc, argv, 'n', MDB_OPT_UINTPTR, &nlines,
	    NULL) != argc)
		return (DCMD_USAGE);

	if (!V8_IS_HEAPOBJECT(addr) || read_typebyte(&type, addr) != 0) {
		mdb_warn("%p is not a heap object\n", addr);
		return (DCMD_ERR);
	}

	typename = enum_lookup_str(v8_types, type, "");
	if (strcmp(typename, "JSFunction") != 0) {
		mdb_warn("%p is not a JSFunction\n", addr);
		return (DCMD_ERR);
	}

	if (read_heap_ptr(&funcinfop, addr, V8_OFF_JSFUNCTION_SHARED) != 0 ||
	    read_heap_ptr(&scriptp, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_SCRIPT) != 0 ||
	    read_heap_ptr(&funcnamep, scriptp, V8_OFF_SCRIPT_NAME) != 0) {
		mdb_warn("%p: failed to find script for function\n", addr);
		return (DCMD_ERR);
	}

	if (read_heap_maybesmi(&tokpos, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_FUNCTION_TOKEN_POSITION) != 0 ||
	    read_heap_maybesmi(&endpos, funcinfop,
	    V8_OFF_SHAREDFUNCTIONINFO_END_POSITION) != 0) {
		mdb_warn("%p: failed to find function's boundaries\n", addr);
	}

	if (jsstr_print(funcnamep, JSSTR_NUDE, &bufp, &len) == 0)
		mdb_printf("file: %s\n", buf);

	if (tokpos != endpos)
		jsfunc_lines(scriptp, tokpos, endpos, nlines, "%5d ");
	mdb_printf("\n");
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_jsfunctions(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	findjsobjects_state_t *fjs = &findjsobjects_state;
	findjsobjects_func_t *func;
	uintptr_t funcinfo;
	boolean_t showrange = B_FALSE;
	const char *name = NULL, *filename = NULL;
	uintptr_t instr = 0;

	if (mdb_getopts(argc, argv,
	    'x', MDB_OPT_UINTPTR, &instr,
	    'X', MDB_OPT_SETBITS, B_TRUE, &showrange,
	    'n', MDB_OPT_STR, &name,
	    's', MDB_OPT_STR, &filename,
	    NULL) != argc)
		return (DCMD_USAGE);

	if (findjsobjects_run(fjs) != 0)
		return (DCMD_ERR);

	if (!showrange)
		mdb_printf("%?s %8s %-40s %s\n", "FUNC", "#FUNCS", "NAME",
		    "FROM");
	else
		mdb_printf("%?s %8s %?s %?s %-40s %s\n", "FUNC", "#FUNCS",
		    "START", "END", "NAME", "FROM");

	for (func = fjs->fjs_funcs; func != NULL; func = func->fjsf_next) {
		uintptr_t code, ilen;

		funcinfo = func->fjsf_shared;

		if (func->fjsf_location[0] == '\0') {
			uintptr_t tokpos, script, lends;
			ptrdiff_t tokposoff =
			    V8_OFF_SHAREDFUNCTIONINFO_FUNCTION_TOKEN_POSITION;

			/*
			 * We don't want to actually decode the token position
			 * as an SMI here, so we re-encode it when we pass it to
			 * jsfunc_lineno() below.
			 */
			if (read_heap_maybesmi(&tokpos, funcinfo,
			    tokposoff) != 0 ||
			    read_heap_ptr(&script, funcinfo,
			    V8_OFF_SHAREDFUNCTIONINFO_SCRIPT) != 0 ||
			    read_heap_ptr(&lends, script,
			    V8_OFF_SCRIPT_LINE_ENDS) != 0 ||
			    jsfunc_lineno(lends, V8_VALUE_SMI(tokpos),
			    func->fjsf_location,
			    sizeof (func->fjsf_location), NULL) != 0) {
				func->fjsf_location[0] = '\0';
			}
		}

		if (name != NULL && strstr(func->fjsf_funcname, name) == NULL)
			continue;

		if (filename != NULL &&
		    strstr(func->fjsf_scriptname, filename) == NULL)
			continue;

		code = 0;
		ilen = 0;
		if ((showrange || instr != 0) &&
		    (read_heap_ptr(&code, funcinfo,
		    V8_OFF_SHAREDFUNCTIONINFO_CODE) != 0 ||
		    read_heap_ptr(&ilen, code,
		    V8_OFF_CODE_INSTRUCTION_SIZE) != 0)) {
			code = 0;
			ilen = 0;
		}

		if ((instr != 0 && ilen != 0) &&
		    (instr < code + V8_OFF_CODE_INSTRUCTION_START ||
		    instr >= code + V8_OFF_CODE_INSTRUCTION_START + ilen))
			continue;

		if (!showrange) {
			mdb_printf("%?p %8d %-40s %s %s\n",
			    func->fjsf_instances.fjsi_addr,
			    func->fjsf_ninstances, func->fjsf_funcname,
			    func->fjsf_scriptname, func->fjsf_location);
		} else {
			uintptr_t code, ilen;

			if (read_heap_ptr(&code, funcinfo,
			    V8_OFF_SHAREDFUNCTIONINFO_CODE) != 0 ||
			    read_heap_ptr(&ilen, code,
			    V8_OFF_CODE_INSTRUCTION_SIZE) != 0) {
				mdb_printf("%?p %8d %?s %?s %-40s %s %s\n",
				    func->fjsf_instances.fjsi_addr,
				    func->fjsf_ninstances, "?", "?",
				    func->fjsf_funcname, func->fjsf_scriptname,
				    func->fjsf_location);
			} else {
				mdb_printf("%?p %8d %?p %?p %-40s %s %s\n",
				    func->fjsf_instances.fjsi_addr,
				    func->fjsf_ninstances,
				    code + V8_OFF_CODE_INSTRUCTION_START,
				    code + V8_OFF_CODE_INSTRUCTION_START + ilen,
				    func->fjsf_funcname, func->fjsf_scriptname,
				    func->fjsf_location);
			}
		}
	}

	return (DCMD_OK);
}

static void
dcmd_jsfunctions_help(void)
{
	mdb_printf("%s\n\n",
"Lists JavaScript functions, optionally filtered by a substring of the\n"
"function name or script filename or by the instruction address.  This uses\n"
"the cache created by ::findjsobjects.  If ::findjsobjects has not already\n"
"been run, this command runs it automatically without printing the output.\n"
"This can take anywhere from a second to several minutes, depending on the\n"
"size of the core dump.\n"
"\n"
"It's important to keep in mind that each time you create a function in\n"
"JavaScript (even from a function definition that has already been used),\n"
"the VM must create a new object to represent it.  For example, if your\n"
"program has a function A that returns a closure B, the VM will create new\n"
"instances of the closure function (B) each time the surrounding function (A)\n"
"is called.  To show this, the output of this command consists of one line \n"
"per function definition that appears in the JavaScript source, and the\n"
"\"#FUNCS\" column shows how many different functions were created by VM from\n"
"this definition.");

	mdb_dec_indent(2);
	mdb_printf("%<b>OPTIONS%</b>\n");
	mdb_inc_indent(2);

	mdb_printf("%s\n",
"  -f file  List functions that were defined in a file whose name contains\n"
"           this substring.\n"
"  -n func  List functions whose name contains this substring\n"
"  -x instr List functions whose compiled instructions include this address\n"
"  -X       Show where the function's instructions are stored in memory\n");
}

/* ARGSUSED */
static int
dcmd_v8field(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	v8_class_t *clp;
	v8_field_t *flp;
	const char *klass, *field;
	uintptr_t offset = 0;

	/*
	 * We may be invoked with either two arguments (class and field name) or
	 * three (an offset to save).
	 */
	if (argc != 2 && argc != 3)
		return (DCMD_USAGE);

	if (argv[0].a_type != MDB_TYPE_STRING ||
	    argv[1].a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	klass = argv[0].a_un.a_str;
	field = argv[1].a_un.a_str;

	if (argc == 3) {
		if (argv[2].a_type != MDB_TYPE_STRING)
			return (DCMD_USAGE);

		offset = mdb_strtoull(argv[2].a_un.a_str);
	}

	for (clp = v8_classes; clp != NULL; clp = clp->v8c_next)
		if (strcmp(clp->v8c_name, klass) == 0)
			break;

	if (clp == NULL) {
		(void) mdb_printf("error: no such class: \"%s\"", klass);
		return (DCMD_ERR);
	}

	for (flp = clp->v8c_fields; flp != NULL; flp = flp->v8f_next)
		if (strcmp(field, flp->v8f_name) == 0)
			break;

	if (flp == NULL) {
		if (argc == 2) {
			mdb_printf("error: no such field in class \"%s\": "
			    "\"%s\"", klass, field);
			return (DCMD_ERR);
		}

		flp = conf_field_create(clp, field, offset);
		if (flp == NULL) {
			mdb_warn("failed to create field");
			return (DCMD_ERR);
		}
	} else if (argc == 3) {
		flp->v8f_offset = offset;
	}

	mdb_printf("%s::%s at offset 0x%x\n", klass, field, flp->v8f_offset);
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_v8array(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint8_t type;
	uintptr_t *array;
	size_t ii, len;

	if (read_typebyte(&type, addr) != 0)
		return (DCMD_ERR);

	if (type != V8_TYPE_FIXEDARRAY) {
		mdb_warn("%p is not an instance of FixedArray\n", addr);
		return (DCMD_ERR);
	}

	if (read_heap_array(addr, &array, &len, UM_SLEEP | UM_GC) != 0)
		return (DCMD_ERR);

	for (ii = 0; ii < len; ii++)
		mdb_printf("%p\n", array[ii]);

	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_jsstack(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uintptr_t raddr;
	jsframe_t jsf;

	bzero(&jsf, sizeof (jsf));
	jsf.jsf_nlines = 5;

	if (mdb_getopts(argc, argv,
	    'a', MDB_OPT_SETBITS, B_TRUE, &jsf.jsf_showall,
	    'v', MDB_OPT_SETBITS, B_TRUE, &jsf.jsf_verbose,
	    'f', MDB_OPT_STR, &jsf.jsf_func,
	    'n', MDB_OPT_UINTPTR, &jsf.jsf_nlines,
	    'p', MDB_OPT_STR, &jsf.jsf_prop,
	    NULL) != argc)
		return (DCMD_USAGE);

	/*
	 * The "::jsframe" walker iterates the valid frame pointers, but the
	 * "::jsframe" dcmd looks at the frame after the one it was given, so we
	 * have to explicitly examine the top frame here.
	 */
	if (!(flags & DCMD_ADDRSPEC)) {
		if (load_current_context(&addr, &raddr) != 0 ||
		    do_jsframe(addr, raddr, &jsf) != 0)
			return (DCMD_ERR);
	}

	if (mdb_pwalk_dcmd("jsframe", "jsframe", argc, argv, addr) == -1)
		return (DCMD_ERR);

	jsframe_print_skipped(&jsf);
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_v8str(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	boolean_t opt_v = B_FALSE;
	char buf[512 * 1024];
	char *bufp;
	size_t len;

	if (mdb_getopts(argc, argv, 'v', MDB_OPT_SETBITS, B_TRUE, &opt_v,
	    NULL) != argc)
		return (DCMD_USAGE);

	bufp = buf;
	len = sizeof (buf);
	if (jsstr_print(addr, (opt_v ? JSSTR_VERBOSE : JSSTR_NONE) |
	    JSSTR_QUOTED, &bufp, &len) != 0)
		return (DCMD_ERR);

	mdb_printf("%s\n", buf);
	return (DCMD_OK);
}

static void
dcmd_v8load_help(void)
{
	v8_cfg_t *cfp, **cfgpp;

	mdb_printf(
	    "To traverse in-memory V8 structures, the V8 dmod requires\n"
	    "configuration that describes the layout of various V8 structures\n"
	    "in memory.  Normally, this information is pulled from metadata\n"
	    "in the target binary.  However, it's possible to use the module\n"
	    "with a binary not built with metadata by loading one of the\n"
	    "canned configurations.\n\n");

	mdb_printf("Available configurations:\n");

	(void) mdb_inc_indent(4);

	for (cfgpp = v8_cfgs; *cfgpp != NULL; cfgpp++) {
		cfp = *cfgpp;
		mdb_printf("%-10s    %s\n", cfp->v8cfg_name, cfp->v8cfg_label);
	}

	(void) mdb_dec_indent(4);
}

/* ARGSUSED */
static int
dcmd_v8load(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	v8_cfg_t *cfgp = NULL, **cfgpp;

	if (v8_classes != NULL) {
		mdb_warn("v8 module already configured\n");
		return (DCMD_ERR);
	}

	if (argc < 1 || argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	for (cfgpp = v8_cfgs; *cfgpp != NULL; cfgpp++) {
		cfgp = *cfgpp;
		if (strcmp(argv->a_un.a_str, cfgp->v8cfg_name) == 0)
			break;
	}

	if (cfgp == NULL || cfgp->v8cfg_name == NULL) {
		mdb_warn("unknown configuration: \"%s\"\n", argv->a_un.a_str);
		return (DCMD_ERR);
	}

	if (autoconfigure(cfgp) == -1) {
		mdb_warn("autoconfigure failed\n");
		return (DCMD_ERR);
	}

	mdb_printf("V8 dmod configured based on %s\n", cfgp->v8cfg_name);
	return (DCMD_OK);
}

/* ARGSUSED */
static int
dcmd_v8warnings(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	v8_warnings ^= 1;
	mdb_printf("v8 warnings are now %s\n", v8_warnings ? "on" : "off");

	return (DCMD_OK);
}

static int
walk_jsframes_init(mdb_walk_state_t *wsp)
{
	if (wsp->walk_addr != NULL)
		return (WALK_NEXT);

	if (load_current_context(&wsp->walk_addr, NULL) != 0)
		return (WALK_ERR);

	return (WALK_NEXT);
}

static int
walk_jsframes_step(mdb_walk_state_t *wsp)
{
	uintptr_t addr, next;
	int rv;

	addr = wsp->walk_addr;
	rv = wsp->walk_callback(wsp->walk_addr, NULL, wsp->walk_cbdata);

	if (rv != WALK_NEXT)
		return (rv);

	if (mdb_vread(&next, sizeof (next), addr) == -1)
		return (WALK_ERR);

	if (next == NULL)
		return (WALK_DONE);

	wsp->walk_addr = next;
	return (WALK_NEXT);
}

typedef struct jsprop_walk_data {
	int jspw_nprops;
	int jspw_current;
	uintptr_t *jspw_props;
} jsprop_walk_data_t;

/*ARGSUSED*/
static int
walk_jsprop_nprops(const char *desc, uintptr_t val, void *arg)
{
	jsprop_walk_data_t *jspw = arg;
	jspw->jspw_nprops++;

	return (0);
}

/*ARGSUSED*/
static int
walk_jsprop_props(const char *desc, uintptr_t val, void *arg)
{
	jsprop_walk_data_t *jspw = arg;
	jspw->jspw_props[jspw->jspw_current++] = val;

	return (0);
}

static int
walk_jsprop_init(mdb_walk_state_t *wsp)
{
	jsprop_walk_data_t *jspw;
	uintptr_t addr;
	uint8_t type;

	if ((addr = wsp->walk_addr) == NULL) {
		mdb_warn("'jsprop' does not support global walks\n");
		return (WALK_ERR);
	}

	if (!V8_IS_HEAPOBJECT(addr) || read_typebyte(&type, addr) != 0 ||
	    type != V8_TYPE_JSOBJECT) {
		mdb_warn("%p is not a JSObject\n", addr);
		return (WALK_ERR);
	}

	jspw = mdb_zalloc(sizeof (jsprop_walk_data_t), UM_SLEEP | UM_GC);

	if (jsobj_properties(addr, walk_jsprop_nprops, jspw, NULL) == -1) {
		mdb_warn("couldn't iterate over properties for %p\n", addr);
		return (WALK_ERR);
	}

	jspw->jspw_props = mdb_zalloc(jspw->jspw_nprops *
	    sizeof (uintptr_t), UM_SLEEP | UM_GC);

	if (jsobj_properties(addr, walk_jsprop_props, jspw, NULL) == -1) {
		mdb_warn("couldn't iterate over properties for %p\n", addr);
		return (WALK_ERR);
	}

	jspw->jspw_current = 0;
	wsp->walk_data = jspw;

	return (WALK_NEXT);
}

static int
walk_jsprop_step(mdb_walk_state_t *wsp)
{
	jsprop_walk_data_t *jspw = wsp->walk_data;
	int rv;

	if (jspw->jspw_current >= jspw->jspw_nprops)
		return (WALK_DONE);

	if ((rv = wsp->walk_callback(jspw->jspw_props[jspw->jspw_current++],
	    NULL, wsp->walk_cbdata)) != WALK_NEXT)
		return (rv);

	return (WALK_NEXT);
}

/*
 * MDB linkage
 */

static const mdb_dcmd_t v8_mdb_dcmds[] = {
	/*
	 * Commands to inspect Node-level state
	 */
	{ "nodebuffer", ":[-a]",
		"print details about the given Node Buffer", dcmd_nodebuffer },

	/*
	 * Commands to inspect JavaScript-level state
	 */
	{ "jsconstructor", ":[-v]",
		"print the constructor for a JavaScript object",
		dcmd_jsconstructor },
	{ "jsframe", ":[-aiv] [-f function] [-p property] [-n numlines]",
		"summarize a JavaScript stack frame", dcmd_jsframe },
	{ "jsprint", ":[-ab] [-d depth] [member]", "print a JavaScript object",
		dcmd_jsprint },
	{ "jssource", ":[-n numlines]",
		"print the source code for a JavaScript function",
		dcmd_jssource },
	{ "jsstack", "[-av] [-f function] [-p property] [-n numlines]",
		"print a JavaScript stacktrace", dcmd_jsstack },
	{ "findjsobjects", "?[-vb] [-r | -c cons | -p prop]", "find JavaScript "
		"objects", dcmd_findjsobjects, dcmd_findjsobjects_help },
	{ "jsfunctions", "[-X] [-s file_filter] [-n name_filter] "
	    "[-x instr_filter]", "list JavaScript functions",
	    dcmd_jsfunctions, dcmd_jsfunctions_help },

	/*
	 * Commands to inspect V8-level state
	 */
	{ "v8array", ":", "print elements of a V8 FixedArray",
		dcmd_v8array },
	{ "v8classes", NULL, "list known V8 heap object C++ classes",
		dcmd_v8classes },
	{ "v8code", ":[-d]", "print information about a V8 Code object",
		dcmd_v8code },
	{ "v8field", "classname fieldname offset",
		"manually add a field to a given class", dcmd_v8field },
	{ "v8function", ":[-d]", "print JSFunction object details",
		dcmd_v8function },
	{ "v8internal", ":[fieldidx]", "print v8 object internal fields",
		dcmd_v8internal },
	{ "v8load", "version", "load canned config for a specific V8 version",
		dcmd_v8load, dcmd_v8load_help },
	{ "v8frametypes", NULL, "list known V8 frame types",
		dcmd_v8frametypes },
	{ "v8print", ":[class]", "print a V8 heap object",
		dcmd_v8print, dcmd_v8print_help },
	{ "v8str", ":[-v]", "print the contents of a V8 string",
		dcmd_v8str },
	{ "v8type", ":", "print the type of a V8 heap object",
		dcmd_v8type },
	{ "v8types", NULL, "list known V8 heap object types",
		dcmd_v8types },
	{ "v8warnings", NULL, "toggle V8 warnings",
		dcmd_v8warnings },

	{ NULL }
};

static const mdb_walker_t v8_mdb_walkers[] = {
	{ "jsframe", "walk V8 JavaScript stack frames",
		walk_jsframes_init, walk_jsframes_step },
	{ "jsprop", "walk property values for an object",
		walk_jsprop_init, walk_jsprop_step },
	{ NULL }
};

static mdb_modinfo_t v8_mdb = { MDB_API_VERSION, v8_mdb_dcmds, v8_mdb_walkers };

static void
configure(void)
{
	char *success;
	v8_cfg_t *cfgp = NULL;
	GElf_Sym sym;
	int major, minor, build, patch;

	if (mdb_readsym(&major, sizeof (major),
	    "_ZN2v88internal7Version6major_E") == -1 ||
	    mdb_readsym(&minor, sizeof (minor),
	    "_ZN2v88internal7Version6minor_E") == -1 ||
	    mdb_readsym(&build, sizeof (build),
	    "_ZN2v88internal7Version6build_E") == -1 ||
	    mdb_readsym(&patch, sizeof (patch),
	    "_ZN2v88internal7Version6patch_E") == -1) {
		mdb_warn("failed to determine V8 version");
		return;
	}

	v8_major = major;
	v8_minor = minor;
	v8_build = build;
	v8_patch = patch;
	mdb_printf("V8 version: %d.%d.%d.%d\n",
	    v8_major, v8_minor, v8_build, v8_patch);

	/*
	 * First look for debug metadata embedded within the binary, which may
	 * be present in recent V8 versions built with postmortem metadata.
	 */
	if (mdb_lookup_by_name("v8dbg_SmiTag", &sym) == 0) {
		cfgp = &v8_cfg_target;
		success = "Autoconfigured V8 support from target";
	} else if (v8_major == 3 && v8_minor == 1 && v8_build == 8) {
		cfgp = &v8_cfg_04;
		success = "Configured V8 support based on node v0.4";
	} else if (v8_major == 3 && v8_minor == 6 && v8_build == 6) {
		cfgp = &v8_cfg_06;
		success = "Configured V8 support based on node v0.6";
	} else {
		mdb_printf("mdb_v8: target has no debug metadata and "
		    "no existing config found\n");
		return;
	}

	if (autoconfigure(cfgp) != 0) {
		mdb_warn("failed to autoconfigure from target; "
		    "commands may have incorrect results!\n");
		return;
	}

	mdb_printf("%s\n", success);
}

static void
enable_demangling(void)
{
	const char *symname = "_ZN2v88internal7Version6major_E";
	GElf_Sym sym;
	char buf[64];

	/*
	 * Try to determine whether C++ symbol demangling has been enabled.  If
	 * not, enable it.
	 */
	if (mdb_lookup_by_name("_ZN2v88internal7Version6major_E", &sym) != 0)
		return;

	(void) mdb_snprintf(buf, sizeof (buf), "%a", sym.st_value);
	if (strstr(buf, symname) != NULL)
		(void) mdb_eval("$G");
}

const mdb_modinfo_t *
_mdb_init(void)
{
	configure();
	enable_demangling();
	return (&v8_mdb);
}
