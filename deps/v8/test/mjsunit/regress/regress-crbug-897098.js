// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const arr = [1.1,2.2,3.3];
arr.pop();
const start = {toString: function() {arr.pop();}}
arr.includes(0, start);
