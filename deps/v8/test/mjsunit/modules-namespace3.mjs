// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as foo from "modules-namespace3.mjs";
export * from "modules-namespace3.mjs";
export var bar;
assertEquals(["bar", "default"], Object.getOwnPropertyNames(foo));
export default function() {};
