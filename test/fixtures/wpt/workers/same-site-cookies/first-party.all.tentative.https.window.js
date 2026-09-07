// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/workers/same-site-cookies/resources/util.js

'use strict';

// Here's the set-up for this test:
// Step 1 (window) Set cookies.
// Step 2 (window) Set up listener for cookie message and start worker.
// Step 3 (redirect) Redirect to worker script.
// Step 4 (worker) Send cookie message to window.
// Step 5 (window) Receive cookie message and cleanup.

async_test(t => {
    // Step 1
    const cookie_set_window = window.open("/workers/same-site-cookies/resources/set_cookies.py");
    cookie_set_window.onload =  t.step_func(_ => {
        // Step 2
        const worker = new SharedWorker("/workers/same-site-cookies/resources/worker_redirect.py", {sameSiteCookies: "all"});
        worker.port.onmessage = t.step_func(e => {
            // Step 5
            getCookieNames().then(t.step_func((cookies) => {
                assert_equals(e.data + cookies, "ReadOnLoad:StrictLaxNone,ReadOnFetch:StrictLaxNone,SetOnRedirectLoad:StrictLaxNone,SetOnLoad:StrictLaxNone,SetOnRedirectFetch:StrictLaxNone,SetOnFetch:StrictLaxNone", "Worker should get/set all cookies");
                cookie_set_window.close();
                t.done();
            }));
        });
    });
}, "Check SharedWorker sameSiteCookies option all for first-party");
