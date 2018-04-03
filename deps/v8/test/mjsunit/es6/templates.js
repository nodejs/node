// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var num = 5;
var str = "str";
function fn() { return "result"; }
var obj = {
  num: num,
  str: str,
  fn: function() { return "result"; }
};

(function testBasicExpressions() {
  assertEquals("foo 5 bar", `foo ${num} bar`);
  assertEquals("foo str bar", `foo ${str} bar`);
  assertEquals("foo [object Object] bar", `foo ${obj} bar`);
  assertEquals("foo result bar", `foo ${fn()} bar`);
  assertEquals("foo 5 bar", `foo ${obj.num} bar`);
  assertEquals("foo str bar", `foo ${obj.str} bar`);
  assertEquals("foo result bar", `foo ${obj.fn()} bar`);
})();

(function testExpressionsContainingTemplates() {
  assertEquals("foo bar 5", `foo ${`bar ${num}`}`);
})();

(function testMultilineTemplates() {
  assertEquals("foo\n    bar\n    baz", `foo
    bar
    baz`);

  assertEquals("foo\n  bar\n  baz", eval("`foo\r\n  bar\r  baz`"));
})();

(function testLineContinuation() {
  assertEquals("\n", `\

`);
})();

(function testTaggedTemplates() {
  var calls = 0;
  (function(s) {
    calls++;
  })`test`;
  assertEquals(1, calls);

  calls = 0;
  // assert tag is invoked in right context
  obj = {
    fn: function() {
      calls++;
      assertEquals(obj, this);
    }
  };

  obj.fn`test`;
  assertEquals(1, calls);

  calls = 0;
  // Simple templates only have a callSiteObj
  (function(s) {
    calls++;
    assertEquals(1, arguments.length);
  })`test`;
  assertEquals(1, calls);

  // Templates containing expressions have the values of evaluated expressions
  calls = 0;
  (function(site, n, s, o, f, r) {
    calls++;
    assertEquals(6, arguments.length);
    assertEquals("number", typeof n);
    assertEquals("string", typeof s);
    assertEquals("object", typeof o);
    assertEquals("function", typeof f);
    assertEquals("result", r);
  })`${num}${str}${obj}${fn}${fn()}`;
  assertEquals(1, calls);

  // The TV and TRV of NoSubstitutionTemplate :: `` is the empty code unit
  // sequence.
  calls = 0;
  (function(s) {
    calls++;
    assertEquals(1, s.length);
    assertEquals(1, s.raw.length);
    assertEquals("", s[0]);

    // Failure: expected <""> found <"foo  barfoo  barfoo foo foo foo testtest">
    assertEquals("", s.raw[0]);
  })``;
  assertEquals(1, calls);

  // The TV and TRV of TemplateHead :: `${ is the empty code unit sequence.
  calls = 0;
  (function(s) {
    calls++;
    assertEquals(2, s.length);
    assertEquals(2, s.raw.length);
    assertEquals("", s[0]);
    assertEquals("", s.raw[0]);
  })`${1}`;
  assertEquals(1, calls);

  // The TV and TRV of TemplateMiddle :: }${ is the empty code unit sequence.
  calls = 0;
  (function(s) {
    calls++;
    assertEquals(3, s.length);
    assertEquals(3, s.raw.length);
    assertEquals("", s[1]);
    assertEquals("", s.raw[1]);
  })`${1}${2}`;
  assertEquals(1, calls);

  // The TV and TRV of TemplateTail :: }` is the empty code unit sequence.
  calls = 0;
  (function(s) {
    calls++;
    assertEquals(2, s.length);
    assertEquals(2, s.raw.length);
    assertEquals("", s[1]);
    assertEquals("", s.raw[1]);
  })`${1}`;
  assertEquals(1, calls);

  // The TV of NoSubstitutionTemplate :: ` TemplateCharacters ` is the TV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("foo", s[0]); })`foo`;
  assertEquals(1, calls);

  // The TV of TemplateHead :: ` TemplateCharacters ${ is the TV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("foo", s[0]); })`foo${1}`;
  assertEquals(1, calls);

  // The TV of TemplateMiddle :: } TemplateCharacters ${ is the TV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("foo", s[1]); })`${1}foo${2}`;
  assertEquals(1, calls);

  // The TV of TemplateTail :: } TemplateCharacters ` is the TV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("foo", s[1]); })`${1}foo`;
  assertEquals(1, calls);

  // The TV of TemplateCharacters :: TemplateCharacter is the TV of
  // TemplateCharacter.
  calls = 0;
  (function(s) { calls++; assertEquals("f", s[0]); })`f`;
  assertEquals(1, calls);

  // The TV of TemplateCharacter :: $ is the code unit value 0x0024.
  calls = 0;
  (function(s) { calls++; assertEquals("$", s[0]); })`$`;
  assertEquals(1, calls);

  // The TV of TemplateCharacter :: \ EscapeSequence is the CV of
  // EscapeSequence.
  calls = 0;
  (function(s) { calls++; assertEquals("안녕", s[0]); })`\uc548\uB155`;
  (function(s) { calls++; assertEquals("\xff", s[0]); })`\xff`;
  (function(s) { calls++; assertEquals("\n", s[0]); })`\n`;
  assertEquals(3, calls);

  // The TV of TemplateCharacter :: LineContinuation is the TV of
  // LineContinuation. The TV of LineContinuation :: \ LineTerminatorSequence is
  // the empty code unit sequence.
  calls = 0;
  (function(s) { calls++; assertEquals("", s[0]); })`\
`;
  assertEquals(1, calls);

  // The TRV of NoSubstitutionTemplate :: ` TemplateCharacters ` is the TRV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("test", s.raw[0]); })`test`;
  assertEquals(1, calls);

  // The TRV of TemplateHead :: ` TemplateCharacters ${ is the TRV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("test", s.raw[0]); })`test${1}`;
  assertEquals(1, calls);

  // The TRV of TemplateMiddle :: } TemplateCharacters ${ is the TRV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("test", s.raw[1]); })`${1}test${2}`;
  assertEquals(1, calls);

  // The TRV of TemplateTail :: } TemplateCharacters ` is the TRV of
  // TemplateCharacters.
  calls = 0;
  (function(s) { calls++; assertEquals("test", s.raw[1]); })`${1}test`;
  assertEquals(1, calls);

  // The TRV of TemplateCharacters :: TemplateCharacter is the TRV of
  // TemplateCharacter.
  calls = 0;
  (function(s) { calls++; assertEquals("f", s.raw[0]); })`f`;
  assertEquals(1, calls);

  // The TRV of TemplateCharacter :: $ is the code unit value 0x0024.
  calls = 0;
  (function(s) { calls++; assertEquals("\u0024", s.raw[0]); })`$`;
  assertEquals(1, calls);

  // The TRV of EscapeSequence :: 0 is the code unit value 0x0030.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005C\u0030", s.raw[0]); })`\0`;
  assertEquals(1, calls);

  // The TRV of TemplateCharacter :: \ EscapeSequence is the sequence consisting
  // of the code unit value 0x005C followed by the code units of TRV of
  // EscapeSequence.

  //   The TRV of EscapeSequence :: HexEscapeSequence is the TRV of the
  //   HexEscapeSequence.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005Cxff", s.raw[0]); })`\xff`;
  assertEquals(1, calls);

  //   The TRV of EscapeSequence :: UnicodeEscapeSequence is the TRV of the
  //   UnicodeEscapeSequence.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005Cuc548", s.raw[0]); })`\uc548`;
  assertEquals(1, calls);

  //   The TRV of CharacterEscapeSequence :: SingleEscapeCharacter is the TRV of
  //   the SingleEscapeCharacter.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005C\u0027", s.raw[0]); })`\'`;
  (function(s) { calls++; assertEquals("\u005C\u0022", s.raw[0]); })`\"`;
  (function(s) { calls++; assertEquals("\u005C\u005C", s.raw[0]); })`\\`;
  (function(s) { calls++; assertEquals("\u005Cb", s.raw[0]); })`\b`;
  (function(s) { calls++; assertEquals("\u005Cf", s.raw[0]); })`\f`;
  (function(s) { calls++; assertEquals("\u005Cn", s.raw[0]); })`\n`;
  (function(s) { calls++; assertEquals("\u005Cr", s.raw[0]); })`\r`;
  (function(s) { calls++; assertEquals("\u005Ct", s.raw[0]); })`\t`;
  (function(s) { calls++; assertEquals("\u005Cv", s.raw[0]); })`\v`;
  (function(s) { calls++; assertEquals("\u005C`", s.raw[0]); })`\``;
  assertEquals(10, calls);

  //   The TRV of CharacterEscapeSequence :: NonEscapeCharacter is the CV of the
  //   NonEscapeCharacter.
  calls = 0;
  (function(s) { calls++; assertEquals("\u005Cz", s.raw[0]); })`\z`;
  assertEquals(1, calls);

  // The TRV of LineTerminatorSequence :: <LF> is the code unit value 0x000A.
  // The TRV of LineTerminatorSequence :: <CR> is the code unit value 0x000A.
  // The TRV of LineTerminatorSequence :: <CR><LF> is the sequence consisting of
  // the code unit value 0x000A.
  calls = 0;
  function testRawLineNormalization(cs) {
    calls++;
    assertEquals(cs.raw[0], "\n\n\n");
    assertEquals(cs.raw[1], "\n\n\n");
  }
  eval("testRawLineNormalization`\r\n\n\r${1}\r\n\n\r`");
  assertEquals(1, calls);

  // The TRV of LineContinuation :: \ LineTerminatorSequence is the sequence
  // consisting of the code unit value 0x005C followed by the code units of TRV
  // of LineTerminatorSequence.
  calls = 0;
  function testRawLineContinuation(cs) {
    calls++;
    assertEquals(cs.raw[0], "\u005C\n\u005C\n\u005C\n");
    assertEquals(cs.raw[1], "\u005C\n\u005C\n\u005C\n");
  }
  eval("testRawLineContinuation`\\\r\n\\\n\\\r${1}\\\r\n\\\n\\\r`");
  assertEquals(1, calls);
})();


