// META: title=Headers errors
// META: global=window,worker

"use strict";

test(function() {
  assert_throws_js(TypeError, function() { new Headers([["name"]]); });
}, "Create headers giving an array having one string as init argument");

test(function() {
  assert_throws_js(TypeError, function() { new Headers([["invalid", "invalidValue1", "invalidValue2"]]); });
}, "Create headers giving an array having three strings as init argument");

test(function() {
  assert_throws_js(TypeError, function() { new Headers([["invalidĀ", "Value1"]]); });
}, "Create headers giving bad header name as init argument");

test(function() {
  assert_throws_js(TypeError, function() { new Headers([["name", "invalidValueĀ"]]); });
}, "Create headers giving bad header value as init argument");

var badNames = ["invalidĀ", {}];
var badValues = ["invalidĀ"];

badNames.forEach(function(name) {
  test(function() {
    var headers = new Headers();
    assert_throws_js(TypeError, function() { headers.get(name); });
  }, "Check headers get with an invalid name " + name);
});

badNames.forEach(function(name) {
  test(function() {
    var headers = new Headers();
    assert_throws_js(TypeError, function() { headers.delete(name); });
  }, "Check headers delete with an invalid name " + name);
});

badNames.forEach(function(name) {
  test(function() {
    var headers = new Headers();
    assert_throws_js(TypeError, function() { headers.has(name); });
  }, "Check headers has with an invalid name " + name);
});

badNames.forEach(function(name) {
  test(function() {
    var headers = new Headers();
    assert_throws_js(TypeError, function() { headers.set(name, "Value1"); });
  }, "Check headers set with an invalid name " + name);
});

badValues.forEach(function(value) {
  test(function() {
    var headers = new Headers();
    assert_throws_js(TypeError, function() { headers.set("name", value); });
  }, "Check headers set with an invalid value " + value);
});

badNames.forEach(function(name) {
  test(function() {
    var headers = new Headers();
    assert_throws_js(TypeError, function() { headers.append("invalidĀ", "Value1"); });
  }, "Check headers append with an invalid name " + name);
});

badValues.forEach(function(value) {
  test(function() {
    var headers = new Headers();
    assert_throws_js(TypeError, function() { headers.append("name", value); });
  }, "Check headers append with an invalid value " + value);
});

test(function() {
  var headers = new Headers([["name", "value"]]);
  assert_throws_js(TypeError, function() { headers.forEach(); });
  assert_throws_js(TypeError, function() { headers.forEach(undefined); });
  assert_throws_js(TypeError, function() { headers.forEach(1); });
}, "Headers forEach throws if argument is not callable");

test(function() {
  var headers = new Headers([["name1", "value1"], ["name2", "value2"], ["name3", "value3"]]);
  var counter = 0;
  try {
      headers.forEach(function(value, name) {
          counter++;
          if (name == "name2")
                throw "error";
      });
  } catch (e) {
      assert_equals(counter, 2);
      assert_equals(e, "error");
      return;
  }
  assert_unreached();
}, "Headers forEach loop should stop if callback is throwing exception");
