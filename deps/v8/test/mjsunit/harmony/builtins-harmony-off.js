// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Ensure that Builtins::GetJSBuiltinState() is up-to-date when none
// of the harmony features is enabled.
%VerifyGetJSBuiltinState(false);

// Make sure Temporal stuff is initialized if it's available.
globalThis.Temporal;
