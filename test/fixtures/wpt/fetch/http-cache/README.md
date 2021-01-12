## HTTP Caching Tests

These tests cover HTTP-specified behaviours for caches, primarily from
[RFC7234](http://httpwg.org/specs/rfc7234.html), but as seen through the
lens of Fetch.

A few notes:

* By its nature, caching is optional; some tests expecting a response to be
  cached might fail because the client chose not to cache it, or chose to
  race the cache with a network request.

* Likewise, some tests might fail because there is a separate document-level
  cache that's ill-defined; see [this
  issue](https://github.com/whatwg/fetch/issues/354).

* [Partial content tests](partial.html) (a.k.a. Range requests) are not specified
  in Fetch; tests are included here for interest only.

* Some browser caches will behave differently when reloading /
  shift-reloading, despite the `cache mode` staying the same.

* At the moment, Edge doesn't appear to using HTTP caching in conjunction
  with Fetch at all.


## Test Format

Each test run gets its own URL and randomized content and operates independently.

Each test is an an array of objects, with the following members:

- `name` - The name of the test.
- `requests` - a list of request objects (see below).

Possible members of a request object:

- template - A template object for the request, by name.
- request_method - A string containing the HTTP method to be used.
- request_headers - An array of `[header_name_string, header_value_string]` arrays to
                    emit in the request.
- request_body - A string to use as the request body.
- mode - The mode string to pass to `fetch()`.
- credentials - The credentials string to pass to `fetch()`.
- cache - The cache string to pass to `fetch()`.
- pause_after - Boolean controlling a 3-second pause after the request completes.
- response_status - A `[number, string]` array containing the HTTP status code
                    and phrase to return.
- response_headers - An array of `[header_name_string, header_value_string]` arrays to
                     emit in the response. These values will also be checked like
                     expected_response_headers, unless there is a third value that is
                     `false`. See below for special handling considerations.
- response_body - String to send as the response body. If not set, it will contain
                  the test identifier.
- expected_type - One of `["cached", "not_cached", "lm_validate", "etag_validate", "error"]`
- expected_status - A number representing a HTTP status code to check the response for.
                    If not set, the value of `response_status[0]` will be used; if that
                    is not set, 200 will be used.
- expected_request_headers - An array of `[header_name_string, header_value_string]` representing
                              headers to check the request for.
- expected_response_headers - An array of `[header_name_string, header_value_string]` representing
                              headers to check the response for. See also response_headers.
- expected_response_text - A string to check the response body against. If not present, `response_body` will be checked if present and non-null; otherwise the response body will be checked for the test uuid (unless the status code disallows a body). Set to `null` to disable all response body checking.

Some headers in `response_headers` are treated specially:

* For date-carrying headers, if the value is a number, it will be interpreted as a delta to the time of the first request at the server.
* For URL-carrying headers, the value will be appended as a query parameter for `target`.

See the source for exact details.

