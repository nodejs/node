// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let a = { foo: 4 };
Object.seal(a);
assertTrue(Object.getOwnPropertyDescriptor(a, 'foo').writable);
Object.defineProperty(a, 'foo', { writable: false });
assertFalse(Object.getOwnPropertyDescriptor(a, 'foo').writable);
