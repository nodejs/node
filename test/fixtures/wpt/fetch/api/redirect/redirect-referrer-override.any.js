// META: timeout=long
// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function getExpectation(expectations, initPolicy, initScenario, redirectPolicy, redirectScenario) {
  let policies = [
    expectations[initPolicy][initScenario],
    expectations[redirectPolicy][redirectScenario]
  ];

  if (policies.includes("omitted")) {
    return null;
  } else if (policies.includes("origin")) {
    return referrerOrigin;
  } else {
    // "stripped-referrer"
    return referrerUrl;
  }
}

function testReferrerAfterRedirection(desc, redirectUrl, redirectLocation, referrerPolicy, redirectReferrerPolicy, expectedReferrer) {
  var url = redirectUrl;
  var urlParameters = "?location=" + encodeURIComponent(redirectLocation);
  var description = desc + ", " + referrerPolicy + " init, " + redirectReferrerPolicy + " redirect header ";

  if (redirectReferrerPolicy)
    urlParameters += "&redirect_referrerpolicy=" + redirectReferrerPolicy;

  var requestInit = {"redirect": "follow", "referrerPolicy": referrerPolicy};
    promise_test(function(test) {
      return fetch(url + urlParameters, requestInit).then(function(response) {
        assert_equals(response.status, 200, "Inspect header response's status is 200");
        assert_equals(response.headers.get("x-request-referer"), expectedReferrer ? expectedReferrer : null, "Check referrer header");
      });
    }, description);
}

var referrerOrigin = get_host_info().HTTP_ORIGIN + "/";
var referrerUrl = location.href;

var redirectUrl = RESOURCES_DIR + "redirect.py";
var locationUrl = get_host_info().HTTP_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "inspect-headers.py?headers=referer";
var crossLocationUrl = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "inspect-headers.py?cors&headers=referer";

var expectations = {
  "no-referrer": {
    "same-origin": "omitted",
    "cross-origin": "omitted"
  },
  "no-referrer-when-downgrade": {
    "same-origin": "stripped-referrer",
    "cross-origin": "stripped-referrer"
  },
  "origin": {
    "same-origin": "origin",
    "cross-origin": "origin"
  },
  "origin-when-cross-origin": {
    "same-origin": "stripped-referrer",
    "cross-origin": "origin",
  },
  "same-origin": {
    "same-origin": "stripped-referrer",
    "cross-origin": "omitted"
  },
  "strict-origin": {
    "same-origin": "origin",
    "cross-origin": "origin"
  },
  "strict-origin-when-cross-origin": {
    "same-origin": "stripped-referrer",
    "cross-origin": "origin"
  },
  "unsafe-url": {
    "same-origin": "stripped-referrer",
    "cross-origin": "stripped-referrer"
  }
};

for (var initPolicy in expectations) {
  for (var redirectPolicy in expectations) {

    // Redirect to same-origin URL
    testReferrerAfterRedirection(
      "Same origin redirection",
      redirectUrl,
      locationUrl,
      initPolicy,
      redirectPolicy,
      getExpectation(expectations, initPolicy, "same-origin", redirectPolicy, "same-origin"));

    // Redirect to cross-origin URL
    testReferrerAfterRedirection(
      "Cross origin redirection",
      redirectUrl,
      crossLocationUrl,
      initPolicy,
      redirectPolicy,
      getExpectation(expectations, initPolicy, "same-origin", redirectPolicy, "cross-origin"));
  }
}

done();
