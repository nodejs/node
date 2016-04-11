// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GeneratorFunctionPrototype = utils.ImportNow("GeneratorFunctionPrototype");
var GeneratorFunction = utils.ImportNow("GeneratorFunction");
var GlobalFunction = global.Function;
var MakeTypeError;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
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
    DEBUG_PREPARE_STEP_IN_IF_STEPPING(this);
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
    toStringTagSymbol, "Generator", DONT_ENUM | READ_ONLY);
%InternalSetPrototype(GeneratorFunctionPrototype, GlobalFunction.prototype);
%AddNamedProperty(GeneratorFunctionPrototype,
    toStringTagSymbol, "GeneratorFunction", DONT_ENUM | READ_ONLY);
%AddNamedProperty(GeneratorFunctionPrototype, "constructor",
    GeneratorFunction, DONT_ENUM | READ_ONLY);
%InternalSetPrototype(GeneratorFunction, GlobalFunction);

})