(function testCallSiteObj() {
  var calls = 0;
  function tag(cs) {
    calls++;
    assertTrue(cs.hasOwnProperty("raw"));
    assertTrue(Object.isFrozen(cs));
    assertTrue(Object.isFrozen(cs.raw));
    var raw = Object.getOwnPropertyDescriptor(cs, "raw");
    assertFalse(raw.writable);
    assertFalse(raw.configurable);
    assertFalse(raw.enumerable);
    assertEquals(Array.prototype, Object.getPrototypeOf(cs.raw));
    assertTrue(Array.isArray(cs.raw));
    assertEquals(Array.prototype, Object.getPrototypeOf(cs));
    assertTrue(Array.isArray(cs));

    var cooked0 = Object.getOwnPropertyDescriptor(cs, "0");
    assertFalse(cooked0.writable);
    assertFalse(cooked0.configurable);
    assertTrue(cooked0.enumerable);

    var raw0 = Object.getOwnPropertyDescriptor(cs.raw, "0");
    assertFalse(raw0.writable);
    assertFalse(raw0.configurable);
    assertTrue(raw0.enumerable);

    var length = Object.getOwnPropertyDescriptor(cs, "length");
    assertFalse(length.writable);
    assertFalse(length.configurable);
    assertFalse(length.enumerable);

    length = Object.getOwnPropertyDescriptor(cs.raw, "length");
    assertFalse(length.writable);
    assertFalse(length.configurable);
    assertFalse(length.enumerable);
  }
  tag`${1}`;
  assertEquals(1, calls);
})();


