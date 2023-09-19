// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("((__v_4 = __v_4, __v_0) => eval(__v_4))()", ReferenceError)
