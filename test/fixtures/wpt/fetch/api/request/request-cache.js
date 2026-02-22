/**
 * Each test is run twice: once using etag/If-None-Match and once with
 * date/If-Modified-Since.  Each test run gets its own URL and randomized
 * content and operates independently.
 *
 * The test steps are run with request_cache.length fetch requests issued
 * and their immediate results sanity-checked.  The cache.py server script
 * stashes an entry containing any If-None-Match, If-Modified-Since, Pragma,
 * and Cache-Control observed headers for each request it receives.  When
 * the test fetches have run, this state is retrieved from cache.py and the
 * expected_* lists are checked, including their length.
 *
 * This means that if a request_* fetch is expected to hit the cache and not
 * touch the network, then there will be no entry for it in the expect_*
 * lists.  AKA (request_cache.length - expected_validation_headers.length)
 * should equal the number of cache hits that didn't touch the network.
 *
 * Test dictionary keys:
 * - state: required string that determines whether the Expires response for
 *   the fetched document should be set in the future ("fresh") or past
 *   ("stale").
 * - vary: optional string to be passed to the server for it to quote back
 *   in a Vary header on the response to us.
 * - cache_control: optional string to be passed to the server for it to
 *   quote back in a Cache-Control header on the response to us.
 * - redirect: optional string "same-origin" or "cross-origin".  If
 *   provided, the server will issue an absolute redirect to the script on
 *   the same or a different origin, as appropriate.  The redirected
 *   location is the script with the redirect parameter removed, so the
 *   content/state/etc. will be as if you hadn't specified a redirect.
 * - request_cache: required array of cache modes to use (via `cache`).
 * - request_headers: optional array of explicit fetch `headers` arguments.
 *   If provided, the server will log an empty dictionary for each request
 *   instead of the request headers it would normally log.
 * - response: optional array of specialized response handling.  Right now,
 *   "error" array entries indicate a network error response is expected
 *   which will reject with a TypeError.
 * - expected_validation_headers: required boolean array indicating whether
 *   the server should have seen an If-None-Match/If-Modified-Since header
 *   in the request.
 * - expected_no_cache_headers: required boolean array indicating whether
 *   the server should have seen Pragma/Cache-control:no-cache headers in
 *   the request.
 * - expected_max_age_headers: optional boolean array indicating whether
 *   the server should have seen a Cache-Control:max-age=0 header in the
 *   request.
 */

var now = new Date();

