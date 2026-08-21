promise_test(() => fetch("resources/percent-encoding.json").then(res => res.json()).then(runTests), "Loading data…");

function runTests(testUnits) {
  for (const testUnit of testUnits) {
    // Ignore comments
    if (typeof testUnit === "string") {
      continue;
    }
    for (const encoding of Object.keys(testUnit.output)) {
      async_test(t => {
        const frame = document.body.appendChild(document.createElement("iframe"));
        t.add_cleanup(() => frame.remove());
        frame.onload = t.step_func_done(() => {
          const output = frame.contentDocument.querySelector("a");
          // Test that the fragment is always UTF-8 encoded
          assert_equals(output.hash, `#${testUnit.output["utf-8"]}`, "fragment");
          assert_equals(output.search, `?${testUnit.output[encoding]}`, "query");
        });
        frame.src = `resources/percent-encoding.py?encoding=${encoding}&value=${toBase64(testUnit.input)}`;
      }, `Input ${testUnit.input} with encoding ${encoding}`);
    }
  }
}

// Use base64 to avoid relying on the URL parser to get UTF-8 percent-encoding correctly. This does
// not use btoa directly as that only works with code points in the range U+0000 to U+00FF,
// inclusive.
function toBase64(input) {
  const bytes = new TextEncoder().encode(input);
  const byteString = Array.from(bytes, byte => String.fromCharCode(byte)).join("");
  const encoded = self.btoa(byteString);
  return encoded;
}
