// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-property

function t(re, s) { assertTrue(re.test(s)); }
function f(re, s) { assertFalse(re.test(s)); }

t(/\p{Bidi_Class=L}+/u, "Is this the real life?");
t(/\p{bc=Left_To_Right}+/u, "Is this just fantasy?");
t(/\p{bc=AL}+/u, "السلام عليكم‎");
t(/\p{bc=Arabic_Letter}+/u, "متشرف بمعرفتك‎");

t(/\p{Line_Break=Glue}/u, "\u00A0");
t(/\p{lb=AL}/u, "~");

assertThrows("/\\p{Block=}/u");
assertThrows("/\\p{=}/u");
assertThrows("/\\p{=L}/u");
assertThrows("/\\p{=Hiragana}/u");
assertThrows("/\\p{Block=CJK=}/u");

assertThrows("/\\p{Age=V8_0}/u");
assertThrows("/\\p{General_Category=Letter}/u");
assertThrows("/\\p{gc=L}/u");
assertThrows("/\\p{General_Category_Mask=Letter}/u");
assertThrows("/\\p{gcm=L}/u");
