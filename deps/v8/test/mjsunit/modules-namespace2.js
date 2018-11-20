// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MODULE

assertEquals(
    ["b", "c", "get_a", "ns2", "set_a", "zzz", Symbol.toStringTag],
    Reflect.ownKeys(ns));

import * as foo from "modules-skip-1.js";
assertSame(foo.a, ns.b);
assertSame(foo.a, ns.c);
assertSame(foo.get_a, ns.get_a);
assertSame(foo.set_a, ns.set_a);
assertEquals(123, ns.zzz);

assertSame(ns, ns.ns2.ns);
import * as ns from "modules-skip-namespace.js";
export {ns};
