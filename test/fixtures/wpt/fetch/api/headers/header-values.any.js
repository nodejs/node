// META: title=Header value test
// META: global=window,worker
// META: timeout=long

// Invalid values
[0, 0x0A, 0x0D].forEach(val => {
  val = "x" + String.fromCharCode(val) + "x"

  // XMLHttpRequest is not available in service workers
  if (!self.GLOBAL.isWorker()) {
    test(() => {
      let xhr = new XMLHttpRequest()
      xhr.open("POST", "/")
      assert_throws_dom("SyntaxError", () => xhr.setRequestHeader("value-test", val))
    }, "XMLHttpRequest with value " + encodeURI(val) + " needs to throw")
  }

  promise_test(t => promise_rejects_js(t, TypeError, fetch("/", { headers: {"value-test": val} })), "fetch() with value " + encodeURI(val) + " needs to throw")
})

// Valid values
let headerValues =[]
for(let i = 0; i < 0x100; i++) {
  if(i === 0 || i === 0x0A || i === 0x0D) {
    continue
  }
  headerValues.push("x" + String.fromCharCode(i) + "x")
}
var url = "../resources/inspect-headers.py?headers="
headerValues.forEach((_, i) => {
  url += "val" + i + "|"
})

// XMLHttpRequest is not available in service workers
if (!self.GLOBAL.isWorker()) {
  async_test((t) => {
    let xhr = new XMLHttpRequest()
    xhr.open("POST", url)
    headerValues.forEach((val, i) => {
      xhr.setRequestHeader("val" + i, val)
    })
    xhr.onload = t.step_func_done(() => {
      headerValues.forEach((val, i) => {
        assert_equals(xhr.getResponseHeader("x-request-val" + i), val)
      })
    })
    xhr.send()
  }, "XMLHttpRequest with all valid values")
}

promise_test((t) => {
  const headers = new Headers
  headerValues.forEach((val, i) => {
    headers.append("val" + i, val)
  })
  return fetch(url, { headers }).then((res) => {
    headerValues.forEach((val, i) => {
      assert_equals(res.headers.get("x-request-val" + i), val)
    })
  })
}, "fetch() with all valid values")
