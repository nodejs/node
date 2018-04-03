// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

%SetAllocationTimeout(1000, 90);
(new constructor)[0x40000000] = null;
