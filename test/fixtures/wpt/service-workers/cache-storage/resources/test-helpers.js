(function() {
  var next_cache_index = 1;

  // Returns a promise that resolves to a newly created Cache object. The
  // returned Cache will be destroyed when |test| completes.
  function create_temporary_cache(test) {
    var uniquifier = String(++next_cache_index);
    var cache_name = self.location.pathname + '/' + uniquifier;

    test.add_cleanup(function() {
        self.caches.delete(cache_name);
      });

    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        });
  }

  self.create_temporary_cache = create_temporary_cache;
})();

// Runs |test_function| with a temporary unique Cache passed in as the only
// argument. The function is run as a part of Promise chain owned by
// promise_test(). As such, it is expected to behave in a manner identical (with
// the exception of the argument) to a function passed into promise_test().
//
// E.g.:
//    cache_test(function(cache) {
//      // Do something with |cache|, which is a Cache object.
//    }, "Some Cache test");
function cache_test(test_function, description) {
  promise_test(function(test) {
      return create_temporary_cache(test)
        .then(function(cache) { return test_function(cache, test); });
    }, description);
}

// A set of Request/Response pairs to be used with prepopulated_cache_test().
var simple_entries = [
  {
    name: 'a',
    request: new Request('http://example.com/a'),
    response: new Response('')
  },

  {
    name: 'b',
    request: new Request('http://example.com/b'),
    response: new Response('')
  },

  {
    name: 'a_with_query',
    request: new Request('http://example.com/a?q=r'),
    response: new Response('')
  },

  {
    name: 'A',
    request: new Request('http://example.com/A'),
    response: new Response('')
  },

  {
    name: 'a_https',
    request: new Request('https://example.com/a'),
    response: new Response('')
  },

  {
    name: 'a_org',
    request: new Request('http://example.org/a'),
    response: new Response('')
  },

  {
    name: 'cat',
    request: new Request('http://example.com/cat'),
    response: new Response('')
  },

  {
    name: 'catmandu',
    request: new Request('http://example.com/catmandu'),
    response: new Response('')
  },

  {
    name: 'cat_num_lives',
    request: new Request('http://example.com/cat?lives=9'),
    response: new Response('')
  },

  {
    name: 'cat_in_the_hat',
    request: new Request('http://example.com/cat/in/the/hat'),
    response: new Response('')
  },

  {
    name: 'non_2xx_response',
    request: new Request('http://example.com/non2xx'),
    response: new Response('', {status: 404, statusText: 'nope'})
  },

  {
    name: 'error_response',
    request: new Request('http://example.com/error'),
    response: Response.error()
  },
];

// A set of Request/Response pairs to be used with prepopulated_cache_test().
// These contain a mix of test cases that use Vary headers.
var vary_entries = [
  {
    name: 'vary_cookie_is_cookie',
    request: new Request('http://example.com/c',
                         {headers: {'Cookies': 'is-for-cookie'}}),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  },

  {
    name: 'vary_cookie_is_good',
    request: new Request('http://example.com/c',
                         {headers: {'Cookies': 'is-good-enough-for-me'}}),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  },

  {
    name: 'vary_cookie_absent',
    request: new Request('http://example.com/c'),
    response: new Response('',
                           {headers: {'Vary': 'Cookies'}})
  }
];

// Run |test_function| with a Cache object and a map of entries. Prior to the
// call, the Cache is populated by cache entries from |entries|. The latter is
// expected to be an Object mapping arbitrary keys to objects of the form
// {request: <Request object>, response: <Response object>}. Entries are
// serially added to the cache in the order specified.
//
// |test_function| should return a Promise that can be used with promise_test.
function prepopulated_cache_test(entries, test_function, description) {
  cache_test(function(cache) {
      var p = Promise.resolve();
      var hash = {};
      entries.forEach(function(entry) {
          hash[entry.name] = entry;
          p = p.then(function() {
              return cache.put(entry.request.clone(), entry.response.clone())
                  .catch(function(e) {
                      assert_unreached(
                          'Test setup failed for entry ' + entry.name + ': ' + e
                      );
                  });
          });
      });
      return p
        .then(function() {
            assert_equals(Object.keys(hash).length, entries.length);
        })
        .then(function() {
            return test_function(cache, hash);
        });
    }, description);
}

