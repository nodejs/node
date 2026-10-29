// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that an existing file returns true
assertTrue(d8.file.exists("test/mjsunit/d8/files/exists.js"));

// Test that a directory returns true (stat succeeds)
assertTrue(d8.file.exists("test/mjsunit/d8/files"));

// Test that a non-existing file returns false
assertFalse(d8.file.exists("test/mjsunit/d8/files/non_existent_file.js"));
