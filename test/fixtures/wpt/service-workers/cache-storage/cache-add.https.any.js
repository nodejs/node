// META: title=Cache.add and Cache.addAll
// META: global=window,worker
// META: script=/common/get-host-info.sub.js
// META: script=./resources/test-helpers.js
// META: timeout=long

const { REMOTE_HOST } = get_host_info();

cache_test(function(cache, test) {
    return promise_rejects_js(
      test,
      TypeError,
      cache.add(),
      'Cache.add should throw a TypeError when no arguments are given.');
  }, 'Cache.add called with no arguments');

cache_test(function(cache) {
    return cache.add('./resources/simple.txt')
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
          return cache.match('./resources/simple.txt');
        })
        .then(function(response) {
          assert_class_string(response, 'Response',
                              'Cache.add should put a resource in the cache.');
          return response.text();
        })
        .then(function(body) {
          assert_equals(body, 'a simple text file\n',
                        'Cache.add should retrieve the correct body.');
        });
  }, 'Cache.add called with relative URL specified as a string');

cache_test(function(cache, test) {
    return promise_rejects_js(
      test,
      TypeError,
      cache.add('javascript://this-is-not-http-mmkay'),
      'Cache.add should throw a TypeError for non-HTTP/HTTPS URLs.');
  }, 'Cache.add called with non-HTTP/HTTPS URL');

cache_test(function(cache) {
    var request = new Request('./resources/simple.txt');
    return cache.add(request)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
        });
  }, 'Cache.add called with Request object');

cache_test(function(cache, test) {
    var request = new Request('./resources/simple.txt',
                              {method: 'POST', body: 'This is a body.'});
    return promise_rejects_js(
      test,
      TypeError,
      cache.add(request),
      'Cache.add should throw a TypeError for non-GET requests.');
  }, 'Cache.add called with POST request');

cache_test(function(cache) {
    var request = new Request('./resources/simple.txt');
    return cache.add(request)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
        })
      .then(function() {
          return cache.add(request);
        })
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.add should resolve with undefined on success.');
        });
  }, 'Cache.add called twice with the same Request object');

cache_test(function(cache) {
    var request = new Request('./resources/simple.txt');
    return request.text()
      .then(function() {
          assert_false(request.bodyUsed);
        })
      .then(function() {
          return cache.add(request);
        });
  }, 'Cache.add with request with null body (not consumed)');

cache_test(function(cache, test) {
    return promise_rejects_js(
      test,
      TypeError,
      cache.add('./resources/fetch-status.py?status=206'),
      'Cache.add should reject on partial response');
  }, 'Cache.add with 206 response');

cache_test(function(cache, test) {
    var urls = ['./resources/fetch-status.py?status=206',
                './resources/fetch-status.py?status=200'];
    var requests = urls.map(function(url) {
        return new Request(url);
      });
    return promise_rejects_js(
      test,
      TypeError,
      cache.addAll(requests),
      'Cache.addAll should reject with TypeError if any request fails');
  }, 'Cache.addAll with 206 response');

cache_test(function(cache, test) {
    var urls = ['./resources/fetch-status.py?status=206',
                './resources/fetch-status.py?status=200'];
    var requests = urls.map(function(url) {
        var cross_origin_url = new URL(url, location.href);
        cross_origin_url.hostname = REMOTE_HOST;
        return new Request(cross_origin_url.href, { mode: 'no-cors' });
      });
    return promise_rejects_js(
      test,
      TypeError,
      cache.addAll(requests),
      'Cache.addAll should reject with TypeError if any request fails');
  }, 'Cache.addAll with opaque-filtered 206 response');

cache_test(function(cache, test) {
    return promise_rejects_js(
      test,
      TypeError,
      cache.add('this-does-not-exist-please-dont-create-it'),
      'Cache.add should reject if response is !ok');
  }, 'Cache.add with request that results in a status of 404');


