// META: global=dedicatedworker,sharedworker
test(() => {
  assert_equals(String(WorkerLocation).replace(/\n/g, " ").replace(/\s\s+/g, " "), "function WorkerLocation() { [native code] }");
  assert_true(location instanceof Object);
  assert_equals(location.href, 'http://{{host}}:{{ports[http][0]}}/workers/Worker-location.sub.any.worker.js');
  assert_equals(location.origin, "http://{{host}}:{{ports[http][0]}}");
  assert_equals(location.protocol, "http:");
  assert_equals(location.host, "{{host}}:{{ports[http][0]}}");
  assert_equals(location.hostname, "{{host}}");
  assert_equals(location.port, "{{ports[http][0]}}");
  assert_equals(location.pathname, "/workers/Worker-location.sub.any.worker.js");
  assert_equals(location.search, "");
  assert_equals(location.hash, "");
}, 'Test WorkerLocation properties.');
