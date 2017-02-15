// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-harmony-restrictive-declarations

// At some point, this code led to DCHECK errors in debug mode

for (; false;) function foo() {};

for (x in []) function foo() {};
