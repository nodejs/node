promise_test(() => fetch("resources/toascii.json").then(res => res.json()).then(runTests), "Loading data…");

function makeURL(type, input) {
  input = "https://" + input + "/x"
  if(type === "url") {
    return new URL(input)
  } else {
    const url = document.createElement(type)
    url.href = input
    return url
  }
}

function runTests(tests) {
  for(var i = 0, l = tests.length; i < l; i++) {
    let hostTest = tests[i]
    if (typeof hostTest === "string") {
      continue // skip comments
    }
    const typeName = { "url": "URL", "a": "<a>", "area": "<area>" }
    ;["url", "a", "area"].forEach((type) => {
      test(() => {
        if(hostTest.output !== null) {
          const url = makeURL("url", hostTest.input)
          assert_equals(url.host, hostTest.output)
          assert_equals(url.hostname, hostTest.output)
          assert_equals(url.pathname, "/x")
          assert_equals(url.href, "https://" + hostTest.output + "/x")
        } else {
          if(type === "url") {
            assert_throws(new TypeError, () => makeURL("url", hostTest.input))
          } else {
            const url = makeURL(type, hostTest.input)
            assert_equals(url.host, "")
            assert_equals(url.hostname, "")
            assert_equals(url.pathname, "")
            assert_equals(url.href, "https://" + hostTest.input + "/x")
          }
        }
      }, hostTest.input + " (using " + typeName[type] + ")")
      ;["host", "hostname"].forEach((val) => {
        test(() => {
          const url = makeURL(type, "x")
          url[val] = hostTest.input
          if(hostTest.output !== null) {
            assert_equals(url[val], hostTest.output)
          } else {
            assert_equals(url[val], "x")
          }
        }, hostTest.input + " (using " + typeName[type] + "." + val + ")")
      })
    })
  }
}
