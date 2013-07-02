// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

  return %_GeneratorNext(this, value);
}

function GeneratorObjectThrow(exn) {
  if (!IS_GENERATOR(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['[Generator].prototype.throw', this]);
  }

  return %_GeneratorThrow(this, exn);
}

function GeneratorFunctionPrototypeConstructor(x) {
  if (%_IsConstructCall()) {
    throw MakeTypeError('not_constructor', ['GeneratorFunctionPrototype']);
  }
}

function GeneratorFunctionConstructor(arg1) {  // length == 1
  var source = NewFunctionString(arguments, 'function*');
  var global_receiver = %GlobalReceiver(global);
  // Compile the string in the constructor and not a helper so that errors
  // appear to come from here.
  var f = %_CallFunction(global_receiver, %CompileString(source, true));
  %FunctionMarkNameShouldPrintAsAnonymous(f);
  return f;
}


function SetUpGenerators() {
  %CheckIsBootstrapping();
  var GeneratorObjectPrototype = GeneratorFunctionPrototype.prototype;
  InstallFunctions(GeneratorObjectPrototype,
                   DONT_ENUM | DONT_DELETE | READ_ONLY,
                   ["next", GeneratorObjectNext,
                    "throw", GeneratorObjectThrow]);
  %SetProperty(GeneratorObjectPrototype, "constructor",
               GeneratorFunctionPrototype, DONT_ENUM | DONT_DELETE | READ_ONLY);
  %SetPrototype(GeneratorFunctionPrototype, $Function.prototype);
  %SetCode(GeneratorFunctionPrototype, GeneratorFunctionPrototypeConstructor);
  %SetProperty(GeneratorFunctionPrototype, "constructor",
               GeneratorFunction, DONT_ENUM | DONT_DELETE | READ_ONLY);
  %SetPrototype(GeneratorFunction, $Function);
  %SetCode(GeneratorFunction, GeneratorFunctionConstructor);
}

SetUpGenerators();
