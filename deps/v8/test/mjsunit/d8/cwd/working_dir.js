// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Resources: test/mjsunit/d8/cwd/working_dir_helper.js.skip
// Resources: test/mjsunit/d8/cwd/working_dir_test.js.skip

// Flags: --quiet-load -C test/mjsunit/d8/cwd working_dir_test.js.skip

// When existing the current test file, the cwd has not been modified yet,
// since -C is passed later.

const d8_relative_test_path = 'test/mjsunit/d8/cwd/working_dir_test.js.skip';

assertEquals(globalThis.cwd_helper_loaded, undefined);
assertTrue(d8.file.exists(d8_relative_test_path));
assertThrows(() => load(d8_relative_test_path));
assertEquals(globalThis.cwd_helper_loaded, undefined);

// The proper relative file loading happens in working_dir_test.js.skip
// which is passed after -C as new CWD-relative path.
