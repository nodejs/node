// META: global=window,worker

"use strict";

promise_test(() => fetch("../cors/resources/not-cors-safelisted.json").then(res => res.json().then(runTests)), "Loading data…");

const longValue = "s".repeat(127);

[
  {
    "headers": ["accept", "accept-language", "content-language"],
    "values": [longValue, "", longValue]
  },
  {
    "headers": ["accept", "accept-language", "content-language"],
    "values": ["", longValue]
  },
  {
    "headers": ["content-type"],
    "values": ["text/plain;" + "s".repeat(116), "text/plain"]
  }
].forEach(testItem => {
  testItem.headers.forEach(header => {
    test(() => {
      const noCorsHeaders = new Request("about:blank", { mode: "no-cors" }).headers;
      testItem.values.forEach((value) => {
        noCorsHeaders.append(header, value);
        assert_equals(noCorsHeaders.get(header), testItem.values[0], '1');
      });
      noCorsHeaders.set(header, testItem.values.join(", "));
      assert_equals(noCorsHeaders.get(header), testItem.values[0], '2');
      noCorsHeaders.delete(header);
      assert_false(noCorsHeaders.has(header));
    }, "\"no-cors\" Headers object cannot have " + header + " set to " + testItem.values.join(", "));
  });
});

function runTests(testArray) {
  testArray = testArray.concat([
    ["dpr", "2"],
    ["rtt", "1.0"],
    ["downlink", "-1.0"],
    ["ect", "6g"],
    ["save-data", "on"],
    ["viewport-width", "100"],
    ["width", "100"],
    ["unknown", "doesitmatter"]
  ]);
  testArray.forEach(testItem => {
    const [headerName, headerValue] = testItem;
    test(() => {
      const noCorsHeaders = new Request("about:blank", { mode: "no-cors" }).headers;
      noCorsHeaders.append(headerName, headerValue);
      assert_false(noCorsHeaders.has(headerName));
      noCorsHeaders.set(headerName, headerValue);
      assert_false(noCorsHeaders.has(headerName));
    }, "\"no-cors\" Headers object cannot have " + headerName + "/" + headerValue + " as header");
  });
}
