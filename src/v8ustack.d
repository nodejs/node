/*
 * V8 DTrace ustack helper for annotating native stack traces with JavaScript
 * function names.  We start with a frame pointer (arg1) and emit a string
 * describing the current function.  We do this by chasing pointers to extract
 * the function's name (if any) and the filename and line number where the
 * function is defined.
 *
 * To use the helper, run node, then use the jstack() DTrace action to capture
 * a JavaScript stacktrace.  You may need to tune the dtrace_helper_actions_max
 * kernel variable to 128.
 */

#include <v8constants.h>
#include <v8abbr.h>

/*
 * V8 represents small integers (SMI) using the upper 31 bits of a 32/64-bit
 * value.  To extract the actual integer value, we must shift it over.
 */
#define	IS_SMI(value)	\
    ((value & V8_SmiTagMask) == V8_SmiTag)
#define	SMI_VALUE(value)	\
    ((uint32_t) ((value) >> V8_SmiValueShift))

/*
 * Heap objects usually start off with a Map pointer, itself another heap
 * object.  However, during garbage collection, the low order bits of the
 * pointer (which are normally 01) are used to record GC state.  Of course, we
 * have no idea if we're in GC or not, so we must always normalize the pointer.
 */
#define	V8_MAP_PTR(ptr)		\
    ((ptr & ~V8_HeapObjectTagMask) | V8_HeapObjectTag)

#define V8_TYPE_SCRIPT(type) \
    ((type) == V8_IT_SCRIPT)

/*
 * Determine the encoding and representation of a V8 string.
 */
#define	V8_TYPE_STRING(type)	\
    (((type) & V8_IsNotStringMask) == V8_StringTag)

#define	V8_STRENC_ASCII(type)	\
    (((type) & V8_StringEncodingMask) == V8_AsciiStringTag)

#define	V8_STRREP_SEQ(type)	\
    (((type) & V8_StringRepresentationMask) == V8_SeqStringTag)
#define	V8_STRREP_CONS(type)	\
    (((type) & V8_StringRepresentationMask) == V8_ConsStringTag)
#define	V8_STRREP_EXT(type)	\
    (((type) & V8_StringRepresentationMask) == V8_ExternalStringTag)

/*
 * String type predicates
 */
#define	ASCII_SEQSTR(value)	\
    (V8_TYPE_STRING(value) && V8_STRENC_ASCII(value) && V8_STRREP_SEQ(value))

#define	TWOBYTE_SEQSTR(value)	\
    (V8_TYPE_STRING(value) && !V8_STRENC_ASCII(value) && V8_STRREP_SEQ(value))

#define	IS_CONSSTR(value)	\
    (V8_TYPE_STRING(value) && V8_STRREP_CONS(value))

#define	ASCII_EXTSTR(value)	\
    (V8_TYPE_STRING(value) && V8_STRENC_ASCII(value) && V8_STRREP_EXT(value))

/*
 * General helper macros
 */
#define	COPYIN_UINT8(addr) (*(uint8_t*) copyin((addr), sizeof(uint8_t)))
#define	COPYIN_UINT32(addr) (*(uint32_t*) copyin((addr), sizeof(uint32_t)))
#define	COPYIN_UINT64(addr) (*(uint64_t*) copyin((addr), sizeof(uint64_t)))

#if defined(__i386)
# define	COPYIN_PTR(addr) COPYIN_UINT32(addr)
# define	off_t uint32_t
# define	APPEND_PTR(p) APPEND_PTR_32(p)
#else
# define	COPYIN_PTR(addr) COPYIN_UINT64(addr)
# define	off_t uint64_t
# define	APPEND_PTR(p) APPEND_PTR_64(p)
#endif

#define	APPEND_CHR(c)			(this->buf[this->off++] = (c))
#define	APPEND_CHR4(s0, s1, s2, s3)	\
    APPEND_CHR(s0);	\
    APPEND_CHR(s1);	\
    APPEND_CHR(s2);	\
    APPEND_CHR(s3);
#define	APPEND_CHR8(s0, s1, s2, s3, s4, s5, s6, s7)	\
    APPEND_CHR4(s0, s1, s2, s3)	\
    APPEND_CHR4(s4, s5, s6, s7)

