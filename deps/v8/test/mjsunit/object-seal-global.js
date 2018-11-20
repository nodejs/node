// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.seal(this);
assertTrue(Object.isSealed(this));
assertFalse(Object.isFrozen(this));
