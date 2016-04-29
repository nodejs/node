// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-property --harmony-unicode-regexps

function t(re, s) { assertTrue(re.test(s)); }
function f(re, s) { assertFalse(re.test(s)); }

t(/\p{Common}+/u, ".");
f(/\p{Common}+/u, "supercalifragilisticexpialidocious");

t(/\p{Han}+/u, "话说天下大势，分久必合，合久必分");
t(/\p{Hani}+/u, "吾庄后有一桃园，花开正盛");
f(/\p{Han}+/u, "おはようございます");
f(/\p{Hani}+/u, "Something is rotten in the state of Denmark");

t(/\p{Latin}+/u, "Wie froh bin ich, daß ich weg bin!");
t(/\p{Latn}+/u,
  "It was a bright day in April, and the clocks were striking thirteen");
f(/\p{Latin}+/u, "奔腾千里荡尘埃，渡水登山紫雾开");
f(/\p{Latn}+/u, "いただきます");

t(/\p{Hiragana}/u, "いただきます");
t(/\p{Hira}/u, "ありがとうございました");
f(/\p{Hiragana}/u,
  "Als Gregor Samsa eines Morgens aus unruhigen Träumen erwachte");
f(/\p{Hira}/u, "Call me Ishmael");

t(/\p{Phoenician}/u, "\u{10900}\u{1091a}");
t(/\p{Phnx}/u, "\u{1091f}\u{10916}");
f(/\p{Phoenician}/u, "Arthur est un perroquet");
f(/\p{Phnx}/u, "设心狠毒非良士，操卓原来一路人");

t(/\p{Grek}/u, "ἄνδρα μοι ἔννεπε, μοῦσα, πολύτροπον, ὃς μάλα πολλὰ");
t(/\p{Greek}/u, "μῆνιν ἄειδε θεὰ Πηληϊάδεω Ἀχιλῆος");
f(/\p{Greek}/u, "高贤未服英雄志，屈节偏生杰士疑");
f(/\p{Greek}/u,
  "Mr. Jones, of the Manor Farm, had locked the hen-houses for the night");
