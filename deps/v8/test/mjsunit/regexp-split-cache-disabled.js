// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --no-regexp-results-cache

// The split cache must be unobservable: with it disabled, every case in
// regexp-split-cache.js must still produce the same results and statics.

d8.file.execute('test/mjsunit/regexp-split-cache.js');
