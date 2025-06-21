// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Crash with --verify-heap
(async function() { for await (let { a = class b { } } of [{}]) { } })();
(async function() { var a; for await ({ a = class b { } } of [{}]) { } })();
