// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-lookbehind

function execl(expectation, regexp, subject) {
  if (regexp instanceof String) regexp = new RegExp(regexp, "u");
  assertEquals(expectation, regexp.exec(subject));
}

function execs(expectation, regexp_source, subject) {
  execl(expectation, new RegExp(regexp_source, "u"), subject);
}

// Character ranges.
execl(["A"], /[A-D]/u, "A");
execs(["A"], "[A-D]", "A");
execl(["ABCD"], /[A-D]+/u, "ZABCDEF");
execs(["ABCD"], "[A-D]+", "ZABCDEF");

execl(["\u{12345}"], /[\u1234-\u{12345}]/u, "\u{12345}");
execs(["\u{12345}"], "[\u1234-\u{12345}]", "\u{12345}");
execl(null, /[^\u1234-\u{12345}]/u, "\u{12345}");
execs(null, "[^\u1234-\u{12345}]", "\u{12345}");

execl(["\u{1234}"], /[\u1234-\u{12345}]/u, "\u{1234}");
execs(["\u{1234}"], "[\u1234-\u{12345}]", "\u{1234}");
execl(null, /[^\u1234-\u{12345}]/u, "\u{1234}");
execs(null, "[^\u1234-\u{12345}]", "\u{1234}");

execl(null, /[\u1234-\u{12345}]/u, "\u{1233}");
execs(null, "[\u1234-\u{12345}]", "\u{1233}");
execl(["\u{1233}"], /[^\u1234-\u{12345}]/u, "\u{1233}");
execs(["\u{1233}"], "[^\u1234-\u{12345}]", "\u{1233}");

execl(["\u{12346}"], /[^\u1234-\u{12345}]/u, "\u{12346}");
execs(["\u{12346}"], "[^\u1234-\u{12345}]", "\u{12346}");
execl(null, /[\u1234-\u{12345}]/u, "\u{12346}");
execs(null, "[\u1234-\u{12345}]", "\u{12346}");

execl(["\u{12342}"], /[\u{12340}-\u{12345}]/u, "\u{12342}");
execs(["\u{12342}"], "[\u{12340}-\u{12345}]", "\u{12342}");
execl(["\u{12342}"], /[\ud808\udf40-\ud808\udf45]/u, "\u{12342}");
execs(["\u{12342}"], "[\ud808\udf40-\ud808\udf45]", "\u{12342}");
execl(null, /[^\u{12340}-\u{12345}]/u, "\u{12342}");
execs(null, "[^\u{12340}-\u{12345}]", "\u{12342}");
execl(null, /[^\ud808\udf40-\ud808\udf45]/u, "\u{12342}");
execs(null, "[^\ud808\udf40-\ud808\udf45]", "\u{12342}");

execl(["\u{ffff}"], /[\u{ff80}-\u{12345}]/u, "\u{ffff}");
execs(["\u{ffff}"], "[\u{ff80}-\u{12345}]", "\u{ffff}");
execl(["\u{ffff}"], /[\u{ff80}-\ud808\udf45]/u, "\u{ffff}");
execs(["\u{ffff}"], "[\u{ff80}-\ud808\udf45]", "\u{ffff}");
execl(null, /[^\u{ff80}-\u{12345}]/u, "\u{ffff}");
execs(null, "[^\u{ff80}-\u{12345}]", "\u{ffff}");
execl(null, /[^\u{ff80}-\ud808\udf45]/u, "\u{ffff}");
execs(null, "[^\u{ff80}-\ud808\udf45]", "\u{ffff}");

// Lone surrogate
execl(["\ud800"], /[^\u{ff80}-\u{12345}]/u, "\uff99\u{d800}A");
execs(["\udc00"], "[^\u{ff80}-\u{12345}]", "\uff99\u{dc00}A");
execl(["\udc01"], /[\u0100-\u{10ffff}]/u, "A\udc01");
execl(["\udc03"], /[\udc01-\udc03]/u, "\ud801\udc02\udc03");
execl(["\ud801"], /[\ud801-\ud803]/u, "\ud802\udc01\ud801");

// Paired sorrogate.
execl(null, /[^\u{ff80}-\u{12345}]/u, "\u{d800}\u{dc00}");
execs(null, "[^\u{ff80}-\u{12345}]", "\u{d800}\u{dc00}");
execl(["\ud800\udc00"], /[\u{ff80}-\u{12345}]/u, "\u{d800}\u{dc00}");
execs(["\ud800\udc00"], "[\u{ff80}-\u{12345}]", "\u{d800}\u{dc00}");
execl(["foo\u{10e6d}bar"], /foo\ud803\ude6dbar/u, "foo\u{10e6d}bar");

