// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

delete String.prototype.concat;

gc();
gc();
gc();
gc();

// This must crash and report an error that Builtin::kStringPrototypeConcat
// is never instantiated.
%VerifyGetJSBuiltinState(false);
