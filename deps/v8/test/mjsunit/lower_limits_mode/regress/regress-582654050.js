// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const kMax = %StringMaxLength();
const chunk = '"' + 'a'.repeat((kMax >> 4) - 5) + '"';
let expr = chunk;
for (let i = 1; i < 16; i++) expr += '-' + chunk;

// Normal non-overflowing CallPrinter output.
assertThrows(
    () => eval('(1||"a")()'), TypeError, /\(1 \|\| "a"\) is not a function/);

// RenderCallSite fallback on CallPrinter overflow (call and for-of iterator).
assertThrows(
    () => eval('(1||' + expr + ')()'), TypeError, /number 1 is not a function/);
assertThrows(
    () => eval('for (let x of (1||' + expr + ')) {}'), TypeError,
    /number 1 is not iterable/);

// ErrorUtils::ThrowSpreadArgError fallback on CallPrinter overflow.
assertThrows(
    () => eval('Math.max(...(null&&' + expr + '))'), TypeError,
    /object null is not iterable/);

// ErrorUtils::ThrowLoadFromNullOrUndefined fallback on CallPrinter overflow.
assertThrows(
    () => eval('const {a} = (null&&' + expr + ')'), TypeError,
    /Cannot destructure property 'a' of 'object null' as it is null\./);