// Lone surrogates
execl(["\ud801\ud801"], /\ud801+/u, "\ud801\udc01\ud801\ud801");
execl(["\udc01\udc01"], /\udc01+/u, "\ud801\ud801\udc01\udc01\udc01");

execl(["\udc02\udc03A"], /\W\WA/u, "\ud801\udc01A\udc02\udc03A");
execl(["\ud801\ud802"], /\ud801./u, "\ud801\udc01\ud801\ud802");
execl(["\udc02\udc03A"], /[\ud800-\udfff][\ud800-\udfff]A/u,
      "\ud801\udc01A\udc02\udc03A");

// Character classes
execl(null, /\w/u, "\ud801\udc01");
execl(["\ud801"], /[^\w]/, "\ud801\udc01");
execl(["\ud801\udc01"], /[^\w]/u, "\ud801\udc01");
execl(["\ud801"], /\W/, "\ud801\udc01");
execl(["\ud801\udc01"], /\W/u, "\ud801\udc01");

execl(["\ud800X"], /.X/u, "\ud800XaX");
execl(["aX"], /.(?<!\ud800)X/u, "\ud800XaX");
execl(["aX"], /.(?<![\ud800-\ud900])X/u, "\ud800XaX");

execl(null, /[]/u, "\u1234");
execl(["0abc"], /[^]abc/u, "0abc");
execl(["\u1234abc"], /[^]abc/u, "\u1234abc");
execl(["\u{12345}abc"], /[^]abc/u, "\u{12345}abc");

execl(null, /[\u{0}-\u{1F444}]/u, "\ud83d\udfff");

// Backward matches of lone surrogates.
execl(["B", "\ud803A"], /(?<=([\ud800-\ud900]A))B/u,
      "\ud801\udc00AB\udc00AB\ud802\ud803AB");
execl(["B", "\udc00A"], /(?<=([\ud800-\u{10300}]A))B/u,
       "\ud801\udc00AB\udc00AB\ud802\ud803AB");
execl(["B", "\udc11A"], /(?<=([\udc00-\udd00]A))B/u,
      "\ud801\udc00AB\udc11AB\ud802\ud803AB");
execl(["X", "\ud800C"], /(?<=(\ud800\w))X/u,
      "\ud800\udc00AX\udc11BX\ud800\ud800CX");
execl(["C", "\ud800\ud800"], /(?<=(\ud800.))\w/u,
      "\ud800\udc00AX\udc11BX\ud800\ud800CX");
execl(["X", "\udc01C"], /(?<=(\udc01\w))X/u,
      "\ud800\udc01AX\udc11BX\udc01\udc01CX");
execl(["C", "\udc01\udc01"], /(?<=(\udc01.))./u,
      "\ud800\udc01AX\udc11BX\udc01\udc01CX");

var L = "\ud800";
var T = "\udc00";
var X = "X";

// Test string contains only match.
function testw(expect, src, subject) {
  var re = new RegExp("^" + src + "$", "u");
  assertEquals(expect, re.test(subject));
}

// Test string starts with match.
function tests(expect, src, subject) {
  var re = new RegExp("^" + src, "u");
  assertEquals(expect, re.test(subject));
}

testw(true, X, X);
testw(true, L, L);
testw(true, T, T);
testw(true, L + T, L + T);
testw(true, T + L, T + L);
testw(false, T, L + T);
testw(false, L, L + T);
testw(true, ".(?<=" + L + ")", L);
testw(true, ".(?<=" + T + ")", T);
testw(true, ".(?<=" + L + T + ")", L + T);
testw(true, ".(?<=" + L + T + ")", L + T);
tests(true, ".(?<=" + T + ")", T + L);
tests(false, ".(?<=" + L + ")", L + T);
tests(false, ".(?<=" + T + ")", L + T);
tests(true, "..(?<=" + T + ")", T + T + L);
tests(true, "..(?<=" + T + ")", X + T + L);
tests(true, "...(?<=" + L + ")", X + T + L);
tests(false, "...(?<=" + T + ")", X + L + T)
tests(true, "..(?<=" + L + T + ")", X + L + T)
tests(true, "..(?<=" + L + T + "(?<=" + L + T + "))", X + L + T);
tests(false, "..(?<=" + L + "(" + T + "))", X + L + T);
tests(false, ".*" + L, X + L + T);
tests(true, ".*" + L, X + L + L + T);
tests(false, ".*" + L, X + L + T + L + T);
tests(false, ".*" + T, X + L + T + L + T);
tests(true, ".*" + T, X + L + T + T + L + T);
