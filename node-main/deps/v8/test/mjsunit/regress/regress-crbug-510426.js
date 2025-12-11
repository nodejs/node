// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var s = new String('a');
s[10000000] = 'bente';
assertEquals(['0', '10000000'], Object.keys(s));
