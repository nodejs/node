// META: global=window,worker
// META: script=/fetch/metadata/resources/helper.js

// Site
promise_test(t => {
  return validate_expectations_custom_url("https://{{host}}:{{ports[https][0]}}/fetch/metadata/resources/echo-as-json.py", {}, {
          "site": "same-origin",
          "user": "",
          "mode": "cors",
          "dest": "empty"
        }, "Same-origin fetch");
}, "Same-origin fetch");

promise_test(t => {
  return validate_expectations_custom_url("https://{{hosts[][www]}}:{{ports[https][0]}}/fetch/metadata/resources/echo-as-json.py", {}, {
          "site": "same-site",
          "user": "",
          "mode": "cors",
          "dest": "empty"
        }, "Same-site fetch");
}, "Same-site fetch");

promise_test(t => {
  return validate_expectations_custom_url("https://{{hosts[alt][www]}}:{{ports[https][0]}}/fetch/metadata/resources/echo-as-json.py", {}, {
          "site": "cross-site",
          "user": "",
          "mode": "cors",
          "dest": "empty"
        }, "Cross-site fetch");
}, "Cross-site fetch");

// Mode
promise_test(t => {
  return validate_expectations_custom_url("https://{{host}}:{{ports[https][0]}}/fetch/metadata/resources/echo-as-json.py", {mode: "same-origin"}, {
          "site": "same-origin",
          "user": "",
          "mode": "same-origin",
          "dest": "empty"
        }, "Same-origin mode");
}, "Same-origin mode");

promise_test(t => {
  return validate_expectations_custom_url("https://{{host}}:{{ports[https][0]}}/fetch/metadata/resources/echo-as-json.py", {mode: "cors"}, {
          "site": "same-origin",
          "user": "",
          "mode": "cors",
          "dest": "empty"
        }, "CORS mode");
}, "CORS mode");

promise_test(t => {
  return validate_expectations_custom_url("https://{{host}}:{{ports[https][0]}}/fetch/metadata/resources/echo-as-json.py", {mode: "no-cors"}, {
          "site": "same-origin",
          "user": "",
          "mode": "no-cors",
          "dest": "empty"
        }, "no-CORS mode");
}, "no-CORS mode");
