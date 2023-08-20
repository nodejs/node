promise_test(() => fetch("resources/IdnaTestV2.json").then(res => res.json()).then(runTests), "Loading data…");

// Performance impact of this seems negligible (performance.now() diff in WebKit went from 48 to 52)
// and there was a preference to let more non-ASCII hit the parser.
function encodeHostEndingCodePoints(input) {
  let output = "";
  for (const codePoint of input) {
    if ([":", "/", "?", "#", "\\"].includes(codePoint)) {
      output += encodeURIComponent(codePoint);
    } else {
      output += codePoint;
    }
  }
  return output;
}

function runTests(idnaTests) {
  for (const idnaTest of idnaTests) {
    if (typeof idnaTest === "string") {
      continue // skip comments
    }
    if (idnaTest.input === "") {
      continue // cannot test empty string input through new URL()
    }
    // Percent-encode the input such that ? and equivalent code points do not end up counting as
    // part of the URL, but are parsed through the host parser instead.
    const encodedInput = encodeHostEndingCodePoints(idnaTest.input);

    test(() => {
      if (idnaTest.output === null) {
        assert_throws_js(TypeError, () => new URL(`https://${encodedInput}/x`));
      } else {
        const url = new URL(`https://${encodedInput}/x`);
        assert_equals(url.host, idnaTest.output);
        assert_equals(url.hostname, idnaTest.output);
        assert_equals(url.pathname, "/x");
        assert_equals(url.href, `https://${idnaTest.output}/x`);
      }
    }, `ToASCII("${idnaTest.input}")${idnaTest.comment ? " " + idnaTest.comment : ""}`);
  }
}
