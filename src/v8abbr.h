/** Copyright Joyent, Inc. and other Node contributors.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
* NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
* DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
* OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
* USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * This header defines short names for V8 constants for use by the ustack
 * helper.
 */

#ifndef SRC_V8ABBR_H_
#define SRC_V8ABBR_H_

/* Frame pointer offsets */
#define V8_OFF_FP_FUNC              V8DBG_OFF_FP_FUNCTION
#define V8_OFF_FP_CONTEXT           V8DBG_OFF_FP_CONTEXT
#define V8_OFF_FP_MARKER            V8DBG_OFF_FP_MARKER

/* Stack frame types */
#define V8_FT_ENTRY                 V8DBG_FRAMETYPE_ENTRYFRAME
#define V8_FT_ENTRYCONSTRUCT        V8DBG_FRAMETYPE_ENTRYCONSTRUCTFRAME
#define V8_FT_EXIT                  V8DBG_FRAMETYPE_EXITFRAME
#define V8_FT_JAVASCRIPT            V8DBG_FRAMETYPE_JAVASCRIPTFRAME
#define V8_FT_OPTIMIZED             V8DBG_FRAMETYPE_OPTIMIZEDFRAME
#define V8_FT_INTERNAL              V8DBG_FRAMETYPE_INTERNALFRAME
#define V8_FT_CONSTRUCT             V8DBG_FRAMETYPE_CONSTRUCTFRAME
#define V8_FT_ADAPTOR               V8DBG_FRAMETYPE_ARGUMENTSADAPTORFRAME
#define V8_FT_STUB                  V8DBG_FRAMETYPE_STUBFRAME

/* Identification masks and tags */
#define V8_SmiTagMask               (V8DBG_SMITAGMASK)
#define V8_SmiTag                   (V8DBG_SMITAG)
#define V8_SmiValueShift            (V8DBG_SMISHIFTSIZE + V8DBG_SMITAGMASK)

#define V8_HeapObjectTagMask        V8DBG_HEAPOBJECTTAGMASK
#define V8_HeapObjectTag            V8DBG_HEAPOBJECTTAG

#define V8_IsNotStringMask          V8DBG_ISNOTSTRINGMASK
#define V8_StringTag                V8DBG_STRINGTAG

#define V8_StringEncodingMask       V8DBG_STRINGENCODINGMASK
#define V8_AsciiStringTag           V8DBG_ONEBYTESTRINGTAG

#define V8_StringRepresentationMask V8DBG_STRINGREPRESENTATIONMASK
#define V8_SeqStringTag             V8DBG_SEQSTRINGTAG
#define V8_ConsStringTag            V8DBG_CONSSTRINGTAG
#define V8_ExternalStringTag        V8DBG_EXTERNALSTRINGTAG

/* Instance types */
#define V8_IT_FIXEDARRAY            V8DBG_TYPE_FIXEDARRAY__FIXED_ARRAY_TYPE
#define V8_IT_CODE                  V8DBG_TYPE_CODE__CODE_TYPE
#define V8_IT_SCRIPT                V8DBG_TYPE_SCRIPT__SCRIPT_TYPE

/* Node-specific offsets */
#define NODE_OFF_EXTSTR_DATA        sizeof(void*)

/*
 * Not all versions of V8 have the offset for the "chars" array in the
 * SeqTwoByteString class, but it's the same as the one for SeqOneByteString.
 */
#ifndef V8DBG_CLASS_SEQTWOBYTESTRING__CHARS__CHAR
#define V8DBG_CLASS_SEQTWOBYTESTRING__CHARS__CHAR \
  V8DBG_CLASS_SEQONEBYTESTRING__CHARS__CHAR
#endif

/* Heap class->field offsets */
#define V8_OFF_HEAP(off)            ((off) - 1)

#define V8_OFF_FUNC_SHARED  \
    V8_OFF_HEAP(V8DBG_CLASS_JSFUNCTION__SHARED__SHAREDFUNCTIONINFO)
#define V8_OFF_RAW_NAME  \
    V8_OFF_HEAP(V8DBG_CLASS_SHAREDFUNCTIONINFO__RAW_NAME__OBJECT)
#define V8_OFF_SHARED_IDENT  \
    V8_OFF_HEAP(V8DBG_CLASS_SHAREDFUNCTIONINFO__FUNCTION_IDENTIFIER__OBJECT)
#define V8_OFF_SHARED_SCRIPT  \
    V8_OFF_HEAP(V8DBG_CLASS_SHAREDFUNCTIONINFO__SCRIPT__OBJECT)
#define V8_OFF_SHARED_FUNTOK  \
    V8_OFF_HEAP(V8DBG_CLASS_SHAREDFUNCTIONINFO__FUNCTION_TOKEN_POSITION__INT)
#define V8_OFF_SCRIPT_NAME  \
    V8_OFF_HEAP(V8DBG_CLASS_SCRIPT__NAME__OBJECT)
#define V8_OFF_SCRIPT_LENDS \
    V8_OFF_HEAP(V8DBG_CLASS_SCRIPT__LINE_ENDS__OBJECT)
#define V8_OFF_STR_LENGTH \
    V8_OFF_HEAP(V8DBG_CLASS_STRING__LENGTH__SMI)
#define V8_OFF_STR_CHARS  \
    V8_OFF_HEAP(V8DBG_CLASS_SEQONEBYTESTRING__CHARS__CHAR)
#define V8_OFF_CONSSTR_CAR  \
    V8_OFF_HEAP(V8DBG_CLASS_CONSSTRING__FIRST__STRING)
#define V8_OFF_CONSSTR_CDR  \
    V8_OFF_HEAP(V8DBG_CLASS_CONSSTRING__SECOND__STRING)
#define V8_OFF_EXTSTR_RSRC  \
    V8_OFF_HEAP(V8DBG_CLASS_EXTERNALSTRING__RESOURCE__OBJECT)
#define V8_OFF_FA_SIZE    \
    V8_OFF_HEAP(V8DBG_CLASS_FIXEDARRAYBASE__LENGTH__SMI)
#define V8_OFF_FA_DATA    \
    V8_OFF_HEAP(V8DBG_CLASS_FIXEDARRAY__DATA__UINTPTR_T)
#define V8_OFF_HEAPOBJ_MAP  \
    V8_OFF_HEAP(V8DBG_CLASS_HEAPOBJECT__MAP__MAP)
#define V8_OFF_MAP_ATTRS  \
    V8_OFF_HEAP(V8DBG_CLASS_MAP__INSTANCE_ATTRIBUTES__INT)
#define V8_OFF_TWOBYTESTR_CHARS  \
    V8_OFF_HEAP(V8DBG_CLASS_SEQTWOBYTESTRING__CHARS__CHAR)

#endif  /* SRC_V8ABBR_H_ */
