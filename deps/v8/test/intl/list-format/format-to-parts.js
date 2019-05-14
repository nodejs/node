// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertListFormat(listFormat, input) {
  var result;
  try {
    result = listFormat.formatToParts(input);
  } catch (e) {
    fail('should not throw exception ' + e);
  }
  assertTrue(Array.isArray(result));
  if (input) {
    assertTrue(result.length >= input.length * 2 - 1);
    for (var i = 0, j = 0; i < result.length; i++) {
      assertEquals('string', typeof result[i].value);
      assertEquals('string', typeof result[i].type);
      assertTrue(result[i].type == 'literal' || result[i].type == 'element');
      if (result[i].type == 'element') {
        assertEquals(String(input[j++]), result[i].value);
        if (i - 1 >= 0) {
          assertEquals('literal', result[i - 1].type);
        }
        if (i + 1 < result.length) {
          assertEquals('literal', result[i + 1].type);
        }
      }
      if (result[i].type == 'literal') {
        assertTrue(result[i].value.length > 0);
        if (i - 1 >= 0) {
          assertEquals('element', result[i - 1].type);
        }
        if (i + 1 < result.length) {
          assertEquals('element', result[i + 1].type);
        }
      }
    }
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
  // Tricky cases
  assertListFormat(listFormat, ['  ', 'b', 'c', 'and']);
  assertListFormat(listFormat, ['  ', 'b', 'c', 'or']);
  assertListFormat(listFormat, ['and']);
  assertListFormat(listFormat, ['or']);

  assertThrows(() => listFormat.formatToParts(null), TypeError);
  assertThrows(() => listFormat.formatToParts([new Date()]), TypeError);
  assertThrows(() => listFormat.formatToParts([1]), TypeError);
  assertThrows(() => listFormat.formatToParts([1, 'b']), TypeError);
  assertThrows(() => listFormat.formatToParts([1, 'b', 3]), TypeError);
  assertThrows(() => listFormat.formatToParts([[3, 4]]), TypeError);
  assertThrows(() => listFormat.formatToParts([undefined, 'world']), TypeError);
  assertThrows(() => listFormat.formatToParts(['hello', undefined]), TypeError);
  assertThrows(() => listFormat.formatToParts([undefined]), TypeError);
  assertThrows(() => listFormat.formatToParts([null, 'world']), TypeError);
  assertThrows(() => listFormat.formatToParts(['hello', null]), TypeError);
  assertThrows(() => listFormat.formatToParts([null]), TypeError);

}
testFormatter(new Intl.ListFormat());
testFormatter(new Intl.ListFormat(["en"]));
testFormatter(new Intl.ListFormat(["en"], {style: 'long'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'short'}));
assertThrows(() => new Intl.ListFormat(["en"], {style: 'narrow'}), RangeError);
testFormatter(new Intl.ListFormat(["en"], {type: 'conjunction'}));
testFormatter(new Intl.ListFormat(["en"], {type: 'disjunction'}));
testFormatter(new Intl.ListFormat(["en"], {type: 'unit'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'long', type: 'conjunction'}));
testFormatter(
    new Intl.ListFormat(["en"], {style: 'short', type: 'conjunction'}));
assertThrows(
    () => new Intl.ListFormat(
        ["en"], {style: 'narrow', type: 'conjunction'}), RangeError);
testFormatter(new Intl.ListFormat(["en"], {style: 'long', type: 'disjunction'}));
testFormatter(
    new Intl.ListFormat(["en"], {style: 'short', type: 'disjunction'}));
assertThrows(
    () => new Intl.ListFormat(
        ["en"], {style: 'narrow', type: 'disjunction'}), RangeError);
testFormatter(new Intl.ListFormat(["en"], {style: 'long', type: 'unit'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'short', type: 'unit'}));
testFormatter(new Intl.ListFormat(["en"], {style: 'narrow', type: 'unit'}));
