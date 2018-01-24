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

# Type query macros.
#
# Note: We have special support for typeof(foo) === 'bar' in the compiler.
#       It will *not* generate a runtime typeof call for the most important
#       values of 'bar'.
macro IS_ARRAY(arg)             = (%_IsArray(arg));
macro IS_FUNCTION(arg)          = (%IsFunction(arg));
macro IS_NULL(arg)              = (arg === null);
macro IS_NULL_OR_UNDEFINED(arg) = (arg == null);
macro IS_NUMBER(arg)            = (typeof(arg) === 'number');
macro IS_STRING(arg)            = (typeof(arg) === 'string');
macro IS_SYMBOL(arg)            = (typeof(arg) === 'symbol');
macro IS_UNDEFINED(arg)         = (arg === (void 0));
macro IS_WEAKMAP(arg)           = (%_IsJSWeakMap(arg));
macro IS_WEAKSET(arg)           = (%_IsJSWeakSet(arg));

# Macro for ES queries of the type: "Type(O) is Object."
macro IS_RECEIVER(arg) = (%_IsJSReceiver(arg));

# Macro for ES queries of the type: "IsCallable(O)"
macro IS_CALLABLE(arg) = (typeof(arg) === 'function');

# Macro for ES RequireObjectCoercible
# https://tc39.github.io/ecma262/#sec-requireobjectcoercible
# Throws a TypeError of the form "[functionName] called on null or undefined".
macro REQUIRE_OBJECT_COERCIBLE(arg, functionName) = if (IS_NULL(%IS_VAR(arg)) || IS_UNDEFINED(arg)) throw %make_type_error(kCalledOnNullOrUndefined, functionName);

# Inline macros. Use %IS_VAR to make sure arg is evaluated only once.
macro TO_BOOLEAN(arg) = (!!(arg));
macro TO_INTEGER(arg) = (%_ToInteger(arg));
macro TO_LENGTH(arg) = (%_ToLength(arg));
macro TO_STRING(arg) = (%_ToString(arg));
macro TO_NUMBER(arg) = (%_ToNumber(arg));
macro TO_OBJECT(arg) = (%_ToObject(arg));
macro HAS_OWN_PROPERTY(obj, key) = (%_Call(ObjectHasOwnProperty, obj, key));

macro DEFINE_METHODS_LEN(obj, class_def, len) = %DefineMethodsInternal(obj, class class_def, len);
macro DEFINE_METHOD_LEN(obj, method_def, len) = %DefineMethodsInternal(obj, class { method_def }, len);
macro DEFINE_METHODS(obj, class_def) = DEFINE_METHODS_LEN(obj, class_def, -1);
macro DEFINE_METHOD(obj, method_def) = DEFINE_METHOD_LEN(obj, method_def, -1);

# Constants.  The compiler constant folds them.
define INFINITY = (1/0);
define UNDEFINED = (void 0);
