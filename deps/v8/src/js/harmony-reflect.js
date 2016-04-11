// Copyright 2013-2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

'use strict';

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalReflect = global.Reflect;
var MakeTypeError;
var ReflectApply = utils.ImportNow("reflect_apply");
var ReflectConstruct = utils.ImportNow("reflect_construct");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

function ReflectEnumerate(obj) {
  if (!IS_RECEIVER(obj))
    throw MakeTypeError(kCalledOnNonObject, "Reflect.enumerate")
  return (function* () { for (var x in obj) yield x })();
}

utils.InstallFunctions(GlobalReflect, DONT_ENUM, [
  "apply", ReflectApply,
  "construct", ReflectConstruct,
  "enumerate", ReflectEnumerate
]);

})
