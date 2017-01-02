# Copyright 2006-2009 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Dictionary that is passed as defines for js2c.py.
# Used for defines that must be defined for all native JS files.

define NONE        = 0;
define READ_ONLY   = 1;
define DONT_ENUM   = 2;
define DONT_DELETE = 4;

# 2^53 - 1
define kMaxSafeInteger = 9007199254740991;

# 2^32 - 1
define kMaxUint32 = 4294967295;

# Native cache ids.
define STRING_TO_REGEXP_CACHE_ID = 0;

# Type query macros.
#
# Note: We have special support for typeof(foo) === 'bar' in the compiler.
#       It will *not* generate a runtime typeof call for the most important
#       values of 'bar'.
macro IS_ARRAY(arg)             = (%_IsArray(arg));
macro IS_ARRAYBUFFER(arg)       = (%_ClassOf(arg) === 'ArrayBuffer');
macro IS_BOOLEAN(arg)           = (typeof(arg) === 'boolean');
macro IS_DATAVIEW(arg)          = (%_ClassOf(arg) === 'DataView');
macro IS_DATE(arg)              = (%IsDate(arg));
macro IS_ERROR(arg)             = (%_ClassOf(arg) === 'Error');
macro IS_FUNCTION(arg)          = (%IsFunction(arg));
macro IS_GENERATOR(arg)         = (%_ClassOf(arg) === 'Generator');
macro IS_GLOBAL(arg)            = (%_ClassOf(arg) === 'global');
macro IS_MAP(arg)               = (%_ClassOf(arg) === 'Map');
macro IS_MAP_ITERATOR(arg)      = (%_ClassOf(arg) === 'Map Iterator');
macro IS_NULL(arg)              = (arg === null);
macro IS_NULL_OR_UNDEFINED(arg) = (arg == null);
macro IS_NUMBER(arg)            = (typeof(arg) === 'number');
macro IS_OBJECT(arg)            = (typeof(arg) === 'object');
macro IS_PROXY(arg)             = (%_IsJSProxy(arg));
macro IS_REGEXP(arg)            = (%_IsRegExp(arg));
macro IS_SCRIPT(arg)            = (%_ClassOf(arg) === 'Script');
macro IS_SET(arg)               = (%_ClassOf(arg) === 'Set');
macro IS_SET_ITERATOR(arg)      = (%_ClassOf(arg) === 'Set Iterator');
macro IS_SHAREDARRAYBUFFER(arg) = (%_ClassOf(arg) === 'SharedArrayBuffer');
macro IS_SIMD_VALUE(arg)        = (%IsSimdValue(arg));
macro IS_STRING(arg)            = (typeof(arg) === 'string');
macro IS_SYMBOL(arg)            = (typeof(arg) === 'symbol');
macro IS_TYPEDARRAY(arg)        = (%_IsTypedArray(arg));
macro IS_UNDEFINED(arg)         = (arg === (void 0));
macro IS_WEAKMAP(arg)           = (%_ClassOf(arg) === 'WeakMap');
macro IS_WEAKSET(arg)           = (%_ClassOf(arg) === 'WeakSet');

# Macro for ES queries of the type: "Type(O) is Object."
macro IS_RECEIVER(arg) = (%_IsJSReceiver(arg));

# Macro for ES queries of the type: "IsCallable(O)"
macro IS_CALLABLE(arg) = (typeof(arg) === 'function');

# Macro for ES6 CheckObjectCoercible
# Will throw a TypeError of the form "[functionName] called on null or undefined".
macro CHECK_OBJECT_COERCIBLE(arg, functionName) = if (IS_NULL(%IS_VAR(arg)) || IS_UNDEFINED(arg)) throw %make_type_error(kCalledOnNullOrUndefined, functionName);

# Inline macros. Use %IS_VAR to make sure arg is evaluated only once.
macro NUMBER_IS_NAN(arg) = (!%_IsSmi(%IS_VAR(arg)) && !(arg == arg));
macro NUMBER_IS_FINITE(arg) = (%_IsSmi(%IS_VAR(arg)) || ((arg == arg) && (arg != 1/0) && (arg != -1/0)));
macro TO_BOOLEAN(arg) = (!!(arg));
macro TO_INTEGER(arg) = (%_ToInteger(arg));
macro TO_INT32(arg) = ((arg) | 0);
macro TO_UINT32(arg) = ((arg) >>> 0);
macro INVERT_NEG_ZERO(arg) = ((arg) + 0);
macro TO_LENGTH(arg) = (%_ToLength(arg));
macro TO_STRING(arg) = (%_ToString(arg));
macro TO_NUMBER(arg) = (%_ToNumber(arg));
macro TO_OBJECT(arg) = (%_ToObject(arg));
macro HAS_OWN_PROPERTY(obj, key) = (%_Call(ObjectHasOwnProperty, obj, key));

