// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

PI = [];
PI[250] = PI;
Object.seal(PI);
assertTrue(Object.isSealed(PI));
var proxy = new Proxy(PI, PI);
Object.freeze(proxy);
assertTrue(Object.isFrozen(proxy));
