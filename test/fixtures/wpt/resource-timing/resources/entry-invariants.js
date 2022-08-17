// Asserts that the given attributes are present in 'entry' and hold equal
// values.
const assert_all_equal_ = (entry, attributes) => {
  let first = attributes[0];
  attributes.slice(1).forEach(other => {
    assert_equals(entry[first], entry[other],
      `${first} should be equal to ${other}`);
  });
}

// Asserts that the given attributes are present in 'entry' and hold values
// that are sorted in the same order as given in 'attributes'.
const assert_ordered_ = (entry, attributes) => {
  let before = attributes[0];
  attributes.slice(1).forEach(after => {
    assert_greater_than_equal(entry[after], entry[before],
      `${after} should be greater than ${before}`);
    before = after;
  });
}

// Asserts that the given attributes are present in 'entry' and hold a value of
// 0.
const assert_zeroed_ = (entry, attributes) => {
  attributes.forEach(attribute => {
    assert_equals(entry[attribute], 0, `${attribute} should be 0`);
  });
}

// Asserts that the given attributes are present in 'entry' and hold a value of
// 0 or more.
const assert_not_negative_ = (entry, attributes) => {
  attributes.forEach(attribute => {
    assert_greater_than_equal(entry[attribute], 0,
      `${attribute} should be greater than or equal to 0`);
  });
}

// Asserts that the given attributes are present in 'entry' and hold a value
// greater than 0.
const assert_positive_ = (entry, attributes) => {
  attributes.forEach(attribute => {
    assert_greater_than(entry[attribute], 0,
      `${attribute} should be greater than 0`);
  });
}

