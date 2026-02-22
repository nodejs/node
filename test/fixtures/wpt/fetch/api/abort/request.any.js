// META: timeout=long
// META: global=window,worker

const BODY_FUNCTION_AND_DATA = {
  arrayBuffer: null,
  blob: null,
  formData: new FormData(),
  json: new Blob(["{}"]),
  text: null,
};

for (const [bodyFunction, body] of Object.entries(BODY_FUNCTION_AND_DATA)) {
  promise_test(async () => {
    const controller = new AbortController();
    const signal = controller.signal;
    const request = new Request("../resources/data.json", {
      method: "post",
      signal,
      body,
    });

    controller.abort();
    await request[bodyFunction]();
    assert_true(
      true,
      `An aborted request should still be able to run ${bodyFunction}()`
    );
  }, `Calling ${bodyFunction}() on an aborted request`);

  promise_test(async () => {
    const controller = new AbortController();
    const signal = controller.signal;
    const request = new Request("../resources/data.json", {
      method: "post",
      signal,
      body,
    });

    const p = request[bodyFunction]();
    controller.abort();
    await p;
    assert_true(
      true,
      `An aborted request should still be able to run ${bodyFunction}()`
    );
  }, `Aborting a request after calling ${bodyFunction}()`);

  if (!body) {
    promise_test(async () => {
      const controller = new AbortController();
      const signal = controller.signal;
      const request = new Request("../resources/data.json", {
        method: "post",
        signal,
        body,
      });

      // consuming happens synchronously, so don't wait
      fetch(request).catch(() => {});

      controller.abort();
      await request[bodyFunction]();
      assert_true(
        true,
        `An aborted consumed request should still be able to run ${bodyFunction}() when empty`
      );
    }, `Calling ${bodyFunction}() on an aborted consumed empty request`);
  }

  promise_test(async t => {
    const controller = new AbortController();
    const signal = controller.signal;
    const request = new Request("../resources/data.json", {
      method: "post",
      signal,
      body: body || new Blob(["foo"]),
    });

    // consuming happens synchronously, so don't wait
    fetch(request).catch(() => {});

    controller.abort();
    await promise_rejects_js(t, TypeError, request[bodyFunction]());
  }, `Calling ${bodyFunction}() on an aborted consumed nonempty request`);
}
