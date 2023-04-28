// META: script=/common/subset-tests-by-key.js
// META: timeout=long
// META: variant=?include=file
// META: variant=?include=javascript
// META: variant=?include=mailto
// META: variant=?exclude=(file|javascript|mailto)

function bURL(url, base) {
  return base ? new URL(url, base) : new URL(url)
}

function runURLTests(urltests) {
  for(var i = 0, l = urltests.length; i < l; i++) {
    var expected = urltests[i]
    if (typeof expected === "string") continue // skip comments

    function getKey(expected) {
      if (expected.protocol) {
        return expected.protocol.replace(":", "");
      }
      if (expected.failure) {
        return expected.input.split(":")[0];
      }
      return "other";
    }

    subsetTestByKey(getKey(expected), test, function() {
      if (expected.failure) {
        assert_throws_js(TypeError, function() {
          bURL(expected.input, expected.base)
        })
        return
      }

      var url = bURL(expected.input, expected.base)
      assert_equals(url.href, expected.href, "href")
      assert_equals(url.protocol, expected.protocol, "protocol")
      assert_equals(url.username, expected.username, "username")
      assert_equals(url.password, expected.password, "password")
      assert_equals(url.host, expected.host, "host")
      assert_equals(url.hostname, expected.hostname, "hostname")
      assert_equals(url.port, expected.port, "port")
      assert_equals(url.pathname, expected.pathname, "pathname")
      assert_equals(url.search, expected.search, "search")
      if ("searchParams" in expected) {
        assert_true("searchParams" in url)
        assert_equals(url.searchParams.toString(), expected.searchParams, "searchParams")
      }
      assert_equals(url.hash, expected.hash, "hash")
    }, "Parsing: <" + expected.input + "> against <" + expected.base + ">")
  }
}

promise_test(() => fetch("resources/urltestdata.json").then(res => res.json()).then(runURLTests), "Loading data…");
