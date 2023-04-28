// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertListFormat(listFormat, input) {
  try {
    let result = listFormat.format(input);
    assertEquals('string', typeof result);
    if (input) {
      for (var i = 0; i < input.length; i++) {
        assertTrue(result.indexOf(input[i]) >= 0);
      }
    }
  } catch (e) {
    fail('should not throw exception ' + e);
  }
}

function testFormatter(listFormat) {
  assertListFormat(listFormat, []);
  assertListFormat(listFormat, undefined);
  assertListFormat(listFormat, ['1']);
  assertListFormat(listFormat, ['a']);
  assertListFormat(listFormat, ['1', 'b']);
  assertListFormat(listFormat, ['1', 'b', '3']);
  assertListFormat(listFormat, ['a', 'b']);
  assertListFormat(listFormat, ['a', 'b', 'c']);
  assertListFormat(listFormat, ['a', 'b', 'c', 'd']);
  assertListFormat(listFormat, ['作者', '譚永鋒', '1', (new Date()).toString()]);
  assertListFormat(listFormat, ['作者', '譚永鋒', '1', 'b', '3']);

  assertThrows(() => listFormat.format(null), TypeError);
  assertThrows(() => listFormat.format([new Date()]), TypeError);
  assertThrows(() => listFormat.format([1]), TypeError);
  assertThrows(() => listFormat.format([1, 'b']), TypeError);
  assertThrows(() => listFormat.format([1, 'b', 3]), TypeError);
  assertThrows(() => listFormat.format([[3, 4]]), TypeError);
  assertThrows(() => listFormat.format([undefined, 'world']), TypeError);
  assertThrows(() => listFormat.format(['hello', undefined]), TypeError);
  assertThrows(() => listFormat.format([undefined]), TypeError);
  assertThrows(() => listFormat.format([null, 'world']), TypeError);
  assertThrows(() => listFormat.format(['hello', null]), TypeError);
  assertThrows(() => listFormat.format([null]), TypeError);

  // Test that Cons strings are handled correctly.
  let arr = ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m"];
  assertListFormat(listFormat, [arr + "n"]);
}
testFormatter(new Intl.ListFormat());
testFormatter(new Intl.ListFormat(["en"]));
testFormatter(new Intl.ListFormat(["en"], {style: 'long'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'short'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'narrow'}));
testFormatter(new Intl.ListFormat(["en"], {type: 'conjunction'}));
testFormatter(new Intl.ListFormat(["en"], {type: 'disjunction'}));
testFormatter(new Intl.ListFormat(["en"], {type: 'unit'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'long', type: 'conjunction'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'short', type: 'conjunction'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'narrow', type: 'conjunction'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'long', type: 'disjunction'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'short', type: 'disjunction'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'narrow', type: 'disjunction'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'long', type: 'unit'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'short', type: 'unit'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'narrow', type: 'unit'}));
