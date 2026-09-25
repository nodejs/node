// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that even if all checks match, an error is still considered a failure.

// CHECK: hello
console.log("hello");
throw 123;