# Private names.
macro IS_PRIVATE(sym) = (%SymbolIsPrivate(sym));
macro HAS_PRIVATE(obj, key) = HAS_OWN_PROPERTY(obj, key);
macro HAS_DEFINED_PRIVATE(obj, sym) = (!IS_UNDEFINED(obj[sym]));
macro GET_PRIVATE(obj, sym) = (obj[sym]);
macro SET_PRIVATE(obj, sym, val) = (obj[sym] = val);

# To avoid ES2015 Function name inference.
macro ANONYMOUS_FUNCTION(fn) = (0, (fn));

# Constants.  The compiler constant folds them.
define INFINITY = (1/0);
define UNDEFINED = (void 0);

# Macros implemented in Python.
python macro CHAR_CODE(str) = ord(str[1]);

# Layout of internal RegExpLastMatchInfo object.
define REGEXP_NUMBER_OF_CAPTURES = 0;
define REGEXP_LAST_SUBJECT = 1;
define REGEXP_LAST_INPUT = 2;
define REGEXP_FIRST_CAPTURE = 3;
define CAPTURE0 = 3;  # Aliases REGEXP_FIRST_CAPTURE.
define CAPTURE1 = 4;

macro NUMBER_OF_CAPTURES(array) = ((array)[REGEXP_NUMBER_OF_CAPTURES]);
macro LAST_SUBJECT(array) = ((array)[REGEXP_LAST_SUBJECT]);
macro LAST_INPUT(array) = ((array)[REGEXP_LAST_INPUT]);
macro CAPTURE(index) = (REGEXP_FIRST_CAPTURE + (index));

# Macros for internal slot access.
macro REGEXP_GLOBAL(regexp) = (%_RegExpFlags(regexp) & 1);
macro REGEXP_IGNORE_CASE(regexp) = (%_RegExpFlags(regexp) & 2);
macro REGEXP_MULTILINE(regexp) = (%_RegExpFlags(regexp) & 4);
macro REGEXP_STICKY(regexp) = (%_RegExpFlags(regexp) & 8);
macro REGEXP_UNICODE(regexp) = (%_RegExpFlags(regexp) & 16);
macro REGEXP_SOURCE(regexp) = (%_RegExpSource(regexp));

# For the regexp capture override array.  This has the same
# format as the arguments to a function called from
# String.prototype.replace.
macro OVERRIDE_MATCH(override) = ((override)[0]);
macro OVERRIDE_POS(override) = ((override)[(override).length - 2]);
macro OVERRIDE_SUBJECT(override) = ((override)[(override).length - 1]);
# 1-based so index of 1 returns the first capture
macro OVERRIDE_CAPTURE(override, index) = ((override)[(index)]);

# For messages.js
# Matches Script::Type from objects.h
define TYPE_NATIVE = 0;
define TYPE_EXTENSION = 1;
define TYPE_NORMAL = 2;

# Matches Script::CompilationType from objects.h
define COMPILATION_TYPE_HOST = 0;
define COMPILATION_TYPE_EVAL = 1;
define COMPILATION_TYPE_JSON = 2;

# Must match PropertyFilter in property-details.h
define PROPERTY_FILTER_NONE = 0;
define PROPERTY_FILTER_ONLY_ENUMERABLE = 2;
define PROPERTY_FILTER_SKIP_STRINGS = 8;
define PROPERTY_FILTER_SKIP_SYMBOLS = 16;

# Use for keys, values and entries iterators.
define ITERATOR_KIND_KEYS = 1;
define ITERATOR_KIND_VALUES = 2;
define ITERATOR_KIND_ENTRIES = 3;

macro FIXED_ARRAY_GET(array, index) = (%_FixedArrayGet(array, (index) | 0));
macro FIXED_ARRAY_SET(array, index, value) = (%_FixedArraySet(array, (index) | 0, value));
# TODO(adamk): Find a more robust way to force Smi representation.
macro FIXED_ARRAY_SET_SMI(array, index, value) = (FIXED_ARRAY_SET(array, index, (value) | 0));

