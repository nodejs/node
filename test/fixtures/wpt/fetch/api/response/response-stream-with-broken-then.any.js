// META: global=window,worker
// META: script=../resources/utils.js

promise_test(async () => {
  // t.add_cleanup doesn't work when Object.prototype.then is overwritten, so
  // these tests use add_completion_callback for cleanup instead.
  add_completion_callback(() => delete Object.prototype.then);
  const hello = new TextEncoder().encode('hello');
  const bye = new TextEncoder().encode('bye');
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue(hello);
      controller.close();
    }
  });
  const resp = new Response(rs);
  Object.prototype.then = (onFulfilled) => {
    delete Object.prototype.then;
    onFulfilled({done: false, value: bye});
  };
  const text = await resp.text();
  delete Object.prototype.then;
  assert_equals(text, 'hello', 'The value should be "hello".');
}, 'Attempt to inject {done: false, value: bye} via Object.prototype.then.');

promise_test(async (t) => {
  add_completion_callback(() => delete Object.prototype.then);
  const hello = new TextEncoder().encode('hello');
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue(hello);
      controller.close();
    }
  });
  const resp = new Response(rs);
  Object.prototype.then = (onFulfilled) => {
    delete Object.prototype.then;
    onFulfilled({done: false, value: undefined});
  };
  const text = await resp.text();
  delete Object.prototype.then;
  assert_equals(text, 'hello', 'The value should be "hello".');
}, 'Attempt to inject value: undefined via Object.prototype.then.');

promise_test(async (t) => {
  add_completion_callback(() => delete Object.prototype.then);
  const hello = new TextEncoder().encode('hello');
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue(hello);
      controller.close();
    }
  });
  const resp = new Response(rs);
  Object.prototype.then = (onFulfilled) => {
    delete Object.prototype.then;
    onFulfilled(undefined);
  };
  const text = await resp.text();
  delete Object.prototype.then;
  assert_equals(text, 'hello', 'The value should be "hello".');
}, 'Attempt to inject undefined via Object.prototype.then.');

promise_test(async (t) => {
  add_completion_callback(() => delete Object.prototype.then);
  const hello = new TextEncoder().encode('hello');
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue(hello);
      controller.close();
    }
  });
  const resp = new Response(rs);
  Object.prototype.then = (onFulfilled) => {
    delete Object.prototype.then;
    onFulfilled(8.2);
  };
  const text = await resp.text();
  delete Object.prototype.then;
  assert_equals(text, 'hello', 'The value should be "hello".');
}, 'Attempt to inject 8.2 via Object.prototype.then.');

promise_test(async () => {
  add_completion_callback(() => delete Object.prototype.then);
  const hello = new TextEncoder().encode('hello');
  const bye = new TextEncoder().encode('bye');
  const resp = new Response(hello);
  Object.prototype.then = (onFulfilled) => {
    delete Object.prototype.then;
    onFulfilled({done: false, value: bye});
  };
  const text = await resp.text();
  delete Object.prototype.then;
  assert_equals(text, 'hello', 'The value should be "hello".');
}, 'intercepting arraybuffer to text conversion via Object.prototype.then ' +
   'should not be possible');

promise_test(async () => {
  add_completion_callback(() => delete Object.prototype.then);
  const u8a123 = new Uint8Array([1, 2, 3]);
  const u8a456 = new Uint8Array([4, 5, 6]);
  const resp = new Response(u8a123);
  const writtenBytes = [];
  const ws = new WritableStream({
    write(chunk) {
      writtenBytes.push(...Array.from(chunk));
    }
  });
  Object.prototype.then = (onFulfilled) => {
    delete Object.prototype.then;
    onFulfilled({done: false, value: u8a456});
  };
  await resp.body.pipeTo(ws);
  delete Object.prototype.then;
  assert_array_equals(writtenBytes, u8a123, 'The value should be [1, 2, 3]');
}, 'intercepting arraybuffer to body readable stream conversion via ' +
   'Object.prototype.then should not be possible');
