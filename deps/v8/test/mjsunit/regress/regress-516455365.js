// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Case 1: Negative lookbehind in Unicode Sets mode should only match at code point boundaries (index 0).
const lookbehind_matches = [... "😀😀😀".matchAll(/(?<!.)/gv)].map(m => m.index);
assertEquals([0], lookbehind_matches);

// Case 2: Negative lookahead in Unicode Sets mode should only match at the end of the string (index 6).
const lookahead_matches = [... "😀😀😀".matchAll(/(?!.)/gv)].map(m => m.index);
assertEquals([6], lookahead_matches);

// Case 3: Subject ending with a lone lead surrogate (verifying dynamic search advance bounds checks).
const subject_with_lone_lead = "A\uD800";
const lone_lead_matches = [... subject_with_lone_lead.matchAll(/(?<=[\s\S])/sug)].map(m => m.index);
assertEquals([1, 2], lone_lead_matches);
