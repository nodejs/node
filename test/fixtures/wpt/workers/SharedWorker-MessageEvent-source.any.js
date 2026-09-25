// META: global=sharedworker
const t = async_test("Make sure that MessageEvent.source is properly set in connect event.");
onconnect = t.step_func_done((event) => {
  assert_equals(Object.getPrototypeOf(event), MessageEvent.prototype);
  assert_equals(event.source, event.ports[0]);
});
