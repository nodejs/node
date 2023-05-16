// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://code.google.com/p/v8/issues/detail?id=5213

assertEquals(0, Math.pow(2,-2147483648));
assertEquals(0, Math.pow(2,-9223372036854775808));
