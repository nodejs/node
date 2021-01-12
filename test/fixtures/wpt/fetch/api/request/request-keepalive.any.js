// META: global=window,worker
// META: title=Request keepalive
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js

test(() => {
  assert_false(new Request('/').keepalive, 'default');
  assert_true(new Request('/', {keepalive: true}).keepalive, 'true');
  assert_false(new Request('/', {keepalive: false}).keepalive, 'false');
  assert_true(new Request('/', {keepalive: 1}).keepalive, 'truish');
  assert_false(new Request('/', {keepalive: 0}).keepalive, 'falsy');
}, 'keepalive flag');

test(() => {
  const init = {method: 'POST', keepalive: true, body: new ReadableStream()};
  assert_throws_js(TypeError, () => {new Request('/', init)});
}, 'keepalive flag with stream body');
