// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var object = {};
var global = this;
var call = Function.call.bind(Function.call);

var globalSloppyArrow = () => this;
var globalStrictArrow = () => { "use strict"; return this; };
var globalSloppyArrowEval = (s) => eval(s);
var globalStrictArrowEval = (s) => { "use strict"; return eval(s); };

var sloppyFunctionArrow = function() {
  return (() => this)();
};
var strictFunctionArrow = function() {
  "use strict";
  return (() => this)();
};
var sloppyFunctionEvalArrow = function() {
  return eval("(() => this)()");
};
var strictFunctionEvalArrow = function() {
  "use strict";
  return eval("(() => this)()");
};
var sloppyFunctionArrowEval = function(s) {
  return (() => eval(s))();
};
var strictFunctionArrowEval = function(s) {
  "use strict";
  return (() => eval(s))();
};

var withObject = { 'this': object }
var arrowInsideWith, arrowInsideWithEval;
with (withObject) {
  arrowInsideWith = () => this;
  arrowInsideWithEval = (s) => eval(s);
}

assertEquals(global, call(globalSloppyArrow, object));
assertEquals(global, call(globalStrictArrow, object));
assertEquals(global, call(globalSloppyArrowEval, object, "this"));
assertEquals(global, call(globalStrictArrowEval, object, "this"));
assertEquals(global, call(globalSloppyArrowEval, object, "(() => this)()"));
assertEquals(global, call(globalStrictArrowEval, object, "(() => this)()"));

assertEquals(object, call(sloppyFunctionArrow, object));
assertEquals(global, call(sloppyFunctionArrow, undefined));
assertEquals(object, call(strictFunctionArrow, object));
assertEquals(undefined, call(strictFunctionArrow, undefined));

assertEquals(object, call(sloppyFunctionEvalArrow, object));
assertEquals(global, call(sloppyFunctionEvalArrow, undefined));
assertEquals(object, call(strictFunctionEvalArrow, object));
assertEquals(undefined, call(strictFunctionEvalArrow, undefined));

assertEquals(object, call(sloppyFunctionArrowEval, object, "this"));
assertEquals(global, call(sloppyFunctionArrowEval, undefined, "this"));
assertEquals(object, call(strictFunctionArrowEval, object, "this"));
assertEquals(undefined, call(strictFunctionArrowEval, undefined, "this"));

assertEquals(object,
             call(sloppyFunctionArrowEval, object, "(() => this)()"));
assertEquals(global,
             call(sloppyFunctionArrowEval, undefined, "(() => this)()"));
assertEquals(object,
             call(strictFunctionArrowEval, object, "(() => this)()"));
assertEquals(undefined,
             call(strictFunctionArrowEval, undefined, "(() => this)()"));

assertEquals(global, call(arrowInsideWith, undefined));
assertEquals(global, call(arrowInsideWith, object));
assertEquals(global, call(arrowInsideWithEval, undefined, "this"));
assertEquals(global, call(arrowInsideWithEval, object, "this"));
assertEquals(global, call(arrowInsideWithEval, undefined, "(() => this)()"));
assertEquals(global, call(arrowInsideWithEval, object, "(() => this)()"));
