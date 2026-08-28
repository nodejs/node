// Registration tests that mostly exercise the service worker script contents or
// response.
function registration_tests_script(register_method, type) {
  promise_test(function(t) {
      var script = 'resources/invalid-chunked-encoding.py';
      var scope = 'resources/scope/invalid-chunked-encoding/';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Registration of invalid chunked encoding script should fail.');
    }, 'Registering invalid chunked encoding script');

  promise_test(function(t) {
      var script = 'resources/invalid-chunked-encoding-with-flush.py';
      var scope = 'resources/scope/invalid-chunked-encoding-with-flush/';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Registration of invalid chunked encoding script should fail.');
    }, 'Registering invalid chunked encoding script with flush');

  promise_test(function(t) {
      var script = 'resources/malformed-worker.py?parse-error';
      var scope = 'resources/scope/parse-error';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Registration of script including parse error should fail.');
    }, 'Registering script including parse error');

  promise_test(function(t) {
      var script = 'resources/malformed-worker.py?undefined-error';
      var scope = `resources/scope/undefined-error/${type}`;
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Registration of script including undefined error should fail.');
    }, `Registering script including undefined error: ${type}`);

  promise_test(function(t) {
      var script = 'resources/malformed-worker.py?uncaught-exception';
      var scope = `resources/scope/uncaught-exception/${type}`;
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Registration of script including uncaught exception should fail.');
    }, `Registering script including uncaught exception: ${type}`);

  if (type === 'classic') {
    promise_test(function(t) {
        var script = 'resources/malformed-worker.py?import-malformed-script';
        var scope = 'resources/scope/import-malformed-script';
        return promise_rejects_js(t,
            TypeError,
            register_method(script, {scope: scope}),
            'Registration of script importing malformed script should fail.');
      }, 'Registering script importing malformed script');
  }

  if (type === 'module') {
    promise_test(function(t) {
        var script = 'resources/malformed-worker.py?top-level-await';
        var scope = 'resources/scope/top-level-await';
        return promise_rejects_js(t,
            TypeError,
            register_method(script, {scope: scope}),
            'Registration of script with top-level await should fail.');
      }, 'Registering script with top-level await');

    promise_test(function(t) {
        var script = 'resources/malformed-worker.py?instantiation-error';
        var scope = 'resources/scope/instantiation-error';
        return promise_rejects_js(t,
            TypeError,
            register_method(script, {scope: scope}),
            'Registration of script with module instantiation error should fail.');
      }, 'Registering script with module instantiation error');

    promise_test(function(t) {
        var script = 'resources/malformed-worker.py?instantiation-error-and-top-level-await';
        var scope = 'resources/scope/instantiation-error-and-top-level-await';
        return promise_rejects_js(t,
            TypeError,
            register_method(script, {scope: scope}),
            'Registration of script with module instantiation error and top-level await should fail.');
      }, 'Registering script with module instantiation error and top-level await');
  }

  promise_test(function(t) {
      var script = 'resources/no-such-worker.js';
      var scope = 'resources/scope/no-such-worker';
      return promise_rejects_js(t,
          TypeError,
          register_method(script, {scope: scope}),
          'Registration of non-existent script should fail.');
    }, 'Registering non-existent script');

  if (type === 'classic') {
    promise_test(function(t) {
        var script = 'resources/malformed-worker.py?import-no-such-script';
        var scope = 'resources/scope/import-no-such-script';
        return promise_rejects_js(t,
            TypeError,
            register_method(script, {scope: scope}),
            'Registration of script importing non-existent script should fail.');
      }, 'Registering script importing non-existent script');
  }

  promise_test(function(t) {
      var script = 'resources/malformed-worker.py?caught-exception';
      var scope = 'resources/scope/caught-exception';
      return register_method(script, {scope: scope})
        .then(function(registration) {
            assert_true(
              registration instanceof ServiceWorkerRegistration,
              'Successfully registered.');
            return registration.unregister();
          });
    }, 'Registering script including caught exception');

}
