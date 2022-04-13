promise_test(() => fetch("resources/urltestdata.json").then(res => res.json()).then(runURLTests), "Loading data…");

function setBase(base) {
  document.getElementById("base").href = base
}

function bURL(url, base) {
  base = base || "about:blank"
  setBase(base)
  var a = document.createElement("a")
  a.setAttribute("href", url)
  return a
}

function runURLTests(urltests) {
  for(var i = 0, l = urltests.length; i < l; i++) {
    var expected = urltests[i]
    if (typeof expected === "string" || !("origin" in expected)) continue

    test(function() {
      var url = bURL(expected.input, expected.base)
      assert_equals(url.origin, expected.origin, "origin")
    }, "Parsing origin: <" + expected.input + "> against <" + expected.base + ">")
  }
}
