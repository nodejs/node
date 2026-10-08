// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-runs=2

import('data:text/javascript,')
setTimeout(() => {});
d8.terminate();
(function () {})();