#define	APPEND_DGT(i, d)	\
    (((i) / (d)) ? APPEND_CHR('0' + ((i)/(d) % 10)) : 0)

#define	APPEND_NUM(i)		\
    APPEND_DGT((i), 100000);	\
    APPEND_DGT((i), 10000);	\
    APPEND_DGT((i), 1000);	\
    APPEND_DGT((i), 100);	\
    APPEND_DGT((i), 10);	\
    APPEND_DGT((i), 1);

#define	APPEND_HEX(d)	\
    APPEND_CHR((d) < 10 ? '0' + (d) : 'a' - 10 + (d))

#define	APPEND_PTR_32(p)	\
    APPEND_HEX((p >> 28) & 0xf);	\
    APPEND_HEX((p >> 24) & 0xf);	\
    APPEND_HEX((p >> 20) & 0xf);	\
    APPEND_HEX((p >> 16) & 0xf);	\
    APPEND_HEX((p >> 12) & 0xf);	\
    APPEND_HEX((p >> 8) & 0xf);	\
    APPEND_HEX((p >> 4) & 0xf);	\
    APPEND_HEX((p) & 0xf);

#define	APPEND_PTR_64(p)	\
    APPEND_PTR_32(p >> 32)	\
    APPEND_PTR_32(p)

/*
 * The following macros are used to output ASCII SeqStrings, ConsStrings, and
 * Node.js ExternalStrings.  To represent each string, we use three fields:
 *
 *    "str":	a pointer to the string itself
 *
 *    "len":	the string length
 *
 *    "attrs":	the type identifier for the string, which indicates the
 *    		encoding and representation.  We're only interested in strings
 *    		whose representation is one of:
 *
 *	SeqOneByteString stored directly as a char array inside the object
 *
 *	SeqTwoByteString stored as a UTF-16 char array inside the object
 *
 *	ConsString	 pointer to two strings that should be concatenated
 *
 * 	ExternalString	 pointer to a char* outside the V8 heap
 */

/*
 * Load "len" and "attrs" for the given "str".
 */
#define	LOAD_STRFIELDS(str, len, attrs)					\
    len = SMI_VALUE(COPYIN_PTR(str + V8_OFF_STR_LENGTH));	\
    this->map = V8_MAP_PTR(COPYIN_PTR(str + V8_OFF_HEAPOBJ_MAP));	\
    attrs = COPYIN_UINT8(this->map + V8_OFF_MAP_ATTRS);

#define	APPEND_SEQSTR(str, len, attrs) \
    APPEND_SEQONEBYTESTR(str, len, attrs) \
    APPEND_SEQTWOBYTESTR(str, len, attrs)

/*
 * Print out the given SeqOneByteString, or do nothing if the string is not an ASCII
 * SeqOneByteString.
 */
#define	APPEND_SEQONEBYTESTR(str, len, attrs) 				\
    dtrace:helper:ustack:						\
    /!this->done && len > 0 && ASCII_SEQSTR(attrs)/			\
    {									\
	copyinto(str + V8_OFF_STR_CHARS, len, this->buf + this->off);	\
	this->off += len;						\
    }

/*
 * LOOP_ITER: macro to paste "block" while "ivar" is less than "dynmax" and
 * "statmax".  The subsequent LOOP_{4,8} macros facilitate pasting the same
 * thing 4 and 8 times, respectively.  Like much of the rest of the code in this
 * file, this is regrettably necessary given the constraints under which we're
 * expected to run.
 */
#define	LOOP_ITER(ivar, dynmax, statmax, block) \
	((ivar) < (dynmax)) && ((ivar) < (statmax)) && (block); (ivar)++;

#define	LOOP_4(block) \
	block \
	block \
	block \
	block \

#define	LOOP_8(block) \
	LOOP_4(block) \
	LOOP_4(block)

/*
 * Print out the given SeqTwoByteString, or do nothing if the string is not an ASCII
 * SeqTwoByteString.  NOTE: if you bump MAX_TWOBYTESTR_CHARS, you'll also need
 * to modify the LOOP_* macro calls below to match.
 */
