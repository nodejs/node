// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags:

var a = [5.65];
a.splice(0);
var b = a.splice(-4, 9, 10);