(function testUTF16ByteOrderMark() {
  assertEquals("\uFEFFtest", `\uFEFFtest`);
  assertEquals("\uFEFFtest", eval("`\uFEFFtest`"));
})();


(function testStringRawAsTagFn() {
  assertEquals("\\u0065\\`\\r\\r\\n\\ntestcheck",
               String.raw`\u0065\`\r\r\n\n${"test"}check`);
  assertEquals("\\\n\\\n\\\n", eval("String.raw`\\\r\\\r\n\\\n`"));
  assertEquals("", String.raw``);
})();


(function testCallSiteCaching() {
  var callSites = [];
  function tag(cs) { callSites.push(cs); }
  var a = 1;
  var b = 2;

  tag`head${a}tail`;
  tag`head${b}tail`;

  assertEquals(2, callSites.length);
  assertSame(callSites[0], callSites[1]);

  eval("tag`head${a}tail`");
  assertEquals(3, callSites.length);
  assertSame(callSites[1], callSites[2]);

  eval("tag`head${b}tail`");
  assertEquals(4, callSites.length);
  assertSame(callSites[2], callSites[3]);

  (new Function("tag", "a", "b", "return tag`head${a}tail`;"))(tag, 1, 2);
  assertEquals(5, callSites.length);
  assertSame(callSites[3], callSites[4]);

  (new Function("tag", "a", "b", "return tag`head${b}tail`;"))(tag, 1, 2);
  assertEquals(6, callSites.length);
  assertSame(callSites[4], callSites[5]);

  callSites = [];

  tag`foo${a}bar`;
  tag`foo\${.}bar`;
  assertEquals(2, callSites.length);
  assertEquals(2, callSites[0].length);
  assertEquals(1, callSites[1].length);

  callSites = [];

  eval("tag`\\\r\n\\\n\\\r`");
  eval("tag`\\\r\n\\\n\\\r`");
  assertEquals(2, callSites.length);
  assertSame(callSites[0], callSites[1]);
  assertEquals("", callSites[0][0]);
  assertEquals("\\\n\\\n\\\n", callSites[0].raw[0]);

  callSites = [];

  tag`\uc548\ub155`;
  tag`\uc548\ub155`;
  assertEquals(2, callSites.length);
  assertSame(callSites[0], callSites[1]);
  assertEquals("안녕", callSites[0][0]);
  assertEquals("\\uc548\\ub155", callSites[0].raw[0]);

  callSites = [];

  tag`\uc548\ub155`;
  tag`안녕`;
  assertEquals(2, callSites.length);
  assertTrue(callSites[0] !== callSites[1]);
  assertEquals("안녕", callSites[0][0]);
  assertEquals("\\uc548\\ub155", callSites[0].raw[0]);
  assertEquals("안녕", callSites[1][0]);
  assertEquals("안녕", callSites[1].raw[0]);

  // Extra-thorough UTF8 decoding test.
  callSites = [];

  tag`Iñtërnâtiônàlizætiøn\u2603\uD83D\uDCA9`;
  tag`Iñtërnâtiônàlizætiøn☃💩`;

  assertEquals(2, callSites.length);
  assertTrue(callSites[0] !== callSites[1]);
  assertEquals("Iñtërnâtiônàlizætiøn☃💩", callSites[0][0]);
  assertEquals(
      "Iñtërnâtiônàlizætiøn\\u2603\\uD83D\\uDCA9", callSites[0].raw[0]);
  assertEquals("Iñtërnâtiônàlizætiøn☃💩", callSites[1][0]);
  assertEquals("Iñtërnâtiônàlizætiøn☃💩", callSites[1].raw[0]);
})();


