// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let table = new WebAssembly.Table({element: "anyfunc", initial: 1});
assertThrows(() => table.get(3612882876), RangeError);
