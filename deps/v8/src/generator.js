// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalFunction = global.Function;

var NewFunctionString;

utils.Import(function(from) {
  NewFunctionString = from.NewFunctionString;
});

// ----------------------------------------------------------------------------

// Generator functions and objects are specified by ES6, sections 15.19.3 and
// 15.19.4.

function GeneratorObjectNext(value) {
  if (!IS_GENERATOR(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        '[Generator].prototype.next', this);
  }

  var continuation = %GeneratorGetContinuation(this);
  if (continuation > 0) {
    // Generator is suspended.
    if (DEBUG_IS_ACTIVE) %DebugPrepareStepInIfStepping(this);
    try {
      return %_GeneratorNext(this, value);
    } catch (e) {
      %GeneratorClose(this);
      throw e;
    }
  } else if (continuation == 0) {
    // Generator is already closed.
    return { value: void 0, done: true };
  } else {
    // Generator is running.
    throw MakeTypeError(kGeneratorRunning);
  }
}


function GeneratorObjectThrow(exn) {
  if (!IS_GENERATOR(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        '[Generator].prototype.throw', this);
  }

  var continuation = %GeneratorGetContinuation(this);
  if (continuation > 0) {
    // Generator is suspended.
    try {
      return %_GeneratorThrow(this, exn);
    } catch (e) {
      %GeneratorClose(this);
      throw e;
    }
  } else if (continuation == 0) {
    // Generator is already closed.
    throw exn;
  } else {
    // Generator is running.
    throw MakeTypeError(kGeneratorRunning);
  }
}


function GeneratorFunctionConstructor(arg1) {  // length == 1
  var source = NewFunctionString(arguments, 'function*');
  var global_proxy = %GlobalProxy(GeneratorFunctionConstructor);
  // Compile the string in the constructor and not a helper so that errors
  // appear to come from here.
  var f = %_CallFunction(global_proxy, %CompileString(source, true));
  %FunctionMarkNameShouldPrintAsAnonymous(f);
  return f;
}

// ----------------------------------------------------------------------------

// Both Runtime_GeneratorNext and Runtime_GeneratorThrow are supported by
// neither Crankshaft nor TurboFan, disable optimization of wrappers here.
%NeverOptimizeFunction(GeneratorObjectNext);
%NeverOptimizeFunction(GeneratorObjectThrow);

// Set up non-enumerable functions on the generator prototype object.
var GeneratorObjectPrototype = GeneratorFunctionPrototype.prototype;
utils.InstallFunctions(GeneratorObjectPrototype,
                       DONT_ENUM,
                      ["next", GeneratorObjectNext,
                       "throw", GeneratorObjectThrow]);

%AddNamedProperty(GeneratorObjectPrototype, "constructor",
    GeneratorFunctionPrototype, DONT_ENUM | READ_ONLY);
%AddNamedProperty(GeneratorObjectPrototype,
    symbolToStringTag, "Generator", DONT_ENUM | READ_ONLY);
%InternalSetPrototype(GeneratorFunctionPrototype, GlobalFunction.prototype);
%AddNamedProperty(GeneratorFunctionPrototype,
    symbolToStringTag, "GeneratorFunction", DONT_ENUM | READ_ONLY);
%AddNamedProperty(GeneratorFunctionPrototype, "constructor",
    GeneratorFunction, DONT_ENUM | READ_ONLY);
%InternalSetPrototype(GeneratorFunction, GlobalFunction);
%SetCode(GeneratorFunction, GeneratorFunctionConstructor);

})
