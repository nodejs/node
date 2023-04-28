// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let i = 0;
let re = /./g;
re.exec = () => {
  if (i++ == 0) return { length: 2 ** 16 };
  return null;
};

"".replace(re);
