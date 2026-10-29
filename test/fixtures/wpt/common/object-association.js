"use strict";

// This is for testing whether an object (e.g., a global property) is associated with Window, or
// with Document. Recall that Window and Document are 1:1 except when doing a same-origin navigation
// away from the initial about:blank. In that case the Window object gets reused for the new
// Document.
//
// So:
// - If something is per-Window, then it should maintain its identity across an about:blank
//   navigation.
// - If something is per-Document, then it should be recreated across an about:blank navigation.

window.testIsPerWindow = propertyName => {
  runTests(propertyName, assert_equals, "must not");
};

window.testIsPerDocument = propertyName => {
  runTests(propertyName, assert_not_equals, "must");
};

function runTests(propertyName, equalityOrInequalityAsserter, mustOrMustNotReplace) {
  async_test(t => {
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    const frame = iframe.contentWindow;

    const before = frame[propertyName];
    assert_implements(before, `window.${propertyName} must be implemented`);

    iframe.onload = t.step_func_done(() => {
      const after = frame[propertyName];
      equalityOrInequalityAsserter(after, before);
    });

    iframe.src = "/common/blank.html";
  }, `Navigating from the initial about:blank ${mustOrMustNotReplace} replace window.${propertyName}`);

  // Per spec, discarding a browsing context should not change any of the global objects.
  test(() => {
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    const frame = iframe.contentWindow;

    const before = frame[propertyName];
    assert_implements(before, `window.${propertyName} must be implemented`);

    iframe.remove();

    const after = frame[propertyName];
    assert_equals(after, before, `window.${propertyName} should not change after iframe.remove()`);
  }, `Discarding the browsing context must not change window.${propertyName}`);

  // Per spec, document.open() should not change any of the global objects. In historical versions
  // of the spec, it did, so we test here.
  async_test(t => {
    const iframe = document.createElement("iframe");

    iframe.onload = t.step_func_done(() => {
      const frame = iframe.contentWindow;
      const before = frame[propertyName];
      assert_implements(before, `window.${propertyName} must be implemented`);

      frame.document.open();

      const after = frame[propertyName];
      assert_equals(after, before);

      frame.document.close();
    });

    iframe.src = "/common/blank.html";
    document.body.appendChild(iframe);
  }, `document.open() must not replace window.${propertyName}`);
}
