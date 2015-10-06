// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -----------------------------------------------------------------------
// Utils

var imports = UNDEFINED;
var imports_from_experimental = UNDEFINED;
var exports_to_runtime = UNDEFINED;
var exports_container = {};

// Export to other scripts.
// In normal natives, this exports functions to other normal natives.
// In experimental natives, this exports to other experimental natives and
// to normal natives that import using utils.ImportFromExperimental.
function Export(f) {
  f(exports_container);
}


// Export to the native context for calls from the runtime.
function ExportToRuntime(f) {
  f.next = exports_to_runtime;
  exports_to_runtime = f;
}


// Import from other scripts. The actual importing happens in PostNatives and
// PostExperimental so that we can import from scripts executed later. However,
// that means that the import is not available until the very end. If the
// import needs to be available immediate, use ImportNow.
// In normal natives, this imports from other normal natives.
// In experimental natives, this imports from other experimental natives and
// whitelisted exports from normal natives.
function Import(f) {
  f.next = imports;
  imports = f;
}

// Import immediately from exports of previous scripts. We need this for
// functions called during bootstrapping. Hooking up imports in PostNatives
// would be too late.
function ImportNow(f) {
  f(exports_container);
}


// In normal natives, import from experimental natives.
// Not callable from experimental natives.
function ImportFromExperimental(f) {
  f.next = imports_from_experimental;
  imports_from_experimental = f;
}


function SetFunctionName(f, name, prefix) {
  if (IS_SYMBOL(name)) {
    name = "[" + %SymbolDescription(name) + "]";
  }
  if (IS_UNDEFINED(prefix)) {
    %FunctionSetName(f, name);
  } else {
    %FunctionSetName(f, prefix + " " + name);
  }
}


function InstallConstants(object, constants) {
  %CheckIsBootstrapping();
  %OptimizeObjectForAddingMultipleProperties(object, constants.length >> 1);
  var attributes = DONT_ENUM | DONT_DELETE | READ_ONLY;
  for (var i = 0; i < constants.length; i += 2) {
    var name = constants[i];
    var k = constants[i + 1];
    %AddNamedProperty(object, name, k, attributes);
  }
  %ToFastProperties(object);
}


function InstallFunctions(object, attributes, functions) {
  %CheckIsBootstrapping();
  %OptimizeObjectForAddingMultipleProperties(object, functions.length >> 1);
  for (var i = 0; i < functions.length; i += 2) {
    var key = functions[i];
    var f = functions[i + 1];
    SetFunctionName(f, key);
    %FunctionRemovePrototype(f);
    %AddNamedProperty(object, key, f, attributes);
    %SetNativeFlag(f);
  }
  %ToFastProperties(object);
}


// Helper function to install a getter-only accessor property.
function InstallGetter(object, name, getter, attributes) {
  %CheckIsBootstrapping();
  if (typeof attributes == "undefined") {
    attributes = DONT_ENUM;
  }
  SetFunctionName(getter, name, "get");
  %FunctionRemovePrototype(getter);
  %DefineAccessorPropertyUnchecked(object, name, getter, null, attributes);
  %SetNativeFlag(getter);
}


// Helper function to install a getter/setter accessor property.
function InstallGetterSetter(object, name, getter, setter) {
  %CheckIsBootstrapping();
  SetFunctionName(getter, name, "get");
  SetFunctionName(setter, name, "set");
  %FunctionRemovePrototype(getter);
  %FunctionRemovePrototype(setter);
  %DefineAccessorPropertyUnchecked(object, name, getter, setter, DONT_ENUM);
  %SetNativeFlag(getter);
  %SetNativeFlag(setter);
}


// Prevents changes to the prototype of a built-in function.
// The "prototype" property of the function object is made non-configurable,
// and the prototype object is made non-extensible. The latter prevents
// changing the __proto__ property.
function SetUpLockedPrototype(
    constructor, fields, methods) {
  %CheckIsBootstrapping();
  var prototype = constructor.prototype;
  // Install functions first, because this function is used to initialize
  // PropertyDescriptor itself.
  var property_count = (methods.length >> 1) + (fields ? fields.length : 0);
  if (property_count >= 4) {
    %OptimizeObjectForAddingMultipleProperties(prototype, property_count);
  }
  if (fields) {
    for (var i = 0; i < fields.length; i++) {
      %AddNamedProperty(prototype, fields[i],
                        UNDEFINED, DONT_ENUM | DONT_DELETE);
    }
  }
  for (var i = 0; i < methods.length; i += 2) {
    var key = methods[i];
    var f = methods[i + 1];
    %AddNamedProperty(prototype, key, f, DONT_ENUM | DONT_DELETE | READ_ONLY);
    %SetNativeFlag(f);
  }
  %InternalSetPrototype(prototype, null);
  %ToFastProperties(prototype);
}

