// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var arr = [];
arr[4] = "Item 4";
arr[8] = "Item 8";
var arr2 = [".", "!", "?"];
assertEquals([void 0, void 0, void 0, void 0, "Item 4", void 0, void 0,
              void 0, "Item 8", ".", "!", "?"], arr.concat(arr2));
