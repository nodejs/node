// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --js-defer-import-eval --bundle

// Regression test for crbug.com/525234471: touching a deferred namespace whose
// dependency belongs to an async cycle (A <-> B, with top-level await) used to
// consider only the individual module's status when deciding whether the graph
// is ready for synchronous evaluation. B could already be kEvaluated while its
// cycle root A was still kEvaluatingAsync, so evaluating D synchronously
// produced a pending (not fulfilled) evaluation promise and crashed in
// JSDeferredModuleNamespace::EvaluateModuleSync[1]. The whole strongly-connected
// component must be settled before the deferred module is evaluated.
// [1] - https://github.com/tc39/proposal-defer-import-eval/issues/84

// JS_BUNDLE_MODULE:setup.mjs
export const p = Promise.withResolvers();

// promises used to signal start of evaluation for A
export const pA_start = Promise.withResolvers();

// JS_BUNDLE_MODULE:resolve-p.mjs
import {p} from "setup.mjs";

p.resolve();

// JS_BUNDLE_MODULE:A.mjs
import {p, pA_start} from "setup.mjs";
import "./B.mjs";

pA_start.resolve(); // It signals start of A's evaluation
globalThis.evaluationOrder.push("A-before-await");
await p;
globalThis.evaluationOrder.push("A-after-await");

// JS_BUNDLE_MODULE:B.mjs
import "./A.mjs";

globalThis.evaluationOrder.push("B");

// JS_BUNDLE_MODULE:D.mjs
import "./B.mjs";
globalThis.evaluationOrder.push("D");

// JS_BUNDLE_MODULE:C.mjs
import "Middle.mjs";
import "resolve-p.mjs"; // It signals that p is resolved to unblock Middle.mjs

// JS_BUNDLE_MODULE:Middle.mjs
import defer * as nsD from "./D.mjs";
// This should execute only after `D`'s async deps (i.e `A`) is evaluated.
globalThis.evaluationOrder.push("Middle-before-nsD.z");
nsD.z;
globalThis.evaluationOrder.push("Middle-after-nsD.z");

// JS_BUNDLE_MODULE_ENTRYPOINT:entry.mjs
import {pA_start} from "setup.mjs";
globalThis.evaluationOrder = [];

const pA = import("./A.mjs");
await pA_start.promise;
// This is to force that Middle.mjs will evaluate after A.
const pC = import("./C.mjs");
await Promise.all([pA, pC]);

// The async cycle {A, B} must be fully settled ("A-after-await") before
// nsD.z forces the synchronous evaluation of the deferred module D.
assertEquals([
  "B",
  "A-before-await",
  "A-after-await",
  "Middle-before-nsD.z",
  "D",
  "Middle-after-nsD.z",
], globalThis.evaluationOrder);
