// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var value = 0;

[{ set prop(v) { value = v } }.prop = 12 ] = [ 1 ]

assertEquals(1, value);