cache_test(function(cache, test) {
    return promise_rejects_js(
      test,
      TypeError,
      cache.add('./resources/fetch-status.py?status=500'),
      'Cache.add should reject if response is !ok');
  }, 'Cache.add with request that results in a status of 500');

cache_test(function(cache, test) {
    return promise_rejects_js(
      test,
      TypeError,
      cache.addAll(),
      'Cache.addAll with no arguments should throw TypeError.');
  }, 'Cache.addAll with no arguments');

cache_test(function(cache, test) {
    // Assumes the existence of ../resources/simple.txt and ../resources/blank.html
    var urls = ['./resources/simple.txt', undefined, './resources/blank.html'];
    return promise_rejects_js(
      test,
      TypeError,
      cache.addAll(urls),
      'Cache.addAll should throw TypeError for an undefined argument.');
  }, 'Cache.addAll with a mix of valid and undefined arguments');

cache_test(function(cache) {
    return cache.addAll([])
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.addAll should resolve with undefined on ' +
                        'success.');
          return cache.keys();
        })
      .then(function(result) {
          assert_equals(result.length, 0,
                        'There should be no entry in the cache.');
        });
  }, 'Cache.addAll with an empty array');

cache_test(function(cache) {
    // Assumes the existence of ../resources/simple.txt and
    // ../resources/blank.html
    var urls = ['./resources/simple.txt',
                self.location.href,
                './resources/blank.html'];
    return cache.addAll(urls)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.addAll should resolve with undefined on ' +
                        'success.');
          return Promise.all(
            urls.map(function(url) { return cache.match(url); }));
        })
      .then(function(responses) {
          assert_class_string(
            responses[0], 'Response',
            'Cache.addAll should put a resource in the cache.');
          assert_class_string(
            responses[1], 'Response',
            'Cache.addAll should put a resource in the cache.');
          assert_class_string(
            responses[2], 'Response',
            'Cache.addAll should put a resource in the cache.');
          return Promise.all(
            responses.map(function(response) { return response.text(); }));
        })
      .then(function(bodies) {
          assert_equals(
            bodies[0], 'a simple text file\n',
            'Cache.add should retrieve the correct body.');
          assert_equals(
            bodies[2], '<!DOCTYPE html>\n<title>Empty doc</title>\n',
            'Cache.add should retrieve the correct body.');
        });
  }, 'Cache.addAll with string URL arguments');

cache_test(function(cache) {
    // Assumes the existence of ../resources/simple.txt and
    // ../resources/blank.html
    var urls = ['./resources/simple.txt',
                self.location.href,
                './resources/blank.html'];
    var requests = urls.map(function(url) {
        return new Request(url);
      });
    return cache.addAll(requests)
      .then(function(result) {
          assert_equals(result, undefined,
                        'Cache.addAll should resolve with undefined on ' +
                        'success.');
          return Promise.all(
            urls.map(function(url) { return cache.match(url); }));
        })
      .then(function(responses) {
          assert_class_string(
            responses[0], 'Response',
            'Cache.addAll should put a resource in the cache.');
          assert_class_string(
            responses[1], 'Response',
            'Cache.addAll should put a resource in the cache.');
          assert_class_string(
            responses[2], 'Response',
            'Cache.addAll should put a resource in the cache.');
          return Promise.all(
            responses.map(function(response) { return response.text(); }));
        })
      .then(function(bodies) {
          assert_equals(
            bodies[0], 'a simple text file\n',
            'Cache.add should retrieve the correct body.');
          assert_equals(
            bodies[2], '<!DOCTYPE html>\n<title>Empty doc</title>\n',
            'Cache.add should retrieve the correct body.');
        });
  }, 'Cache.addAll with Request arguments');

