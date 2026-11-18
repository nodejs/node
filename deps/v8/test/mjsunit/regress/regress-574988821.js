// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --dispatch-table-gc-interval=1 --trace-gc-object-stats

// Bootstrapping the isolate with these flags triggers a JSDispatchTable GC
// during JSFunction allocation before its dispatch_handle is initialized.
