// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js
// META: script=resources/corspreflight.js

var corsUrl = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "preflight.py";

corsPreflight("CORS [DELETE], server allows", corsUrl, "DELETE", true);
corsPreflight("CORS [DELETE], server refuses", corsUrl, "DELETE", false);
corsPreflight("CORS [PUT], server allows", corsUrl, "PUT", true);
corsPreflight("CORS [PUT], server allows, check preflight has user agent", corsUrl + "?checkUserAgentHeaderInPreflight", "PUT", true);
corsPreflight("CORS [PUT], server refuses", corsUrl, "PUT", false);
corsPreflight("CORS [PATCH], server allows", corsUrl, "PATCH", true);
corsPreflight("CORS [PATCH], server refuses", corsUrl, "PATCH", false);
corsPreflight("CORS [patcH], server allows", corsUrl, "patcH", true);
corsPreflight("CORS [patcH], server refuses", corsUrl, "patcH", false);
corsPreflight("CORS [NEW], server allows", corsUrl, "NEW", true);
corsPreflight("CORS [NEW], server refuses", corsUrl, "NEW", false);
corsPreflight("CORS [chicken], server allows", corsUrl, "chicken", true);
corsPreflight("CORS [chicken], server refuses", corsUrl, "chicken", false);

corsPreflight("CORS [GET] [x-test-header: allowed], server allows", corsUrl, "GET", true, [["x-test-header1", "allowed"]]);
corsPreflight("CORS [GET] [x-test-header: refused], server refuses", corsUrl, "GET", false, [["x-test-header1", "refused"]]);

var headers = [
    ["x-test-header1", "allowedOrRefused"],
    ["x-test-header2", "allowedOrRefused"],
    ["X-test-header3", "allowedOrRefused"],
    ["x-test-header-b", "allowedOrRefused"],
    ["x-test-header-D", "allowedOrRefused"],
    ["x-test-header-C", "allowedOrRefused"],
    ["x-test-header-a", "allowedOrRefused"],
    ["Content-Type", "allowedOrRefused"],
];
var safeHeaders= [
    ["Accept", "*"],
    ["Accept-Language", "bzh"],
    ["Content-Language", "eu"],
];

corsPreflight("CORS [GET] [several headers], server allows", corsUrl, "GET", true, headers, safeHeaders);
corsPreflight("CORS [GET] [several headers], server refuses", corsUrl, "GET", false, headers, safeHeaders);
corsPreflight("CORS [PUT] [several headers], server allows", corsUrl, "PUT", true, headers, safeHeaders);
corsPreflight("CORS [PUT] [several headers], server refuses", corsUrl, "PUT", false, headers, safeHeaders);

corsPreflight("CORS [PUT] [only safe headers], server allows", corsUrl, "PUT", true, null, safeHeaders);

promise_test(async t => {
  const url = `${corsUrl}?allow_headers=*`;
  await promise_rejects_js(t, TypeError, fetch(url, {
    headers: {
      authorization: 'foobar'
    }
  }));
}, '"authorization" should not be covered by the wildcard symbol');

promise_test(async t => {
  const url = `${corsUrl}?allow_headers=authorization`;
  await fetch(url, { headers: {
    authorization: 'foobar'
  }});
}, '"authorization" should be covered by "authorization"');