// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests taken from:
// http://mathias.html5.org/tests/javascript/string/

assertEquals('_'.anchor('b'), '<a name="b">_</a>');
assertEquals('<'.anchor('<'), '<a name="<"><</a>');
assertEquals('_'.anchor(0x2A), '<a name="42">_</a>');
assertEquals('_'.anchor('\x22'), '<a name="&quot;">_</a>');
assertEquals(String.prototype.anchor.call(0x2A, 0x2A), '<a name="42">42</a>');
assertThrows(function() {
  String.prototype.anchor.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.anchor.call(null);
}, TypeError);
assertEquals(String.prototype.anchor.length, 1);

assertEquals('_'.big(), '<big>_</big>');
assertEquals('<'.big(), '<big><</big>');
assertEquals(String.prototype.big.call(0x2A), '<big>42</big>');
assertThrows(function() {
  String.prototype.big.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.big.call(null);
}, TypeError);
assertEquals(String.prototype.big.length, 0);

assertEquals('_'.blink(), '<blink>_</blink>');
assertEquals('<'.blink(), '<blink><</blink>');
assertEquals(String.prototype.blink.call(0x2A), '<blink>42</blink>');
assertThrows(function() {
  String.prototype.blink.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.blink.call(null);
}, TypeError);
assertEquals(String.prototype.blink.length, 0);

assertEquals('_'.bold(), '<b>_</b>');
assertEquals('<'.bold(), '<b><</b>');
assertEquals(String.prototype.bold.call(0x2A), '<b>42</b>');
assertThrows(function() {
  String.prototype.bold.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.bold.call(null);
}, TypeError);
assertEquals(String.prototype.bold.length, 0);

assertEquals('_'.fixed(), '<tt>_</tt>');
assertEquals('<'.fixed(), '<tt><</tt>');
assertEquals(String.prototype.fixed.call(0x2A), '<tt>42</tt>');
assertThrows(function() {
  String.prototype.fixed.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.fixed.call(null);
}, TypeError);
assertEquals(String.prototype.fixed.length, 0);

assertEquals('_'.fontcolor('b'), '<font color="b">_</font>');
assertEquals('<'.fontcolor('<'), '<font color="<"><</font>');
assertEquals('_'.fontcolor(0x2A), '<font color="42">_</font>');
assertEquals('_'.fontcolor('\x22'), '<font color="&quot;">_</font>');
assertEquals(String.prototype.fontcolor.call(0x2A, 0x2A),
  '<font color="42">42</font>');
assertThrows(function() {
  String.prototype.fontcolor.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.fontcolor.call(null);
}, TypeError);
assertEquals(String.prototype.fontcolor.length, 1);

assertEquals('_'.fontsize('b'), '<font size="b">_</font>');
assertEquals('<'.fontsize('<'), '<font size="<"><</font>');
assertEquals('_'.fontsize(0x2A), '<font size="42">_</font>');
assertEquals('_'.fontsize('\x22'), '<font size="&quot;">_</font>');
assertEquals(String.prototype.fontsize.call(0x2A, 0x2A),
  '<font size="42">42</font>');
assertThrows(function() {
  String.prototype.fontsize.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.fontsize.call(null);
}, TypeError);
assertEquals(String.prototype.fontsize.length, 1);

assertEquals('_'.italics(), '<i>_</i>');
assertEquals('<'.italics(), '<i><</i>');
assertEquals(String.prototype.italics.call(0x2A), '<i>42</i>');
assertThrows(function() {
  String.prototype.italics.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.italics.call(null);
}, TypeError);
assertEquals(String.prototype.italics.length, 0);

assertEquals('_'.link('b'), '<a href="b">_</a>');
assertEquals('<'.link('<'), '<a href="<"><</a>');
assertEquals('_'.link(0x2A), '<a href="42">_</a>');
assertEquals('_'.link('\x22'), '<a href="&quot;">_</a>');
assertEquals(String.prototype.link.call(0x2A, 0x2A), '<a href="42">42</a>');
assertThrows(function() {
  String.prototype.link.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.link.call(null);
}, TypeError);
assertEquals(String.prototype.link.length, 1);

assertEquals('_'.small(), '<small>_</small>');
assertEquals('<'.small(), '<small><</small>');
assertEquals(String.prototype.small.call(0x2A), '<small>42</small>');
assertThrows(function() {
  String.prototype.small.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.small.call(null);
}, TypeError);
assertEquals(String.prototype.small.length, 0);

assertEquals('_'.strike(), '<strike>_</strike>');
assertEquals('<'.strike(), '<strike><</strike>');
assertEquals(String.prototype.strike.call(0x2A), '<strike>42</strike>');
assertThrows(function() {
  String.prototype.strike.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.strike.call(null);
}, TypeError);
assertEquals(String.prototype.strike.length, 0);

assertEquals('_'.sub(), '<sub>_</sub>');
assertEquals('<'.sub(), '<sub><</sub>');
assertEquals(String.prototype.sub.call(0x2A), '<sub>42</sub>');
assertThrows(function() {
  String.prototype.sub.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.sub.call(null);
}, TypeError);
assertEquals(String.prototype.sub.length, 0);

assertEquals('_'.sup(), '<sup>_</sup>');
assertEquals('<'.sup(), '<sup><</sup>');
assertEquals(String.prototype.sup.call(0x2A), '<sup>42</sup>');
assertThrows(function() {
  String.prototype.sup.call(undefined);
}, TypeError);
assertThrows(function() {
  String.prototype.sup.call(null);
}, TypeError);
assertEquals(String.prototype.sup.length, 0);


(function TestToString() {
  var calls = 0;
  var obj = {
    toString() {
      calls++;
      return 'abc';
    },
    valueOf() {
      assertUnreachable();
    }
  };

  var methodNames = [
    'anchor',
    'big',
    'blink',
    'bold',
    'fixed',
    'fontcolor',
    'fontsize',
    'italics',
    'link',
    'small',
    'strike',
    'sub',
    'sup',
  ];
  for (var name of methodNames) {
    calls = 0;
    String.prototype[name].call(obj);
    assertEquals(1, calls);
  }
})();


(function TestDeleteStringRelace() {
  assertEquals('<a name="n">s</a>', 's'.anchor('n'));
  assertTrue(delete String.prototype.replace);
  assertEquals('<a name="n">s</a>', 's'.anchor('n'));
})();
