// META: title=Headers structure
// META: global=window,worker

"use strict";

test(function() {
  new Headers();
}, "Create headers from no parameter");

test(function() {
  new Headers(undefined);
}, "Create headers from undefined parameter");

test(function() {
  new Headers({});
}, "Create headers from empty object");

var parameters = [null, 1];
parameters.forEach(function(parameter) {
  test(function() {
    assert_throws_js(TypeError, function() { new Headers(parameter) });
  }, "Create headers with " + parameter + " should throw");
});

var headerDict = {"name1": "value1",
                  "name2": "value2",
                  "name3": "value3",
                  "name4": null,
                  "name5": undefined,
                  "name6": 1,
                  "Content-Type": "value4"
};

var headerSeq = [];
for (var name in headerDict)
  headerSeq.push([name, headerDict[name]]);

test(function() {
  var headers = new Headers(headerSeq);
  for (name in headerDict) {
    assert_equals(headers.get(name), String(headerDict[name]),
      "name: " + name + " has value: " + headerDict[name]);
  }
  assert_equals(headers.get("length"), null, "init should be treated as a sequence, not as a dictionary");
}, "Create headers with sequence");

test(function() {
  var headers = new Headers(headerDict);
  for (name in headerDict) {
    assert_equals(headers.get(name), String(headerDict[name]),
      "name: " + name + " has value: " + headerDict[name]);
  }
}, "Create headers with record");

test(function() {
  var headers = new Headers(headerDict);
  var headers2 = new Headers(headers);
  for (name in headerDict) {
    assert_equals(headers2.get(name), String(headerDict[name]),
      "name: " + name + " has value: " + headerDict[name]);
  }
}, "Create headers with existing headers");

test(function() {
  var headers = new Headers()
  headers[Symbol.iterator] = function *() {
    yield ["test", "test"]
  }
  var headers2 = new Headers(headers)
  assert_equals(headers2.get("test"), "test")
}, "Create headers with existing headers with custom iterator");

test(function() {
  var headers = new Headers();
  for (name in headerDict) {
    headers.append(name, headerDict[name]);
    assert_equals(headers.get(name), String(headerDict[name]),
      "name: " + name + " has value: " + headerDict[name]);
  }
}, "Check append method");

test(function() {
  var headers = new Headers();
  for (name in headerDict) {
    headers.set(name, headerDict[name]);
    assert_equals(headers.get(name), String(headerDict[name]),
      "name: " + name + " has value: " + headerDict[name]);
  }
}, "Check set method");

test(function() {
  var headers = new Headers(headerDict);
  for (name in headerDict)
    assert_true(headers.has(name),"headers has name " + name);

  assert_false(headers.has("nameNotInHeaders"),"headers do not have header: nameNotInHeaders");
}, "Check has method");

test(function() {
  var headers = new Headers(headerDict);
  for (name in headerDict) {
    assert_true(headers.has(name),"headers have a header: " + name);
    headers.delete(name)
    assert_true(!headers.has(name),"headers do not have anymore a header: " + name);
  }
}, "Check delete method");

test(function() {
  var headers = new Headers(headerDict);
  for (name in headerDict)
    assert_equals(headers.get(name), String(headerDict[name]),
      "name: " + name + " has value: " + headerDict[name]);

  assert_equals(headers.get("nameNotInHeaders"), null, "header: nameNotInHeaders has no value");
}, "Check get method");

var headerEntriesDict = {"name1": "value1",
                          "Name2": "value2",
                          "name": "value3",
                          "content-Type": "value4",
                          "Content-Typ": "value5",
                          "Content-Types": "value6"
};
var sortedHeaderDict = {};
var headerValues = [];
var sortedHeaderKeys = Object.keys(headerEntriesDict).map(function(value) {
  sortedHeaderDict[value.toLowerCase()] = headerEntriesDict[value];
  headerValues.push(headerEntriesDict[value]);
  return value.toLowerCase();
}).sort();

var iteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));
function checkIteratorProperties(iterator) {
  var prototype = Object.getPrototypeOf(iterator);
  assert_equals(Object.getPrototypeOf(prototype), iteratorPrototype);

  var descriptor = Object.getOwnPropertyDescriptor(prototype, "next");
  assert_true(descriptor.configurable, "configurable");
  assert_true(descriptor.enumerable, "enumerable");
  assert_true(descriptor.writable, "writable");
}

