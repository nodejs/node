// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug
// Test the mirror object for symbols.

function testSymbolMirror(symbol, description) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(symbol);
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertTrue(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.SymbolMirror);

  // Check the mirror properties.
  assertTrue(mirror.isSymbol());
  assertEquals(description, mirror.description());
  assertEquals('symbol', mirror.type());
  assertTrue(mirror.isPrimitive());
  var description_text = description === undefined ? "" : description;
  assertEquals('Symbol(' + description_text + ')', mirror.toText());
  assertSame(symbol, mirror.value());

  // Parse JSON representation and check.
  var fromJSON = eval('(' + json + ')');
  assertEquals('symbol', fromJSON.type);
  assertEquals(description, fromJSON.description);
}

// Test a number of different symbols.
testSymbolMirror(Symbol("a"), "a");
testSymbolMirror(Symbol(12), "12");
testSymbolMirror(Symbol.for("b"), "b");
testSymbolMirror(Symbol(), undefined);
