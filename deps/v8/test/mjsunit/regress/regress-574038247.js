// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle

// JS_BUNDLE_MODULE:c1.mjs
export const x = 1;

// JS_BUNDLE_MODULE:c2.mjs
export const x = 2;

// JS_BUNDLE_MODULE:p.mjs
export * from "n.mjs";
export * from "c1.mjs";

// JS_BUNDLE_MODULE:n.mjs
export * from "p.mjs";

// JS_BUNDLE_MODULE:outer.mjs
export * from "m.mjs";
export * from "p.mjs";
export * from "c2.mjs";

// JS_BUNDLE_MODULE:m.mjs
const a = 1;
export { a, a as b, a as c, a as d, a as e };
export * from "outer.mjs";
export * from "n.mjs";

// JS_BUNDLE_MODULE:trigger.mjs
import { x } from "outer.mjs";

// JS_BUNDLE_MODULE:outer_no_ns.mjs
export * from "m_no_ns.mjs";
export * from "p.mjs";
export * from "c2.mjs";

// JS_BUNDLE_MODULE:m_no_ns.mjs
export const a = 1;
export * from "outer_no_ns.mjs";
export * from "n.mjs";

// JS_BUNDLE_MODULE:trigger_no_ns.mjs
import { x } from "outer_no_ns.mjs";

// JS_BUNDLE_MODULE:import_x_from_m_no_ns.mjs
import { x } from "m_no_ns.mjs";

// JS_BUNDLE_MODULE_ENTRYPOINT
import * as ns from "m.mjs";
import { a } from "m_no_ns.mjs";

assertEquals(1, a);

function read(o) {
  return o.a + o.b + o.c + o.d + o.e;
}

for (let i = 0; i < 12; i++) {
  assertEquals(5, read(ns));
}

await assertThrowsAsync(import("trigger.mjs"), SyntaxError);
assertEquals(5, read(ns));

await assertThrowsAsync(import("trigger_no_ns.mjs"), SyntaxError);
await assertThrowsAsync(import("import_x_from_m_no_ns.mjs"), SyntaxError);
