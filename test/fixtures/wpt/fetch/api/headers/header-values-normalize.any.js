// META: title=Header value normalizing test
// META: global=window,worker
// META: timeout=long

for(let i = 0; i < 0x21; i++) {
  let fail = false,
      strip = false

  // REMOVE 0x0B/0x0C exception once https://github.com/web-platform-tests/wpt/issues/8372 is fixed
  if(i === 0x0B || i === 0x0C)
    continue

  if(i === 0) {
    fail = true
  }

  if(i === 0x09 || i === 0x0A || i === 0x0D || i === 0x20) {
    strip = true
  }

  let url = "../resources/inspect-headers.py?headers=val1|val2|val3",
      val = String.fromCharCode(i),
      expectedVal = strip ? "" : val,
      val1 = val,
      expectedVal1 = expectedVal,
      val2 = "x" + val,
      expectedVal2 = "x" + expectedVal,
      val3 = val + "x",
      expectedVal3 = expectedVal + "x"

  // XMLHttpRequest is not available in service workers
  if (!self.GLOBAL.isWorker()) {
    async_test((t) => {
      let xhr = new XMLHttpRequest()
      xhr.open("POST", url)
      if(fail) {
          assert_throws_dom("SyntaxError", () => xhr.setRequestHeader("val1", val1))
          assert_throws_dom("SyntaxError", () => xhr.setRequestHeader("val2", val2))
          assert_throws_dom("SyntaxError", () => xhr.setRequestHeader("val3", val3))
          t.done()
      } else {
        xhr.setRequestHeader("val1", val1)
        xhr.setRequestHeader("val2", val2)
        xhr.setRequestHeader("val3", val3)
        xhr.onload = t.step_func_done(() => {
          assert_equals(xhr.getResponseHeader("x-request-val1"), expectedVal1)
          assert_equals(xhr.getResponseHeader("x-request-val2"), expectedVal2)
          assert_equals(xhr.getResponseHeader("x-request-val3"), expectedVal3)
        })
        xhr.send()
      }
    }, "XMLHttpRequest with value " + encodeURI(val))
  }

  promise_test((t) => {
    if(fail) {
      return Promise.all([
        promise_rejects_js(t, TypeError, fetch(url, { headers: {"val1": val1} })),
        promise_rejects_js(t, TypeError, fetch(url, { headers: {"val2": val2} })),
        promise_rejects_js(t, TypeError, fetch(url, { headers: {"val3": val3} }))
      ])
    } else {
      return fetch(url, { headers: {"val1": val1, "val2": val2, "val3": val3} }).then((res) => {
        assert_equals(res.headers.get("x-request-val1"), expectedVal1)
        assert_equals(res.headers.get("x-request-val2"), expectedVal2)
        assert_equals(res.headers.get("x-request-val3"), expectedVal3)
      })
    }
  }, "fetch() with value " + encodeURI(val))
}
