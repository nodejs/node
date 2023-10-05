// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Reflect.get(new Error(), "stack", false);
assertThrows(() => Reflect.get(new Error(), "stack", null), TypeError);
assertThrows(() => Reflect.get(new Error(), "stack", undefined), TypeError);
