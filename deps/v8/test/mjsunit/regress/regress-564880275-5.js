// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle

// JS_BUNDLE_MODULE_ENTRYPOINT
await new Promise(r => setTimeout(r, 0));
d8.terminate();
(() => {})();
