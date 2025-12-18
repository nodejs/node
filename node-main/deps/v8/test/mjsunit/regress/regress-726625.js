// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function abc() { return; }
assertThrows("abc" + String.fromCharCode(65534) + "(1)");
