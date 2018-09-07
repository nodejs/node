// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("/\\p{Bidi_Class=L}+/u");
assertThrows("/\\p{bc=Left_To_Right}+/u");
assertThrows("/\\p{bc=AL}+/u");
assertThrows("/\\p{bc=Arabic_Letter}+/u");

assertThrows("/\\p{Line_Break=Glue}/u");
assertThrows("/\\p{lb=AL}/u");

assertThrows("/\\p{Block=}/u");
assertThrows("/\\p{=}/u");
assertThrows("/\\p{=L}/u");
assertThrows("/\\p{=Hiragana}/u");
assertThrows("/\\p{Block=CJK=}/u");

assertThrows("/\\p{Age=V8_0}/u");
assertDoesNotThrow("/\\p{General_Category=Letter}/u");
assertDoesNotThrow("/\\p{gc=L}/u");
assertThrows("/\\p{General_Category_Mask=Letter}/u");
assertThrows("/\\p{gcm=L}/u");
