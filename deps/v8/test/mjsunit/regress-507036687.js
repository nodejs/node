// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
    arguments.length = 0x40000000;
    Array.prototype.slice.call(arguments, arguments.length - 1);
})();
