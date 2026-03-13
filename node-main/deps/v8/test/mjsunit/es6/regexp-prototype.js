// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ES6 21.2.4.1
var proto_desc = Object.getOwnPropertyDescriptor(RegExp, "prototype");
assertFalse(proto_desc.writable);
assertFalse(proto_desc.enumerable);
assertFalse(proto_desc.configurable);

// ES6 21.2.5.1
var proto = proto_desc.value;
assertFalse(proto instanceof RegExp);
assertEquals(undefined, Object.getOwnPropertyDescriptor(proto, "valueOf"));
assertEquals(proto.valueOf, Object.prototype.valueOf);
var proto_constr = Object.getOwnPropertyDescriptor(proto, "constructor");
assertEquals(RegExp, proto_constr.value);
