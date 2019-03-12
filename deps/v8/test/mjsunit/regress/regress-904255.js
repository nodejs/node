// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("((__v_0 = __v_0.replace(...new Array(), '0').slice(...new Int32Array(), '0')) => print())()", ReferenceError);
