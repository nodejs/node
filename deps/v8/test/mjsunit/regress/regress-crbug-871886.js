// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let arr = [1.5, 2.5];
arr.slice(0,
  { valueOf: function () {
      arr.length = 0;
      return 2;
    }
  });