// Helper for testing with Headers objects. Compares Headers instances
// by serializing |expected| and |actual| to arrays and comparing.
function assert_header_equals(actual, expected, description) {
    assert_class_string(actual, "Headers", description);
    var header;
    var actual_headers = [];
    var expected_headers = [];
    for (header of actual)
        actual_headers.push(header[0] + ": " + header[1]);
    for (header of expected)
        expected_headers.push(header[0] + ": " + header[1]);
    assert_array_equals(actual_headers, expected_headers,
                        description + " Headers differ.");
}

// Helper for testing with Response objects. Compares simple
// attributes defined on the interfaces, as well as the headers. It
// does not compare the response bodies.
function assert_response_equals(actual, expected, description) {
    assert_class_string(actual, "Response", description);
    ["type", "url", "status", "ok", "statusText"].forEach(function(attribute) {
        assert_equals(actual[attribute], expected[attribute],
                      description + " Attributes differ: " + attribute + ".");
    });
    assert_header_equals(actual.headers, expected.headers, description);
}

// Assert that the two arrays |actual| and |expected| contain the same
// set of Responses as determined by assert_response_equals. The order
// is not significant.
//
// |expected| is assumed to not contain any duplicates.
function assert_response_array_equivalent(actual, expected, description) {
    assert_true(Array.isArray(actual), description);
    assert_equals(actual.length, expected.length, description);
    expected.forEach(function(expected_element) {
        // assert_response_in_array treats the first argument as being
        // 'actual', and the second as being 'expected array'. We are
        // switching them around because we want to be resilient
        // against the |actual| array containing duplicates.
        assert_response_in_array(expected_element, actual, description);
    });
}

// Asserts that two arrays |actual| and |expected| contain the same
// set of Responses as determined by assert_response_equals(). The
// corresponding elements must occupy corresponding indices in their
// respective arrays.
function assert_response_array_equals(actual, expected, description) {
    assert_true(Array.isArray(actual), description);
    assert_equals(actual.length, expected.length, description);
    actual.forEach(function(value, index) {
        assert_response_equals(value, expected[index],
                               description + " : object[" + index + "]");
    });
}

// Equivalent to assert_in_array, but uses assert_response_equals.
function assert_response_in_array(actual, expected_array, description) {
    assert_true(expected_array.some(function(element) {
        try {
            assert_response_equals(actual, element);
            return true;
        } catch (e) {
            return false;
        }
    }), description);
}

// Helper for testing with Request objects. Compares simple
// attributes defined on the interfaces, as well as the headers.
function assert_request_equals(actual, expected, description) {
    assert_class_string(actual, "Request", description);
    ["url"].forEach(function(attribute) {
        assert_equals(actual[attribute], expected[attribute],
                      description + " Attributes differ: " + attribute + ".");
    });
    assert_header_equals(actual.headers, expected.headers, description);
}

// Asserts that two arrays |actual| and |expected| contain the same
// set of Requests as determined by assert_request_equals(). The
// corresponding elements must occupy corresponding indices in their
// respective arrays.
function assert_request_array_equals(actual, expected, description) {
    assert_true(Array.isArray(actual), description);
    assert_equals(actual.length, expected.length, description);
    actual.forEach(function(value, index) {
        assert_request_equals(value, expected[index],
                              description + " : object[" + index + "]");
    });
}

// Deletes all caches, returning a promise indicating success.
function delete_all_caches() {
  return self.caches.keys()
    .then(function(keys) {
      return Promise.all(keys.map(self.caches.delete.bind(self.caches)));
    });
}
