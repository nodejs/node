// Registration tests that mostly exercise the scope option.
function registration_tests_scope(register_method) {
  promise_test(function(t) {
      var script = 'resources/empty-worker.js';
      var scope = 'resources/scope%2fencoded-slash-in-scope';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'URL-encoded slash in the scope should be rejected.');
    }, 'Scope including URL-encoded slash');

  promise_test(function(t) {
      var script = 'resources/empty-worker.js';
      var scope = 'resources/scope%5cencoded-slash-in-scope';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'URL-encoded backslash in the scope should be rejected.');
    }, 'Scope including URL-encoded backslash');

  promise_test(function(t) {
      var script = 'resources/empty-worker.js';
      var scope = 'data:text/html,';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'scope URL scheme is not "http" or "https"');
    }, 'Scope URL scheme is a data: URL');

  promise_test(function(t) {
      var script = 'resources/empty-worker.js';
      var scope = new URL('resources', location).href.replace('https:', 'ftp:');
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'scope URL scheme is not "http" or "https"');
    }, 'Scope URL scheme is an ftp: URL');

  promise_test(function(t) {
      // URL-encoded full-width 'scope'.
      var name = '%ef%bd%93%ef%bd%83%ef%bd%8f%ef%bd%90%ef%bd%85';
      var script = 'resources/empty-worker.js';
      var scope = 'resources/' + name + '/escaped-multibyte-character-scope';
      return register_method(script, {scope: scope})
        .then(function(registration) {
            assert_equals(
              registration.scope,
              normalizeURL(scope),
              'URL-encoded multibyte characters should be available.');
            return registration.unregister();
          });
    }, 'Scope including URL-encoded multibyte characters');

  promise_test(function(t) {
      // Non-URL-encoded full-width "scope".
      var name = String.fromCodePoint(0xff53, 0xff43, 0xff4f, 0xff50, 0xff45);
      var script = 'resources/empty-worker.js';
      var scope = 'resources/' + name  + '/non-escaped-multibyte-character-scope';
      return register_method(script, {scope: scope})
        .then(function(registration) {
            assert_equals(
              registration.scope,
              normalizeURL(scope),
              'Non-URL-encoded multibyte characters should be available.');
            return registration.unregister();
          });
    }, 'Scope including non-escaped multibyte characters');

  promise_test(function(t) {
      var script = 'resources/empty-worker.js';
      var scope = 'resources/././scope/self-reference-in-scope';
      return register_method(script, {scope: scope})
        .then(function(registration) {
            assert_equals(
              registration.scope,
              normalizeURL('resources/scope/self-reference-in-scope'),
              'Scope including self-reference should be normalized.');
            return registration.unregister();
          });
    }, 'Scope including self-reference');

  promise_test(function(t) {
      var script = 'resources/empty-worker.js';
      var scope = 'resources/../resources/scope/parent-reference-in-scope';
      return register_method(script, {scope: scope})
        .then(function(registration) {
            assert_equals(
              registration.scope,
              normalizeURL('resources/scope/parent-reference-in-scope'),
              'Scope including parent-reference should be normalized.');
            return registration.unregister();
          });
    }, 'Scope including parent-reference');

  promise_test(function(t) {
      var script = 'resources/empty-worker.js';
      var scope = 'resources/scope////consecutive-slashes-in-scope';
      return register_method(script, {scope: scope})
        .then(function(registration) {
            // Although consecutive slashes in the scope are not unified, the
            // scope is under the script directory and registration should
            // succeed.
            assert_equals(
              registration.scope,
              normalizeURL(scope),
              'Should successfully be registered.');
            return registration.unregister();
          })
    }, 'Scope including consecutive slashes');

  promise_test(function(t) {
      var script = 'resources/empty-worker.js';
      var scope = 'filesystem:' + normalizeURL('resources/scope/filesystem-scope-url');
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Registering with the scope that has same-origin filesystem: URL ' +
              'should fail with TypeError.');
    }, 'Scope URL is same-origin filesystem: URL');
}
