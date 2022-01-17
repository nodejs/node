// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ASCII case folding with two-byte subject string.
assertTrue(/[a]/ui.test("\u{20a0}a"));
assertTrue(/[a]/ui.test("\u{20a0}A"));
assertTrue(/[A]/ui.test("\u{20a0}a"));
assertTrue(/[A]/ui.test("\u{20a0}A"));

// Non-unicode use toUpperCase mappings.
assertFalse(/[\u00e5]/i.test("\u212b"));
assertFalse(/[\u212b]/i.test("\u00e5\u1234"));
assertFalse(/[\u212b]/i.test("\u00e5"));

assertTrue("\u212b".toLowerCase() == "\u00e5");
assertTrue("\u00c5".toLowerCase() == "\u00e5");
assertTrue("\u00e5".toUpperCase() == "\u00c5");

// Unicode uses case folding mappings.
assertFalse(/\u00e5/ui.test("\u212b"));
assertTrue(/\u00e5/ui.test("\u00c5"));
assertTrue(/\u00e5/ui.test("\u00e5"));
assertFalse(/\u00e5/ui.test("\u212b"));
assertTrue(/\u00c5/ui.test("\u00e5"));
assertFalse(/\u00c5/ui.test("\u212b"));
assertTrue(/\u00c5/ui.test("\u00c5"));
assertFalse(/\u212b/ui.test("\u00c5"));
assertFalse(/\u212b/ui.test("\u00e5"));
assertTrue(/\u212b/ui.test("\u212b"));

// Non-BMP.
assertFalse(/\u{10400}/i.test("\u{10428}"));
assertFalse(/\u{10400}/ui.test("\u{10428}"));
assertFalse(/\ud801\udc00/ui.test("\u{10428}"));
assertFalse(/[\u{10428}]/ui.test("\u{10400}"));
assertFalse(/[\ud801\udc28]/ui.test("\u{10400}"));
assertEquals(["\uff21\u{10400}"],
             /[\uff40-\u{10428}]+/ui.exec("\uff21\u{10400}abc"));

// TODO(v8:10120): Investigate why these don't behave as expected.
{
  // Should be:
  // assertEquals(["abc"], /[^\uff40-\u{10428}]+/ui.exec("\uff21\u{10400}abc\uff23"));
  //
  // But is:
  assertEquals(["\u{ff21}"], /[^\uff40-\u{10428}]+/ui.exec("\uff21\u{10400}abc\uff23"));
}

assertEquals(["\uff53\u24bb"],
             /[\u24d5-\uff33]+/ui.exec("\uff54\uff53\u24bb\u24ba"));

// Full mappings are ignored.
assertFalse(/\u00df/ui.test("SS"));
assertFalse(/\u1f8d/ui.test("\u1f05\u03b9"));

// Simple mappings.
assertFalse(/\u1f8d/ui.test("\u1f85"));

// Common mappings.
assertTrue(/\u1f6b/ui.test("\u1f63"));

// Back references.
assertNull(/(.)\1\1/ui.exec("\u00e5\u212b\u00c5"));
assertNull(/(.)\1/ui.exec("\u{118aa}\u{118ca}"));


// Non-Latin1 maps to Latin1.
assertNull(/^\u017F/ui.exec("s"));
assertNull(/^\u017F/ui.exec("s\u1234"));
assertNull(/^a[\u017F]/ui.exec("as"));
assertNull(/^a[\u017F]/ui.exec("as\u1234"));
