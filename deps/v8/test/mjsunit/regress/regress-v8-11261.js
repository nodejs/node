// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let obj = ({ __proto__: { x: class {} }, y() { return new super.x; } });
obj.y();

class A { static a = class {} }
class B extends A { static b() { return new super.a(); } }
B.b();
