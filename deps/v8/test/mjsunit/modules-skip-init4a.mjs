// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {foo, ns} from "./modules-init4.mjs"
assertEquals(foo(), 42)
assertEquals(ns.foo(), 42)
assertSame(foo, ns.foo)
