// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --force-slow-path

let obj = [1, 2, 3];
obj[Symbol.isConcatSpreadable] = false;
assertEquals([obj], obj.concat());
