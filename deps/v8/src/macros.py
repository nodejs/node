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
# Used for defines that must be defined for all native js files.

const NONE        = 0;
const READ_ONLY   = 1;
const DONT_ENUM   = 2;
const DONT_DELETE = 4;

# Constants used for getter and setter operations.
const GETTER = 0;
const SETTER = 1;

# These definitions must match the index of the properties in objects.h.
const kApiTagOffset               = 0;
const kApiPropertyListOffset      = 1;
const kApiSerialNumberOffset      = 2;
const kApiConstructorOffset       = 2;
const kApiPrototypeTemplateOffset = 5;
const kApiParentTemplateOffset    = 6;

const NO_HINT     = 0;
const NUMBER_HINT = 1;
const STRING_HINT = 2;

const kFunctionTag  = 0;
const kNewObjectTag = 1;

# For date.js.
const HoursPerDay      = 24;
const MinutesPerHour   = 60;
const SecondsPerMinute = 60;
const msPerSecond      = 1000;
const msPerMinute      = 60000;
const msPerHour        = 3600000;
const msPerDay         = 86400000;
const msPerMonth       = 2592000000;

# For apinatives.js
const kUninitialized = -1;

# Note: kDayZeroInJulianDay = ToJulianDay(1970, 0, 1).
const kInvalidDate        = 'Invalid Date';
const kDayZeroInJulianDay = 2440588;
const kMonthMask          = 0x1e0;
const kDayMask            = 0x01f;
const kYearShift          = 9;
const kMonthShift         = 5;

# Limits for parts of the date, so that we support all the dates that
# ECMA 262 - 15.9.1.1 requires us to, but at the same time be sure that
# the date (days since 1970) is in SMI range.
const kMinYear  = -1000000;
const kMaxYear  = 1000000;
const kMinMonth = -10000000;
const kMaxMonth = 10000000;
const kMinDate  = -100000000;
const kMaxDate  = 100000000;

# Native cache ids.
const STRING_TO_REGEXP_CACHE_ID = 0;

# Type query macros.
#
# Note: We have special support for typeof(foo) === 'bar' in the compiler.
#       It will *not* generate a runtime typeof call for the most important
#       values of 'bar'.
macro IS_NULL(arg)              = (arg === null);
macro IS_NULL_OR_UNDEFINED(arg) = (arg == null);
macro IS_UNDEFINED(arg)         = (typeof(arg) === 'undefined');
macro IS_NUMBER(arg)            = (typeof(arg) === 'number');
macro IS_STRING(arg)            = (typeof(arg) === 'string');
macro IS_BOOLEAN(arg)           = (typeof(arg) === 'boolean');
macro IS_OBJECT(arg)            = (%_IsObject(arg));
macro IS_ARRAY(arg)             = (%_IsArray(arg));
macro IS_FUNCTION(arg)          = (%_IsFunction(arg));
macro IS_REGEXP(arg)            = (%_IsRegExp(arg));
macro IS_DATE(arg)              = (%_ClassOf(arg) === 'Date');
macro IS_NUMBER_WRAPPER(arg)    = (%_ClassOf(arg) === 'Number');
macro IS_STRING_WRAPPER(arg)    = (%_ClassOf(arg) === 'String');
macro IS_BOOLEAN_WRAPPER(arg)   = (%_ClassOf(arg) === 'Boolean');
macro IS_ERROR(arg)             = (%_ClassOf(arg) === 'Error');
macro IS_SCRIPT(arg)            = (%_ClassOf(arg) === 'Script');
macro IS_ARGUMENTS(arg)         = (%_ClassOf(arg) === 'Arguments');
macro IS_GLOBAL(arg)            = (%_ClassOf(arg) === 'global');
macro IS_UNDETECTABLE(arg)      = (%_IsUndetectableObject(arg));
macro FLOOR(arg)                = $floor(arg);

# Macro for ECMAScript 5 queries of the type:
# "Type(O) is object."
# This is the same as being either a function or an object in V8 terminology.
macro IS_SPEC_OBJECT_OR_NULL(arg) = (%_IsObject(arg) || %_IsFunction(arg));

# Inline macros. Use %IS_VAR to make sure arg is evaluated only once.
macro NUMBER_IS_NAN(arg) = (!%_IsSmi(%IS_VAR(arg)) && !(arg == arg));
macro TO_INTEGER(arg) = (%_IsSmi(%IS_VAR(arg)) ? arg : ToInteger(arg));
macro TO_INT32(arg) = (%_IsSmi(%IS_VAR(arg)) ? arg : (arg >> 0));
macro TO_UINT32(arg) = (arg >>> 0);
macro TO_STRING_INLINE(arg) = (IS_STRING(%IS_VAR(arg)) ? arg : NonStringToString(arg));


# Macros implemented in Python.
python macro CHAR_CODE(str) = ord(str[1]);

# Constants used on an array to implement the properties of the RegExp object.
const REGEXP_NUMBER_OF_CAPTURES = 0;
const REGEXP_FIRST_CAPTURE = 3;

# We can't put macros in macros so we use constants here.
# REGEXP_NUMBER_OF_CAPTURES
macro NUMBER_OF_CAPTURES(array) = ((array)[0]);

# Limit according to ECMA 262 15.9.1.1
const MAX_TIME_MS = 8640000000000000;

# Gets the value of a Date object. If arg is not a Date object
# a type error is thrown.
macro DATE_VALUE(arg) = (%_ClassOf(arg) === 'Date' ? %_ValueOf(arg) : ThrowDateTypeError());
macro DAY(time) = ($floor(time / 86400000));
macro MONTH_FROM_TIME(time) = (MonthFromTime(time));
macro DATE_FROM_TIME(time) = (DateFromTime(time));
macro YEAR_FROM_TIME(time) = (YearFromTime(time));
macro HOUR_FROM_TIME(time) = (Modulo($floor(time / 3600000), 24));
macro MIN_FROM_TIME(time) = (Modulo($floor(time / 60000), 60));
macro SEC_FROM_TIME(time) = (Modulo($floor(time / 1000), 60));
macro MS_FROM_TIME(time) = (Modulo(time, 1000));

# Last input and last subject of regexp matches.
macro LAST_SUBJECT(array) = ((array)[1]);
macro LAST_INPUT(array) = ((array)[2]);

# REGEXP_FIRST_CAPTURE
macro CAPTURE(index) = (3 + (index));
const CAPTURE0 = 3;
const CAPTURE1 = 4;
