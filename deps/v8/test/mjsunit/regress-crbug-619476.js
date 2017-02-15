// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var x = {};
// Crashes in debug mode if an erroneous DCHECK in dfb8d333 is not removed.
eval, x[eval];