#define	MAX_TWOBYTESTR_CHARS	128
#define	MAX_TWOBYTESTR_BYTES	(2 * MAX_TWOBYTESTR_CHARS)
#define	TO_ASCII(c)		((c) < 128 ? (c) : '?')

#define	APPEND_SEQTWOBYTESTR(str, len, attrs) 				\
    dtrace:helper:ustack: 						\
    /!this->done && len > 0 && TWOBYTE_SEQSTR(attrs)/ 			\
    {									\
	this->i = 0;							\
	this->stbuf = (uint16_t *)alloca(MAX_TWOBYTESTR_BYTES + 2); 	\
	copyinto(str + V8_OFF_TWOBYTESTR_CHARS,				\
	    MAX_TWOBYTESTR_BYTES, this->stbuf);				\
	this->stbuf[MAX_TWOBYTESTR_BYTES - 1] = '\0';			\
	this->stbuf[MAX_TWOBYTESTR_BYTES] = '\0';			\
									\
	LOOP_8(LOOP_8(LOOP_4(LOOP_ITER(this->i, len,			\
	    MAX_TWOBYTESTR_CHARS,					\
	    APPEND_CHR(TO_ASCII(this->stbuf[this->i]))))))		\
									\
	this->i = 0;							\
	this->stbuf = 0;						\
    }

/*
 * Print out the given Node.js ExternalString, or do nothing if the string is
 * not an ASCII ExternalString.
 */
#define	APPEND_NODESTR(str, len, attrs)					\
    dtrace:helper:ustack:						\
    /!this->done && len > 0 && ASCII_EXTSTR(attrs)/			\
    {									\
	this->resource = COPYIN_PTR(str + V8_OFF_EXTSTR_RSRC);		\
	this->dataptr = COPYIN_PTR(this->resource + NODE_OFF_EXTSTR_DATA);	\
	copyinto(this->dataptr, len, this->buf + this->off);			\
	this->off += len;							\
    }

/*
 * Recall that each ConsString points to two other strings which are
 * semantically concatenated.  Of course, these strings may themselves by
 * ConsStrings, but in D we can only expand this recursion to a finite level.
 * Thankfully, function and script names are generally not more than a few
 * levels deep, so we unroll the expansion up to three levels.  Even this is
 * pretty hairy: we use strings "s0", ..., "s13", (each with "str", "len", and
 * "attr" fields -- see above) to store the expanded strings.  We expand the
 * original string into s0 and s7, then s0 into s1 and s4, etc:
 *
 *
 *                   +----  str  ----+
 *                  /                 \                  <--  1st expansion
 *                 /                   \
 *              s0                       s7
 *            /    \                  /     \
 *           /      \                /       \           <--  2nd expansion
 *          /        \              /         \
 *        s1          s4          s8           s11
 *       /  \        /  \        /  \          /  \      <--  3rd expansion
 *     s2    s3    s5    s6    s9    s10    s12    s13
 *
 * Of course, for a given string, any of these expansions may not be needed.
 * For example, we may expand str and find that s0 is already a SeqString,
 * while s7 requires further expansion.  So when we expand a ConsString, we
 * zero the length of the string itself, and then at the end we print out
 * all non-zero-length strings in order (including both internal nodes and
 * leafs in the tree above) to get the final output.
 */
#define	EXPAND_START()							\
    dtrace:helper:ustack:	\
    /!this->done/	\
    {	\
	this->s0str = this->s1str = this->s2str = (off_t) 0;	\
	this->s3str = this->s4str = this->s5str = (off_t) 0;	\
	this->s6str = this->s7str = this->s8str = (off_t) 0;	\
	this->s9str = this->s10str = this->s11str = (off_t) 0;	\
	this->s12str = this->s13str = (off_t) 0;	\
									\
	this->s0len = this->s1len = this->s2len = (off_t) 0;	\
	this->s3len = this->s4len = this->s5len = (off_t) 0;	\
	this->s6len = this->s7len = this->s8len = (off_t) 0;	\
	this->s9len = this->s10len = this->s11len = (off_t) 0;	\
	this->s12len = this->s13len = (off_t) 0;	\
									\
	this->s0attrs = this->s1attrs = this->s2attrs = 0;		\
	this->s3attrs = this->s4attrs = this->s5attrs = 0;		\
	this->s6attrs = this->s7attrs = this->s8attrs = 0;		\
	this->s9attrs = this->s10attrs = this->s11attrs = 0;		\
	this->s12attrs = this->s13attrs = 0;				\
    }