function base_path() {
  return location.pathname.replace(/\/[^\/]*$/, '/');
}
function make_url(uuid, id, value, content, info) {
  var dates = {
    fresh: new Date(now.getFullYear() + 1, now.getMonth(), now.getDay()).toGMTString(),
    stale: new Date(now.getFullYear() - 1, now.getMonth(), now.getDay()).toGMTString(),
  };
  var vary = "";
  if ("vary" in info) {
    vary = "&vary=" + info.vary;
  }
  var cache_control = "";
  if ("cache_control" in info) {
    cache_control = "&cache_control=" + info.cache_control;
  }
  var redirect = "";

  var ignore_request_headers = "";
  if ("request_headers" in info) {
    // Ignore the request headers that we send since they may be synthesized by the test.
    ignore_request_headers = "&ignore";
  }
  var url_sans_redirect = "resources/cache.py?token=" + uuid +
    "&content=" + content +
    "&" + id + "=" + value +
    "&expires=" + dates[info.state] +
    vary + cache_control + ignore_request_headers;
  // If there's a redirect, the target is the script without any redirect at
  // either the same domain or a different domain.
  if ("redirect" in info) {
    var host_info = get_host_info();
    var origin;
    switch (info.redirect) {
      case "same-origin":
        origin = host_info['HTTP_ORIGIN'];
        break;
      case "cross-origin":
        origin = host_info['HTTP_REMOTE_ORIGIN'];
        break;
    }
    var redirected_url = origin + base_path() + url_sans_redirect;
    return url_sans_redirect + "&redirect=" + encodeURIComponent(redirected_url);
  } else {
    return url_sans_redirect;
  }
}
function expected_status(type, identifier, init) {
  if (type == "date" &&
      init.headers &&
      init.headers["If-Modified-Since"] == identifier) {
    // The server will respond with a 304 in this case.
    return [304, "Not Modified"];
  }
  return [200, "OK"];
}
function expected_response_text(type, identifier, init, content) {
  if (type == "date" &&
      init.headers &&
      init.headers["If-Modified-Since"] == identifier) {
    // The server will respond with a 304 in this case.
    return "";
  }
  return content;
}
function server_state(uuid) {
  return fetch("resources/cache.py?querystate&token=" + uuid)
    .then(function(response) {
      return response.text();
    }).then(function(text) {
      // null will be returned if the server never received any requests
      // for the given uuid.  Normalize that to an empty list consistent
      // with our representation.
      return JSON.parse(text) || [];
    });
}
function make_test(type, info) {
  return function(test) {
    var uuid = token();
    var identifier = (type == "tag" ? Math.random() : now.toGMTString());
    var content = Math.random().toString();
    var url = make_url(uuid, type, identifier, content, info);
    var fetch_functions = [];
    for (var i = 0; i < info.request_cache.length; ++i) {
      fetch_functions.push(function(idx) {
        var init = {cache: info.request_cache[idx]};
        if ("request_headers" in info) {
          init.headers = info.request_headers[idx];
        }
        if (init.cache === "only-if-cached") {
          // only-if-cached requires we use same-origin mode.
          init.mode = "same-origin";
        }
        return fetch(url, init)
          .then(function(response) {
            if ("response" in info && info.response[idx] === "error") {
              assert_true(false, "fetch should have been an error");
              return;
            }
            assert_array_equals([response.status, response.statusText],
                                expected_status(type, identifier, init));
            return response.text();
          }).then(function(text) {
            assert_equals(text, expected_response_text(type, identifier, init, content));
          }, function(reason) {
            if ("response" in info && info.response[idx] === "error") {
              assert_throws_js(TypeError, function() { throw reason; });
            } else {
              throw reason;
            }
          });
      });
    }
    var i = 0;
    function run_next_step() {
      if (fetch_functions.length) {
        return fetch_functions.shift()(i++)
          .then(run_next_step);
      } else {
        return Promise.resolve();
      }
    }
    return run_next_step()
      .then(function() {
        // Now, query the server state
        return server_state(uuid);
      }).then(function(state) {
        var expectedState = [];
        info.expected_validation_headers.forEach(function (validate) {
          if (validate) {
            if (type == "tag") {
              expectedState.push({"If-None-Match": '"' + identifier + '"'});
            } else {
              expectedState.push({"If-Modified-Since": identifier});
            }
          } else {
            expectedState.push({});
          }
        });
        for (var i = 0; i < info.expected_no_cache_headers.length; ++i) {
          if (info.expected_no_cache_headers[i]) {
            expectedState[i]["Pragma"] = "no-cache";
            expectedState[i]["Cache-Control"] = "no-cache";
          }
        }
        if ("expected_max_age_headers" in info) {
          for (var i = 0; i < info.expected_max_age_headers.length; ++i) {
            if (info.expected_max_age_headers[i]) {
              expectedState[i]["Cache-Control"] = "max-age=0";
            }
          }
        }
        assert_equals(state.length, expectedState.length);
        for (var i = 0; i < state.length; ++i) {
          for (var header in state[i]) {
            assert_equals(state[i][header], expectedState[i][header]);
            delete expectedState[i][header];
          }
          for (var header in expectedState[i]) {
            assert_false(header in state[i]);
          }
        }
      });
  };
}

function run_tests(tests)
{
  tests.forEach(function(info) {
    promise_test(make_test("tag", info), info.name + " with Etag and " + info.state + " response");
    promise_test(make_test("date", info), info.name + " with Last-Modified and " + info.state + " response");
  });
}
