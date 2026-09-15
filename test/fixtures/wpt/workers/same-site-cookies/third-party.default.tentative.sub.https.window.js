// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/workers/same-site-cookies/resources/util.js

'use strict';

// Here's the set-up for this test:
// Step 1 (window) Set cookies.
// Step 2 (top-frame) Set up listener for cookie message and open cross-site iframe.
// Step 3 (sub-frame) Open iframe same-site to top-frame.
// Step 4 (sub-sub-frame) Set up listener for message and start worker.
// Step 5 (redirect) Redirect to worker script.
// Step 6 (worker) Send cookie message to iframe.
// Step 7 (sub-sub-frame) Receive message and pass on to window.
// Step 8 (top-frame) Receive cookie message and cleanup.

async_test(t => {
  // Step 1
  const cookie_set_window = window.open("/workers/same-site-cookies/resources/set_cookies.py");
  cookie_set_window.onload =  t.step_func(_ => {
    // Step 2
    window.addEventListener("message", t.step_func(e => {
      // Step 8
      getCookieNames().then(t.step_func((cookies) => {
        assert_equals(e.data + cookies, "ReadOnLoad:None,ReadOnFetch:None,SetOnRedirectLoad:None,SetOnLoad:None,SetOnRedirectFetch:None,SetOnFetch:None", "Worker should get/set SameSite=None cookies only");
        cookie_set_window.close();
        t.done();
      }));
    }));
    let iframe = document.createElement("iframe");
    iframe.src = "https://{{hosts[alt][]}}:{{ports[https][0]}}/workers/same-site-cookies/resources/iframe.sub.html?type=default";
    document.body.appendChild(iframe);
  });
}, "Check SharedWorker sameSiteCookies option default for third-party");
