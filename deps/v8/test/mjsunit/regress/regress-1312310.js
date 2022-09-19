// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --stress-wasm-code-gc --gc-interval=46 --cache=code --no-lazy

// No contents - just the flag combination above triggered the MSAN failure.
