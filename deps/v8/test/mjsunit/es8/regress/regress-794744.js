// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Object.getOwnPropertyDescriptors loads %FunctionPrototype%.caller, an
// accessor property which inspects the current callstack. Verify that this
// callstack iteration doesn't crash when there are no JS frames on the stack.
Promise.resolve(function () {}).then(Object.getOwnPropertyDescriptors);
