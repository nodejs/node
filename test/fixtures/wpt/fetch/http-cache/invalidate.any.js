// META: global=window,worker
// META: title=HTTP Cache - Invalidation
// META: timeout=long
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=http-cache.js

var tests = [
  {
    name: 'HTTP cache invalidates after a successful response from a POST',
    requests: [
      {
        template: "fresh"
      }, {
        request_method: "POST",
        request_body: "abc"
      }, {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache does not invalidate after a failed response from an unsafe request',
    requests: [
      {
        template: "fresh"
      }, {
        request_method: "POST",
        request_body: "abc",
        response_status: [500, "Internal Server Error"]
      }, {
        expected_type: "cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates after a successful response from a PUT',
    requests: [
      {
        template: "fresh"
      }, {
        template: "fresh",
        request_method: "PUT",
        request_body: "abc"
      }, {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates after a successful response from a DELETE',
    requests: [
      {
        template: "fresh"
      }, {
        request_method: "DELETE",
        request_body: "abc"
      }, {
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates after a successful response from an unknown method',
    requests: [
      {
        template: "fresh"
      }, {
        request_method: "FOO",
        request_body: "abc"
      }, {
        expected_type: "not_cached"
      }
    ]
  },


  {
    name: 'HTTP cache invalidates Location URL after a successful response from a POST',
    requests: [
      {
        template: "location"
      }, {
        request_method: "POST",
        request_body: "abc",
        template: "lcl_response"
      }, {
        template: "location",
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache does not invalidate Location URL after a failed response from an unsafe request',
    requests: [
      {
        template: "location"
      }, {
        template: "lcl_response",
        request_method: "POST",
        request_body: "abc",
        response_status: [500, "Internal Server Error"]
      }, {
        template: "location",
        expected_type: "cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates Location URL after a successful response from a PUT',
    requests: [
      {
        template: "location"
      }, {
        template: "lcl_response",
        request_method: "PUT",
        request_body: "abc"
      }, {
        template: "location",
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates Location URL after a successful response from a DELETE',
    requests: [
      {
        template: "location"
      }, {
        template: "lcl_response",
        request_method: "DELETE",
        request_body: "abc"
      }, {
        template: "location",
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates Location URL after a successful response from an unknown method',
    requests: [
      {
        template: "location"
      }, {
        template: "lcl_response",
        request_method: "FOO",
        request_body: "abc"
      }, {
        template: "location",
        expected_type: "not_cached"
      }
    ]
  },



  {
    name: 'HTTP cache invalidates Content-Location URL after a successful response from a POST',
    requests: [
      {
        template: "content_location"
      }, {
        request_method: "POST",
        request_body: "abc",
        template: "lcl_response"
      }, {
        template: "content_location",
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache does not invalidate Content-Location URL after a failed response from an unsafe request',
    requests: [
      {
        template: "content_location"
      }, {
        template: "lcl_response",
        request_method: "POST",
        request_body: "abc",
        response_status: [500, "Internal Server Error"]
      }, {
        template: "content_location",
        expected_type: "cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates Content-Location URL after a successful response from a PUT',
    requests: [
      {
        template: "content_location"
      }, {
        template: "lcl_response",
        request_method: "PUT",
        request_body: "abc"
      }, {
        template: "content_location",
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates Content-Location URL after a successful response from a DELETE',
    requests: [
      {
        template: "content_location"
      }, {
        template: "lcl_response",
        request_method: "DELETE",
        request_body: "abc"
      }, {
        template: "content_location",
        expected_type: "not_cached"
      }
    ]
  },
  {
    name: 'HTTP cache invalidates Content-Location URL after a successful response from an unknown method',
    requests: [
      {
        template: "content_location"
      }, {
        template: "lcl_response",
        request_method: "FOO",
        request_body: "abc"
      }, {
        template: "content_location",
        expected_type: "not_cached"
      }
    ]
  }

];
run_tests(tests);