/*
 * Expand the ConsString "str" (represented by "str", "len", and "attrs") into
 * strings "s1" (represented by "s1s", "s1l", and "s1a") and "s2" (represented
 * by "s2s", "s2l", "s2a").  If "str" is not a ConsString, do nothing.
 */
#define	EXPAND_STR(str, len, attrs, s1s, s1l, s1a, s2s, s2l, s2a)	\
    dtrace:helper:ustack:	\
    /!this->done && len > 0 && IS_CONSSTR(attrs)/	\
    {	\
	len = 0;							\
									\
	s1s = COPYIN_PTR(str + V8_OFF_CONSSTR_CAR);	\
	LOAD_STRFIELDS(s1s, s1l, s1a)					\
									\
	s2s = COPYIN_PTR(str + V8_OFF_CONSSTR_CDR);	\
	LOAD_STRFIELDS(s2s, s2l, s2a)					\
    }

/*
 * Print out a ConsString by expanding it up to three levels and printing out
 * the resulting SeqStrings.
 */
#define	APPEND_CONSSTR(str, len, attrs)					\
    EXPAND_START()							\
    EXPAND_STR(str, len, attrs,						\
	this->s0str, this->s0len, this->s0attrs,			\
	this->s7str, this->s7len, this->s7attrs)			\
    EXPAND_STR(this->s0str, this->s0len, this->s0attrs,			\
	this->s1str, this->s1len, this->s1attrs,			\
	this->s4str, this->s4len, this->s4attrs)			\
    EXPAND_STR(this->s1str, this->s1len, this->s1attrs,			\
	this->s2str, this->s2len, this->s2attrs,			\
	this->s3str, this->s3len, this->s3attrs)			\
    EXPAND_STR(this->s4str, this->s4len, this->s4attrs,			\
	this->s5str, this->s5len, this->s5attrs,			\
	this->s6str, this->s6len, this->s6attrs)			\
    EXPAND_STR(this->s7str, this->s7len, this->s7attrs,			\
	this->s8str, this->s8len, this->s8attrs,			\
	this->s11str, this->s11len, this->s11attrs)			\
    EXPAND_STR(this->s8str, this->s8len, this->s8attrs,			\
	this->s9str, this->s9len, this->s9attrs,			\
	this->s10str, this->s10len, this->s10attrs)			\
    EXPAND_STR(this->s11str, this->s11len, this->s11attrs,		\
	this->s12str, this->s12len, this->s12attrs,			\
	this->s13str, this->s13len, this->s13attrs)			\
									\
    APPEND_SEQSTR(str, len, attrs)					\
    APPEND_SEQSTR(this->s0str, this->s0len, this->s0attrs)		\
    APPEND_SEQSTR(this->s1str, this->s1len, this->s1attrs)		\
    APPEND_SEQSTR(this->s2str, this->s2len, this->s2attrs)		\
    APPEND_SEQSTR(this->s3str, this->s3len, this->s3attrs)		\
    APPEND_SEQSTR(this->s4str, this->s4len, this->s4attrs)		\
    APPEND_SEQSTR(this->s5str, this->s5len, this->s5attrs)		\
    APPEND_SEQSTR(this->s6str, this->s6len, this->s6attrs)		\
    APPEND_SEQSTR(this->s7str, this->s7len, this->s7attrs)		\
    APPEND_SEQSTR(this->s8str, this->s8len, this->s8attrs)		\
    APPEND_SEQSTR(this->s9str, this->s9len, this->s9attrs)		\
    APPEND_SEQSTR(this->s10str, this->s10len, this->s10attrs)		\
    APPEND_SEQSTR(this->s11str, this->s11len, this->s11attrs)		\
    APPEND_SEQSTR(this->s12str, this->s12len, this->s12attrs)		\
    APPEND_SEQSTR(this->s13str, this->s13len, this->s13attrs)		\


