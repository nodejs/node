// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-staging

// Tests that when a block has exactly one error during disposal the message is
// correctly rethrown.
{
  using x = 1; // Not an object
}