test(function() {
  var headers = new Headers(headerEntriesDict);
  var actual = headers.keys();
  checkIteratorProperties(actual);

  sortedHeaderKeys.forEach(function(key) {
      const entry = actual.next();
      assert_false(entry.done);
      assert_equals(entry.value, key);
  });
  assert_true(actual.next().done);
  assert_true(actual.next().done);

  for (const key of headers.keys())
      assert_true(sortedHeaderKeys.indexOf(key) != -1);
}, "Check keys method");

test(function() {
  var headers = new Headers(headerEntriesDict);
  var actual = headers.values();
  checkIteratorProperties(actual);

  sortedHeaderKeys.forEach(function(key) {
      const entry = actual.next();
      assert_false(entry.done);
      assert_equals(entry.value, sortedHeaderDict[key]);
  });
  assert_true(actual.next().done);
  assert_true(actual.next().done);

  for (const value of headers.values())
      assert_true(headerValues.indexOf(value) != -1);
}, "Check values method");

test(function() {
  var headers = new Headers(headerEntriesDict);
  var actual = headers.entries();
  checkIteratorProperties(actual);

  sortedHeaderKeys.forEach(function(key) {
      const entry = actual.next();
      assert_false(entry.done);
      assert_equals(entry.value[0], key);
      assert_equals(entry.value[1], sortedHeaderDict[key]);
  });
  assert_true(actual.next().done);
  assert_true(actual.next().done);

  for (const entry of headers.entries())
      assert_equals(entry[1], sortedHeaderDict[entry[0]]);
}, "Check entries method");

test(function() {
  var headers = new Headers(headerEntriesDict);
  var actual = headers[Symbol.iterator]();

  sortedHeaderKeys.forEach(function(key) {
      const entry = actual.next();
      assert_false(entry.done);
      assert_equals(entry.value[0], key);
      assert_equals(entry.value[1], sortedHeaderDict[key]);
  });
  assert_true(actual.next().done);
  assert_true(actual.next().done);
}, "Check Symbol.iterator method");

test(function() {
  var headers = new Headers(headerEntriesDict);
  var reference = sortedHeaderKeys[Symbol.iterator]();
  headers.forEach(function(value, key, container) {
      assert_equals(headers, container);
      const entry = reference.next();
      assert_false(entry.done);
      assert_equals(key, entry.value);
      assert_equals(value, sortedHeaderDict[entry.value]);
  });
  assert_true(reference.next().done);
}, "Check forEach method");

test(() => {
  const headers = new Headers({"foo": "2", "baz": "1", "BAR": "0"});
  const actualKeys = [];
  const actualValues = [];
  for (const [header, value] of headers) {
    actualKeys.push(header);
    actualValues.push(value);
    headers.delete("foo");
  }
  assert_array_equals(actualKeys, ["bar", "baz"]);
  assert_array_equals(actualValues, ["0", "1"]);
}, "Iteration skips elements removed while iterating");

test(() => {
  const headers = new Headers({"foo": "2", "baz": "1", "BAR": "0", "quux": "3"});
  const actualKeys = [];
  const actualValues = [];
  for (const [header, value] of headers) {
    actualKeys.push(header);
    actualValues.push(value);
    if (header === "baz")
      headers.delete("bar");
  }
  assert_array_equals(actualKeys, ["bar", "baz", "quux"]);
  assert_array_equals(actualValues, ["0", "1", "3"]);
}, "Removing elements already iterated over causes an element to be skipped during iteration");

test(() => {
  const headers = new Headers({"foo": "2", "baz": "1", "BAR": "0", "quux": "3"});
  const actualKeys = [];
  const actualValues = [];
  for (const [header, value] of headers) {
    actualKeys.push(header);
    actualValues.push(value);
    if (header === "baz")
      headers.append("X-yZ", "4");
  }
  assert_array_equals(actualKeys, ["bar", "baz", "foo", "quux", "x-yz"]);
  assert_array_equals(actualValues, ["0", "1", "2", "3", "4"]);
}, "Appending a value pair during iteration causes it to be reached during iteration");

test(() => {
  const headers = new Headers({"foo": "2", "baz": "1", "BAR": "0", "quux": "3"});
  const actualKeys = [];
  const actualValues = [];
  for (const [header, value] of headers) {
    actualKeys.push(header);
    actualValues.push(value);
    if (header === "baz")
      headers.append("abc", "-1");
  }
  assert_array_equals(actualKeys, ["bar", "baz", "baz", "foo", "quux"]);
  assert_array_equals(actualValues, ["0", "1", "1", "2", "3"]);
}, "Prepending a value pair before the current element position causes it to be skipped during iteration and adds the current element a second time");
