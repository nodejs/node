// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const simpleRegex = /simplepattern123/;
console.log(simpleRegex.test("this is a simplepattern123"));

const flagsRegex = /SomeMixedCasePattern/gi;
"A sentence with SomeMixedCasePattern and another SOMEMIXEDCASEPATTERN.".match(flagsRegex);

const replaced = "The file is image.jpeg".replace(/jpeg/, "png");

const index = "Find the word needle in this haystack".search(/needle/);

const complexRegex = /user-(id[0-9]+)-group([A-Z]+)/;
"user-id12345-groupABC".match(complexRegex);

const parts = "valueAseparatorvalueBseparatorvalueC".split(/separator/);