/*
 * Print out the given SeqString, ConsString, or ExternalString.
 * APPEND_CONSSTR implicitly handles SeqStrings as the degenerate case of an
 * expanded ConsString.
 */
#define	APPEND_V8STR(str, len, attrs)					\
    APPEND_CONSSTR(str, len, attrs)					\
    APPEND_NODESTR(str, len, attrs)

/*
 * In this first clause we initialize all variables.  We must explicitly clear
 * them because they may contain values left over from previous iterations.
 */
dtrace:helper:ustack:
{
	/* input */
	this->fp = arg1;

	/* output/flow control */
	this->buf = (char*) alloca(128);
	this->off = 0;
	this->done = 0;

	/* program state */
	this->ctx = (off_t) 0;
	this->marker = (off_t) 0;
	this->func = (off_t) 0;
	this->shared = (off_t) 0;
	this->map = (off_t) 0;
	this->attrs = 0;
	this->funcnamestr = (off_t) 0;
	this->funcnamelen = 0;
	this->funcnameattrs = 0;
	this->script = (off_t) 0;
	this->scriptattrs = 0;
	this->scriptnamestr = (off_t) 0;
	this->scriptnamelen = 0;
	this->scriptnameattrs = 0;
	this->position = 0;
	this->line_ends = (off_t) 0;
	this->le_attrs = 0;

	/* binary search fields */
	this->bsearch_min = 0;
	this->bsearch_max = 0;
	this->ii = 0;
}

/*
 * Like V8, we first check if we've got an ArgumentsAdaptorFrame.  We've got
 * nothing to add for such frames, so we bail out quickly.
 */
dtrace:helper:ustack:
{
	this->ctx = COPYIN_PTR(this->fp + V8_OFF_FP_CONTEXT);
}

dtrace:helper:ustack:
/IS_SMI(this->ctx) && SMI_VALUE(this->ctx) == V8_FT_ADAPTOR/
{
	this->done = 1;
	APPEND_CHR8('<','<',' ','a','d','a','p','t');
	APPEND_CHR8('o','r',' ','>','>','\0','\0','\0');
	stringof(this->buf);
}

/*
 * Check for other common frame types for which we also have nothing to add.
 */
dtrace:helper:ustack:
/!this->done/
{
	this->marker = COPYIN_PTR(this->fp + V8_OFF_FP_MARKER);
}

dtrace:helper:ustack:
/!this->done && IS_SMI(this->marker) &&
 SMI_VALUE(this->marker) == V8_FT_ENTRY/
{
	this->done = 1;
	APPEND_CHR8('<','<',' ','e','n','t','r','y');
	APPEND_CHR4(' ','>','>','\0');
	stringof(this->buf);
}

dtrace:helper:ustack:
/!this->done && IS_SMI(this->marker) &&
 SMI_VALUE(this->marker) == V8_FT_ENTRYCONSTRUCT/
{
	this->done = 1;
	APPEND_CHR8('<','<',' ','e','n','t','r','y');
	APPEND_CHR8('_','c','o','n','s','t','r','u');
	APPEND_CHR4('t',' ','>','>');
	APPEND_CHR('\0');
	stringof(this->buf);
}

dtrace:helper:ustack:
/!this->done && IS_SMI(this->marker) &&
 SMI_VALUE(this->marker) == V8_FT_EXIT/
{
	this->done = 1;
	APPEND_CHR8('<','<',' ','e','x','i','t',' ');
	APPEND_CHR4('>','>','\0','\0');
	stringof(this->buf);
}

dtrace:helper:ustack:
/!this->done && IS_SMI(this->marker) &&
 SMI_VALUE(this->marker) == V8_FT_INTERNAL/
{
	this->done = 1;
	APPEND_CHR8('<','<',' ','i','n','t','e','r');
	APPEND_CHR8('n','a','l',' ','>','>','\0','\0');
	stringof(this->buf);
}

