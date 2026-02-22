// META: global=window,worker
// META: script=../resources/utils.js

// See https://github.com/whatwg/fetch/issues/883 for the behavior covered by
// this test. As of writing, the Fetch spec has not been updated to cover these.

// redirectLocation tests that a Location header of |locationHeader| is resolved
// to a URL which ends in |expectedUrlSuffix|. |locationHeader| is interpreted
// as a byte sequence via isomorphic encode, as described in [INFRA]. This
// allows the caller to specify byte sequences which are not valid UTF-8.
// However, this means, e.g., U+2603 must be passed in as "\xe2\x98\x83", its
// UTF-8 encoding, not "\u2603".
//
// [INFRA] https://infra.spec.whatwg.org/#isomorphic-encode
function redirectLocation(
    desc, redirectUrl, locationHeader, expectedUrlSuffix) {
  promise_test(function(test) {
    // Note we use escape() instead of encodeURIComponent(), so that characters
    // are escaped as bytes in the isomorphic encoding.
    var url = redirectUrl + '?simple=1&location=' + escape(locationHeader);

    return fetch(url, {'redirect': 'follow'}).then(function(resp) {
      assert_true(
          resp.url.endsWith(expectedUrlSuffix),
          resp.url + ' ends with ' + expectedUrlSuffix);
    });
  }, desc);
}

var redirUrl = RESOURCES_DIR + 'redirect.py';
redirectLocation(
    'Redirect to escaped UTF-8', redirUrl, 'top.txt?%E2%98%83%e2%98%83',
    'top.txt?%E2%98%83%e2%98%83');
redirectLocation(
    'Redirect to unescaped UTF-8', redirUrl, 'top.txt?\xe2\x98\x83',
    'top.txt?%E2%98%83');
redirectLocation(
    'Redirect to escaped and unescaped UTF-8', redirUrl,
    'top.txt?\xe2\x98\x83%e2%98%83', 'top.txt?%E2%98%83%e2%98%83');
redirectLocation(
    'Escaping produces double-percent', redirUrl, 'top.txt?%\xe2\x98\x83',
    'top.txt?%%E2%98%83');
redirectLocation(
    'Redirect to invalid UTF-8', redirUrl, 'top.txt?\xff', 'top.txt?%FF');

done();
