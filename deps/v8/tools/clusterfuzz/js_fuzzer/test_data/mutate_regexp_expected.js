// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: mutate_unicode_regexp.js
const simpleRegex = /\u0073\u0069\x6d\x70\u006c\u0065\x70\u0061\u{74}\u0074\u{65}\u0072\u006e\u{31}\x32\x33/u;
console.log(simpleRegex.test("this is a simplepattern123"));
const flagsRegex = /\u{53}\u006f\u006d\x65\x4d\u0069\u{78}\x65\x64\u{43}\u{61}\u{73}\u0065\u{50}\x61\u0074\x74\u{65}\x72\x6e/giu;
"A sentence with SomeMixedCasePattern and another SOMEMIXEDCASEPATTERN.".match(flagsRegex);
const replaced = "The file is image.jpeg".replace(/\x6a\u0070\x65\u{67}/u, "png");
const index = "Find the word needle in this haystack".search(/\x6e\u0065\u0065\u0064\x6c\u{65}/u);
const complexRegex = /\x75\u0073\x65\u0072-(\u{69}\u0064[\u{30}-\u{39}]+)-\x67\u{72}\x6f\u0075\x70([\u0041-\u{5a}]+)/u;
"user-id12345-groupABC".match(complexRegex);
const parts = "valueAseparatorvalueBseparatorvalueC".split(/\x73\x65\u0070\u{61}\u0072\u0061\u0074\u006f\u{72}/u);
