// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var v = [];
Object.defineProperty(v, "length", {value: 3, writable: false});
assertThrows(()=>{v.pop();});
assertThrows(()=>{v.shift();});
