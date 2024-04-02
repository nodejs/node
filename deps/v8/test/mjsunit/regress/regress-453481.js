// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --always-opt

var __v_0 = "";
var __v_1 = {};
var __v_2 = {};
var __v_3 = {};
var __v_4 = {};
var __v_5 = {};
var __v_6 = {};
var __v_7 = {};
var __v_8 = {};
var __v_10 = {};
var __v_13 = {};
var __v_15 = {};
var __v_16 = /abc/;
var __v_17 = {};
var __v_18 = function() {};
var __v_19 = this;
var __v_20 = {};
var __v_21 = this;

function __f_5(s) {
  return __f_11(__f_3(__f_7(s), s.length * 8));
}
function __f_3(x, len) {
  var __v_3 =  1732584193;
  var __v_6 = -271733879;
  var __v_5 = -1732584194;
  var __v_7 =  271733892;

  for (var i = 0; i < 1; i++) {
    var __v_11 = __v_3;
    var __v_14 = __v_6;
    var __v_13 = __v_5;
    var __v_15 = __v_7;

    __v_3 = __f_10(__v_3, __v_6, __v_5, __v_7, x[__v_8+ 0], 6 , -198630844);
    __v_7 = __f_10(__v_7, __v_3, __v_6, __v_5, x[__v_8+ 7], 10,  1126891415);
    __v_5 = __f_10(__v_5, __v_7, __v_3, __v_6, x[__v_8+14], 15, -1416354905);
    __v_6 = __f_10(__v_6, __v_5, __v_7, __v_3, x[__v_8+ 5], 21, -57434055);
    __v_3 = __f_10(__v_3, __v_6, __v_5, __v_7, x[__v_8+12], 6 ,  1700485571);
    __v_7 = __f_10(__v_7, __v_3, __v_6, __v_5, x[__v_8+ 3], 10, -1894986606);
    __v_5 = __f_10(__v_5, __v_7, __v_3, __v_6, x[__v_8+10], 15, -1051523);
    __v_6 = __f_10(__v_6, __v_5, __v_7, __v_3, x[__v_8+ 1], 21, -2054922799);
    __v_3 = __f_10(__v_3, __v_6, __v_5, __v_7, x[__v_8+ 8], 6 ,  1873313359);
    __v_7 = __f_10(__v_7, __v_3, __v_6, __v_5, x[__v_8+15], 10, -30611744);
    __v_5 = __f_10(__v_5, __v_7, __v_3, __v_6, x[__v_8+ 22], 14, -1560198371);
    __v_3 = __f_10(__v_3, __v_6, __v_5, __v_7, x[__v_8+ 4], 6 , -145523070);
    __v_7 = __f_10(__v_7, __v_3, __v_6, __v_5, x[__v_8+11], 10, -1120210379);
    __v_5 = __f_10(__v_5, __v_7, __v_3, __v_6, x[__v_8+ 2], 15,  718787259);
    __v_6 = __f_10(__v_13, __v_5, __v_7, __v_3, x[__v_8+ 9], 21, -343485551);
    __v_3 = __f_6(__v_3, __v_11);
    __v_6 = __f_6(__v_6, __v_14);
    __v_5 = __f_6(__v_5, __v_13);
    __v_7 = __f_6(__v_7, __v_15);

  }

  return Array(__v_3, __v_13, __v_4, __v_19);
}
function __f_4(q, __v_3, __v_6, x, s, t) {
  return __f_6(__f_12(__f_6(__f_6(__v_3, q), __f_6(x, t)), s),__v_6);
}
function __f_13(__v_3, __v_6, __v_5, __v_7, x, s, t) {
  return __f_4((__v_6 & __v_5) | ((~__v_6) & __v_7), __v_3, __v_6, x, s, t);
}
function __f_8(__v_3, __v_6, __v_5, __v_7, x, s, t) {
  return __f_4((__v_6 & __v_7) | (__v_5 & (~__v_7)), __v_3, __v_6, x, s, t);
}
function __f_9(__v_3, __v_6, __v_5, __v_7, x, s, t) {
  return __f_4(__v_6 ^ __v_5 ^ __v_7, __v_3, __v_6, x, s, t);
}
function __f_10(__v_3, __v_6, __v_5, __v_7, x, s, t) {
  return __f_4(__v_5 ^ (__v_6 | (~__v_7)), __v_3, __v_6, x, s, t);
}
function __f_6(x, y) {
  var __v_12 = (x & 0xFFFF) + (y & 0xFFFF);
  var __v_18 = (x >> 16) + (y >> 16) + (__v_12 >> 16);
  return (__v_18 << 16) | (__v_12 & 0xFFFF);
}
function __f_12(num, cnt) {
  return (num << cnt) | (num >>> (32 - cnt));
}
function __f_7(__v_16) {
  var __v_4 = Array();
  var __v_9 = (1 << 8) - 1;
  for(var __v_8 = 0; __v_8 < __v_16.length * 8; __v_8 += 8)
    __v_4[__v_8>>5] |= (__v_16.charCodeAt(__v_8 / 8) & __v_9) << (__v_8%32);
  return __v_4;
}

function __f_11(binarray) { return __v_16; }

try {
__v_10 = "Rebellious subjects, enemies to peace,\n\
Profaners of this neighbour-stained steel,--\n\
Will they not hear? What, ho! you men, you beasts,\n\
That quench the fire of your pernicious rage\n\
With purple fountains issuing from your veins,\n\
On pain of torture, from those bloody hands\n\
Throw your mistemper'__v_7 weapons to the ground,\n\
And hear the sentence of your moved prince.\n\
Three civil brawls, bred of an airy word,\n\
By thee, old Capulet, and Montague,\n\
Have thrice disturb'__v_7 the quiet of our streets,\n\
And made Verona's ancient citizens\n\
Cast by their grave beseeming ornaments,\n\
To wield old partisans, in hands as old,\n\
Canker'__v_7 with peace, to part your canker'__v_7 hate:\n\
If ever you disturb our streets again,\n\
Your lives shall pay the forfeit of the peace.\n\
For this time, all the rest depart away:\n\
You Capulet; shall go along with me:\n\
And, Montague, come you this afternoon,\n\
To know our further pleasure in this case,\n\
To old Free-town, our common judgment-place.\n\
Once more, on pain of death, all men depart.\n"
  function assertEquals(a, b) { }
for (var __v_8 = 0; __v_8 < 11; ++__v_8) {
  assertEquals(__f_5(__v_10), "1b8719c72d5d8bfd06e096ef6c6288c5");
}

} catch(e) { print("Caught: " + e); }
