// META: script=resources/fetch-tests.js

function fetch_should_succeed(test, request) {
  return fetch(request).then(response => response.text());
}

function fetch_should_fail(test, url, method = 'GET') {
  return promise_rejects_js(test, TypeError, fetch(url, {method: method}));
}

fetch_tests('fetch', fetch_should_succeed, fetch_should_fail);

promise_test(t => {
  const blob_contents = 'test blob contents';
  const blob_type = 'image/png';
  const blob = new Blob([blob_contents], {type: blob_type});
  const url = URL.createObjectURL(blob);

  return fetch(url).then(response => {
    assert_equals(response.headers.get('Content-Type'), blob_type);
  });
}, 'fetch should return Content-Type from Blob');

promise_test(t => {
  const blob_contents = 'test blob contents';
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);
  const request = new Request(url);

  // Revoke the object URL.  Request should take a reference to the blob as
  // soon as it receives it in open(), so the request succeeds even though we
  // revoke the URL before calling fetch().
  URL.revokeObjectURL(url);

  return fetch_should_succeed(t, request).then(text => {
    assert_equals(text, blob_contents);
  });
}, 'Revoke blob URL after creating Request, will fetch');

promise_test(function(t) {
  const blob_contents = 'test blob contents';
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);

  const result = fetch_should_succeed(t, url).then(text => {
    assert_equals(text, blob_contents);
  });

  // Revoke the object URL. fetch should have already resolved the blob URL.
  URL.revokeObjectURL(url);

  return result;
}, 'Revoke blob URL after calling fetch, fetch should succeed');
