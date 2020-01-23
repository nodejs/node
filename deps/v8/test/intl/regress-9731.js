// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertFalse(/k/i.test('\u212A'));
assertTrue(/k/i.test('K'));
assertTrue(/k/i.test('k'));

assertFalse(/K/i.test('\u212A'));
assertTrue(/K/i.test('K'));
assertTrue(/K/i.test('k'));

assertTrue(/\u212A/i.test('\u212A'));
assertFalse(/\u212A/i.test('k'));
assertFalse(/\u212A/i.test('K'));
