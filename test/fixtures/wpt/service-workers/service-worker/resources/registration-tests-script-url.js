// Registration tests that mostly exercise the scriptURL parameter.
function registration_tests_script_url(register_method) {
  promise_test(function(t) {
        var script = 'resources%2fempty-worker.js';
        var scope = 'resources/scope/encoded-slash-in-script-url';
        return promise_rejects_js(t,
            TypeError,
            register_method(script, {scope: scope}),
            'URL-encoded slash in the script URL should be rejected.');
      }, 'Script URL including URL-encoded slash');

  promise_test(function(t) {
      var script = 'resources%2Fempty-worker.js';
      var scope = 'resources/scope/encoded-slash-in-script-url';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'URL-encoded slash in the script URL should be rejected.');
    }, 'Script URL including uppercase URL-encoded slash');

  promise_test(function(t) {
      var script = 'resources%5cempty-worker.js';
      var scope = 'resources/scope/encoded-slash-in-script-url';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'URL-encoded backslash in the script URL should be rejected.');
    }, 'Script URL including URL-encoded backslash');

  promise_test(function(t) {
      var script = 'resources%5Cempty-worker.js';
      var scope = 'resources/scope/encoded-slash-in-script-url';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'URL-encoded backslash in the script URL should be rejected.');
    }, 'Script URL including uppercase URL-encoded backslash');

  promise_test(function(t) {
      var script = 'data:application/javascript,';
      var scope = 'resources/scope/data-url-in-script-url';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Data URLs should not be registered as service workers.');
    }, 'Script URL is a data URL');

  promise_test(function(t) {
      var script = 'data:application/javascript,';
      var scope = new URL('resources/scope/data-url-in-script-url', location);
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Data URLs should not be registered as service workers.');
    }, 'Script URL is a data URL and scope URL is not relative');

  promise_test(function(t) {
      var script = 'resources/././empty-worker.js';
      var scope = 'resources/scope/parent-reference-in-script-url';
      return register_method(script, {scope: scope})
        .then(function(registration) {
            assert_equals(
              get_newest_worker(registration).scriptURL,
              normalizeURL('resources/empty-worker.js'),
              'Script URL including self-reference should be normalized.');
            return registration.unregister();
          });
    }, 'Script URL including self-reference');

  promise_test(function(t) {
      var script = 'resources/../resources/empty-worker.js';
      var scope = 'resources/scope/parent-reference-in-script-url';
      return register_method(script, {scope: scope})
        .then(function(registration) {
            assert_equals(
              get_newest_worker(registration).scriptURL,
              normalizeURL('resources/empty-worker.js'),
              'Script URL including parent-reference should be normalized.');
            return registration.unregister();
          });
    }, 'Script URL including parent-reference');
}
