// META: global=window,worker

promise_test(() => {
  return fetch("/xhr/resources/echo-headers.py", {headers: [["THIS-is-A-test", 1], ["THIS-IS-A-TEST", 2]] }).then(res => res.text()).then(body => {
    assert_regexp_match(body, /THIS-is-A-test: 1, 2/)
  })
}, "Multiple headers with the same name, different case (THIS-is-A-test first)")

promise_test(() => {
  return fetch("/xhr/resources/echo-headers.py", {headers: [["THIS-IS-A-TEST", 1], ["THIS-is-A-test", 2]] }).then(res => res.text()).then(body => {
    assert_regexp_match(body, /THIS-IS-A-TEST: 1, 2/)
  })
}, "Multiple headers with the same name, different case (THIS-IS-A-TEST first)")
