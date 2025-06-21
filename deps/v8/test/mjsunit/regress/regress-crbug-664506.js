// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --predictable

gc();
gc();
assertEquals("[object Object]", Object.prototype.toString.call({}));
gc();
assertEquals("[object Array]", Object.prototype.toString.call([]));
