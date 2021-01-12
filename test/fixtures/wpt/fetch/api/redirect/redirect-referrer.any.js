// META: timeout=long
// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function testReferrerAfterRedirection(desc, redirectUrl, redirectLocation, referrerPolicy, redirectReferrerPolicy, expectedReferrer) {
  var url = redirectUrl;
  var urlParameters = "?location=" + encodeURIComponent(redirectLocation);

  if (redirectReferrerPolicy)
    urlParameters += "&redirect_referrerpolicy=" + redirectReferrerPolicy;

  var requestInit = {"redirect": "follow", "referrerPolicy": referrerPolicy};

    promise_test(function(test) {
      return fetch(url + urlParameters, requestInit).then(function(response) {
        assert_equals(response.status, 200, "Inspect header response's status is 200");
        assert_equals(response.headers.get("x-request-referer"), expectedReferrer ? expectedReferrer : null, "Check referrer header");
      });
    }, desc);
}

var referrerOrigin = get_host_info().HTTP_ORIGIN + "/";
var referrerUrl = location.href;

var redirectUrl = RESOURCES_DIR + "redirect.py";
var locationUrl = get_host_info().HTTP_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "inspect-headers.py?headers=referer";
var crossLocationUrl =  get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "inspect-headers.py?cors&headers=referer";

testReferrerAfterRedirection("Same origin redirection, empty init, unsafe-url redirect header ", redirectUrl, locationUrl, "", "unsafe-url", referrerUrl);
testReferrerAfterRedirection("Same origin redirection, empty init, no-referrer-when-downgrade redirect header ", redirectUrl, locationUrl, "", "no-referrer-when-downgrade", referrerUrl);
testReferrerAfterRedirection("Same origin redirection, empty init, same-origin redirect header ", redirectUrl, locationUrl, "", "same-origin", referrerUrl);
testReferrerAfterRedirection("Same origin redirection, empty init, origin redirect header ", redirectUrl, locationUrl, "", "origin", referrerOrigin);
testReferrerAfterRedirection("Same origin redirection, empty init, origin-when-cross-origin redirect header ", redirectUrl, locationUrl, "", "origin-when-cross-origin", referrerUrl);
testReferrerAfterRedirection("Same origin redirection, empty init, no-referrer redirect header ", redirectUrl, locationUrl, "", "no-referrer", null);
testReferrerAfterRedirection("Same origin redirection, empty init, strict-origin redirect header ", redirectUrl, locationUrl, "", "strict-origin", referrerOrigin);
testReferrerAfterRedirection("Same origin redirection, empty init, strict-origin-when-cross-origin redirect header ", redirectUrl, locationUrl, "", "strict-origin-when-cross-origin", referrerUrl);

testReferrerAfterRedirection("Same origin redirection, empty redirect header, unsafe-url init ", redirectUrl, locationUrl, "unsafe-url", "", referrerUrl);
testReferrerAfterRedirection("Same origin redirection, empty redirect header, no-referrer-when-downgrade init ", redirectUrl, locationUrl, "no-referrer-when-downgrade", "", referrerUrl);
testReferrerAfterRedirection("Same origin redirection, empty redirect header, same-origin init ", redirectUrl, locationUrl, "same-origin", "", referrerUrl);
testReferrerAfterRedirection("Same origin redirection, empty redirect header, origin init ", redirectUrl, locationUrl, "origin", "", referrerOrigin);
testReferrerAfterRedirection("Same origin redirection, empty redirect header, origin-when-cross-origin init ", redirectUrl, locationUrl, "origin-when-cross-origin", "", referrerUrl);
testReferrerAfterRedirection("Same origin redirection, empty redirect header, no-referrer init ", redirectUrl, locationUrl, "no-referrer", "", null);
testReferrerAfterRedirection("Same origin redirection, empty redirect header, strict-origin init ", redirectUrl, locationUrl, "strict-origin", "", referrerOrigin);
testReferrerAfterRedirection("Same origin redirection, empty redirect header, strict-origin-when-cross-origin init ", redirectUrl, locationUrl, "strict-origin-when-cross-origin", "", referrerUrl);

testReferrerAfterRedirection("Cross origin redirection, empty init, unsafe-url redirect header ", redirectUrl, crossLocationUrl, "", "unsafe-url", referrerUrl);
testReferrerAfterRedirection("Cross origin redirection, empty init, no-referrer-when-downgrade redirect header ", redirectUrl, crossLocationUrl, "", "no-referrer-when-downgrade", referrerUrl);
testReferrerAfterRedirection("Cross origin redirection, empty init, same-origin redirect header ", redirectUrl, crossLocationUrl, "", "same-origin", null);
testReferrerAfterRedirection("Cross origin redirection, empty init, origin redirect header ", redirectUrl, crossLocationUrl, "", "origin", referrerOrigin);
testReferrerAfterRedirection("Cross origin redirection, empty init, origin-when-cross-origin redirect header ", redirectUrl, crossLocationUrl, "", "origin-when-cross-origin", referrerOrigin);
testReferrerAfterRedirection("Cross origin redirection, empty init, no-referrer redirect header ", redirectUrl, crossLocationUrl, "", "no-referrer", null);
testReferrerAfterRedirection("Cross origin redirection, empty init, strict-origin redirect header ", redirectUrl, crossLocationUrl, "", "strict-origin", referrerOrigin);
testReferrerAfterRedirection("Cross origin redirection, empty init, strict-origin-when-cross-origin redirect header ", redirectUrl, crossLocationUrl, "", "strict-origin-when-cross-origin", referrerOrigin);

testReferrerAfterRedirection("Cross origin redirection, empty redirect header, unsafe-url init ", redirectUrl, crossLocationUrl, "unsafe-url", "", referrerUrl);
testReferrerAfterRedirection("Cross origin redirection, empty redirect header, no-referrer-when-downgrade init ", redirectUrl, crossLocationUrl, "no-referrer-when-downgrade", "", referrerUrl);
testReferrerAfterRedirection("Cross origin redirection, empty redirect header, same-origin init ", redirectUrl, crossLocationUrl, "same-origin", "", null);
testReferrerAfterRedirection("Cross origin redirection, empty redirect header, origin init ", redirectUrl, crossLocationUrl, "origin", "", referrerOrigin);
testReferrerAfterRedirection("Cross origin redirection, empty redirect header, origin-when-cross-origin init ", redirectUrl, crossLocationUrl, "origin-when-cross-origin", "", referrerOrigin);
testReferrerAfterRedirection("Cross origin redirection, empty redirect header, no-referrer init ", redirectUrl, crossLocationUrl, "no-referrer", "", null);
testReferrerAfterRedirection("Cross origin redirection, empty redirect header, strict-origin init ", redirectUrl, crossLocationUrl, "strict-origin", "", referrerOrigin);
testReferrerAfterRedirection("Cross origin redirection, empty redirect header, strict-origin-when-cross-origin init ", redirectUrl, crossLocationUrl, "strict-origin-when-cross-origin", "", referrerOrigin);

done();
