const badRequestArgTests = [
  {
    args: ["", { "window": "http://test.url" }],
    testName: "RequestInit's window is not null"
  },
  {
    args: ["http://:not a valid URL"],
    testName: "Input URL is not valid"
  },
  {
    args: ["http://user:pass@test.url"],
    testName: "Input URL has credentials"
  },
  {
    args: ["", { "mode": "navigate" }],
    testName: "RequestInit's mode is navigate"
  },
  {
    args: ["", { "referrer": "http://:not a valid URL" }],
    testName: "RequestInit's referrer is invalid"
  },
  {
    args: ["", { "method": "IN VALID" }],
    testName: "RequestInit's method is invalid"
  },
  {
    args: ["", { "method": "TRACE" }],
    testName: "RequestInit's method is forbidden"
  },
  {
    args: ["", { "mode": "no-cors", "method": "PUT" }],
    testName: "RequestInit's mode is no-cors and method is not simple"
  },
  {
    args: ["", { "mode": "cors", "cache": "only-if-cached" }],
    testName: "RequestInit's cache mode is only-if-cached and mode is not same-origin"
  },
  {
    args: ["test", { "cache": "only-if-cached", "mode": "cors" }],
    testName: "Request with cache mode: only-if-cached and fetch mode cors"
  },
  {
    args: ["test", { "cache": "only-if-cached", "mode": "no-cors" }],
    testName: "Request with cache mode: only-if-cached and fetch mode no-cors"
  }
];

badRequestArgTests.push(
  ...["referrerPolicy", "mode", "credentials", "cache", "redirect"].map(optionProp => {
    const options = {};
    options[optionProp] = "BAD";
    return {
      args: ["", options],
      testName: `Bad ${optionProp} init parameter value`
    };
  })
);
