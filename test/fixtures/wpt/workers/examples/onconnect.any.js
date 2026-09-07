// META: global=sharedworker
const t = async_test("onconnect is called");
onconnect = t.step_func_done((event) => {
});
