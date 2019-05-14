// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-slow-asserts

Error.prepareStackTrace = function(e, frames) { return frames; }
assertThrows(function() { new Error().stack[0].getMethodName.call({}); });
