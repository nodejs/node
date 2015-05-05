// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var $Reflect = global.Reflect;

function SetUpReflect() {
  %CheckIsBootstrapping();

  InstallFunctions($Reflect, DONT_ENUM, $Array(
   "apply", ReflectApply,
   "construct", ReflectConstruct
  ));
}

SetUpReflect();
