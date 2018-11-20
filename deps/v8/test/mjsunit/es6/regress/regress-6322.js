// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Crash with --verify-heap
(function*() { for (let { a = class b { } } of [{}]) { } })().next();
(function() { for (let { a = class b { } } of [{}]) { } })();
(function() { var a; for ({ a = class b { } } of [{}]) { } })();

(function() { for (let [a = class b { } ] = [[]]; ;) break; })();
(function() { var a; for ([a = class b { } ] = [[]]; ;) break; })();
