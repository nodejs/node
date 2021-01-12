// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function runTest(url, init, expectedReferrer, title) {
    promise_test(function(test) {
        url += (url.indexOf('?') !== -1 ? '&' : '?') + "headers=referer&cors";

        return fetch(url , init).then(function(resp) {
            assert_equals(resp.status, 200, "HTTP status is 200");
            assert_equals(resp.headers.get("x-request-referer"), expectedReferrer, "Request's referrer is correct");
        });
    }, title);
}

var fetchedUrl = RESOURCES_DIR + "inspect-headers.py";
var corsFetchedUrl = get_host_info().HTTP_REMOTE_ORIGIN  + dirname(location.pathname) + RESOURCES_DIR + "inspect-headers.py";
var redirectUrl = RESOURCES_DIR + "redirect.py?location=" ;
var corsRedirectUrl = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "redirect.py?location=";

runTest(fetchedUrl, { referrerPolicy: "origin-when-cross-origin"}, location.toString(), "origin-when-cross-origin policy on a same-origin URL");
runTest(corsFetchedUrl, { referrerPolicy: "origin-when-cross-origin"}, get_host_info().HTTP_ORIGIN + "/", "origin-when-cross-origin policy on a cross-origin URL");
runTest(redirectUrl + corsFetchedUrl, { referrerPolicy: "origin-when-cross-origin"}, get_host_info().HTTP_ORIGIN + "/", "origin-when-cross-origin policy on a cross-origin URL after same-origin redirection");
runTest(corsRedirectUrl + fetchedUrl, { referrerPolicy: "origin-when-cross-origin"}, get_host_info().HTTP_ORIGIN + "/", "origin-when-cross-origin policy on a same-origin URL after cross-origin redirection");


var referrerUrlWithCredentials = get_host_info().HTTP_ORIGIN.replace("http://", "http://username:password@");
runTest(fetchedUrl, {referrer: referrerUrlWithCredentials}, get_host_info().HTTP_ORIGIN + "/", "Referrer with credentials should be stripped");
var referrerUrlWithFragmentIdentifier = get_host_info().HTTP_ORIGIN + "#fragmentIdentifier";
runTest(fetchedUrl, {referrer: referrerUrlWithFragmentIdentifier}, get_host_info().HTTP_ORIGIN + "/", "Referrer with fragment ID should be stripped");
