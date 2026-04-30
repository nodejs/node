// META: global=window,worker
test(() => {
  assert_throws_js(TypeError, () => { new URLPattern(new URL('https://example.org/%(')); } );
  assert_throws_js(TypeError, () => { new URLPattern(new URL('https://example.org/%((')); } );
  assert_throws_js(TypeError, () => { new URLPattern('(\\'); } );
}, `Test unclosed token`);

test(() => {
  new URLPattern(undefined, undefined);
}, `Test constructor with undefined`);
