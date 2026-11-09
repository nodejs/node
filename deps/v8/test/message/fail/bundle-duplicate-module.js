// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle

// JS_BUNDLE_MODULE:a.mjs
export const a = 1;

// JS_BUNDLE_MODULE:a.mjs
export const a = 2;

// JS_BUNDLE_MODULE_ENTRYPOINT
import {a} from 'a.mjs';
