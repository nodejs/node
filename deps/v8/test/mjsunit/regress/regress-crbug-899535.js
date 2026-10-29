// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let a = [1.1, 2.2, 3.3];
a.includes(4.4, { toString: () => a.length = 0 });
