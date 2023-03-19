// The results of this test are all over the map due to browsers behaving very differently for
// javascript: URLs.
//
// Chromium is pretty close execution-wise, but it parses javascript: URLs incorrectly.
// Gecko navigates to non-string return values of the result of executing a javascript: URL.
// WebKit executes javascript: URLs too early and has a harness error due to URL parsing.
//
// The expectations below should match the HTML and URL standards.
[
  {
    "description": "javascript: URL that fails to parse due to invalid host",
    "input": "javascript://test:test/%0aglobalThis.shouldNotExistA=1",
    "property": "shouldNotExistA",
    "expected": undefined
  },
  {
    "description": "javascript: URL that fails to parse due to invalid host and has a U+0009 in scheme",
    "input": "java\x09script://test:test/%0aglobalThis.shouldNotExistB=1",
    "property": "shouldNotExistB",
    "expected": undefined
  },
  {
    "description": "javascript: URL without an opaque path",
    "input": "javascript://host/1%0a//../0/;globalThis.shouldBeOne=1;/%0aglobalThis.shouldBeOne=2;/..///",
    "property": "shouldBeOne",
    "expected": 1
  },
  {
    "description": "javascript: URL containing a JavaScript string split over path and query",
    // Use ";undefined" to avoid returning a string.
    "input": "javascript:globalThis.shouldBeAStringA = \"https://whatsoever.com/?a=b&c=5&x=y\";undefined",
    "property": "shouldBeAStringA",
    "expected": "https://whatsoever.com/?a=b&c=5&x=y"
  },
  {
    "description": "javascript: URL containing a JavaScript string split over path and query and has a U+000A in scheme",
    // Use ";undefined" to avoid returning a string.
    "input": "java\x0Ascript:globalThis.shouldBeAStringB = \"https://whatsoever.com/?a=b&c=5&x=y\";undefined",
    "property": "shouldBeAStringB",
    "expected": "https://whatsoever.com/?a=b&c=5&x=y"
  }
].forEach(({ description, input, property, expected }) => {
  // Use promise_test so the tests run in sequence. Needed for globalThis.verify below.
  promise_test(t => {
    const anchor = document.body.appendChild(document.createElement("a"));
    t.add_cleanup(() => anchor.remove());
    anchor.href = input;
    assert_equals(globalThis[property], undefined, "Property is undefined before the click");
    anchor.click();
    assert_equals(globalThis[property], undefined, "Property is undefined immediately after the click");

    // Since we cannot reliably queue a task to run after the task queued as a result of the click()
    // above, we do another click() with a new URL.
    return new Promise(resolve => {
      globalThis.verify = t.step_func(() => {
        assert_equals(globalThis[property], expected, `Property is ${expected} once the navigation happened`);
        resolve();
      });
      anchor.href = "javascript:globalThis.verify()";
      anchor.click();
    });
  }, description);
});
