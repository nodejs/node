// META: script=/common/get-host-info.sub.js

const crossOriginURL = get_host_info().HTTP_REMOTE_ORIGIN + "/fetch/cross-origin-resource-policy/resources/hello.py?corp=";

[
  "same",
  "same, same-origin",
  "SAME-ORIGIN",
  "Same-Origin",
  "same-origin, <>",
  "same-origin, same-origin",
  "https://www.example.com",  // See https://github.com/whatwg/fetch/issues/760
].forEach(incorrectHeaderValue => {
  // Note: an incorrect value results in a successful load, so this test is only meaningful in
  // implementations with support for the header.
  promise_test(t => {
    return fetch(crossOriginURL + encodeURIComponent(incorrectHeaderValue), { mode: "no-cors" });
  }, "Parsing Cross-Origin-Resource-Policy: " + incorrectHeaderValue);
});
