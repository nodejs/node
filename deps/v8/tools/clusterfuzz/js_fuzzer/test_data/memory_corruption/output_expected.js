// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: memory_corruption/input.js
foo();
bar();
let __v_0 = {};
foo();
corruptDataWithReplace(__v_0, [[0, 17748], [1, 315924689], [1, 4152517616], [0, 8357], [1, 1943503952], [1, 2091250053], [1, 929344739], [1, 2510428983]], 3261100141, 8, 2, 2n);
function __f_1() {
  foo();
  corruptDataWithBitflip(__v_0, [], 151377587, 32, 0, 14);
  const __v_1 = 0;
  const __v_2 = 1;
  const __v_3 = 2;
  const __v_4 = 3;
  foo();
  corruptWithWorker(__v_2, [[1, 1632394196], [1, 3531751247], [0, 36503], [1, 1231774720], [1, 3436876839], [1, 652243045], [1, 2347339401]], 1003422988, 8, 2, 0);
  bar();
  corruptDataWithReplace(__v_1, [], 778592082, 32, 0, 241n);
  corruptDataWithIncrement(__v_1, [[1, 3879217609], [0, 27232], [1, 4222070261]], 2980956946, 32, 0, 23n);
  baz();
  corruptDataWithBitflip(__v_1, [[1, 1065858838], [1, 2868749407], [1, 2050438824], [1, 1703359991], [1, 214440527], [1, 3367516418], [1, 2970962957], [1, 2860691989], [1, 1012339718]], 2142390489, 8, 0, 7);
  corruptDataWithReplace(__v_1, [[1, 188791006], [0, 52683], [1, 2636824918], [0, 31346], [1, 480358261], [0, 8620]], 4145043319, 8, 2, 5n);
  if (true) {
    bar();
    corruptWithWorker(__v_0, [[1, 3624234735], [0, 49848]], 1244535830, 32, 0, 31);
    corruptDataWithIncrement(__v_0, [[0, 32411], [1, 2540051489]], 3842582106, 16, 2, 2501n);
    baz();
  }
}
foo();
corruptDataWithIncrement(__v_0, [[1, 789869674], [1, 3661824984], [1, 1666356204], [1, 2297174851], [1, 219988030]], 2413034646, 16, 0, 2n);
corruptDataWithBitflip(__v_0, [[0, 13342], [0, 38074]], 4079036055, 32, 0, 5);
bar();
corruptDataWithReplace(__v_0, [[1, 1939389094], [1, 3783543282]], 3893436976, 16, 2, 175n);
baz();
corruptDataWithIncrement(__v_0, [], 3949905025, 16, 0, 4n);
