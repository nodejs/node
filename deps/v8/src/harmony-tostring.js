// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file relies on the fact that the following declaration has been made
// in runtime.js and symbol.js:
// var $Object = global.Object;
// var $Symbol = global.Symbol;

var symbolToStringTag = InternalSymbol("Symbol.toStringTag");

var kBuiltinStringTags = {
  "__proto__": null,
  "Arguments": true,
  "Array": true,
  "Boolean": true,
  "Date": true,
  "Error": true,
  "Function": true,
  "Number": true,
  "RegExp": true,
  "String": true
};

DefaultObjectToString = ObjectToStringHarmony;
// ES6 draft 08-24-14, section 19.1.3.6
function ObjectToStringHarmony() {
  if (IS_UNDEFINED(this) && !IS_UNDETECTABLE(this)) return "[object Undefined]";
  if (IS_NULL(this)) return "[object Null]";
  var O = ToObject(this);
  var builtinTag = %_ClassOf(O);
  var tag = O[symbolToStringTag];
  if (IS_UNDEFINED(tag)) {
    tag = builtinTag;
  } else if (!IS_STRING(tag)) {
    return "[object ???]"
  } else if (tag !== builtinTag && kBuiltinStringTags[tag]) {
    return "[object ~" + tag + "]";
  }
  return "[object " + tag + "]";
}

function HarmonyToStringExtendSymbolPrototype() {
  %CheckIsBootstrapping();

  InstallConstants($Symbol, $Array(
    // TODO(dslomov, caitp): Move to symbol.js when shipping
   "toStringTag", symbolToStringTag
  ));
}

HarmonyToStringExtendSymbolPrototype();

function HarmonyToStringExtendObjectPrototype() {
  %CheckIsBootstrapping();

  // Can't use InstallFunctions() because will fail in Debug mode.
  // Emulate InstallFunctions() here.
  %FunctionSetName(ObjectToStringHarmony, "toString");
  %FunctionRemovePrototype(ObjectToStringHarmony);
  %SetNativeFlag(ObjectToStringHarmony);

  // Set up the non-enumerable functions on the Array prototype object.
  var desc = ToPropertyDescriptor({
    value: ObjectToStringHarmony
  });
  DefineOwnProperty($Object.prototype, "toString", desc, false);

  %ToFastProperties($Object.prototype);
}

HarmonyToStringExtendObjectPrototype();