const invariants = {
  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates for any resource fetched over HTTP without
  // redirects but passing the Timing-Allow-Origin checks.
  assert_tao_pass_no_redirect_http: entry => {
    assert_ordered_(entry, [
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);

    assert_zeroed_(entry, [
      "workerStart",
      "secureConnectionStart",
      "redirectStart",
      "redirectEnd",
    ]);

    assert_not_negative_(entry, [
      "duration",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "transferSize",
      "encodedBodySize",
      "decodedBodySize",
    ]);
  },

  // Like assert_tao_pass_no_redirect_http but for empty response bodies.
  assert_tao_pass_no_redirect_http_empty: entry => {
    assert_ordered_(entry, [
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);

    assert_zeroed_(entry, [
      "workerStart",
      "secureConnectionStart",
      "redirectStart",
      "redirectEnd",
      "encodedBodySize",
      "decodedBodySize",
    ]);

    assert_not_negative_(entry, [
      "duration",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "transferSize",
    ]);
  },

  // Like assert_tao_pass_no_redirect_http but for resources fetched over HTTPS
  assert_tao_pass_no_redirect_https: entry => {
    assert_ordered_(entry, [
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "secureConnectionStart",
      "connectStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);

    assert_zeroed_(entry, [
      "workerStart",
      "redirectStart",
      "redirectEnd",
    ]);

    assert_not_negative_(entry, [
      "duration",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "transferSize",
      "encodedBodySize",
      "decodedBodySize",
    ]);
  },

  // Like assert_tao_pass_no_redirect_https but for resources that did encounter
  // at least one HTTP redirect.
  assert_tao_pass_with_redirect_https: entry => {
    assert_ordered_(entry, [
      "fetchStart",
      "redirectStart",
      "redirectEnd",
      "domainLookupStart",
      "domainLookupEnd",
      "secureConnectionStart",
      "connectStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);

    assert_zeroed_(entry, [
      "workerStart",
    ]);

    assert_not_negative_(entry, [
      "duration",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "transferSize",
      "encodedBodySize",
      "decodedBodySize",
    ]);
  },

  // Like assert_tao_pass_no_redirect_http but, since the resource's bytes
  // won't be retransmitted, the encoded and decoded sizes must be zero.
  assert_tao_pass_304_not_modified_http: entry => {
    assert_ordered_(entry, [
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);

    assert_zeroed_(entry, [
      "workerStart",
      "secureConnectionStart",
      "redirectStart",
      "redirectEnd",
      "encodedBodySize",
      "decodedBodySize",
    ]);

    assert_not_negative_(entry, [
      "duration",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "transferSize",
    ]);
  },

  // Like assert_tao_pass_304_not_modified_http but for resources fetched over
  // HTTPS.
  assert_tao_pass_304_not_modified_https: entry => {
    assert_ordered_(entry, [
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "secureConnectionStart",
      "connectStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);

    assert_zeroed_(entry, [
      "workerStart",
      "redirectStart",
      "redirectEnd",
      "encodedBodySize",
      "decodedBodySize",
    ]);

    assert_not_negative_(entry, [
      "duration",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "transferSize",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates for any resource subsequently fetched over a
  // persistent connection. When this happens, we expect that certain
  // attributes describing transport layer behaviour will be equal.
  assert_connection_reused: entry => {
    assert_all_equal_(entry, [
      "fetchStart",
      "connectStart",
      "connectEnd",
      "domainLookupStart",
      "domainLookupEnd",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates for any resource fetched over HTTP through an HTTP
  // redirect.
  assert_same_origin_redirected_resource: entry => {
    assert_positive_(entry, [
      "redirectStart",
    ]);

    assert_equals(entry.redirectStart, entry.startTime,
      "redirectStart should be equal to startTime");

    assert_ordered_(entry, [
      "redirectStart",
      "redirectEnd",
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates for any resource fetched over HTTPS through a
  // cross-origin redirect.
  // (e.g. GET http://remote.com/foo => 302 Location: https://remote.com/foo)
  assert_cross_origin_redirected_resource: entry => {
    assert_zeroed_(entry, [
      "redirectStart",
      "redirectEnd",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "secureConnectionStart",
      "requestStart",
      "responseStart",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "responseEnd",
    ]);

    assert_ordered_(entry, [
      "fetchStart",
      "responseEnd",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates when
  // 1. An HTTP request is made for a same-origin resource.
  // 2. The response to 1 is an HTTP redirect (like a 302).
  // 3. The location from 2 is a cross-origin HTTPS URL.
  // 4. The response to fetching the URL from 3 does not set a matching TAO header.
  assert_http_to_cross_origin_redirected_resource: entry => {
    assert_zeroed_(entry, [
      "redirectStart",
      "redirectEnd",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "secureConnectionStart",
      "requestStart",
      "responseStart",
    ]);

    assert_positive_(entry, [
      "fetchStart",
      "responseEnd",
    ]);

    assert_ordered_(entry, [
      "fetchStart",
      "responseEnd",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates when
  // 1. An HTTPS request is made for a same-origin resource.
  // 2. The response to 1 is an HTTP redirect (like a 302).
  // 3. The location from 2 is a cross-origin HTTPS URL.
  // 4. The response to fetching the URL from 3 sets a matching TAO header.
  assert_tao_enabled_cross_origin_redirected_resource: entry => {
    assert_positive_(entry, [
      "redirectStart",
    ]);
    assert_ordered_(entry, [
      "redirectStart",
      "redirectEnd",
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "secureConnectionStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);
  },

  // Asserts that attributes of the given PerformanceResourceTiming entry match
  // what the spec dictates when
  // 1. An HTTP request is made for a same-origin resource
  // 2. The response to 1 is an HTTP redirect (like a 302).
  // 3. The location from 2 is a cross-origin HTTPS URL.
  // 4. The response to fetching the URL from 3 sets a matching TAO header.
  assert_http_to_tao_enabled_cross_origin_https_redirected_resource: entry => {
    assert_zeroed_(entry, [
      // Note that, according to the spec, the secureConnectionStart attribute
      // should describe the connection for the first resource request when
      // there are redirects. Since the initial request is over HTTP,
      // secureConnectionStart must be 0.
      "secureConnectionStart",
    ]);
    assert_positive_(entry, [
      "redirectStart",
    ]);
    assert_ordered_(entry, [
      "redirectStart",
      "redirectEnd",
      "fetchStart",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "requestStart",
      "responseStart",
      "responseEnd",
    ]);
  },

  assert_same_origin_redirected_from_cross_origin_resource: entry => {
    assert_zeroed_(entry, [
      "workerStart",
      "redirectStart",
      "redirectEnd",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "secureConnectionStart",
      "requestStart",
      "responseStart",
      "transferSize",
      "encodedBodySize",
      "decodedBodySize",
    ]);

    assert_ordered_(entry, [
      "fetchStart",
      "responseEnd",
    ]);

    assert_equals(entry.fetchStart, entry.startTime,
      "fetchStart must equal startTime");
  },

  assert_tao_failure_resource: entry => {
    assert_equals(entry.entryType, "resource", "entryType must always be 'resource'");

    assert_positive_(entry, [
      "startTime",
    ]);

    assert_not_negative_(entry, [
      "duration",
    ]);

    assert_zeroed_(entry, [
      "redirectStart",
      "redirectEnd",
      "domainLookupStart",
      "domainLookupEnd",
      "connectStart",
      "connectEnd",
      "secureConnectionStart",
      "requestStart",
      "responseStart",
      "transferSize",
      "encodedBodySize",
      "decodedBodySize",
    ]);
  }

};

const attribute_test_internal = (loader, path, validator, run_test, test_label) => {
  promise_test(
    async () => {
      let loaded_entry = new Promise((resolve, reject) => {
        new PerformanceObserver((entry_list, self) => {
          try {
            const name_matches = entry_list.getEntries().forEach(entry => {
              if (entry.name.includes(path)) {
                resolve(entry);
              }
            });
          } catch(e) {
            // By surfacing exceptions through the Promise interface, tests can
            // fail fast with a useful message instead of timing out.
            reject(e);
          }
        }).observe({"type": "resource"});
      });

      await loader(path, validator);
      const entry = await(loaded_entry);
      run_test(entry);
  }, test_label);
};

// Given a resource-loader, a path (a relative path or absolute URL), and a
// PerformanceResourceTiming test, applies the loader to the resource path
// and tests the resulting PerformanceResourceTiming entry.
const attribute_test = (loader, path, run_test, test_label) => {
  attribute_test_internal(loader, path, () => {}, run_test, test_label);
};

// Similar to attribute test, but on top of that, validates the added element,
// to ensure the test does what it intends to do.
const attribute_test_with_validator = (loader, path, validator, run_test, test_label) => {
  attribute_test_internal(loader, path, validator, run_test, test_label);
};

const network_error_entry_test = (originalURL, args, label) => {
  const url = new URL(originalURL, location.href);
  const search = new URLSearchParams(url.search.substr(1));
  const timeBefore = performance.now();
  loader = () => new Promise(resolve => fetch(url, args).catch(resolve));

  attribute_test(
    loader, url,
    () => {
      const timeAfter = performance.now();
      const names = performance.getEntriesByType('resource').filter(e => e.initiatorType === 'fetch').map(e => e.name);
      const entries = performance.getEntriesByName(url.toString());
      assert_equals(entries.length, 1, 'resource timing entry for network error');
      const entry = entries[0]
      assert_equals(entry.startTime, entry.fetchStart, 'startTime and fetchStart should be equal');
      assert_greater_than_equal(entry.startTime, timeBefore, 'startTime and fetchStart should be greater than the time before fetching');
      assert_greater_than_equal(timeAfter, entry.responseEnd, 'endTime should be less than the time right after returning from the fetch');
      invariants.assert_tao_failure_resource(entry);
  }, `A ResourceTiming entry should be created for network error of type ${label}`);
}
