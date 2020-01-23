// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertFalse(/ſ/i.test('ſ'.toUpperCase()));
assertFalse(/ſ/i.test('ſ'.toUpperCase()[0]));
assertTrue(/ſ/i.test('ſ'));
assertTrue(/ſ/i.test('ſ'[0]));
assertFalse(/ſ/i.test('s'.toUpperCase()));
assertFalse(/ſ/i.test('s'.toUpperCase()[0]));
assertFalse(/ſ/i.test('S'.toUpperCase()));
assertFalse(/ſ/i.test('S'.toUpperCase()[0]));
assertFalse(/ſ/i.test('S'));
assertFalse(/ſ/i.test('S'[0]));