(function testExtendedArrayPrototype() {
  Object.defineProperty(Array.prototype, 0, {
    set: function() {
      assertUnreachable();
    },
    configurable: true
  });
  function tag(){}
  tag`a${1}b`;
  delete Array.prototype[0];
})();


(function testRawLineNormalization() {
  function raw0(callSiteObj) {
    return callSiteObj.raw[0];
  }
  assertEquals(eval("raw0`\r`"), "\n");
  assertEquals(eval("raw0`\r\n`"), "\n");
  assertEquals(eval("raw0`\r\r\n`"), "\n\n");
  assertEquals(eval("raw0`\r\n\r\n`"), "\n\n");
  assertEquals(eval("raw0`\r\r\r\n`"), "\n\n\n");
})();


(function testHarmonyUnicode() {
  function raw0(callSiteObj) {
    return callSiteObj.raw[0];
  }
  assertEquals(raw0`a\u{62}c`, "a\\u{62}c");
  assertEquals(raw0`a\u{000062}c`, "a\\u{000062}c");
  assertEquals(raw0`a\u{0}c`, "a\\u{0}c");

  assertEquals(`a\u{62}c`, "abc");
  assertEquals(`a\u{000062}c`, "abc");
})();


(function testLiteralAfterRightBrace() {
  // Regression test for https://code.google.com/p/v8/issues/detail?id=3734
  function f() {}
  `abc`;

  function g() {}`def`;

  {
    // block
  }
  `ghi`;

  {
    // block
  }`jkl`;
})();


