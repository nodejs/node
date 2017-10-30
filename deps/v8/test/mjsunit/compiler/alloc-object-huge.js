// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax
// Flags: --max-inlined-bytecode-size=999999
// Flags: --max-inlined-bytecode-size-cumulative=999999

// Test that huge constructors (more than 256 this assignments) are
// handled correctly.

// Test huge constructor when being inlined into hydrogen.
function test() {
  return new huge();
}
test();
test();
%OptimizeFunctionOnNextCall(test);
var o = test();
assertEquals(1, o.foo1);
assertEquals(257, o.foo257);

// Test huge constructor with specialized constructor stub.
var o = new huge();
assertEquals(1, o.foo1);
assertEquals(257, o.foo257);

// The huge constructor, nothing interesting beyond this point.
function huge() {
  this.foo1 = 1;
  this.foo2 = 2;
  this.foo3 = 3;
  this.foo4 = 4;
  this.foo5 = 5;
  this.foo6 = 6;
  this.foo7 = 7;
  this.foo8 = 8;
  this.foo9 = 9;
  this.foo10 = 10;
  this.foo11 = 11;
  this.foo12 = 12;
  this.foo13 = 13;
  this.foo14 = 14;
  this.foo15 = 15;
  this.foo16 = 16;
  this.foo17 = 17;
  this.foo18 = 18;
  this.foo19 = 19;
  this.foo20 = 20;
  this.foo21 = 21;
  this.foo22 = 22;
  this.foo23 = 23;
  this.foo24 = 24;
  this.foo25 = 25;
  this.foo26 = 26;
  this.foo27 = 27;
  this.foo28 = 28;
  this.foo29 = 29;
  this.foo30 = 30;
  this.foo31 = 31;
  this.foo32 = 32;
  this.foo33 = 33;
  this.foo34 = 34;
  this.foo35 = 35;
  this.foo36 = 36;
  this.foo37 = 37;
  this.foo38 = 38;
  this.foo39 = 39;
  this.foo40 = 40;
  this.foo41 = 41;
  this.foo42 = 42;
  this.foo43 = 43;
  this.foo44 = 44;
  this.foo45 = 45;
  this.foo46 = 46;
  this.foo47 = 47;
  this.foo48 = 48;
  this.foo49 = 49;
  this.foo50 = 50;
  this.foo51 = 51;
  this.foo52 = 52;
  this.foo53 = 53;
  this.foo54 = 54;
  this.foo55 = 55;
  this.foo56 = 56;
  this.foo57 = 57;
  this.foo58 = 58;
  this.foo59 = 59;
  this.foo60 = 60;
  this.foo61 = 61;
  this.foo62 = 62;
  this.foo63 = 63;
  this.foo64 = 64;
  this.foo65 = 65;
  this.foo66 = 66;
  this.foo67 = 67;
  this.foo68 = 68;
  this.foo69 = 69;
  this.foo70 = 70;
  this.foo71 = 71;
  this.foo72 = 72;
  this.foo73 = 73;
  this.foo74 = 74;
  this.foo75 = 75;
  this.foo76 = 76;
  this.foo77 = 77;
  this.foo78 = 78;
  this.foo79 = 79;
  this.foo80 = 80;
  this.foo81 = 81;
  this.foo82 = 82;
  this.foo83 = 83;
  this.foo84 = 84;
  this.foo85 = 85;
  this.foo86 = 86;
  this.foo87 = 87;
  this.foo88 = 88;
  this.foo89 = 89;
  this.foo90 = 90;
  this.foo91 = 91;
  this.foo92 = 92;
  this.foo93 = 93;
  this.foo94 = 94;
  this.foo95 = 95;
  this.foo96 = 96;
  this.foo97 = 97;
  this.foo98 = 98;
  this.foo99 = 99;
  this.foo100 = 100;
  this.foo101 = 101;
  this.foo102 = 102;
  this.foo103 = 103;
  this.foo104 = 104;
  this.foo105 = 105;
  this.foo106 = 106;
  this.foo107 = 107;
  this.foo108 = 108;
  this.foo109 = 109;
  this.foo110 = 110;
  this.foo111 = 111;
  this.foo112 = 112;
  this.foo113 = 113;
  this.foo114 = 114;
  this.foo115 = 115;
  this.foo116 = 116;
  this.foo117 = 117;
  this.foo118 = 118;
  this.foo119 = 119;
  this.foo120 = 120;
  this.foo121 = 121;
  this.foo122 = 122;
  this.foo123 = 123;
  this.foo124 = 124;
  this.foo125 = 125;
  this.foo126 = 126;
  this.foo127 = 127;
  this.foo128 = 128;
  this.foo129 = 129;
  this.foo130 = 130;
  this.foo131 = 131;
  this.foo132 = 132;
  this.foo133 = 133;
  this.foo134 = 134;
  this.foo135 = 135;
  this.foo136 = 136;
  this.foo137 = 137;
  this.foo138 = 138;
  this.foo139 = 139;
  this.foo140 = 140;
  this.foo141 = 141;
  this.foo142 = 142;
  this.foo143 = 143;
  this.foo144 = 144;
  this.foo145 = 145;
  this.foo146 = 146;
  this.foo147 = 147;
  this.foo148 = 148;
  this.foo149 = 149;
  this.foo150 = 150;
  this.foo151 = 151;
  this.foo152 = 152;
  this.foo153 = 153;
  this.foo154 = 154;
  this.foo155 = 155;
  this.foo156 = 156;
  this.foo157 = 157;
  this.foo158 = 158;
  this.foo159 = 159;
  this.foo160 = 160;
  this.foo161 = 161;
  this.foo162 = 162;
  this.foo163 = 163;
  this.foo164 = 164;
  this.foo165 = 165;
  this.foo166 = 166;
  this.foo167 = 167;
  this.foo168 = 168;
  this.foo169 = 169;
  this.foo170 = 170;
  this.foo171 = 171;
  this.foo172 = 172;
  this.foo173 = 173;
  this.foo174 = 174;
  this.foo175 = 175;
  this.foo176 = 176;
  this.foo177 = 177;
  this.foo178 = 178;
  this.foo179 = 179;
  this.foo180 = 180;
  this.foo181 = 181;
  this.foo182 = 182;
  this.foo183 = 183;
  this.foo184 = 184;
  this.foo185 = 185;
  this.foo186 = 186;
  this.foo187 = 187;
  this.foo188 = 188;
  this.foo189 = 189;
  this.foo190 = 190;
  this.foo191 = 191;
  this.foo192 = 192;
  this.foo193 = 193;
  this.foo194 = 194;
  this.foo195 = 195;
  this.foo196 = 196;
  this.foo197 = 197;
  this.foo198 = 198;
  this.foo199 = 199;
  this.foo200 = 200;
  this.foo201 = 201;
  this.foo202 = 202;
  this.foo203 = 203;
  this.foo204 = 204;
  this.foo205 = 205;
  this.foo206 = 206;
  this.foo207 = 207;
  this.foo208 = 208;
  this.foo209 = 209;
  this.foo210 = 210;
  this.foo211 = 211;
  this.foo212 = 212;
  this.foo213 = 213;
  this.foo214 = 214;
  this.foo215 = 215;
  this.foo216 = 216;
  this.foo217 = 217;
  this.foo218 = 218;
  this.foo219 = 219;
  this.foo220 = 220;
  this.foo221 = 221;
  this.foo222 = 222;
  this.foo223 = 223;
  this.foo224 = 224;
  this.foo225 = 225;
  this.foo226 = 226;
  this.foo227 = 227;
  this.foo228 = 228;
  this.foo229 = 229;
  this.foo230 = 230;
  this.foo231 = 231;
  this.foo232 = 232;
  this.foo233 = 233;
  this.foo234 = 234;
  this.foo235 = 235;
  this.foo236 = 236;
  this.foo237 = 237;
  this.foo238 = 238;
  this.foo239 = 239;
  this.foo240 = 240;
  this.foo241 = 241;
  this.foo242 = 242;
  this.foo243 = 243;
  this.foo244 = 244;
  this.foo245 = 245;
  this.foo246 = 246;
  this.foo247 = 247;
  this.foo248 = 248;
  this.foo249 = 249;
  this.foo250 = 250;
  this.foo251 = 251;
  this.foo252 = 252;
  this.foo253 = 253;
  this.foo254 = 254;
  this.foo255 = 255;
  this.foo256 = 256;
  this.foo257 = 257;
}
