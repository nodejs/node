// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var proto = {};
function constr() {}
constr.prototype = proto;
obj = new constr();
proto[Symbol.toStringTag] = "foo";
assertEquals('[object foo]', obj.toString());
