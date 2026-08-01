//META: global=worker
test(() => {
  const proto = {};
  assert_equals(String(Object.getPrototypeOf(WorkerLocation)).replace(/\n/g, " ").replace(/\s\s+/g, " "), "function () { [native code] }");
  Object.setPrototypeOf(WorkerLocation, proto);
  assert_equals(Object.getPrototypeOf(WorkerLocation), proto);
}, 'Tests that setting the proto of a built in constructor is not reset.');
