// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function tag(strings, ...values) {
  let a = 0;
  for (let i = 0; i < strings.length; ++i) a += strings[i].length;
  return a;
}

function driver(n) {
  let result = 0;
  for (let i = 0; i < n; ++i) {
    result += tag`${"Hello"} ${"cruel"} ${"slow"} ${"world"}!\n`;
    result += tag`${"Why"} ${"is"} ${"this"} ${"so"} ${"damn"} ${"slow"}?!\n`;
  }
 return result;
}

function TaggedTemplate() {
  driver(1e4);
}

function TaggedTemplateWarmUp() {
  driver(1e1);
  driver(1e2);
  driver(1e3);
}

createSuite('TaggedTemplate', 10, TaggedTemplate, TaggedTemplateWarmUp);

var _templateObject = _taggedTemplateLiteralLoose(
  ["", " ", " ", " ", "!\n"],
  ["", " ", " ", " ", "!\\n"]
),
_templateObject2 = _taggedTemplateLiteralLoose(
  ["", " ", " ", " ", " ", " ", "?!\n"],
  ["", " ", " ", " ", " ", " ", "?!\\n"]
);

function _taggedTemplateLiteralLoose(strings, raw) {
  strings.raw = raw;
  return strings;
}

function driverLoose(n) {
  var result = 0;
  for (var i = 0; i < n; ++i) {
    result += tag(_templateObject, "Hello", "cruel", "slow", "world");
    result += tag(_templateObject2, "Why", "is", "this", "so", "damn", "slow");
  }
  return result;
}

function TaggedTemplateLoose() {
  driverLoose(1e4);
}

function TaggedTemplateLooseWarmUp() {
  driverLoose(1e1);
  driverLoose(1e2);
  driverLoose(1e3);
}

createSuite('TaggedTemplateLoose', 10, TaggedTemplateLoose, TaggedTemplateLooseWarmUp);
