// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, shared, exports) {

'use strict';

%CheckIsBootstrapping();

var GlobalReflect = global.Reflect;

$installFunctions(GlobalReflect, DONT_ENUM, [
  "apply", $reflectApply,
  "construct", $reflectConstruct
]);

})
