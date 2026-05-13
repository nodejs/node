// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

// Regression test for crbug.com/498464176. Calling next() and throw() on an
// async generator synchronously, before any microtasks run, causes the kYield
// AsyncResumeTask dispatch to call AsyncGeneratorResumeNext which processes the
// queued throw and drains the queue. When the resulting promise rejection
// triggers a stack capture, TryGetCurrentTaskPromise must handle an empty
// generator queue without DCHECKing.

async function* gen() { yield; }
const g = gen();
g.next();
g.throw();
