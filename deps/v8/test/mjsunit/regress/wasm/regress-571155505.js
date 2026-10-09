// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-snapshot

let table = new WebAssembly.Table({element: 'structref', initial: 1});
