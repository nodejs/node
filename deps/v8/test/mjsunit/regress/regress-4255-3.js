// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation
// comments to trigger lazy compilation comments to trigger lazy compilation

'use strict';
for (let i in [1, 2, 3]) { function f() {} }
// Trigger OSR.
for (var i = 0; i < 1000000; i++);