(function testLegacyOctal() {
  assertEquals('\u0000', `\0`);
  assertEquals('\u0000a', `\0a`);
  for (var i = 0; i < 10; i++) {
    var code = "`\\0" + i + "`";
    assertThrows(code, SyntaxError);
    // Not an error if tagged.
    code = "(function(){})" + code;
    assertDoesNotThrow(code, SyntaxError);
  }

  assertEquals('\\0', String.raw`\0`);
})();


(function testSyntaxErrorsNonEscapeCharacter() {
  assertThrows("`\\x`", SyntaxError);
  assertThrows("`\\u`", SyntaxError);
  for (var i = 1; i < 8; i++) {
    var code = "`\\" + i + "`";
    assertThrows(code, SyntaxError);
    // Not an error if tagged.
    code = "(function(){})" + code;
    assertDoesNotThrow(code, SyntaxError);
  }
})();


(function testValidNumericEscapes() {
  assertEquals("8", `\8`);
  assertEquals("9", `\9`);
})();


(function testLegacyOctalEscapesInExpressions() {
  // Allowed in sloppy expression
  assertEquals("\x07", `${"\07"}`);

  // Disallowed in template tail
  assertThrows("`${\"\\07\"}\\07`", SyntaxError);

  // Disallowed in strict expression
  assertThrows("`${(function() { \"use strict\"; return \"\\07\"; })()}`",
               SyntaxError);
})();


var global = this;
(function testCallNew() {
  "use strict";
  var called = false;
  var calledWith;
  global.log = function(x) { called = true; calledWith = x; }

  assertInstanceof(new Function`log("test")`, Object);
  assertTrue(called);
  assertSame("test", calledWith);
  delete global.log;
})();


(function testCallNew2() {
  "use strict";
  var log = [];
  function tag(x) {
    log.push(x);
    if (!(this instanceof tag)) {
      return tag;
    }
    this.x = x === void 0 ? null : x;
    return this;
  }
  // No arguments passed to constructor
  var instance = new tag`x``y``z`;
  assertInstanceof(instance, tag);
  assertSame(tag.prototype, Object.getPrototypeOf(instance));
  assertEquals({ x: null }, instance);
  assertEquals([["x"], ["y"], ["z"], undefined], log);

  // Arguments passed to constructor
  log.length = 0;
  instance = new tag`x2` `y2` `z2` (`test`);
  assertInstanceof(instance, tag);
  assertSame(tag.prototype, Object.getPrototypeOf(instance));
  assertEquals({ x: "test" }, instance);
  assertEquals([["x2"], ["y2"], ["z2"], "test"], log);
})();


(function testCallResultOfTagFn() {
  "use strict";
  var i = 0;
  var raw = [];
  function tag(cs) {
    var args = Array.prototype.slice.call(arguments);
    var text = String.raw.apply(null, args);
    if (i++ < 2) {
      raw.push("tag;" + text);
      return tag;
    }

    raw.push("raw;" + text);
    return text;
  }
  assertEquals("test3", tag`test1``test2``test3`);
  assertEquals([
    "tag;test1",
    "tag;test2",
    "raw;test3"
  ], raw);
})();


(function testReturnValueAsTagFn() {
  "use strict";
  var i = 0;
  function makeTag() {
    return function tag(cs) {
      var args = Array.prototype.slice.call(arguments, 1);
      var rcs = [];
      rcs.raw = cs.map(function(s) {
        return '!' + s + '!';
      });
      args.unshift(rcs);
      return String.raw.apply(null, args);
    }
  }
  assertEquals('!hi!', makeTag()`hi`);
  assertEquals('!test!0!test!', makeTag()`test${0}test`);
  assertEquals('!!', makeTag()``);
});


