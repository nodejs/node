// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Error.prepareStackTrace = function(e, frames) { return frames; }
assertThrows(() => new Error().stack[0].getMethodName.call({}), TypeError);

Error.prepareStackTrace = function(e, frames) { return frames.map(frame => new Proxy(frame, {})); }
assertThrows(() => new Error().stack[0].getMethodName(), TypeError);

Error.prepareStackTrace = function(e, frames) { return frames; }
assertEquals(null, new Error().stack[0].getMethodName());
