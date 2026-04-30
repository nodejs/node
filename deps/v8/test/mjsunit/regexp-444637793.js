// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-experimental-regexp-engine
// Flags: --experimental-regexp-engine-capture-group-opt

// Minimal reproduction of https://issues.chromium.org/issues/444637793.

assertEquals(
    ['xaey', 'xaey', 'ae', undefined, undefined],
    /(x(a(b(c)+d){0}e)y)/l.exec('xaey'));
assertEquals(
    ['xaey', 'xaey', 'ae', undefined, undefined, 'y'],
    /(x(a(b(c)+d){0}e)(y))/l.exec('xaey'));
assertEquals(
    ['xaeyyyyy', 'xaeyyyyy', 'ae', undefined, undefined, 'yyyyy'],
    /(x(a(b(c)+d){0}e)(y+))/l.exec('xaeyyyyy'));