(function testToStringSubstitutions() {
  var a = {
    toString: function() { return "a"; },
    valueOf: function() { return "-a-"; }
  };
  var b = {
    toString: function() { return "b"; },
    valueOf: function() { return "-b-"; }
  };
  assertEquals("a", `${a}`);
  assertEquals("ab", `${a}${b}`);
  assertEquals("-a--b-", `${a + b}`);
  assertEquals("-a-", `${a + ""}`);
  assertEquals("1a", `1${a}`);
  assertEquals("1a2", `1${a}2`);
  assertEquals("1a2b", `1${a}2${b}`);
  assertEquals("1a2b3", `1${a}2${b}3`);
})();


(function testToStringSubstitutionsOrder() {
  var subs = [];
  var log = [];
  function getter(name, value) {
    return {
      get: function() {
        log.push("get" + name);
        return value;
      },
      set: function(v) {
        log.push("set" + name);
      }
    };
  }
  Object.defineProperties(subs, {
    0: getter(0, "a"),
    1: getter(1, "b"),
    2: getter(2, "c")
  });

  assertEquals("-a-b-c-", `-${subs[0]}-${subs[1]}-${subs[2]}-`);
  assertArrayEquals(["get0", "get1", "get2"], log);
})();


(function testTaggedToStringSubstitutionsOrder() {
  var subs = [];
  var log = [];
  var tagged = [];
  function getter(name, value) {
    return {
      get: function() {
        log.push("get" + name);
        return value;
      },
      set: function(v) {
        log.push("set" + name);
      }
    };
  }
  Object.defineProperties(subs, {
    0: getter(0, 1),
    1: getter(1, 2),
    2: getter(2, 3)
  });

  function tag(cs) {
    var n_substitutions = arguments.length - 1;
    var n_cooked = cs.length;
    var e = cs[0];
    var i = 0;
    assertEquals(n_cooked, n_substitutions + 1);
    while (i < n_substitutions) {
      var sub = arguments[i++ + 1];
      var tail = cs[i];
      tagged.push(sub);
      e = e.concat(sub, tail);
    }
    return e;
  }

  assertEquals("-1-2-3-", tag`-${subs[0]}-${subs[1]}-${subs[2]}-`);
  assertArrayEquals(["get0", "get1", "get2"], log);
  assertArrayEquals([1, 2, 3], tagged);

  tagged.length = 0;
  log.length = 0;
  assertEquals("-1-", tag`-${subs[0]}-`);
  assertArrayEquals(["get0"], log);
  assertArrayEquals([1], tagged);
})();


// Since the first argument to the tag function is always an array,
// eval calls will always just return that array.
(function testEvalTagStrict() {
  "use strict";
  var f = (x) => eval`a${x}b`;
  var result = f();
  assertEquals(["a", "b"], result);
  assertSame(result, f());
})();


(function testEvalTagSloppy() {
  var f = (x) => eval`a${x}b`;
  var result = f();
  assertEquals(["a", "b"], result);
  assertSame(result, f());
})();

(function testTaggedTemplateInvalidAssignmentTargetStrict() {
  "use strict";
  function f() {}
  assertThrows(() => Function("++f`foo`"), ReferenceError);
  assertThrows(() => Function("f`foo`++"), ReferenceError);
  assertThrows(() => Function("--f`foo`"), ReferenceError);
  assertThrows(() => Function("f`foo`--"), ReferenceError);
  assertThrows(() => Function("f`foo` = 1"), ReferenceError);
})();

(function testTaggedTemplateInvalidAssignmentTargetSloppy() {
  function f() {}
  assertThrows(() => Function("++f`foo`"), ReferenceError);
  assertThrows(() => Function("f`foo`++"), ReferenceError);
  assertThrows(() => Function("--f`foo`"), ReferenceError);
  assertThrows(() => Function("f`foo`--"), ReferenceError);
  assertThrows(() => Function("f`foo` = 1"), ReferenceError);
})();
