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

  if (DEBUG_IS_ACTIVE) %DebugPrepareStepInIfStepping(this);
  return %_GeneratorNext(this, value);
}

function GeneratorObjectThrow(exn) {
  if (!IS_GENERATOR(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['[Generator].prototype.throw', this]);
  }

  return %_GeneratorThrow(this, exn);
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
  var source = NewFunctionString(arguments, 'function*');
  var global_proxy = %GlobalProxy(global);
  // Compile the string in the constructor and not a helper so that errors
  // appear to come from here.
  var f = %_CallFunction(global_proxy, %CompileString(source, true));
  %FunctionMarkNameShouldPrintAsAnonymous(f);
  return f;
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
      GeneratorFunctionPrototype, DONT_ENUM | DONT_DELETE | READ_ONLY);
  %InternalSetPrototype(GeneratorFunctionPrototype, $Function.prototype);
  %SetCode(GeneratorFunctionPrototype, GeneratorFunctionPrototypeConstructor);
  %AddNamedProperty(GeneratorFunctionPrototype, "constructor",
      GeneratorFunction, DONT_ENUM | DONT_DELETE | READ_ONLY);
  %InternalSetPrototype(GeneratorFunction, $Function);
  %SetCode(GeneratorFunction, GeneratorFunctionConstructor);
}

SetUpGenerators();
