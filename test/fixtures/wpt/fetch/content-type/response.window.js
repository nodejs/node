promise_test(() => {
  return fetch("resources/content-types.json").then(res => res.json()).then(runTests);
}, "Loading JSON…");

function runTests(tests) {
  tests.forEach(testUnit => {
    runFrameTest(testUnit, false);
    runFrameTest(testUnit, true);
    runFetchTest(testUnit, false);
    runFetchTest(testUnit, true);
    runRequestResponseTest(testUnit, "Request");
    runRequestResponseTest(testUnit, "Response");
  });
}

function runFrameTest(testUnit, singleHeader) {
  // Note: window.js is always UTF-8
  const encoding = testUnit.encoding !== null ? testUnit.encoding : "UTF-8";
  async_test(t => {
    const frame = document.body.appendChild(document.createElement("iframe"));
    t.add_cleanup(() => frame.remove());
    frame.src = getURL(testUnit.contentType, singleHeader);
    frame.onload = t.step_func_done(() => {
      // Edge requires toUpperCase()
      const doc = frame.contentDocument;
      assert_equals(doc.characterSet.toUpperCase(), encoding.toUpperCase());
      if (testUnit.documentContentType === "text/plain") {
        assert_equals(doc.body.textContent, "<b>hi</b>\n");
      } else if (testUnit.documentContentType === "text/html") {
        assert_equals(doc.body.firstChild.localName, "b");
        assert_equals(doc.body.firstChild.textContent, "hi");
      }
      assert_equals(doc.contentType, testUnit.documentContentType);
    });
  }, getDesc("<iframe>", testUnit.contentType, singleHeader));
}

function getDesc(type, input, singleHeader) {
  return type + ": " + (singleHeader ? "combined" : "separate") + " response Content-Type: " + input.join(" ");
}

function getURL(input, singleHeader) {
  // Edge does not support URLSearchParams
  let url = "resources/content-type.py?"
  if (singleHeader) {
    url += "single_header&"
  }
  input.forEach(val => {
    url += "value=" + encodeURIComponent(val) + "&";
  });
  return url;
}

function runFetchTest(testUnit, singleHeader) {
  promise_test(async t => {
    const blob = await (await fetch(getURL(testUnit.contentType, singleHeader))).blob();
    assert_equals(blob.type, testUnit.mimeType);
  }, getDesc("fetch()", testUnit.contentType, singleHeader));
}

function runRequestResponseTest(testUnit, stringConstructor) {
  promise_test(async t => {
    // Cannot give Response a body as that will set Content-Type, but Request needs a URL
    const constructorArgument = stringConstructor === "Request" ? "about:blank" : undefined;
    const r = new self[stringConstructor](constructorArgument);
    testUnit.contentType.forEach(val => {
      r.headers.append("Content-Type", val);
    });
    const blob = await r.blob();
    assert_equals(blob.type, testUnit.mimeType);
  }, getDesc(stringConstructor, testUnit.contentType, true));
}
