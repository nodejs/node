// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regexp tests covering features NOT well exercised by the JetStream2/3
// corpus (regexp-jetstream.js): u/v/y/s/d flags, lookbehind, named groups,
// Unicode property escapes, inline modifiers, 2-byte subjects, surrogate
// handling.
//
// Structure: CASES is a pure data table; one row per case. Each row uses
// a regexp literal `re` plus a `sub` subject string and a list of
// consecutive `exec()` calls. Compile-error cases (a small set) keep the
// pattern as `src` strings and assert that `new RegExp(src, flags)` throws.

(function() {

// ---- harness ----

function check(c) {
  if (c.compileError) {
    assertThrows(() => new RegExp(c.src, c.flags), SyntaxError,
                 undefined, `case ${c.n}: expected compile error`);
    return;
  }
  const re = c.re;
  for (let k = 0; k < c.calls.length; k++) {
    const call = c.calls[k];
    if (call.li_pre !== undefined) re.lastIndex = call.li_pre;
    const tag = `case ${c.n} call ${k}`;
    const res = re.exec(c.sub);
    if (call.exp === null) {
      assertNull(res, tag);
      assertEquals(call.li, re.lastIndex, `${tag} lastIndex`);
      continue;
    }
    assertNotNull(res, tag);
    assertEquals(call.exp.r, Array.from(res), `${tag} result`);
    assertEquals(call.exp.idx, res.index, `${tag} index`);
    assertEquals(call.li, re.lastIndex, `${tag} lastIndex`);
    if (call.exp.groups !== undefined) {
      assertEquals(call.exp.groups, {...res.groups}, `${tag} groups`);
    } else {
      assertEquals(undefined, res.groups, `${tag} groups`);
    }
    if (call.exp.indices !== undefined) {
      assertEquals(call.exp.indices, Array.from(res.indices),
                   `${tag} indices`);
      if (call.exp.indicesGroups !== undefined) {
        assertEquals(call.exp.indicesGroups, {...res.indices.groups},
                     `${tag} indices.groups`);
      } else {
        assertEquals(undefined, res.indices.groups, `${tag} indices.groups`);
      }
    } else {
      assertEquals(undefined, res.indices, `${tag} indices`);
    }
  }
}

const CASES = [

  // ---- u-basics ----
  {n:0,cat:"u-basics",re:/\u{1F600}/u,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:1,cat:"u-basics",re:/\u{61}/u,sub:"a",calls:[{exp:{r:["a"],idx:0},li:0}]},
  {n:2,cat:"u-basics",re:/\u{10FFFF}/u,sub:"\udbff\udfff",calls:[{exp:{r:["\udbff\udfff"],idx:0},li:0}]},
  {n:3,cat:"u-basics",re:/^.$/u,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:4,cat:"u-basics",re:/^.$/,sub:"\ud83d\ude00",calls:[{exp:null,li:0}]},
  {n:5,cat:"u-basics",re:/^.{2}$/,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:6,cat:"u-basics",re:/.+/u,sub:"a\ud83d\ude00b\ud83d\ude01c",calls:[{exp:{r:["a\ud83d\ude00b\ud83d\ude01c"],idx:0},li:0}]},
  {n:7,cat:"u-basics",re:/[\u{1F600}-\u{1F64F}]+/u,sub:"\ud83d\ude00\ud83d\ude02\ud83d\ude4fa",calls:[{exp:{r:["\ud83d\ude00\ud83d\ude02\ud83d\ude4f"],idx:0},li:0}]},
  {n:8,cat:"u-basics",re:/\ud83d\ude00/u,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:9,cat:"u-basics",re:/\ud83d\ude00/,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:10,cat:"u-basics",re:/^\u{1F600}$/u,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:11,cat:"u-basics",re:/a|\u{1F600}|b/u,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:12,cat:"u-basics",re:/a|\u{1F600}|b/u,sub:"b",calls:[{exp:{r:["b"],idx:0},li:0}]},
  {n:13,cat:"u-basics",re:/\d+/u,sub:"123abc",calls:[{exp:{r:["123"],idx:0},li:0}]},
  {n:14,cat:"u-basics",re:/[^a-z]+/u,sub:"ab\ud83d\ude00cd",calls:[{exp:{r:["\ud83d\ude00"],idx:2},li:0}]},

  // ---- u-icase ----
  {n:15,cat:"u-icase",re:/k/iu,sub:"\u212a",calls:[{exp:{r:["\u212a"],idx:0},li:0}]},
  {n:16,cat:"u-icase",re:/k/i,sub:"\u212a",calls:[{exp:null,li:0}]},
  {n:17,cat:"u-icase",re:/s/iu,sub:"\u017f",calls:[{exp:{r:["\u017f"],idx:0},li:0}]},
  {n:18,cat:"u-icase",re:/s/i,sub:"\u017f",calls:[{exp:null,li:0}]},
  {n:19,cat:"u-icase",re:/\u{03A3}/iu,sub:"\u03c2",calls:[{exp:{r:["\u03c2"],idx:0},li:0}]},
  {n:20,cat:"u-icase",re:/ss/iu,sub:"\xdf",calls:[{exp:null,li:0}]},

  // ---- v-sets ----
  {n:21,cat:"v-sets",re:/[\q{abc|def}]/v,sub:"abc",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:22,cat:"v-sets",re:/[\q{abc|def}]/v,sub:"def",calls:[{exp:{r:["def"],idx:0},li:0}]},
  {n:23,cat:"v-sets",re:/[\q{abc|def}]/v,sub:"xyz",calls:[{exp:null,li:0}]},
  {n:24,cat:"v-sets",re:/[\p{Letter}&&[a-z]]+/v,sub:"aB cD",calls:[{exp:{r:["a"],idx:0},li:0}]},
  {n:25,cat:"v-sets",re:/[\p{Letter}--[aeiouAEIOU]]+/v,sub:"Hello",calls:[{exp:{r:["H"],idx:0},li:0}]},
  {n:26,cat:"v-sets",re:/[[a-z]--[aeiou]]+/v,sub:"sky",calls:[{exp:{r:["sky"],idx:0},li:0}]},
  {n:27,cat:"v-sets",re:/[[a-z]&&[aeiou]]+/v,sub:"banana",calls:[{exp:{r:["a"],idx:1},li:0}]},
  {n:28,cat:"v-sets",re:/[^[a-z]&&[aeiou]]+/v,sub:"banana",calls:[{exp:{r:["b"],idx:0},li:0}]},
  {n:29,cat:"v-sets",re:/\p{RGI_Emoji_Flag_Sequence}/v,sub:"\ud83c\udde9\ud83c\uddea",calls:[{exp:{r:["\ud83c\udde9\ud83c\uddea"],idx:0},li:0}]},

  // ---- y-sticky ----
  {n:30,cat:"y-sticky",re:/foo/y,sub:"xfoo",calls:[{li_pre:0,exp:null,li:0}]},
  {n:31,cat:"y-sticky",re:/foo/y,sub:"xfoo",calls:[{li_pre:1,exp:{r:["foo"],idx:1},li:4}]},
  {n:32,cat:"y-sticky",re:/foo/y,sub:"xfoo",calls:[{li_pre:2,exp:null,li:0}]},
  {n:33,cat:"y-sticky",re:/./y,sub:"abc",calls:[{exp:{r:["a"],idx:0},li:1},{exp:{r:["b"],idx:1},li:2},{exp:{r:["c"],idx:2},li:3},{exp:null,li:0}]},
  {n:34,cat:"y-sticky",re:/\w/gy,sub:"a1!b2",calls:[{exp:{r:["a"],idx:0},li:1},{exp:{r:["1"],idx:1},li:2},{exp:null,li:0},{exp:{r:["a"],idx:0},li:1},{exp:{r:["1"],idx:1},li:2},{exp:null,li:0}]},
  {n:35,cat:"y-sticky",re:/(\d)(\d)/y,sub:"12",calls:[{li_pre:0,exp:{r:["12","1","2"],idx:0},li:2}]},

  // ---- s-dotall ----
  {n:36,cat:"s-dotall",re:/./s,sub:"\n",calls:[{exp:{r:["\n"],idx:0},li:0}]},
  {n:37,cat:"s-dotall",re:/./,sub:"\n",calls:[{exp:null,li:0}]},
  {n:38,cat:"s-dotall",re:/foo.bar/s,sub:"foo\nbar",calls:[{exp:{r:["foo\nbar"],idx:0},li:0}]},
  {n:39,cat:"s-dotall",re:/foo.bar/,sub:"foo\nbar",calls:[{exp:null,li:0}]},
  {n:40,cat:"s-dotall",re:/a.+b/s,sub:"a\n\nb",calls:[{exp:{r:["a\n\nb"],idx:0},li:0}]},
  {n:41,cat:"s-dotall",re:/^.+$/sm,sub:"line1\nline2",calls:[{exp:{r:["line1\nline2"],idx:0},li:0}]},
  {n:42,cat:"s-dotall",re:/.+/s,sub:"a\r\nb\u2028c\u2029d",calls:[{exp:{r:["a\r\nb\u2028c\u2029d"],idx:0},li:0}]},
  {n:43,cat:"s-dotall",re:/.+/,sub:"a\r\nb\u2028c\u2029d",calls:[{exp:{r:["a"],idx:0},li:0}]},

  // ---- m-multi ----
  {n:44,cat:"m-multi",re:/^foo/m,sub:"x\nfoo",calls:[{exp:{r:["foo"],idx:2},li:0}]},
  {n:45,cat:"m-multi",re:/foo$/m,sub:"foo\nx",calls:[{exp:{r:["foo"],idx:0},li:0}]},
  {n:46,cat:"m-multi",re:/^foo$/m,sub:"foo\nbar\nfoo",calls:[{exp:{r:["foo"],idx:0},li:0}]},
  {n:47,cat:"m-multi",re:/^\w+$/gm,sub:"a\nbb\nccc",calls:[{exp:{r:["a"],idx:0},li:1},{exp:{r:["bb"],idx:2},li:4},{exp:{r:["ccc"],idx:5},li:8},{exp:null,li:0}]},
  {n:48,cat:"m-multi",re:/^B/m,sub:"A\rB",calls:[{exp:{r:["B"],idx:2},li:0}]},
  {n:49,cat:"m-multi",re:/^B/m,sub:"A\u2028B",calls:[{exp:{r:["B"],idx:2},li:0}]},
  {n:50,cat:"m-multi",re:/^B/m,sub:"A\u2029B",calls:[{exp:{r:["B"],idx:2},li:0}]},
  {n:51,cat:"m-multi",re:/^.+$/m,sub:"foo\nbar",calls:[{exp:{r:["foo"],idx:0},li:0}]},

  // ---- d-indices ----
  {n:52,cat:"d-indices",re:/(\w+)\s(\w+)/d,sub:"hello world",calls:[{exp:{r:["hello world","hello","world"],idx:0,indices:[[0,11],[0,5],[6,11]]},li:0}]},
  {n:53,cat:"d-indices",re:/(?<year>\d{4})-(?<month>\d{2})/d,sub:"2026-05",calls:[{exp:{r:["2026-05","2026","05"],idx:0,groups:{"month":"05","year":"2026"},indices:[[0,7],[0,4],[5,7]],indicesGroups:{"month":[5,7],"year":[0,4]}},li:0}]},
  {n:54,cat:"d-indices",re:/(a)(b)?(c)/d,sub:"ac",calls:[{exp:{r:["ac","a",undefined,"c"],idx:0,indices:[[0,2],[0,1],undefined,[1,2]]},li:0}]},
  {n:55,cat:"d-indices",re:/(?<a>x)|(?<b>y)/d,sub:"y",calls:[{exp:{r:["y",undefined,"y"],idx:0,groups:{"a":undefined,"b":"y"},indices:[[0,1],undefined,[0,1]],indicesGroups:{"a":undefined,"b":[0,1]}},li:0}]},
  {n:56,cat:"d-indices",re:/\b(\w+)\b/dg,sub:"a bb ccc",calls:[{exp:{r:["a","a"],idx:0,indices:[[0,1],[0,1]]},li:1}]},

  // ---- lookbehind ----
  {n:57,cat:"lookbehind",re:/(?<=foo)bar/,sub:"foobar",calls:[{exp:{r:["bar"],idx:3},li:0}]},
  {n:58,cat:"lookbehind",re:/(?<=foo)bar/,sub:"xbar",calls:[{exp:null,li:0}]},
  {n:59,cat:"lookbehind",re:/(?<!foo)bar/,sub:"xbar",calls:[{exp:{r:["bar"],idx:1},li:0}]},
  {n:60,cat:"lookbehind",re:/(?<!foo)bar/,sub:"foobar",calls:[{exp:null,li:0}]},
  {n:61,cat:"lookbehind",re:/(?<=\d+)x/,sub:"123x",calls:[{exp:{r:["x"],idx:3},li:0}]},
  {n:62,cat:"lookbehind",re:/(?<=^abc)$/m,sub:"abc\nabc",calls:[{exp:{r:[""],idx:3},li:0}]},
  {n:63,cat:"lookbehind",re:/(?<=(\w))(\d)/,sub:"a1b2",calls:[{exp:{r:["1","a","1"],idx:1},li:0}]},
  {n:64,cat:"lookbehind",src:"(?<=\\U0001F600)x",flags:"u",compileError:true},
  {n:65,cat:"lookbehind",re:/(?<!\d)(\d{4})/,sub:"a 2026",calls:[{exp:{r:["2026","2026"],idx:2},li:0}]},
  {n:66,cat:"lookbehind",re:/(?<=a(?=b))b/,sub:"ab",calls:[{exp:{r:["b"],idx:1},li:0}]},

  // ---- lookahead ----
  {n:67,cat:"lookahead",re:/(?=(\w+))\1/,sub:"foobar",calls:[{exp:{r:["foobar","foobar"],idx:0},li:0}]},
  {n:68,cat:"lookahead",re:/(?=(\d+))(?=\w+)/,sub:"123abc",calls:[{exp:{r:["","123"],idx:0},li:0}]},

  // ---- named-groups ----
  {n:69,cat:"named-groups",re:/(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/,sub:"2026-05-29",calls:[{exp:{r:["2026-05-29","2026","05","29"],idx:0,groups:{"day":"29","month":"05","year":"2026"}},li:0}]},
  {n:70,cat:"named-groups",re:/(?<x>\w+)-\k<x>/,sub:"foo-foo",calls:[{exp:{r:["foo-foo","foo"],idx:0,groups:{"x":"foo"}},li:0}]},
  {n:71,cat:"named-groups",re:/(?<x>\w+)-\k<x>/,sub:"foo-bar",calls:[{exp:null,li:0}]},
  {n:72,cat:"named-groups",re:/(\d+)(?<name>\w+)/,sub:"42abc",calls:[{exp:{r:["42abc","42","abc"],idx:0,groups:{"name":"abc"}},li:0}]},
  {n:73,cat:"named-groups",re:/(?:(?<x>a)|(?<x>b))/,sub:"a",calls:[{exp:{r:["a","a",undefined],idx:0,groups:{"x":"a"}},li:0}]},
  {n:74,cat:"named-groups",re:/(?:(?<x>a)|(?<x>b))/,sub:"b",calls:[{exp:{r:["b",undefined,"b"],idx:0,groups:{"x":"b"}},li:0}]},
  {n:75,cat:"named-groups",re:/(?:(?<x>\d+)-(?<y>\d+))|(?:(?<y>\d+)\.(?<x>\d+))/,sub:"12-34",calls:[{exp:{r:["12-34","12","34",undefined,undefined],idx:0,groups:{"x":"12","y":"34"}},li:0}]},

  // ---- unicode-props ----
  {n:76,cat:"unicode-props",re:/\p{L}+/u,sub:"Hello \u043c\u0438\u0440 \u6f22\u5b57",calls:[{exp:{r:["Hello"],idx:0},li:0}]},
  {n:77,cat:"unicode-props",re:/\p{Letter}+/u,sub:"Hello world!",calls:[{exp:{r:["Hello"],idx:0},li:0}]},
  {n:78,cat:"unicode-props",re:/\p{N}+/u,sub:"A \u0660 \u4e94",calls:[{exp:{r:["\u0660"],idx:2},li:0}]},
  {n:79,cat:"unicode-props",re:/\P{ASCII}+/u,sub:"a\xe9\u4e2db",calls:[{exp:{r:["\xe9\u4e2d"],idx:1},li:0}]},
  {n:80,cat:"unicode-props",re:/\p{Script=Greek}+/u,sub:"\u03b1\u03b2\u03b3ABC\u0391",calls:[{exp:{r:["\u03b1\u03b2\u03b3"],idx:0},li:0}]},
  {n:81,cat:"unicode-props",re:/\p{Script=Latin}+/u,sub:"\u03b1ABC\u4e2d",calls:[{exp:{r:["ABC"],idx:1},li:0}]},
  {n:82,cat:"unicode-props",re:/\p{General_Category=Lowercase_Letter}+/u,sub:"AaBbCc",calls:[{exp:{r:["a"],idx:1},li:0}]},
  {n:83,cat:"unicode-props",re:/\p{Emoji}/u,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:84,cat:"unicode-props",re:/\p{ASCII_Hex_Digit}+/u,sub:"0xDEADf00",calls:[{exp:{r:["0"],idx:0},li:0}]},

  // ---- backrefs ----
  {n:85,cat:"backrefs",re:/(\w+)\s+\1/,sub:"the the",calls:[{exp:{r:["the the","the"],idx:0},li:0}]},
  {n:86,cat:"backrefs",re:/(.)(.)\2\1/,sub:"abba",calls:[{exp:{r:["abba","a","b"],idx:0},li:0}]},
  {n:87,cat:"backrefs",re:/(\w+)-(\w+)-\1-\2/,sub:"foo-bar-foo-bar",calls:[{exp:{r:["foo-bar-foo-bar","foo","bar"],idx:0},li:0}]},
  {n:88,cat:"backrefs",re:/(\u{1F600})\1/u,sub:"\ud83d\ude00\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00\ud83d\ude00","\ud83d\ude00"],idx:0},li:0}]},
  {n:89,cat:"backrefs",re:/(?:(a)|b)\1?/,sub:"b",calls:[{exp:{r:["b",undefined],idx:0},li:0}]},

  // ---- anchors ----
  {n:90,cat:"anchors",re:/\b\w+\b/,sub:"  word  ",calls:[{exp:{r:["word"],idx:2},li:0}]},
  {n:91,cat:"anchors",re:/\B\w+\B/,sub:"aXbXc",calls:[{exp:{r:["XbX"],idx:1},li:0}]},
  {n:92,cat:"anchors",re:/\Bx\B/,sub:"axb",calls:[{exp:{r:["x"],idx:1},li:0}]},
  {n:93,cat:"anchors",re:/\bword\b/,sub:"sword",calls:[{exp:null,li:0}]},
  {n:94,cat:"anchors",re:/\w+\b/,sub:"hello world",calls:[{exp:{r:["hello"],idx:0},li:0}]},
  {n:95,cat:"anchors",re:/\b\w+\b/g,sub:"foo bar baz",calls:[{exp:{r:["foo"],idx:0},li:3},{exp:{r:["bar"],idx:4},li:7},{exp:{r:["baz"],idx:8},li:11},{exp:null,li:0}]},
  {n:96,cat:"anchors",re:/^bar/,sub:"foo\nbar",calls:[{exp:null,li:0}]},
  {n:97,cat:"anchors",re:/^bar/m,sub:"foo\nbar",calls:[{exp:{r:["bar"],idx:4},li:0}]},

  // ---- inline-mod ----
  {n:98,cat:"inline-mod",re:/(?i:abc)def/,sub:"ABCdef",calls:[{exp:{r:["ABCdef"],idx:0},li:0}]},
  {n:99,cat:"inline-mod",re:/(?i:abc)def/,sub:"ABCDEF",calls:[{exp:null,li:0}]},
  {n:100,cat:"inline-mod",re:/(?-i:abc)/i,sub:"ABC",calls:[{exp:null,li:0}]},
  {n:101,cat:"inline-mod",re:/(?-i:abc)/i,sub:"abc",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:102,cat:"inline-mod",re:/(?ims:^abc.def)/,sub:"X\nabc\ndef",calls:[{exp:{r:["abc\ndef"],idx:2},li:0}]},
  {n:103,cat:"inline-mod",re:/X(?i:Y)Z/,sub:"XyZ",calls:[{exp:{r:["XyZ"],idx:0},li:0}]},
  {n:104,cat:"inline-mod",re:/X(?i:Y)Z/,sub:"XYz",calls:[{exp:null,li:0}]},

  // ---- two-byte ----
  {n:105,cat:"two-byte",re:/[a-z]+/,sub:"caf\xe9",calls:[{exp:{r:["caf"],idx:0},li:0}]},
  {n:106,cat:"two-byte",re:/\w+/u,sub:"caf\xe9",calls:[{exp:{r:["caf"],idx:0},li:0}]},
  {n:107,cat:"two-byte",re:/\p{Letter}+/u,sub:"caf\xe9",calls:[{exp:{r:["caf\xe9"],idx:0},li:0}]},
  {n:108,cat:"two-byte",re:/\u00e9/,sub:"caf\xe9",calls:[{exp:{r:["\xe9"],idx:3},li:0}]},
  {n:109,cat:"two-byte",re:/\u00e9/u,sub:"caf\xe9",calls:[{exp:{r:["\xe9"],idx:3},li:0}]},
  {n:110,cat:"two-byte",re:/\u6f22\u5b57/,sub:"a\u6f22\u5b57b",calls:[{exp:{r:["\u6f22\u5b57"],idx:1},li:0}]},
  {n:111,cat:"two-byte",re:/(?<=\u041c)\u0438/u,sub:"\u041c\u0438\u0440",calls:[{exp:{r:["\u0438"],idx:1},li:0}]},
  {n:112,cat:"two-byte",re:/^(.)(.)(.)$/u,sub:"a\ud83d\ude00b",calls:[{exp:{r:["a\ud83d\ude00b","a","\ud83d\ude00","b"],idx:0},li:0}]},
  {n:113,cat:"two-byte",src:"(\\U0001F600+)",flags:"u",compileError:true},
  {n:114,cat:"two-byte",re:/^(.)(.)$/,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00","\ud83d","\ude00"],idx:0},li:0}]},
  {n:115,cat:"two-byte",re:/[^a]+/u,sub:"a\ud83d\ude00b",calls:[{exp:{r:["\ud83d\ude00b"],idx:1},li:0}]},

  // ---- quantifiers ----
  {n:116,cat:"quantifiers",re:/a{0,0}/,sub:"aaa",calls:[{exp:{r:[""],idx:0},li:0}]},
  {n:117,cat:"quantifiers",re:/a{2,3}/,sub:"aaaa",calls:[{exp:{r:["aaa"],idx:0},li:0}]},
  {n:118,cat:"quantifiers",re:/a{0,}/,sub:"aaab",calls:[{exp:{r:["aaa"],idx:0},li:0}]},
  {n:119,cat:"quantifiers",re:/a+?b/,sub:"aaab",calls:[{exp:{r:["aaab"],idx:0},li:0}]},
  {n:120,cat:"quantifiers",re:/a*?b/,sub:"b",calls:[{exp:{r:["b"],idx:0},li:0}]},
  {n:121,cat:"quantifiers",re:/(?:ab){2,4}/,sub:"abababab",calls:[{exp:{r:["abababab"],idx:0},li:0}]},
  {n:122,cat:"quantifiers",re:/a?/g,sub:"aa",calls:[{exp:{r:["a"],idx:0},li:1},{exp:{r:["a"],idx:1},li:2},{exp:{r:[""],idx:2},li:2},{exp:{r:[""],idx:2},li:2}]},
  {n:123,cat:"quantifiers",re:/(?:)/gu,sub:"\ud83d\ude00a",calls:[{exp:{r:[""],idx:0},li:0},{exp:{r:[""],idx:0},li:0},{exp:{r:[""],idx:0},li:0},{exp:{r:[""],idx:0},li:0}]},

  // ---- combo ----
  {n:124,cat:"combo",re:/^(?<w>\w+)$/dgmu,sub:"foo\nbar",calls:[{exp:{r:["foo","foo"],idx:0,groups:{"w":"foo"},indices:[[0,3],[0,3]],indicesGroups:{"w":[0,3]}},li:3}]},
  {n:125,cat:"combo",re:/^(?<w>\w+)$/dgmu,sub:"foo\nbar",calls:[{exp:{r:["foo","foo"],idx:0,groups:{"w":"foo"},indices:[[0,3],[0,3]],indicesGroups:{"w":[0,3]}},li:3},{exp:{r:["bar","bar"],idx:4,groups:{"w":"bar"},indices:[[4,7],[4,7]],indicesGroups:{"w":[4,7]}},li:7},{exp:null,li:0}]},
  {n:126,cat:"combo",src:"(?i)abc",flags:"",compileError:true},
  {n:127,cat:"combo",src:"a(?i)b(?-i)c",flags:"",compileError:true},
  {n:128,cat:"combo",src:"a(?i)b(?-i)c",flags:"",compileError:true},

  // ---- misc ----
  {n:129,cat:"misc",re:/^.{0,3}/,sub:"abcdef",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:130,cat:"misc",re:/^.{0,3}?/,sub:"abcdef",calls:[{exp:{r:[""],idx:0},li:0}]},
  {n:131,cat:"misc",re:/a|ab/,sub:"ab",calls:[{exp:{r:["a"],idx:0},li:0}]},
  {n:132,cat:"misc",re:/ab|a/,sub:"ab",calls:[{exp:{r:["ab"],idx:0},li:0}]},
  {n:133,cat:"misc",re:/(a(b)?)+/,sub:"ababa",calls:[{exp:{r:["ababa","a",undefined],idx:0},li:0}]},
  {n:134,cat:"misc",re:/^(?:(\w)|(\d))+$/,sub:"a1b2",calls:[{exp:{r:["a1b2","2",undefined],idx:0},li:0}]},
  {n:135,cat:"misc",re:/(a*)+/,sub:"aaa",calls:[{exp:{r:["aaa","aaa"],idx:0},li:0}]},
  {n:136,cat:"misc",re:/\cI/,sub:"\t",calls:[{exp:{r:["\t"],idx:0},li:0}]},
  {n:137,cat:"misc",re:/\0/,sub:"\0",calls:[{exp:{r:["\0"],idx:0},li:0}]},
  {n:138,cat:"misc",re:/["\\]+/,sub:"a\"\\b",calls:[{exp:{r:["\"\\"],idx:1},li:0}]},

  // ---- long-literal ----
  {n:139,cat:"long-literal",re:/abcdefghij/,sub:"abcdefghij",calls:[{exp:{r:["abcdefghij"],idx:0},li:0}]},
  {n:140,cat:"long-literal",re:/abcdefghij/,sub:"xxxabcdefghijyyy",calls:[{exp:{r:["abcdefghij"],idx:3},li:0}]},
  {n:141,cat:"long-literal",re:/abcdefghij/,sub:"abcdefghik",calls:[{exp:null,li:0}]},
  {n:142,cat:"long-literal",re:/abcdefghij/,sub:"abcdefghi",calls:[{exp:null,li:0}]},
  {n:143,cat:"long-literal",re:/abcdefghij/,sub:"qqqqqqqqqq",calls:[{exp:null,li:0}]},
  {n:144,cat:"long-literal",re:/[a-z]+abcdef/,sub:"qqqabcdefxx",calls:[{exp:{r:["qqqabcdef"],idx:0},li:0}]},
  {n:145,cat:"long-literal",re:/abcdefghijklmnop/,sub:"XXabcdefghijklmnopYY",calls:[{exp:{r:["abcdefghijklmnop"],idx:2},li:0}]},
  {n:146,cat:"long-literal",re:/abcdefghijklmnop/,sub:"XXabcdefghijklmnoZZ",calls:[{exp:null,li:0}]},
  {n:147,cat:"long-literal",re:/aaaaaa/,sub:"aaaaaaab",calls:[{exp:{r:["aaaaaa"],idx:0},li:0}]},
  {n:148,cat:"long-literal",re:/AbCdEfGhIj/i,sub:"xxabcdefghijxx",calls:[{exp:{r:["abcdefghij"],idx:2},li:0}]},
  {n:149,cat:"long-literal",re:/abcdefghij/u,sub:"xxabcdefghijxx",calls:[{exp:{r:["abcdefghij"],idx:2},li:0}]},
  {n:150,cat:"long-literal",re:/\u0430\u0431\u0432\u0433\u0434\u0435\u0436\u0437/,sub:"aa\u0430\u0431\u0432\u0433\u0434\u0435\u0436\u0437bb",calls:[{exp:{r:["\u0430\u0431\u0432\u0433\u0434\u0435\u0436\u0437"],idx:2},li:0}]},
  {n:151,cat:"long-literal",re:/\u0430\u0431\u0432\u0433\u0434\u0435\u0436\u0437/,sub:"aa\u0430\u0431\u0432\u0433\u0434\u0435\u0436\u0438bb",calls:[{exp:null,li:0}]},
  {n:152,cat:"long-literal",re:/^abcdefghij$/,sub:"abcdefghij",calls:[{exp:{r:["abcdefghij"],idx:0},li:0}]},

  // ---- simple-loops ----
  {n:153,cat:"simple-loops",re:/[^"]*/,sub:"hello\"world",calls:[{exp:{r:["hello"],idx:0},li:0}]},
  {n:154,cat:"simple-loops",re:/[^"\\]*/,sub:"hello\"world",calls:[{exp:{r:["hello"],idx:0},li:0}]},
  {n:155,cat:"simple-loops",re:/[^"\\]*/,sub:"hello\\nworld\"",calls:[{exp:{r:["hello"],idx:0},li:0}]},
  {n:156,cat:"simple-loops",re:/[^\n]*/,sub:"one\ntwo",calls:[{exp:{r:["one"],idx:0},li:0}]},
  {n:157,cat:"simple-loops",re:/\S+/,sub:"word stop",calls:[{exp:{r:["word"],idx:0},li:0}]},
  {n:158,cat:"simple-loops",re:/[^\s]+/,sub:"word stop",calls:[{exp:{r:["word"],idx:0},li:0}]},
  {n:159,cat:"simple-loops",re:/\w+/,sub:"word stop",calls:[{exp:{r:["word"],idx:0},li:0}]},
  {n:160,cat:"simple-loops",re:/\d+/,sub:"12345 end",calls:[{exp:{r:["12345"],idx:0},li:0}]},
  {n:161,cat:"simple-loops",re:/[a-zA-Z0-9_]+/,sub:"foo_bar baz",calls:[{exp:{r:["foo_bar"],idx:0},li:0}]},
  {n:162,cat:"simple-loops",re:/[a-f0-9]+/,sub:"DEADbeef!",calls:[{exp:{r:["beef"],idx:4},li:0}]},
  {n:163,cat:"simple-loops",re:/[a-fA-F0-9]+/,sub:"DEADbeef!",calls:[{exp:{r:["DEADbeef"],idx:0},li:0}]},
  {n:164,cat:"simple-loops",re:/[a-z]+/,sub:"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaZ",calls:[{exp:{r:["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"],idx:0},li:0}]},
  {n:165,cat:"simple-loops",re:/[a-z]+/,sub:"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",calls:[{exp:{r:["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"],idx:0},li:0}]},
  {n:166,cat:"simple-loops",re:/[^"]*?"/,sub:"hello\"world\"",calls:[{exp:{r:["hello\""],idx:0},li:0}]},
  {n:167,cat:"simple-loops",re:/\w+\b/,sub:"hello world",calls:[{exp:{r:["hello"],idx:0},li:0}]},
  {n:168,cat:"simple-loops",re:/[a-zA-Z]+/,sub:"Hello World!",calls:[{exp:{r:["Hello"],idx:0},li:0}]},
  {n:169,cat:"simple-loops",re:/[^"\\\n]*/,sub:"normal text\"end",calls:[{exp:{r:["normal text"],idx:0},li:0}]},
  {n:170,cat:"simple-loops",re:/^[^,]*/,sub:"first,second,third",calls:[{exp:{r:["first"],idx:0},li:0}]},
  {n:171,cat:"simple-loops",re:/"[^"\\]*(?:\\.[^"\\]*)*"/,sub:"\"hello \\\"world\\\"\"",calls:[{exp:{r:["\"hello \\\"world\\\"\""],idx:0},li:0}]},
  {n:172,cat:"simple-loops",re:/\w+/,sub:"word\u0410\u0411\u0412 stop",calls:[{exp:{r:["word"],idx:0},li:0}]},

  // ---- long-alt ----
  {n:173,cat:"long-alt",re:/^(?:a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)$/,sub:"m",calls:[{exp:{r:["m"],idx:0},li:0}]},
  {n:174,cat:"long-alt",re:/^(?:a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)$/,sub:"A",calls:[{exp:null,li:0}]},
  {n:175,cat:"long-alt",re:/(?:a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)/,sub:"AAA m AAA",calls:[{exp:{r:["m"],idx:4},li:0}]},
  {n:176,cat:"long-alt",re:/^(?:true|false|null|undefined|NaN)$/,sub:"false",calls:[{exp:{r:["false"],idx:0},li:0}]},
  {n:177,cat:"long-alt",re:/^(?:true|false|null|undefined|NaN)$/,sub:"true",calls:[{exp:{r:["true"],idx:0},li:0}]},
  {n:178,cat:"long-alt",re:/^(?:true|false|null|undefined|NaN)$/,sub:"maybe",calls:[{exp:null,li:0}]},
  {n:179,cat:"long-alt",re:/(?:foo|barbaz|q)x/,sub:"barbazx",calls:[{exp:{r:["barbazx"],idx:0},li:0}]},
  {n:180,cat:"long-alt",re:/(?:foo|barbaz|q)x/,sub:"qx",calls:[{exp:{r:["qx"],idx:0},li:0}]},
  {n:181,cat:"long-alt",re:/(?:foo|barbaz|q)x/,sub:"foox",calls:[{exp:{r:["foox"],idx:0},li:0}]},
  {n:182,cat:"long-alt",re:/\b(?:the|a|an|and|or|but|if|then|else)\b/,sub:"find the word here",calls:[{exp:{r:["the"],idx:5},li:0}]},
  {n:183,cat:"long-alt",re:/(?:a|b|c)(?:d|e|f)/,sub:"be",calls:[{exp:{r:["be"],idx:0},li:0}]},
  {n:184,cat:"long-alt",re:/(?:a|b|c)(?:d|e|f)/,sub:"cf",calls:[{exp:{r:["cf"],idx:0},li:0}]},
  {n:185,cat:"long-alt",re:/(?:a|b|c)(?:d|e|f)/,sub:"xy",calls:[{exp:null,li:0}]},
  {n:186,cat:"long-alt",re:/(?:foo|)bar/,sub:"foobar",calls:[{exp:{r:["foobar"],idx:0},li:0}]},
  {n:187,cat:"long-alt",re:/(?:foo|)bar/,sub:"bar",calls:[{exp:{r:["bar"],idx:0},li:0}]},
  {n:188,cat:"long-alt",re:/^(?:a|b|c|d|e|f|g|h)$/i,sub:"E",calls:[{exp:{r:["E"],idx:0},li:0}]},
  {n:189,cat:"long-alt",re:/^(?:\u0410|\u0411|\u0412)$/,sub:"\u0411",calls:[{exp:{r:["\u0411"],idx:0},li:0}]},

  // ---- nesting ----
  {n:190,cat:"nesting",re:/((((a))))/,sub:"a",calls:[{exp:{r:["a","a","a","a","a"],idx:0},li:0}]},
  {n:191,cat:"nesting",re:/(?:(?:(?:(?:a))))/,sub:"a",calls:[{exp:{r:["a"],idx:0},li:0}]},
  {n:192,cat:"nesting",re:/((((a))?))?/,sub:"a",calls:[{exp:{r:["a","a","a","a","a"],idx:0},li:0}]},
  {n:193,cat:"nesting",re:/((((a))?))?/,sub:"",calls:[{exp:{r:["",undefined,undefined,undefined,undefined],idx:0},li:0}]},
  {n:194,cat:"nesting",re:/((((a)(b))(c))(d))/,sub:"abcd",calls:[{exp:{r:["abcd","abcd","abc","ab","a","b","c","d"],idx:0},li:0}]},
  {n:195,cat:"nesting",re:/(?=a(?=b(?=c)))abc/,sub:"abc",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:196,cat:"nesting",re:/(?=a(?=b(?=c)))abc/,sub:"abd",calls:[{exp:null,li:0}]},
  {n:197,cat:"nesting",re:/x(?<=(?<=(?<=a)b)c)/,sub:"abcx",calls:[{exp:null,li:0}]},
  {n:198,cat:"nesting",re:/(?<=(a)(b))c/,sub:"abc",calls:[{exp:{r:["c","a","b"],idx:2},li:0}]},
  {n:199,cat:"nesting",re:/(?:(?:a|b)(?:c|d)){2}/,sub:"acbd",calls:[{exp:{r:["acbd"],idx:0},li:0}]},
  {n:200,cat:"nesting",re:/(((((a?)?)?)?)?)?/,sub:"a",calls:[{exp:{r:["a","a","a","a","a","a"],idx:0},li:0}]},

  // ---- backtrack ----
  {n:201,cat:"backtrack",re:/(a+)+b/,sub:"aaab",calls:[{exp:{r:["aaab","aaa"],idx:0},li:0}]},
  {n:202,cat:"backtrack",re:/(a+)+b/,sub:"aaaaa",calls:[{exp:null,li:0}]},
  {n:203,cat:"backtrack",re:/(a|aa)+b/,sub:"aaab",calls:[{exp:{r:["aaab","a"],idx:0},li:0}]},
  {n:204,cat:"backtrack",re:/(a|aa)+b/,sub:"aaaaa",calls:[{exp:null,li:0}]},
  {n:205,cat:"backtrack",re:/(\w+)(\w*)\1/,sub:"foofoo",calls:[{exp:{r:["foofoo","foo",""],idx:0},li:0}]},
  {n:206,cat:"backtrack",re:/(?:(\w)(\w))+/,sub:"abcd",calls:[{exp:{r:["abcd","c","d"],idx:0},li:0}]},
  {n:207,cat:"backtrack",re:/(?:(\w)(\w))+/,sub:"abc",calls:[{exp:{r:["ab","a","b"],idx:0},li:0}]},
  {n:208,cat:"backtrack",re:/.*?b/,sub:"aaaaab",calls:[{exp:{r:["aaaaab"],idx:0},li:0}]},
  {n:209,cat:"backtrack",re:/.*b/,sub:"aaaaab",calls:[{exp:{r:["aaaaab"],idx:0},li:0}]},
  {n:210,cat:"backtrack",re:/(\w+),\1/,sub:"foo,foo",calls:[{exp:{r:["foo,foo","foo"],idx:0},li:0}]},
  {n:211,cat:"backtrack",re:/(\w+),\1/,sub:"foo,bar",calls:[{exp:null,li:0}]},

  // ---- boundary-u ----
  {n:212,cat:"boundary-u",re:/\bword\b/u,sub:"find word here",calls:[{exp:{r:["word"],idx:5},li:0}]},
  {n:213,cat:"boundary-u",re:/\bword\b/u,sub:"wordwise",calls:[{exp:null,li:0}]},
  {n:214,cat:"boundary-u",re:/\bword\b/u,sub:"caf\xe9word here",calls:[{exp:{r:["word"],idx:4},li:0}]},
  {n:215,cat:"boundary-u",re:/\bword\b/u,sub:"foo\ud83d\ude00word here",calls:[{exp:{r:["word"],idx:5},li:0}]},
  {n:216,cat:"boundary-u",re:/\Bo\B/u,sub:"foo",calls:[{exp:{r:["o"],idx:1},li:0}]},
  {n:217,cat:"boundary-u",re:/\B\u0411\B/u,sub:"\u0410\u0411\u0412",calls:[{exp:{r:["\u0411"],idx:1},li:0}]},
  {n:218,cat:"boundary-u",re:/\bWORD\b/iu,sub:"find word here",calls:[{exp:{r:["word"],idx:5},li:0}]},
  {n:219,cat:"boundary-u",re:/\b\w+\b/u,sub:"\ud83d\ude00foo\ud83d\ude01",calls:[{exp:{r:["foo"],idx:2},li:0}]},
  {n:220,cat:"boundary-u",re:/^\b\w+/u,sub:"\xe9foo",calls:[{exp:null,li:0}]},

  // ---- v-classes ----
  {n:221,cat:"v-classes",re:/\d+/v,sub:"123 \uff11\uff12",calls:[{exp:{r:["123"],idx:0},li:0}]},
  {n:222,cat:"v-classes",re:/\w+/v,sub:"foo_bar baz",calls:[{exp:{r:["foo_bar"],idx:0},li:0}]},
  {n:223,cat:"v-classes",re:/\s+/v,sub:"a   b",calls:[{exp:{r:["   "],idx:1},li:0}]},
  {n:224,cat:"v-classes",re:/\d+/iv,sub:"0\uff11",calls:[{exp:{r:["0"],idx:0},li:0}]},
  {n:225,cat:"v-classes",re:/[\W]+/v,sub:"foo bar",calls:[{exp:{r:[" "],idx:3},li:0}]},
  {n:226,cat:"v-classes",re:/[\p{ASCII}&&\d]+/v,sub:"123 \uff11",calls:[{exp:{r:["123"],idx:0},li:0}]},
  {n:227,cat:"v-classes",re:/\p{Letter}+/v,sub:"caf\xe9 word",calls:[{exp:{r:["caf\xe9"],idx:0},li:0}]},
  {n:228,cat:"v-classes",re:/[\p{ASCII}--\d]+/v,sub:"abc123def",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:229,cat:"v-classes",re:/[\p{ASCII}--\s]+/v,sub:"hello world",calls:[{exp:{r:["hello"],idx:0},li:0}]},

  // ---- icase-paths ----
  {n:230,cat:"icase-paths",re:/AbCdEf/i,sub:"abcdef",calls:[{exp:{r:["abcdef"],idx:0},li:0}]},
  {n:231,cat:"icase-paths",re:/AbCdEf/iu,sub:"abcdef",calls:[{exp:{r:["abcdef"],idx:0},li:0}]},
  {n:232,cat:"icase-paths",re:/AbCdEf/iv,sub:"abcdef",calls:[{exp:{r:["abcdef"],idx:0},li:0}]},
  {n:233,cat:"icase-paths",re:/[a-z]+/i,sub:"AB\u212aCD",calls:[{exp:{r:["AB"],idx:0},li:0}]},
  {n:234,cat:"icase-paths",re:/[a-z]+/iu,sub:"AB\u212aCD",calls:[{exp:{r:["AB\u212aCD"],idx:0},li:0}]},
  {n:235,cat:"icase-paths",re:/[\xc0-\xff]+/i,sub:"caf\xe9\xe9",calls:[{exp:{r:["\xe9\xe9"],idx:3},li:0}]},
  {n:236,cat:"icase-paths",re:/[\u{1F600}-\u{1F64F}]/iu,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:237,cat:"icase-paths",re:/\u00b5/i,sub:"\u03bc",calls:[{exp:{r:["\u03bc"],idx:0},li:0}]},
  {n:238,cat:"icase-paths",re:/\u00b5/iu,sub:"\u03bc",calls:[{exp:{r:["\u03bc"],idx:0},li:0}]},
  {n:239,cat:"icase-paths",re:/\u00b5/iv,sub:"\u03bc",calls:[{exp:{r:["\u03bc"],idx:0},li:0}]},
  {n:240,cat:"icase-paths",re:/\u03a3/iu,sub:"\u03c2",calls:[{exp:{r:["\u03c2"],idx:0},li:0}]},
  {n:241,cat:"icase-paths",re:/\u03a3/iu,sub:"\u03c3",calls:[{exp:{r:["\u03c3"],idx:0},li:0}]},
  {n:242,cat:"icase-paths",re:/i/iu,sub:"\u0130",calls:[{exp:null,li:0}]},
  {n:243,cat:"icase-paths",re:/[A-Z]+/i,sub:"foo\u212abar",calls:[{exp:{r:["foo"],idx:0},li:0}]},
  {n:244,cat:"icase-paths",re:/[A-Z]+/iu,sub:"foo\u212abar",calls:[{exp:{r:["foo\u212abar"],idx:0},li:0}]},

  // ---- lone-surrog ----
  {n:245,cat:"lone-surrog",re:/./,sub:"\ud800",calls:[{exp:{r:["\ud800"],idx:0},li:0}]},
  {n:246,cat:"lone-surrog",re:/.+/,sub:"\ud800\ud801\ud802",calls:[{exp:{r:["\ud800\ud801\ud802"],idx:0},li:0}]},
  {n:247,cat:"lone-surrog",re:/[\uD800-\uDFFF]/,sub:"\ud800",calls:[{exp:{r:["\ud800"],idx:0},li:0}]},
  {n:248,cat:"lone-surrog",re:/[\uD800-\uDFFF]/,sub:"\udc00",calls:[{exp:{r:["\udc00"],idx:0},li:0}]},
  {n:249,cat:"lone-surrog",re:/[\uD800-\uDFFF]+/,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d\ude00"],idx:0},li:0}]},
  {n:250,cat:"lone-surrog",re:/\uD800/,sub:"\ud800",calls:[{exp:{r:["\ud800"],idx:0},li:0}]},
  {n:251,cat:"lone-surrog",re:/\uD800/,sub:"\ud801",calls:[{exp:null,li:0}]},
  {n:252,cat:"lone-surrog",re:/\uD83D/,sub:"\ud83d\ude00",calls:[{exp:{r:["\ud83d"],idx:0},li:0}]},
  {n:253,cat:"lone-surrog",re:/^\uD800$/,sub:"\ud800",calls:[{exp:{r:["\ud800"],idx:0},li:0}]},
  {n:254,cat:"lone-surrog",re:/[^\uD800-\uDFFF]+/,sub:"a\ud800b\udc00c",calls:[{exp:{r:["a"],idx:0},li:0}]},
  {n:255,cat:"lone-surrog",re:/\w*/,sub:"ab\ud800cd",calls:[{exp:{r:["ab"],idx:0},li:0}]},
  {n:256,cat:"lone-surrog",re:/.+/,sub:"\ud83d\ude00\ud800",calls:[{exp:{r:["\ud83d\ude00\ud800"],idx:0},li:0}]},

  // ---- lone-surrog-pat ----
  {n:257,cat:"lone-surrog-pat",re:/\uD800+/,sub:"\ud800\ud800\ud800a",calls:[{exp:{r:["\ud800\ud800\ud800"],idx:0},li:0}]},
  {n:258,cat:"lone-surrog-pat",re:/[a\uD800]+/,sub:"a\ud800a",calls:[{exp:{r:["a\ud800a"],idx:0},li:0}]},
  {n:259,cat:"lone-surrog-pat",re:/[a\uD800-\uDFFFb]+/,sub:"a\ud800b",calls:[{exp:{r:["a\ud800b"],idx:0},li:0}]},
  {n:260,cat:"lone-surrog-pat",re:/a|\uD800|c/,sub:"\ud800",calls:[{exp:{r:["\ud800"],idx:0},li:0}]},
  {n:261,cat:"lone-surrog-pat",re:/a|\uD800|c/,sub:"a",calls:[{exp:{r:["a"],idx:0},li:0}]},
  {n:262,cat:"lone-surrog-pat",re:/a|\uD800|c/,sub:"c",calls:[{exp:{r:["c"],idx:0},li:0}]},
  {n:263,cat:"lone-surrog-pat",re:/^\uD800/,sub:"\ud800abc",calls:[{exp:{r:["\ud800"],idx:0},li:0}]},
  {n:264,cat:"lone-surrog-pat",re:/\uD800$/,sub:"abc\ud800",calls:[{exp:{r:["\ud800"],idx:3},li:0}]},
  {n:265,cat:"lone-surrog-pat",re:/[\uD800-\uDFFF]{4}/,sub:"\ud83d\ude00\ud83d\ude01",calls:[{exp:{r:["\ud83d\ude00\ud83d\ude01"],idx:0},li:0}]},
  {n:266,cat:"lone-surrog-pat",re:/\ba/,sub:"\ud800abc",calls:[{exp:{r:["a"],idx:1},li:0}]},
  {n:267,cat:"lone-surrog-pat",re:/.+/,sub:"\ud800\ud83d\ude00",calls:[{exp:{r:["\ud800\ud83d\ude00"],idx:0},li:0}]},
  {n:268,cat:"lone-surrog-pat",re:/(\uD800)\1/,sub:"\ud800\ud800",calls:[{exp:{r:["\ud800\ud800","\ud800"],idx:0},li:0}]},

  // ---- class-sizes ----
  {n:269,cat:"class-sizes",re:/[x]+/,sub:"xxxabc",calls:[{exp:{r:["xxx"],idx:0},li:0}]},
  {n:270,cat:"class-sizes",re:/[xy]+/,sub:"xyxabc",calls:[{exp:{r:["xyx"],idx:0},li:0}]},
  {n:271,cat:"class-sizes",re:/[a-c]+/,sub:"abcd",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:272,cat:"class-sizes",re:/[A-Z]+/,sub:"HELLOworld",calls:[{exp:{r:["HELLO"],idx:0},li:0}]},
  {n:273,cat:"class-sizes",re:/[A-Za-z]+/,sub:"HelloWorld!",calls:[{exp:{r:["HelloWorld"],idx:0},li:0}]},
  {n:274,cat:"class-sizes",re:/[aeiou]+/,sub:"hello world",calls:[{exp:{r:["e"],idx:1},li:0}]},
  {n:275,cat:"class-sizes",re:/[abcdefghijklmnop]+/,sub:"mhopabxx",calls:[{exp:{r:["mhopab"],idx:0},li:0}]},
  {n:276,cat:"class-sizes",re:/[aeiou0-9_]+/,sub:"i9_eou0z",calls:[{exp:{r:["i9_eou0"],idx:0},li:0}]},
  {n:277,cat:"class-sizes",re:/[\x20-\x7e]+/,sub:"hello!",calls:[{exp:{r:["hello!"],idx:0},li:0}]},
  {n:278,cat:"class-sizes",re:/[^x]+/,sub:"aaaxbbb",calls:[{exp:{r:["aaa"],idx:0},li:0}]},
  {n:279,cat:"class-sizes",re:/[^a-z]+/,sub:"AB123ab",calls:[{exp:{r:["AB123"],idx:0},li:0}]},
  {n:280,cat:"class-sizes",re:/[\u{1F600}-\u{1F605}]+/u,sub:"\ud83d\ude03\ud83d\ude04x",calls:[{exp:{r:["\ud83d\ude03\ud83d\ude04"],idx:0},li:0}]},
  {n:281,cat:"class-sizes",re:/[\u{0061}-\u{1F600}]/u,sub:"\ud83d\udd00",calls:[{exp:{r:["\ud83d\udd00"],idx:0},li:0}]},
  {n:282,cat:"class-sizes",re:/[a\u{1F600}]+/u,sub:"a\ud83d\ude00b",calls:[{exp:{r:["a\ud83d\ude00"],idx:0},li:0}]},

  // ---- quant-class ----
  {n:283,cat:"quant-class",re:/\d{4}/,sub:"12345",calls:[{exp:{r:["1234"],idx:0},li:0}]},
  {n:284,cat:"quant-class",re:/[a-z]{2,5}/,sub:"abcdef",calls:[{exp:{r:["abcde"],idx:0},li:0}]},
  {n:285,cat:"quant-class",re:/\w{3,}/,sub:"foo bar",calls:[{exp:{r:["foo"],idx:0},li:0}]},
  {n:286,cat:"quant-class",re:/[a-z]{2,5}?[A-Z]/,sub:"abcdefG",calls:[{exp:{r:["bcdefG"],idx:1},li:0}]},
  {n:287,cat:"quant-class",re:/\d{0}abc/,sub:"abc",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:288,cat:"quant-class",re:/[^"]{3}/,sub:"abcdef\"",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:289,cat:"quant-class",re:/\p{L}{3,5}/u,sub:"Hello12",calls:[{exp:{r:["Hello"],idx:0},li:0}]},
  {n:290,cat:"quant-class",re:/[a-z]{3,5}/iu,sub:"AB\u212aCD",calls:[{exp:{r:["AB\u212aCD"],idx:0},li:0}]},
  {n:291,cat:"quant-class",re:/^[A-Z]{3}/,sub:"ABC123",calls:[{exp:{r:["ABC"],idx:0},li:0}]},
  {n:292,cat:"quant-class",re:/(?:[a-z]{2}){3}/,sub:"abcdefxx",calls:[{exp:{r:["abcdef"],idx:0},li:0}]},
  {n:293,cat:"quant-class",re:/\s{2,4}/,sub:"a    b",calls:[{exp:{r:["    "],idx:1},li:0}]},
  {n:294,cat:"quant-class",re:/\d+?\./,sub:"123.456",calls:[{exp:{r:["123."],idx:0},li:0}]},

  // ---- cap-count ----
  {n:295,cat:"cap-count",re:/(?:abc)+/,sub:"abcabc",calls:[{exp:{r:["abcabc"],idx:0},li:0}]},
  {n:296,cat:"cap-count",re:/(\w+)/,sub:"foo",calls:[{exp:{r:["foo","foo"],idx:0},li:0}]},
  {n:297,cat:"cap-count",re:/(\d)(\d)(\d)(\d)/,sub:"1234",calls:[{exp:{r:["1234","1","2","3","4"],idx:0},li:0}]},
  {n:298,cat:"cap-count",re:/(.)(.)(.)(.)(.)(.)(.)(.)/,sub:"abcdefgh",calls:[{exp:{r:["abcdefgh","a","b","c","d","e","f","g","h"],idx:0},li:0}]},
  {n:299,cat:"cap-count",re:/(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)/,sub:"abcdefghijkl",calls:[{exp:{r:["abcdefghijkl","a","b","c","d","e","f","g","h","i","j","k","l"],idx:0},li:0}]},
  {n:300,cat:"cap-count",re:/(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)/,sub:"abcdefghijklmnop",calls:[{exp:{r:["abcdefghijklmnop","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p"],idx:0},li:0}]},
  {n:301,cat:"cap-count",re:/(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)/,sub:"abcdefghijklmnopqrst",calls:[{exp:{r:["abcdefghijklmnopqrst","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t"],idx:0},li:0}]},
  {n:302,cat:"cap-count",re:/(\w)+/,sub:"abc",calls:[{exp:{r:["abc","c"],idx:0},li:0}]},
  {n:303,cat:"cap-count",re:/(a)(?:b)(c)(?:d)(e)/,sub:"abcde",calls:[{exp:{r:["abcde","a","c","e"],idx:0},li:0}]},
  {n:304,cat:"cap-count",re:/(a)|(b)|(c)/,sub:"b",calls:[{exp:{r:["b",undefined,"b",undefined],idx:0},li:0}]},

  // ---- s-u ----
  {n:305,cat:"s-u",re:/.+/su,sub:"a\nb\ud83d\ude00c",calls:[{exp:{r:["a\nb\ud83d\ude00c"],idx:0},li:0}]},
  {n:306,cat:"s-u",re:/./su,sub:"\n",calls:[{exp:{r:["\n"],idx:0},li:0}]},
  {n:307,cat:"s-u",re:/^.{3}$/su,sub:"\n\ud83d\ude00x",calls:[{exp:{r:["\n\ud83d\ude00x"],idx:0},li:0}]},
  {n:308,cat:"s-u",re:/^.{3}$/s,sub:"\n\ud83d\ude00x",calls:[{exp:null,li:0}]},
  {n:309,cat:"s-u",re:/^.{4}$/s,sub:"\n\ud83d\ude00x",calls:[{exp:{r:["\n\ud83d\ude00x"],idx:0},li:0}]},
  {n:310,cat:"s-u",re:/^.+$/su,sub:"\u2028line",calls:[{exp:{r:["\u2028line"],idx:0},li:0}]},
  {n:311,cat:"s-u",re:/(?<=^)..(?=$)/su,sub:"\ud83d\ude00",calls:[{exp:null,li:0}]},

  // ---- m-u ----
  {n:312,cat:"m-u",re:/^\w+$/mu,sub:"caf\xe9\nhello",calls:[{exp:{r:["hello"],idx:5},li:0}]},
  {n:313,cat:"m-u",re:/^\p{L}+$/mu,sub:"caf\xe9\nhello",calls:[{exp:{r:["caf\xe9"],idx:0},li:0}]},
  {n:314,cat:"m-u",re:/^abc$/mu,sub:"\ud83d\ude00\nabc",calls:[{exp:{r:["abc"],idx:3},li:0}]},
  {n:315,cat:"m-u",re:/\bword\b/mu,sub:"caf\xe9word\nhello word here",calls:[{exp:{r:["word"],idx:4},li:0}]},
  {n:316,cat:"m-u",re:/^[^\n]+$/mu,sub:"line1\n\ud83d\ude00line2",calls:[{exp:{r:["line1"],idx:0},li:0}]},
  {n:317,cat:"m-u",re:/^.{3}$/mu,sub:"\ud83d\ude00x\nabc",calls:[{exp:{r:["abc"],idx:4},li:0}]},

  // ---- d-extras ----
  {n:318,cat:"d-extras",re:/(?<=(foo))bar/d,sub:"foobar",calls:[{exp:{r:["bar","foo"],idx:3,indices:[[3,6],[0,3]]},li:0}]},
  {n:319,cat:"d-extras",re:/(a)?/d,sub:"b",calls:[{exp:{r:["",undefined],idx:0,indices:[[0,0],undefined]},li:0}]},
  {n:320,cat:"d-extras",re:/(?=(foo))/d,sub:"foobar",calls:[{exp:{r:["","foo"],idx:0,indices:[[0,0],[0,3]]},li:0}]},
  {n:321,cat:"d-extras",re:/(?<=(?<pre>foo))bar/d,sub:"foobar",calls:[{exp:{r:["bar","foo"],idx:3,groups:{"pre":"foo"},indices:[[3,6],[0,3]],indicesGroups:{"pre":[0,3]}},li:0}]},
  {n:322,cat:"d-extras",re:/(\w+)/dg,sub:"foo bar baz",calls:[{exp:{r:["foo","foo"],idx:0,indices:[[0,3],[0,3]]},li:3},{exp:{r:["bar","bar"],idx:4,indices:[[4,7],[4,7]]},li:7},{exp:{r:["baz","baz"],idx:8,indices:[[8,11],[8,11]]},li:11},{exp:null,li:0}]},
  {n:323,cat:"d-extras",re:/(\w)/dy,sub:"abc",calls:[{exp:{r:["a","a"],idx:0,indices:[[0,1],[0,1]]},li:1},{exp:{r:["b","b"],idx:1,indices:[[1,2],[1,2]]},li:2},{exp:{r:["c","c"],idx:2,indices:[[2,3],[2,3]]},li:3},{exp:null,li:0}]},
  {n:324,cat:"d-extras",re:/((a)(b))/d,sub:"ab",calls:[{exp:{r:["ab","ab","a","b"],idx:0,indices:[[0,2],[0,2],[0,1],[1,2]]},li:0}]},
  {n:325,cat:"d-extras",re:/(a)|(b)/d,sub:"b",calls:[{exp:{r:["b",undefined,"b"],idx:0,indices:[[0,1],undefined,[0,1]]},li:0}]},
  {n:326,cat:"d-extras",re:/^()/d,sub:"abc",calls:[{exp:{r:["",""],idx:0,indices:[[0,0],[0,0]]},li:0}]},
  {n:327,cat:"d-extras",re:/(?<x>a)?(?<y>b)?/d,sub:"b",calls:[{exp:{r:["b",undefined,"b"],idx:0,groups:{"x":undefined,"y":"b"},indices:[[0,1],undefined,[0,1]],indicesGroups:{"x":undefined,"y":[0,1]}},li:0}]},

  // ---- shared ----
  {n:328,cat:"shared",re:/(?:foo|foo)+/,sub:"foofoo",calls:[{exp:{r:["foofoo"],idx:0},li:0}]},
  {n:329,cat:"shared",re:/(?:abc){2}(?:abc){3}/,sub:"abcabcabcabcabc",calls:[{exp:{r:["abcabcabcabcabc"],idx:0},li:0}]},
  {n:330,cat:"shared",re:/a(?:b|b)c/,sub:"abc",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:331,cat:"shared",re:/(?:foo|bar)baz/,sub:"foobaz",calls:[{exp:{r:["foobaz"],idx:0},li:0}]},
  {n:332,cat:"shared",re:/(?:foo|bar)baz/,sub:"barbaz",calls:[{exp:{r:["barbaz"],idx:0},li:0}]},
  {n:333,cat:"shared",re:/(?:a(?:bc|bc))+/,sub:"abcabc",calls:[{exp:{r:["abcabc"],idx:0},li:0}]},
  {n:334,cat:"shared",re:/(?=foo|foo)foo/,sub:"foo",calls:[{exp:{r:["foo"],idx:0},li:0}]},
  {n:335,cat:"shared",re:/(foo)|(foo)/,sub:"foo",calls:[{exp:{r:["foo","foo",undefined],idx:0},li:0}]},
  {n:336,cat:"shared",re:/(?:[a-z]|[a-z])+/,sub:"abc",calls:[{exp:{r:["abc"],idx:0},li:0}]},
  {n:337,cat:"shared",re:/(?:abc|abc|abc)x/,sub:"abcx",calls:[{exp:{r:["abcx"],idx:0},li:0}]},

  // ---- prefix-scan ----
  {n:338,cat:"prefix-scan",re:/xxx?abcdef/,sub:"qqqxxabcdefyy",calls:[{exp:{r:["xxabcdef"],idx:3},li:0}]},
  {n:339,cat:"prefix-scan",re:/[^a]*abc/,sub:"qqqabc",calls:[{exp:{r:["qqqabc"],idx:0},li:0}]},
  {n:340,cat:"prefix-scan",re:/\w+(?=,)/,sub:"foo,bar,baz",calls:[{exp:{r:["foo"],idx:0},li:0}]},
  {n:341,cat:"prefix-scan",re:/[a-z]+(?=\d)/,sub:"foo bar123",calls:[{exp:{r:["bar"],idx:4},li:0}]},
  {n:342,cat:"prefix-scan",re:/[ab]*c/,sub:"abababc",calls:[{exp:{r:["abababc"],idx:0},li:0}]},
  {n:343,cat:"prefix-scan",re:/(?:foo|food|foot)bar/,sub:"foodbar",calls:[{exp:{r:["foodbar"],idx:0},li:0}]},
  {n:344,cat:"prefix-scan",re:/abcdef/,sub:"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabcdef",calls:[{exp:{r:["abcdef"],idx:100},li:0}]},
  {n:345,cat:"prefix-scan",re:/[^,;]+;/,sub:"a,b;c",calls:[{exp:{r:["b;"],idx:2},li:0}]},
  {n:346,cat:"prefix-scan",re:/^[a-z]+\d/,sub:"abcdef9",calls:[{exp:{r:["abcdef9"],idx:0},li:0}]},
  {n:347,cat:"prefix-scan",re:/(?:foo|bar|baz)X/,sub:"fooX",calls:[{exp:{r:["fooX"],idx:0},li:0}]},
  {n:348,cat:"prefix-scan",re:/(?:foo|bar|baz)X/,sub:"barX",calls:[{exp:{r:["barX"],idx:0},li:0}]},
  {n:349,cat:"prefix-scan",re:/(?:foo|bar|baz)X/,sub:"bazX",calls:[{exp:{r:["bazX"],idx:0},li:0}]},
];

for (const c of CASES) check(c);

})();
