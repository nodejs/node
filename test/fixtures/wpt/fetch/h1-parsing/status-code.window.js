[
  {
    input: "",
    expected: null
  },
  {
    input: "BLAH",
    expected: null
  },
  {
    input: "0 OK",
    expected: {
      status: 0,
      statusText: "OK"
    }
  },
  {
    input: "1 OK",
    expected: {
      status: 1,
      statusText: "OK"
    }
  },
  {
    input: "99 NOT OK",
    expected: {
      status: 99,
      statusText: "NOT OK"
    }
  },
  {
    input: "077 77",
    expected: {
      status: 77,
      statusText: "77"
    }
  },
  {
    input: "099 HELLO",
    expected: {
      status: 99,
      statusText: "HELLO"
    }
  },
  {
    input: "200",
    expected: {
      status: 200,
      statusText: ""
    }
  },
  {
    input: "999 DOES IT MATTER",
    expected: {
      status: 999,
      statusText: "DOES IT MATTER"
    }
  },
  {
    input: "1000 BOO",
    expected: null
  },
  {
    input: "0200 BOO",
    expected: null
  },
  {
    input: "65736 NOT 200 OR SOME SUCH",
    expected: null
  },
  {
    input: "131072 HI",
    expected: null
  },
  {
    input: "-200 TEST",
    expected: null
  },
  {
    input: "0xA",
    expected: null
  },
  {
    input: "C8",
    expected: null
  }
].forEach(({ description, input, expected }) => {
  promise_test(async t => {
    if (expected !== null) {
      const response = await fetch("resources/status-code.py?input=" + input);
      assert_equals(response.status, expected.status);
      assert_equals(response.statusText, expected.statusText);
      assert_equals(response.headers.get("header-parsing"), "is sad");
    } else {
      await promise_rejects_js(t, TypeError, fetch("resources/status-code.py?input=" + input));
    }
  }, `HTTP/1.1 ${input} ${expected === null ? "(network error)" : ""}`);
});
