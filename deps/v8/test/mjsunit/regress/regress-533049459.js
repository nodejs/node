// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

Number.prototype.valueOf;
delete Number.prototype.valueOf;
delete Object.prototype.valueOf;
Object.prototype.valueOf = 1.1;

assertEquals('5', JSON.stringify(new Number(5)));
