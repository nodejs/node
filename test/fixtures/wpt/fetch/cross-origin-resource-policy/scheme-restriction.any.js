// META: script=/common/get-host-info.sub.js

promise_test(t => {
  return promise_rejects_js(t,
                         TypeError,
                         fetch(get_host_info().HTTPS_REMOTE_ORIGIN + "/fetch/cross-origin-resource-policy/resources/hello.py?corp=same-site", { mode: "no-cors" }));
}, "Cross-Origin-Resource-Policy: same-site blocks retrieving HTTPS from HTTP");
