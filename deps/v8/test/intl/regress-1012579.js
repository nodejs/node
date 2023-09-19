// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verify won't crash in ListFormat
//            1    2    3    4    5    6    7    8    9
var list = [ 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i'];
const lf = new Intl.ListFormat();
const parts = lf.formatToParts(list);