// -----------------------------------------------------------------------
// To be called by bootstrapper

function PostNatives(utils) {
  %CheckIsBootstrapping();

  for ( ; !IS_UNDEFINED(imports); imports = imports.next) {
    imports(exports_container);
  }

  var runtime_container = {};
  for ( ; !IS_UNDEFINED(exports_to_runtime);
          exports_to_runtime = exports_to_runtime.next) {
    exports_to_runtime(runtime_container);
  }
  %ImportToRuntime(runtime_container);

  // Whitelist of exports from normal natives to experimental natives and debug.
  var expose_list = [
    "ArrayToString",
    "FunctionSourceString",
    "GetIterator",
    "GetMethod",
    "InnerArrayEvery",
    "InnerArrayFilter",
    "InnerArrayForEach",
    "InnerArrayIndexOf",
    "InnerArrayJoin",
    "InnerArrayLastIndexOf",
    "InnerArrayMap",
    "InnerArrayReduce",
    "InnerArrayReduceRight",
    "InnerArrayReverse",
    "InnerArraySome",
    "InnerArraySort",
    "InnerArrayToLocaleString",
    "IsNaN",
    "MathMax",
    "MathMin",
    "ObjectIsFrozen",
    "ObjectDefineProperty",
    "OwnPropertyKeys",
    "ToNameArray",
    "ToBoolean",
    "ToNumber",
    "ToString",
  ];

  var filtered_exports = {};
  %OptimizeObjectForAddingMultipleProperties(
      filtered_exports, expose_list.length);
  for (var key of expose_list) {
    filtered_exports[key] = exports_container[key];
  }
  %ToFastProperties(filtered_exports);
  exports_container = filtered_exports;

  utils.PostNatives = UNDEFINED;
  utils.ImportFromExperimental = UNDEFINED;
}


function PostExperimentals(utils) {
  %CheckIsBootstrapping();

  for ( ; !IS_UNDEFINED(imports); imports = imports.next) {
    imports(exports_container);
  }
  for ( ; !IS_UNDEFINED(imports_from_experimental);
          imports_from_experimental = imports_from_experimental.next) {
    imports_from_experimental(exports_container);
  }
  var runtime_container = {};
  for ( ; !IS_UNDEFINED(exports_to_runtime);
          exports_to_runtime = exports_to_runtime.next) {
    exports_to_runtime(runtime_container);
  }
  %ImportExperimentalToRuntime(runtime_container);

  exports_container = UNDEFINED;

  utils.PostExperimentals = UNDEFINED;
  utils.PostDebug = UNDEFINED;
  utils.Import = UNDEFINED;
  utils.Export = UNDEFINED;
}


function PostDebug(utils) {
  for ( ; !IS_UNDEFINED(imports); imports = imports.next) {
    imports(exports_container);
  }

  exports_container = UNDEFINED;

  utils.PostDebug = UNDEFINED;
  utils.PostExperimentals = UNDEFINED;
  utils.Import = UNDEFINED;
  utils.Export = UNDEFINED;
}

// -----------------------------------------------------------------------

%OptimizeObjectForAddingMultipleProperties(utils, 14);

utils.Import = Import;
utils.ImportNow = ImportNow;
utils.Export = Export;
utils.ExportToRuntime = ExportToRuntime;
utils.ImportFromExperimental = ImportFromExperimental;
utils.SetFunctionName = SetFunctionName;
utils.InstallConstants = InstallConstants;
utils.InstallFunctions = InstallFunctions;
utils.InstallGetter = InstallGetter;
utils.InstallGetterSetter = InstallGetterSetter;
utils.SetUpLockedPrototype = SetUpLockedPrototype;
utils.PostNatives = PostNatives;
utils.PostExperimentals = PostExperimentals;
utils.PostDebug = PostDebug;

%ToFastProperties(utils);

})
