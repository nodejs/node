// META: global=window,worker
// META: script=/fetch/metadata/resources/helper.js

// Site
promise_test(t => {
  return validate_expectations_custom_url("https://{{hosts[][www]}}:{{ports[https][0]}}/fetch/metadata/resources/echo-as-json.py",
                {
                  mode: "cors",
                  headers: { 'x-test': 'testing' }
                }, {
          "site": "same-site",
          "user": "",
          "mode": "cors",
          "dest": "empty"
        }, "Same-site fetch with preflight");
}, "Same-site fetch with preflight");

promise_test(t => {
  return validate_expectations_custom_url("https://{{hosts[alt][www]}}:{{ports[https][0]}}/fetch/metadata/resources/echo-as-json.py",
                {
                  mode: "cors",
                  headers: { 'x-test': 'testing' }
                }, {
          "site": "cross-site",
          "user": "",
          "mode": "cors",
          "dest": "empty"
        }, "Cross-site fetch with preflight");
}, "Cross-site fetch with preflight");
