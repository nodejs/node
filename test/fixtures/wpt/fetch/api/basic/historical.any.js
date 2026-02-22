// META: global=window,worker

test(() => {
  assert_false("getAll" in new Headers());
  assert_false("getAll" in Headers.prototype);
}, "Headers object no longer has a getAll() method");

test(() => {
  assert_false("type" in new Request("about:blank"));
  assert_false("type" in Request.prototype);
}, "'type' getter should not exist on Request objects");

// See https://github.com/whatwg/fetch/pull/979 for the removal
test(() => {
  assert_false("trailer" in new Response());
  assert_false("trailer" in Response.prototype);
}, "Response object no longer has a trailer getter");
