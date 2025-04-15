// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals([[2, 3], [0, 2]], /(?<=(ab|b|))c/d.exec('abc').indices);
assertEquals([[2, 3], [0, 2]], /(?<=(ba|a|))c/d.exec('bac').indices);
assertEquals([[2, 3], [0, 2]], /(?<=(ab|b))c/d.exec('abc').indices);
assertEquals([[2, 3], [0, 2]], /(?<=(ba|a))c/d.exec('bac').indices);
assertEquals(['g', 'abcdef'], /(?<=(abcdef|f|xy|xf|ay|))g/.exec('abcdefg'));
assertEquals(
    ['g', 'abcdef'], /(?<=(xy|xf|ay|abcdef|f|xy|xf|))g/.exec('abcdefg'));
assertEquals(['g', 'f'], /(?<=(f|xy|xf|ay|abcdef|))g/d.exec('abcdefg'));
assertEquals(['g', 'xf'], /(?<=(xy|xf|f|ay|abcdef|))g/d.exec('abcdexfg'));
