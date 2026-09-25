// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class A { constructor(o) { return o } }

class B extends A { #x() {} }

let o = new B({});
new B(o);
