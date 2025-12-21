// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --expose-gc --allow-natives-syntax --stress-compaction

gc();
let re = /\p{Ll}/iu;
re.test("\u{118D4}");
%SetAllocationTimeout(1, 1);
re.test("\u{118B4}");
