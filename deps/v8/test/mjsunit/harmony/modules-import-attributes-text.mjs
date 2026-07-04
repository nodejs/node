// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-import-text


import text from 'modules-skip-1.txt' with { type: 'text' };

assertEquals("Hello, import text!\n", text);
