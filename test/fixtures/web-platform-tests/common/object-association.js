"use strict";

// For now this only has per-Window tests, but we could expand it to also test per-Document

window.testIsPerWindow = propertyName => {
  test(t => {
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    const frame = iframe.contentWindow;

    const before = frame[propertyName];
    assert_true(before !== undefined && before !== null, `window.${propertyName} must be implemented`);

    iframe.remove();

    const after = frame[propertyName];
    assert_equals(after, before, `window.${propertyName} should not change after iframe.remove()`);
  }, `Discarding the browsing context must not change window.${propertyName}`);

  async_test(t => {
    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    const frame = iframe.contentWindow;

    const before = frame[propertyName];
    assert_true(before !== undefined && before !== null, `window.${propertyName} must be implemented`);

    // Note: cannot use step_func_done for this because it might be called twice, per the below comment.
    iframe.onload = t.step_func(() => {
      if (frame.location.href === "about:blank") {
        // Browsers are not reliable on whether about:blank fires the load event; see
        // https://github.com/whatwg/html/issues/490
        return;
      }

      const after = frame[propertyName];
      assert_equals(after, before);
      t.done();
    });

    iframe.src = "/common/blank.html";
  }, `Navigating from the initial about:blank must not replace window.${propertyName}`);

  // Note: document.open()'s spec doesn't match most browsers; see https://github.com/whatwg/html/issues/1698.
  // However, as explained in https://github.com/whatwg/html/issues/1698#issuecomment-298748641, even an updated spec
  // will probably still reset Window-associated properties.
  async_test(t => {
    const iframe = document.createElement("iframe");

    iframe.onload = t.step_func_done(() => {
      const frame = iframe.contentWindow;
      const before = frame[propertyName];
      assert_true(before !== undefined && before !== null, `window.${propertyName} must be implemented`);

      frame.document.open();

      const after = frame[propertyName];
      assert_not_equals(after, before);

      frame.document.close();
    });

    iframe.src = "/common/blank.html";
    document.body.appendChild(iframe);
  }, `document.open() must replace window.${propertyName}`);
};
