// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

var redirectUrl = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "redirect.py";
var urlSetCookies1 = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "top.txt";
var urlSetCookies2 = get_host_info().HTTP_ORIGIN_WITH_DIFFERENT_PORT + dirname(location.pathname) + RESOURCES_DIR + "top.txt";
var urlCheckCookies = get_host_info().HTTP_ORIGIN_WITH_DIFFERENT_PORT + dirname(location.pathname) + RESOURCES_DIR + "inspect-headers.py?cors&headers=cookie";

var urlSetCookiesParameters = "?pipe=header(Access-Control-Allow-Origin," + location.origin + ")";
urlSetCookiesParameters += "|header(Access-Control-Allow-Credentials,true)";

urlSetCookiesParameters1 = urlSetCookiesParameters + "|header(Set-Cookie,a=1)";
urlSetCookiesParameters2 = urlSetCookiesParameters + "|header(Set-Cookie,a=2)";

urlClearCookiesParameters1 = urlSetCookiesParameters + "|header(Set-Cookie,a=1%3B%20max-age=0)";
urlClearCookiesParameters2 = urlSetCookiesParameters + "|header(Set-Cookie,a=2%3B%20max-age=0)";

promise_test(async (test) => {
    await fetch(urlSetCookies1 + urlSetCookiesParameters1, {"credentials": "include", "mode": "cors"});
    await fetch(urlSetCookies2 + urlSetCookiesParameters2, {"credentials": "include", "mode": "cors"});
}, "Set cookies");

function doTest(usePreflight) {
    promise_test(async (test) => {
        var url = redirectUrl;
        var uuid_token = token();
        var urlParameters = "?token=" + uuid_token + "&max_age=0";
        urlParameters += "&redirect_status=301";
        urlParameters += "&location=" + encodeURIComponent(urlCheckCookies);
        urlParameters += "&allow_headers=a&headers=Cookie";
        headers = [];
        if (usePreflight)
            headers.push(["a", "b"]);

        var requestInit = {"credentials": "include", "mode": "cors", "headers": headers};
        var response = await fetch(url + urlParameters, requestInit);

        assert_equals(response.headers.get("x-request-cookie") , "a=2", "Request includes cookie(s)");
    }, "Testing credentials after cross-origin redirection with CORS and " + (usePreflight ? "" : "no ") + "preflight");
}

doTest(false);
doTest(true);

promise_test(async (test) => {
    await fetch(urlSetCookies1 + urlClearCookiesParameters1, {"credentials": "include", "mode": "cors"});
    await fetch(urlSetCookies2 + urlClearCookiesParameters2, {"credentials": "include", "mode": "cors"});
}, "Clean cookies");
