// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let descriptor;

descriptor = Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype,
                                             Symbol.toStringTag);

assertEquals("Object", descriptor.value);
assertFalse(descriptor.writable);
assertFalse(descriptor.enumerable);
assertTrue(descriptor.configurable);

descriptor = Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype,
                                             Symbol.toStringTag);

assertEquals("Object", descriptor.value);
assertFalse(descriptor.writable);
assertFalse(descriptor.enumerable);
assertTrue(descriptor.configurable);

descriptor = Object.getOwnPropertyDescriptor(Intl.Collator.prototype,
                                             Symbol.toStringTag);

assertEquals("Object", descriptor.value);
assertFalse(descriptor.writable);
assertFalse(descriptor.enumerable);
assertTrue(descriptor.configurable);
