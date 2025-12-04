// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Number, BigInt and Intl.NumberFormat
assertThrows(
    "new Intl.NumberFormat('en', { style: 'unit', unit: 'son'});",
    RangeError,
    "Invalid unit argument for Intl.NumberFormat() 'son'");

assertThrows(
    "123n.toLocaleString('en', { style: 'unit', unit: 'son'});",
    RangeError,
    "Invalid unit argument for BigInt.prototype.toLocaleString() 'son'");

assertThrows(
    "Math.PI.toLocaleString('en', { style: 'unit', unit: 'son'});",
    RangeError,
    "Invalid unit argument for Number.prototype.toLocaleString() 'son'");

// String and Intl.Collator
assertThrows(
    "new Intl.Collator('en', { usage: 'mom'});",
    RangeError,
    "Value mom out of range for Intl.Collator options property usage");

assertThrows(
    "'abc'.localeCompare('efg', 'en', { usage: 'mom'});",
    RangeError,
    "Value mom out of range for String.prototype.localeCompare options property usage");

// Date and Intl.DateTimeFormat
assertThrows(
    "new Intl.DateTimeFormat('en', { hour: 'dad'});",
    RangeError,
    "Value dad out of range for Intl.DateTimeFormat options property hour");

assertThrows(
    "(new Date).toLocaleDateString('en', { hour: 'dad'});",
    RangeError,
    "Value dad out of range for Date.prototype.toLocaleDateString options property hour");

assertThrows(
    "(new Date).toLocaleString('en', { hour: 'dad'});",
    RangeError,
    "Value dad out of range for Date.prototype.toLocaleString options property hour");

assertThrows(
    "(new Date).toLocaleTimeString('en', { hour: 'dad'});",
    RangeError,
    "Value dad out of range for Date.prototype.toLocaleTimeString options property hour");