dtrace:helper:ustack:
/!this->done && IS_SMI(this->marker) &&
 SMI_VALUE(this->marker) == V8_FT_CONSTRUCT/
{
	this->done = 1;
	APPEND_CHR8('<','<',' ','c','o','n','s','t');
	APPEND_CHR8('r','u','c','t','o','r',' ','>');
	APPEND_CHR4('>','\0','\0','\0');
	stringof(this->buf);
}

dtrace:helper:ustack:
/!this->done && IS_SMI(this->marker) &&
 SMI_VALUE(this->marker) == V8_FT_STUB/
{
	this->done = 1;
	APPEND_CHR8('<','<',' ','s','t','u','b',' ');
	APPEND_CHR4('>','>','\0','\0');
	stringof(this->buf);
}

/*
 * Now check for internal frames that we can only identify by seeing that
 * there's a Code object where there would be a JSFunction object for a
 * JavaScriptFrame.
 */
dtrace:helper:ustack:
/!this->done/
{
	this->func = COPYIN_PTR(this->fp + V8_OFF_FP_FUNC);
	this->map = V8_MAP_PTR(COPYIN_PTR(this->func + V8_OFF_HEAPOBJ_MAP));
	this->attrs = COPYIN_UINT8(this->map + V8_OFF_MAP_ATTRS);
}

dtrace:helper:ustack:
/!this->done && this->attrs == V8_IT_CODE/
{
	this->done = 1;
	APPEND_CHR8('<','<',' ','i','n','t','e','r');
	APPEND_CHR8('n','a','l',' ','c','o','d','e');
	APPEND_CHR4(' ','>','>','\0');
	stringof(this->buf);
}

/*
 * At this point, we're either looking at a JavaScriptFrame or an
 * OptimizedFrame.  For now, we assume JavaScript and start by grabbing the
 * function name.
 */
dtrace:helper:ustack:
/!this->done/
{
	this->map = 0;
	this->attrs = 0;

	this->shared = COPYIN_PTR(this->func + V8_OFF_FUNC_SHARED);
	this->funcnamestr = COPYIN_PTR(this->shared + V8_OFF_SHARED_NAME);
	LOAD_STRFIELDS(this->funcnamestr, this->funcnamelen,
	    this->funcnameattrs);
}

dtrace:helper:ustack:
/!this->done && this->funcnamelen == 0/
{
	/*
	 * This is an anonymous function, but if it was invoked as a method of
	 * some object then V8 will have computed an inferred name that we can
	 * include in the stack trace.
	 */
	APPEND_CHR8('(','a','n','o','n',')',' ','a');
	APPEND_CHR('s');
	APPEND_CHR(' ');

	this->funcnamestr = COPYIN_PTR(this->shared + V8_OFF_SHARED_INFERRED);
	LOAD_STRFIELDS(this->funcnamestr, this->funcnamelen,
	    this->funcnameattrs);
}

dtrace:helper:ustack:
/!this->done && this->funcnamelen == 0/
{
	APPEND_CHR('(');
	APPEND_CHR4('a','n','o','n');
	APPEND_CHR(')');
}

APPEND_V8STR(this->funcnamestr, this->funcnamelen, this->funcnameattrs)

/*
 * Now look for the name of the script where the function was defined.  The
 * "script" itself may be undefined for special functions like "RegExp".
 */
dtrace:helper:ustack:
/!this->done/
{
	this->script = COPYIN_PTR(this->shared + V8_OFF_SHARED_SCRIPT);
	this->map = V8_MAP_PTR(COPYIN_PTR(this->script + V8_OFF_HEAPOBJ_MAP));
	this->scriptattrs = COPYIN_UINT8(this->map + V8_OFF_MAP_ATTRS);
}

dtrace:helper:ustack:
/!this->done && !V8_TYPE_SCRIPT(this->scriptattrs)/
{
	APPEND_CHR('\0');
	this->done = 1;
	stringof(this->buf);
}


dtrace:helper:ustack:
/!this->done/
{
	this->scriptnamestr = COPYIN_PTR(this->script + V8_OFF_SCRIPT_NAME);
	LOAD_STRFIELDS(this->scriptnamestr, this->scriptnamelen,
	    this->scriptnameattrs);
}

