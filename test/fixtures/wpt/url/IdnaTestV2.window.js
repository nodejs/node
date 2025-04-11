promise_test(() => fetch("resources/IdnaTestV2.json").then(res => res.json()).then(runTests), "Loading data…");

function runTests(idnaTests) {
  for (const idnaTest of idnaTests) {
    if (typeof idnaTest === "string") {
      continue // skip comments
    }
    if (idnaTest.input === "") {
      continue // cannot test empty string input through new URL()
    }

    test(() => {
      if (idnaTest.output === null) {
        assert_throws_js(TypeError, () => new URL(`https://${idnaTest.input}/x`));
      } else {
        const url = new URL(`https://${idnaTest.input}/x`);
        assert_equals(url.host, idnaTest.output);
        assert_equals(url.hostname, idnaTest.output);
        assert_equals(url.pathname, "/x");
        assert_equals(url.href, `https://${idnaTest.output}/x`);
      }
    }, `ToASCII("${idnaTest.input}")${idnaTest.comment ? " " + idnaTest.comment : ""}`);
  }
}