macro ORDERED_HASH_TABLE_BUCKET_COUNT(table) = (FIXED_ARRAY_GET(table, 0));
macro ORDERED_HASH_TABLE_ELEMENT_COUNT(table) = (FIXED_ARRAY_GET(table, 1));
macro ORDERED_HASH_TABLE_SET_ELEMENT_COUNT(table, count) = (FIXED_ARRAY_SET_SMI(table, 1, count));
macro ORDERED_HASH_TABLE_DELETED_COUNT(table) = (FIXED_ARRAY_GET(table, 2));
macro ORDERED_HASH_TABLE_SET_DELETED_COUNT(table, count) = (FIXED_ARRAY_SET_SMI(table, 2, count));
macro ORDERED_HASH_TABLE_BUCKET_AT(table, bucket) = (FIXED_ARRAY_GET(table, 3 + (bucket)));
macro ORDERED_HASH_TABLE_SET_BUCKET_AT(table, bucket, entry) = (FIXED_ARRAY_SET(table, 3 + (bucket), entry));

macro ORDERED_HASH_TABLE_HASH_TO_BUCKET(hash, numBuckets) = (hash & ((numBuckets) - 1));

macro ORDERED_HASH_SET_ENTRY_TO_INDEX(entry, numBuckets) = (3 + (numBuckets) + ((entry) << 1));
macro ORDERED_HASH_SET_KEY_AT(table, entry, numBuckets) = (FIXED_ARRAY_GET(table, ORDERED_HASH_SET_ENTRY_TO_INDEX(entry, numBuckets)));
macro ORDERED_HASH_SET_CHAIN_AT(table, entry, numBuckets) = (FIXED_ARRAY_GET(table, ORDERED_HASH_SET_ENTRY_TO_INDEX(entry, numBuckets) + 1));

macro ORDERED_HASH_MAP_ENTRY_TO_INDEX(entry, numBuckets) = (3 + (numBuckets) + ((entry) * 3));
macro ORDERED_HASH_MAP_KEY_AT(table, entry, numBuckets) = (FIXED_ARRAY_GET(table, ORDERED_HASH_MAP_ENTRY_TO_INDEX(entry, numBuckets)));
macro ORDERED_HASH_MAP_VALUE_AT(table, entry, numBuckets) = (FIXED_ARRAY_GET(table, ORDERED_HASH_MAP_ENTRY_TO_INDEX(entry, numBuckets) + 1));
macro ORDERED_HASH_MAP_CHAIN_AT(table, entry, numBuckets) = (FIXED_ARRAY_GET(table, ORDERED_HASH_MAP_ENTRY_TO_INDEX(entry, numBuckets) + 2));

# Must match OrderedHashTable::kNotFound.
define NOT_FOUND = -1;

# Check whether debug is active.
define DEBUG_IS_ACTIVE = (%_DebugIsActive() != 0);

# UseCounters from include/v8.h
define kUseAsm = 0;
define kBreakIterator = 1;
define kLegacyConst = 2;
define kMarkDequeOverflow = 3;
define kStoreBufferOverflow = 4;
define kSlotsBufferOverflow = 5;
define kForcedGC = 7;
define kSloppyMode = 8;
define kStrictMode = 9;
define kRegExpPrototypeStickyGetter = 11;
define kRegExpPrototypeToString = 12;
define kRegExpPrototypeUnicodeGetter = 13;
define kIntlV8Parse = 14;
define kIntlPattern = 15;
define kIntlResolved = 16;
define kPromiseChain = 17;
define kPromiseAccept = 18;
define kPromiseDefer = 19;
define kHtmlCommentInExternalScript = 20;
define kHtmlComment = 21;
define kSloppyModeBlockScopedFunctionRedefinition = 22;
define kForInInitializer = 23;
define kArrayProtectorDirtied = 24;
define kArraySpeciesModified = 25;
define kArrayPrototypeConstructorModified = 26;
define kArrayInstanceProtoModified = 27;
define kArrayInstanceConstructorModified = 28;
define kLegacyFunctionDeclaration = 29;
define kRegExpPrototypeSourceGetter = 30;
define kRegExpPrototypeOldFlagGetter = 31;
