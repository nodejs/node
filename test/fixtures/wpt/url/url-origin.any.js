promise_test(() => fetch("resources/urltestdata.json").then(res => res.json()).then(runURLTests), "Loading data…");

function bURL(url, base) {
  return base ? new URL(url, base) : new URL(url)
}

function runURLTests(urltests) {
  for(var i = 0, l = urltests.length; i < l; i++) {
    var expected = urltests[i]
    if (typeof expected === "string" || !("origin" in expected)) continue

    test(function() {
      var url = bURL(expected.input, expected.base)
      assert_equals(url.origin, expected.origin, "origin")
    }, "Origin parsing: <" + expected.input + "> against <" + expected.base + ">")
  }
}
