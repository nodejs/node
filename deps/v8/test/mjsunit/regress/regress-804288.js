// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var arr = [{}];
Object.setPrototypeOf(arr, {});
var ta = new Uint8Array(arr);

let kDeclNoLocals = 0;
