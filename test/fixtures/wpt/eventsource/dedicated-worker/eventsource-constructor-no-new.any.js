test(function() {
  assert_throws_js(TypeError,
  function() {
    EventSource('');
  },
  "Calling EventSource constructor without 'new' must throw");
})
