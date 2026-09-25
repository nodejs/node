// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var arr = new Int8Array(new SharedArrayBuffer(16), 13);
new Uint8Array(arr);
