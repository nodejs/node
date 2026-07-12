// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var number = 3500;
assertEquals(
    "3.500,00\u00a0KM",
    number.toLocaleString('hr-BA', { style: 'currency', currency: 'BAM'}));
