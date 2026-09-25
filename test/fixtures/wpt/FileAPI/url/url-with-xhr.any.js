// META: script=resources/fetch-tests.js

function xhr_should_succeed(test, url) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.onload = test.step_func(() => {
      assert_equals(xhr.status, 200);
      assert_equals(xhr.statusText, 'OK');
      resolve(xhr.response);
    });
    xhr.onerror = () => reject('Got unexpected error event');
    xhr.send();
  });
}

function xhr_should_fail(test, url, method = 'GET') {
  const xhr = new XMLHttpRequest();
  xhr.open(method, url);
  const result1 = new Promise((resolve, reject) => {
    xhr.onload = () => reject('Got unexpected load event');
    xhr.onerror = resolve;
  });
  const result2 = new Promise(resolve => {
    xhr.onreadystatechange = test.step_func(() => {
      if (xhr.readyState !== xhr.DONE) return;
      assert_equals(xhr.status, 0);
      resolve();
    });
  });
  xhr.send();
  return Promise.all([result1, result2]);
}

fetch_tests('XHR', xhr_should_succeed, xhr_should_fail);

async_test(t => {
  const blob_contents = 'test blob contents';
  const blob_type = 'image/png';
  const blob = new Blob([blob_contents], {type: blob_type});
  const url = URL.createObjectURL(blob);
  const xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.onloadend = t.step_func_done(() => {
    assert_equals(xhr.getResponseHeader('Content-Type'), blob_type);
  });
  xhr.send();
}, 'XHR should return Content-Type from Blob');

async_test(t => {
  const blob_contents = 'test blob contents';
  const blob = new Blob([blob_contents]);
  const url = URL.createObjectURL(blob);
  const xhr = new XMLHttpRequest();
  xhr.open('GET', url);

  // Revoke the object URL.  XHR should take a reference to the blob as soon as
  // it receives it in open(), so the request succeeds even though we revoke the
  // URL before calling send().
  URL.revokeObjectURL(url);

  xhr.onload = t.step_func_done(() => {
    assert_equals(xhr.response, blob_contents);
  });
  xhr.onerror = t.unreached_func('Got unexpected error event');

  xhr.send();
}, 'Revoke blob URL after open(), will fetch');
