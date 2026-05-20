// This method generates a number of tests verifying fetching of blob URLs,
// allowing the same tests to be used both with fetch() and XMLHttpRequest.
//
// |fetch_method| is only used in test names, and should describe the
// (javascript) method being used by the other two arguments (i.e. 'fetch' or 'XHR').
//
// |fetch_should_succeed| is a callback that is called with the Test and a URL.
// Fetching the URL is expected to succeed. The callback should return a promise
// resolved with whatever contents were fetched.
//
// |fetch_should_fail| similarly is a callback that is called with the Test, a URL
// to fetch, and optionally a method to use to do the fetch. If no method is
// specified the callback should use the 'GET' method. Fetching of these URLs is
// expected to fail, and the callback should return a promise that resolves iff
// fetching did indeed fail.
function fetch_tests(fetch_method, fetch_should_succeed, fetch_should_fail) {
  const blob_contents = 'test blob contents';
  const blob = new Blob([blob_contents]);

  promise_test(t => {
    const url = URL.createObjectURL(blob);

    return fetch_should_succeed(t, url).then(text => {
      assert_equals(text, blob_contents);
    });
  }, 'Blob URLs can be used in ' + fetch_method);

  promise_test(t => {
    const url = URL.createObjectURL(blob);

    return fetch_should_succeed(t, url + '#fragment').then(text => {
      assert_equals(text, blob_contents);
    });
  }, fetch_method + ' with a fragment should succeed');

  promise_test(t => {
    const url = URL.createObjectURL(blob);
    URL.revokeObjectURL(url);

    return fetch_should_fail(t, url);
  }, fetch_method + ' of a revoked URL should fail');

  promise_test(t => {
    const url = URL.createObjectURL(blob);
    URL.revokeObjectURL(url + '#fragment');

    return fetch_should_succeed(t, url).then(text => {
      assert_equals(text, blob_contents);
    });
  }, 'Only exact matches should revoke URLs, using ' + fetch_method);

  promise_test(t => {
    const url = URL.createObjectURL(blob);

    return fetch_should_fail(t, url + '?querystring');
  }, 'Appending a query string should cause ' + fetch_method + ' to fail');

  promise_test(t => {
    const url = URL.createObjectURL(blob);

    return fetch_should_fail(t, url + '/path');
  }, 'Appending a path should cause ' + fetch_method + ' to fail');

  for (const method of ['HEAD', 'POST', 'DELETE', 'OPTIONS', 'PUT', 'CUSTOM']) {
    const url = URL.createObjectURL(blob);

    promise_test(t => {
      return fetch_should_fail(t, url, method);
    }, fetch_method + ' with method "' + method + '" should fail');
  }
}