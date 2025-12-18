// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var desc = Object.getOwnPropertyDescriptor(Array.prototype, 'concat');
assertEquals(false, desc.enumerable);
