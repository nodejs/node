promise_test(() => {
  return fetch("resources/script-content-types.json").then(res => res.json()).then(runTests);
}, "Loading JSON…");

self.stringFromExecutedScript = undefined;

function runTests(allTestData) {
  allTestData.forEach(testData => {
    runScriptTest(testData, false);
    if (testData.contentType.length > 1) {
      runScriptTest(testData, true);
    }
  });
}

function runScriptTest(testData, singleHeader) {
  async_test(t => {
    const script = document.createElement("script");
      t.add_cleanup(() => {
      script.remove()
      self.stringFromExecutedScript = undefined;
    });
    script.src = getURL(testData.contentType, singleHeader);
    document.head.appendChild(script);
    if (testData.executes) {
      script.onload = t.step_func_done(() => {
        assert_equals(self.stringFromExecutedScript, testData.encoding === "windows-1252" ? "â‚¬" : "€");
      });
      script.onerror = t.unreached_func();
    } else {
      script.onerror = t.step_func_done();
      script.onload = t.unreached_func();
    }
  }, (singleHeader ? "combined" : "separate") + " " + testData.contentType.join(" "));
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
  url += "&content=" + encodeURIComponent("self.stringFromExecutedScript = \"€\"");
  return url;
}