dtrace:helper:ustack:
/!this->done && this->scriptnamelen != 0/
{
	APPEND_CHR4(' ','a','t',' ');
}

APPEND_V8STR(this->scriptnamestr, this->scriptnamelen, this->scriptnameattrs)

/*
 * Now look for file position and line number information.
 */
dtrace:helper:ustack:
/!this->done/
{
	this->position = COPYIN_UINT32(this->shared + V8_OFF_SHARED_FUNTOK);
	this->line_ends = COPYIN_PTR(this->script + V8_OFF_SCRIPT_LENDS);
	this->map = V8_MAP_PTR(COPYIN_PTR(this->line_ends + V8_OFF_HEAPOBJ_MAP));
	this->le_attrs = COPYIN_UINT8(this->map + V8_OFF_MAP_ATTRS);
}

dtrace:helper:ustack:
/!this->done && this->le_attrs != V8_IT_FIXEDARRAY && this->position == 0/
{
	APPEND_CHR('\0');
	this->done = 1;
	stringof(this->buf);
}

dtrace:helper:ustack:
/!this->done && this->le_attrs != V8_IT_FIXEDARRAY/
{
	/*
	 * If the line number array was not a valid FixedArray, it's probably
	 * undefined because V8 has not had to compute it yet.  In this case we
	 * just show the raw position and call it a day.
	 */
	APPEND_CHR4(' ','p','o','s');
	APPEND_CHR(' ');
	APPEND_NUM(SMI_VALUE(this->position));
	APPEND_CHR('\0');
	this->done = 1;
	stringof(this->buf);
}

/*
 * At this point, we've got both a position in the script and an array
 * describing where each line of the file ends.  We can use this to compute the
 * line number by binary searching the array.  (This is also what V8 does when
 * computing stack traces.)
 */
dtrace:helper:ustack:
/!this->done/
{
	/* initialize binary search */
	this->bsearch_line = this->position <
	SMI_VALUE(COPYIN_PTR(this->line_ends + V8_OFF_FA_DATA)) ? 1 : 0;
	this->bsearch_min = 0;
	this->bsearch_max = this->bsearch_line != 0 ? 0 :
	SMI_VALUE(COPYIN_PTR(this->line_ends + V8_OFF_FA_SIZE)) - 1;
}

/*
 * Of course, we can't iterate the binary search indefinitely, so we hardcode 15
 * iterations.  That's enough to precisely identify the line number in files up
 * to 32768 lines of code.
 */
#define	BSEARCH_LOOP							\
    dtrace:helper:ustack:	\
    /!this->done && this->bsearch_max >= 1/	\
    {	\
	this->ii = (this->bsearch_min + this->bsearch_max) >> 1;	\
    }	\
									\
    dtrace:helper:ustack:	\
    /!this->done && this->bsearch_max >= 1 &&	\
     this->position > SMI_VALUE(	\
         COPYIN_PTR(this->line_ends + V8_OFF_FA_DATA +	\
                    this->ii * sizeof (uint32_t)))/	\
    {	\
	this->bsearch_min = this->ii + 1;				\
    }	\
									\
    dtrace:helper:ustack:	\
    /!this->done && this->bsearch_max >= 1 &&	\
     this->position <= SMI_VALUE(	\
         COPYIN_PTR(this->line_ends + V8_OFF_FA_DATA +	\
                    (this->ii - 1) * sizeof (uint32_t)))/	\
    {	\
	this->bsearch_max = this->ii - 1;				\
    }

BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP
BSEARCH_LOOP

dtrace:helper:ustack:
/!this->done && !this->bsearch_line/
{
	this->bsearch_line = this->ii + 1;
}

dtrace:helper:ustack:
/!this->done/
{
	APPEND_CHR(' ');
	APPEND_CHR4('l','i','n','e');
	APPEND_CHR(' ');
	APPEND_NUM(this->bsearch_line);
	APPEND_CHR('\0');
	this->done = 1;
	stringof(this->buf);
}

/* vim: set tabstop=8 softtabstop=8 shiftwidth=8 noexpandtab: */
