// META: title=Headers normalize values
// META: global=window,worker

"use strict";

const expectations = {
  "name1": [" space ", "space"],
  "name2": ["\ttab\t", "tab"],
  "name3": [" spaceAndTab\t", "spaceAndTab"],
  "name4": ["\r\n newLine", "newLine"], //obs-fold cases
  "name5": ["newLine\r\n ", "newLine"],
  "name6": ["\r\n\tnewLine", "newLine"],
  "name7": ["\t\f\tnewLine\n", "\f\tnewLine"],
  "name8": ["newLine\xa0", "newLine\xa0"], // \xa0 == non breaking space
};

test(function () {
  const headerDict = Object.fromEntries(
    Object.entries(expectations).map(([name, [actual]]) => [name, actual]),
  );
  var headers = new Headers(headerDict);
  for (const name in expectations) {
    const expected = expectations[name][1];
    assert_equals(
      headers.get(name),
      expected,
      "name: " + name + " has normalized value: " + expected,
    );
  }
}, "Create headers with not normalized values");

test(function () {
  var headers = new Headers();
  for (const name in expectations) {
    headers.append(name, expectations[name][0]);
    const expected = expectations[name][1];
    assert_equals(
      headers.get(name),
      expected,
      "name: " + name + " has value: " + expected,
    );
  }
}, "Check append method with not normalized values");

test(function () {
  var headers = new Headers();
  for (const name in expectations) {
    headers.set(name, expectations[name][0]);
    const expected = expectations[name][1];
    assert_equals(
      headers.get(name),
      expected,
      "name: " + name + " has value: " + expected,
    );
  }
}, "Check set method with not normalized values");
