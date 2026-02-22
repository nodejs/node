// META: global=window,worker

test(() => {
  const r = new Response(new ReadableStream());
  // highWaterMark: 0 means that nothing will actually be read from the body.
  r.body.pipeTo(new WritableStream({}, {highWaterMark: 0}));
  assert_true(r.bodyUsed, 'bodyUsed should be true');
}, 'using pipeTo on Response body should disturb it synchronously');

test(() => {
  const r = new Response(new ReadableStream());
  r.body.pipeThrough({
    writable: new WritableStream({}, {highWaterMark: 0}),
    readable: new ReadableStream()
  });
  assert_true(r.bodyUsed, 'bodyUsed should be true');
}, 'using pipeThrough on Response body should disturb it synchronously');
