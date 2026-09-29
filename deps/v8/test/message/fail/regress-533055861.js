// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle

// JS_BUNDLE_MODULE:B.mjs
export * as ns2 from './A.mjs';

// JS_BUNDLE_MODULE:A.mjs
import { x } from './B.mjs';
export * as ns from './B.mjs';

// JS_BUNDLE_MODULE_ENTRYPOINT
import './A.mjs';
