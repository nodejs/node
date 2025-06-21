// META: global=window,worker
"use strict";

test(() => {
  assert_equals(typeof queueMicrotask, "function");
}, "It exists and is a function");

test(() => {
  assert_throws_js(TypeError, () => queueMicrotask(), "no argument");
  assert_throws_js(TypeError, () => queueMicrotask(undefined), "undefined");
  assert_throws_js(TypeError, () => queueMicrotask(null), "null");
  assert_throws_js(TypeError, () => queueMicrotask(0), "0");
  assert_throws_js(TypeError, () => queueMicrotask({ handleEvent() { } }), "an event handler object");
  assert_throws_js(TypeError, () => queueMicrotask("window.x = 5;"), "a string");
}, "It throws when given non-functions");

async_test(t => {
  let called = false;
  queueMicrotask(t.step_func_done(() => {
    called = true;
  }));
  assert_false(called);
}, "It calls the callback asynchronously");

async_test(t => {
  queueMicrotask(t.step_func_done(function () { // note: intentionally not an arrow function
    assert_array_equals(arguments, []);
  }), "x", "y");
}, "It does not pass any arguments");

async_test(t => {
  const happenings = [];
  Promise.resolve().then(() => happenings.push("a"));
  queueMicrotask(() => happenings.push("b"));
  Promise.reject().catch(() => happenings.push("c"));
  queueMicrotask(t.step_func_done(() => {
    assert_array_equals(happenings, ["a", "b", "c"]);
  }));
}, "It interleaves with promises as expected");
