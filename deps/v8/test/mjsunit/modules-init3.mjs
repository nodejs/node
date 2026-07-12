// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {check} from "modules-skip-init3.mjs";

assertSame(undefined, w);
assertThrows(() => x, ReferenceError);
assertThrows(() => y, ReferenceError);
assertThrows(() => z, ReferenceError);

assertEquals(23, w = 23);
assertThrows(() => x = 666, ReferenceError);
assertThrows(() => y = 666, ReferenceError);
assertThrows(() => z = 666, TypeError);

export function* v() { return 40 }
export var w = 41;
export let x = 42;
export class y {};
export const z = "hello world";

assertTrue(check());
