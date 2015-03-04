// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// This file relies on the fact that the following declarations have been made
// in runtime.js:
// var $Function = global.Function;

// ----------------------------------------------------------------------------


// Generator functions and objects are specified by ES6, sections 15.19.3 and
// 15.19.4.

function GeneratorObjectNext(value) {
  if (!IS_GENERATOR(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['[Generator].prototype.next', this]);
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
    throw MakeTypeError('generator_running', []);
  }
}

function GeneratorObjectThrow(exn) {
  if (!IS_GENERATOR(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['[Generator].prototype.throw', this]);
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
    throw MakeTypeError('generator_running', []);
  }
}

function GeneratorObjectIterator() {
  return this;
}

function GeneratorFunctionPrototypeConstructor(x) {
  if (%_IsConstructCall()) {
    throw MakeTypeError('not_constructor', ['GeneratorFunctionPrototype']);
  }
}

function GeneratorFunctionConstructor(arg1) {  // length == 1
  return NewFunctionFromString(arguments, 'function*');
}


function SetUpGenerators() {
  %CheckIsBootstrapping();

  // Both Runtime_GeneratorNext and Runtime_GeneratorThrow are supported by
  // neither Crankshaft nor TurboFan, disable optimization of wrappers here.
  %NeverOptimizeFunction(GeneratorObjectNext);
  %NeverOptimizeFunction(GeneratorObjectThrow);

  // Set up non-enumerable functions on the generator prototype object.
  var GeneratorObjectPrototype = GeneratorFunctionPrototype.prototype;
  InstallFunctions(GeneratorObjectPrototype,
                   DONT_ENUM | DONT_DELETE | READ_ONLY,
                   ["next", GeneratorObjectNext,
                    "throw", GeneratorObjectThrow]);

  %FunctionSetName(GeneratorObjectIterator, '[Symbol.iterator]');
  %AddNamedProperty(GeneratorObjectPrototype, symbolIterator,
      GeneratorObjectIterator, DONT_ENUM | DONT_DELETE | READ_ONLY);
  %AddNamedProperty(GeneratorObjectPrototype, "constructor",
      GeneratorFunctionPrototype, DONT_ENUM | READ_ONLY);
  %AddNamedProperty(GeneratorObjectPrototype,
      symbolToStringTag, "Generator", DONT_ENUM | READ_ONLY);
  %InternalSetPrototype(GeneratorFunctionPrototype, $Function.prototype);
  %AddNamedProperty(GeneratorFunctionPrototype,
      symbolToStringTag, "GeneratorFunction", DONT_ENUM | READ_ONLY);
  %SetCode(GeneratorFunctionPrototype, GeneratorFunctionPrototypeConstructor);
  %AddNamedProperty(GeneratorFunctionPrototype, "constructor",
      GeneratorFunction, DONT_ENUM | READ_ONLY);
  %InternalSetPrototype(GeneratorFunction, $Function);
  %SetCode(GeneratorFunction, GeneratorFunctionConstructor);
}

SetUpGenerators();