cache_test(function(cache, test) {
    // Assumes that ../resources/simple.txt and ../resources/blank.html exist.
    // The second resource does not.
    var urls = ['./resources/simple.txt',
                'this-resource-should-not-exist',
                './resources/blank.html'];
    var requests = urls.map(function(url) {
        return new Request(url);
      });
    return promise_rejects_js(
      test,
      TypeError,
      cache.addAll(requests),
      'Cache.addAll should reject with TypeError if any request fails')
      .then(function() {
          return Promise.all(urls.map(function(url) {
              return cache.match(url);
            }));
      })
      .then(function(matches) {
          assert_array_equals(
            matches,
            [undefined, undefined, undefined],
            'If any response fails, no response should be added to cache');
      });
  }, 'Cache.addAll with a mix of succeeding and failing requests');

cache_test(function(cache, test) {
    var request = new Request('./resources/simple.txt');
    return promise_rejects_dom(
      test,
      'InvalidStateError',
      cache.addAll([request, request]),
      'Cache.addAll should throw InvalidStateError if the same request is added ' +
      'twice.');
  }, 'Cache.addAll called with the same Request object specified twice');

cache_test(async function(cache, test) {
    const url = './resources/vary.py?vary=x-shape';
    let requests = [
      new Request(url, { headers: { 'x-shape': 'circle' }}),
      new Request(url, { headers: { 'x-shape': 'square' }}),
    ];
    let result = await cache.addAll(requests);
    assert_equals(result, undefined, 'Cache.addAll() should succeed');
  }, 'Cache.addAll should succeed when entries differ by vary header');

cache_test(async function(cache, test) {
    const url = './resources/vary.py?vary=x-shape';
    let requests = [
      new Request(url, { headers: { 'x-shape': 'circle' }}),
      new Request(url, { headers: { 'x-shape': 'circle' }}),
    ];
    await promise_rejects_dom(
      test,
      'InvalidStateError',
      cache.addAll(requests),
      'Cache.addAll() should reject when entries are duplicate by vary header');
  }, 'Cache.addAll should reject when entries are duplicate by vary header');

// VARY header matching is asymmetric.  Determining if two entries are duplicate
// depends on which entry's response is used in the comparison.  The target
// response's VARY header determines what request headers are examined.  This
// test verifies that Cache.addAll() duplicate checking handles this asymmetric
// behavior correctly.
cache_test(async function(cache, test) {
    const base_url = './resources/vary.py';

    // Define a request URL that sets a VARY header in the
    // query string to be echoed back by the server.
    const url = base_url + '?vary=x-size';

    // Set a cookie to override the VARY header of the response
    // when the request is made with credentials.  This will
    // take precedence over the query string vary param.  This
    // is a bit confusing, but it's necessary to construct a test
    // where the URL is the same, but the VARY headers differ.
    //
    // Note, the test could also pass this information in additional
    // request headers.  If the cookie approach becomes too unwieldy
    // this test could be rewritten to use that technique.
    await fetch(base_url + '?set-vary-value-override-cookie=x-shape');
    test.add_cleanup(_ => fetch(base_url + '?clear-vary-value-override-cookie'));

    let requests = [
      // This request will result in a Response with a "Vary: x-shape"
      // header.  This *will not* result in a duplicate match with the
      // other entry.
      new Request(url, { headers: { 'x-shape': 'circle',
                                    'x-size': 'big' },
                         credentials: 'same-origin' }),

      // This request will result in a Response with a "Vary: x-size"
      // header.  This *will* result in a duplicate match with the other
      // entry.
      new Request(url, { headers: { 'x-shape': 'square',
                                    'x-size': 'big' },
                         credentials: 'omit' }),
    ];
    await promise_rejects_dom(
      test,
      'InvalidStateError',
      cache.addAll(requests),
      'Cache.addAll() should reject when one entry has a vary header ' +
      'matching an earlier entry.');

    // Test the reverse order now.
    await promise_rejects_dom(
      test,
      'InvalidStateError',
      cache.addAll(requests.reverse()),
      'Cache.addAll() should reject when one entry has a vary header ' +
      'matching a later entry.');

  }, 'Cache.addAll should reject when one entry has a vary header ' +
     'matching another entry');

done();
