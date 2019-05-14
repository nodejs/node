"use strict";

// This does not work as you expect because mutation observer compound microtasks are confusing.
// Basically you can only use it once per test.
function queueMicrotaskViaMO(cb) {
  const observer = new MutationObserver(cb);
  const node = document.createTextNode("");
  observer.observe(node, { characterData: true });
  node.data = "foo";
}

// Need to use promise_test to get sequential ordering; otherwise the global mutation observer
// compound microtask business screws us over.

promise_test(() => {
  return new Promise(resolve => {
    const happenings = [];

    queueMicrotaskViaMO(() => happenings.push("x"));
    queueMicrotask(() => happenings.push("a"));

    queueMicrotask(() => {
      assert_array_equals(happenings, ["x", "a"]);
      resolve();
    });
  });
}, "It interleaves with MutationObservers as expected");

promise_test(() => {
  return new Promise(resolve => {
    const happenings = [];

    queueMicrotask(() => happenings.push("a"));
    Promise.reject().catch(() => happenings.push("x"));
    queueMicrotaskViaMO(() => happenings.push(1));
    Promise.resolve().then(() => happenings.push("y"));
    queueMicrotask(() => happenings.push("b"));
    queueMicrotask(() => happenings.push("c"));

    queueMicrotask(() => {
      assert_array_equals(happenings, ["a", "x", 1, "y", "b", "c"]);
      resolve();
    });
  });
}, "It interleaves with MutationObservers and promises together as expected");
