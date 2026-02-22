// Changing the body after it have been passed to Response/Request
// should not change the outcome of the consumed body

const url = 'http://a';
const method = 'post';

promise_test(async t => {
  const body = new FormData();
  body.set('a', '1');
  const res = new Response(body);
  const req = new Request(url, { method, body });
  body.set('a', '2');
  assert_true((await res.formData()).get('a') === '1');
  assert_true((await req.formData()).get('a') === '1');
}, 'FormData is cloned');

promise_test(async t => {
  const body = new URLSearchParams({a: '1'});
  const res = new Response(body);
  const req = new Request(url, { method, body });
  body.set('a', '2');
  assert_true((await res.formData()).get('a') === '1');
  assert_true((await req.formData()).get('a') === '1');
}, 'URLSearchParams is cloned');

promise_test(async t => {
  const body = new Uint8Array([97]); // a
  const res = new Response(body);
  const req = new Request(url, { method, body });
  body[0] = 98; // b
  assert_true(await res.text() === 'a');
  assert_true(await req.text() === 'a');
}, 'TypedArray is cloned');

promise_test(async t => {
  const body = new Uint8Array([97]); // a
  const res = new Response(body.buffer);
  const req = new Request(url, { method, body: body.buffer });
  body[0] = 98; // b
  assert_true(await res.text() === 'a');
  assert_true(await req.text() === 'a');
}, 'ArrayBuffer is cloned');

promise_test(async t => {
  const body = new Blob(['a']);
  const res = new Response(body);
  const req = new Request(url, { method, body });
  assert_true(await res.blob() !== body);
  assert_true(await req.blob() !== body);
}, 'Blob is cloned');
