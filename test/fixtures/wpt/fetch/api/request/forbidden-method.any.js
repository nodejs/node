// META: global=window,worker

// https://fetch.spec.whatwg.org/#forbidden-method
for (const method of [
    'CONNECT', 'TRACE', 'TRACK',
    'connect', 'trace', 'track'
  ]) {
  test(function() {
    assert_throws_js(TypeError,
      function() { new Request('./', {method: method}); }
    );
  }, 'Request() with a forbidden method ' + method + ' must throw.');
}
