// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that star exports combined with circular indirect exports resolve
// correctly.  Module "a" star-exports from "b", "c", and "d".  Both "b" and
// "d" re-export {foo} from "a" (circular), while "c" provides the actual
// binding (export let foo = 1).  Resolution must find the binding in "c"
// without throwing on the cycles through "b" and "d".

import "modules-skip-star-exports-indirect-cycle-a.mjs";

import {foo} from "modules-skip-star-exports-indirect-cycle-a.mjs";
assertEquals(1, foo);
