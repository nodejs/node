// META: global=window,worker

// These tests verify that stream creation is not affected by changes to
// Object.prototype.

const creationCases = {
  fetch: async () => fetch(location.href),
  request: () => new Request(location.href, {method: 'POST', body: 'hi'}),
  response: () => new Response('bye'),
  consumeEmptyResponse: () => new Response().text(),
  consumeNonEmptyResponse: () => new Response(new Uint8Array([64])).text(),
  consumeEmptyRequest: () => new Request(location.href).text(),
  consumeNonEmptyRequest: () => new Request(location.href,
                                            {method: 'POST', body: 'yes'}).arrayBuffer(),
};

for (const creationCase of Object.keys(creationCases)) {
  for (const accessorName of ['start', 'type', 'size', 'highWaterMark']) {
    promise_test(async t => {
      Object.defineProperty(Object.prototype, accessorName, {
        get() { throw Error(`Object.prototype.${accessorName} was accessed`); },
        configurable: true
      });
      t.add_cleanup(() => {
        delete Object.prototype[accessorName];
        return Promise.resolve();
      });
      await creationCases[creationCase]();
    }, `throwing Object.prototype.${accessorName} accessor should not affect ` +
       `stream creation by '${creationCase}'`);

    promise_test(async t => {
      // -1 is a convenient value which is invalid, and should cause the
      // constructor to throw, for all four fields.
      Object.prototype[accessorName] = -1;
      t.add_cleanup(() => {
        delete Object.prototype[accessorName];
        return Promise.resolve();
      });
      await creationCases[creationCase]();
    }, `Object.prototype.${accessorName} accessor returning invalid value ` +
       `should not affect stream creation by '${creationCase}'`);
  }

  promise_test(async t => {
    Object.prototype.start = controller => controller.error(new Error('start'));
      t.add_cleanup(() => {
        delete Object.prototype.start;
        return Promise.resolve();
      });
      await creationCases[creationCase]();
    }, `Object.prototype.start function which errors the stream should not ` +
       `affect stream creation by '${creationCase}'`);
}
