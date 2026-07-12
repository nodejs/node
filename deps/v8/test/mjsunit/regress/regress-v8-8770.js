// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const re = /^.*?Y((?=X?).)*Y$/s;
const sult = "YABCY";
const result = re.exec(sult);

assertNotNull(result);
assertArrayEquals([sult, "C"], result);
