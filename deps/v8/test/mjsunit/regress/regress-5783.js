// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class C {}
class D extends C { constructor(...args) { super(...args, 75) } }
D.__proto__ = null;
assertThrows(() => new D(), TypeError);
