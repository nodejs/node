// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

"use strict";

%CheckIsBootstrapping();

// ----------------------------------------------------------------------------
// Imports

var GlobalObject = global.Object;
var iteratorSymbol = utils.ImportNow("iterator_symbol");

// ----------------------------------------------------------------------------
// Object

// Set up non-enumerable functions on the Object.prototype object.
DEFINE_METHOD(
  GlobalObject.prototype,
  // ES6 19.1.3.5 Object.prototype.toLocaleString([reserved1 [,reserved2]])
  toLocaleString() {
    REQUIRE_OBJECT_COERCIBLE(this, "Object.prototype.toLocaleString");
    return this.toString();
  }
);

// ES6 7.3.9
function GetMethod(obj, p) {
  var func = obj[p];
  if (IS_NULL_OR_UNDEFINED(func)) return UNDEFINED;
  if (IS_CALLABLE(func)) return func;
  throw %make_type_error(kCalledNonCallable, typeof func);
}

// ----------------------------------------------------------------------------
// Iterator related spec functions.

// ES6 7.4.1 GetIterator(obj, method)
function GetIterator(obj, method) {
  if (IS_UNDEFINED(method)) {
    method = obj[iteratorSymbol];
  }
  if (!IS_CALLABLE(method)) {
    throw %make_type_error(kNotIterable, obj);
  }
  var iterator = %_Call(method, obj);
  if (!IS_RECEIVER(iterator)) {
    throw %make_type_error(kNotAnIterator, iterator);
  }
  return iterator;
}

// ----------------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.GetIterator = GetIterator;
  to.GetMethod = GetMethod;
});

})
