// META: global=window,worker
// META: script=../resources/utils.js

function corsMultipleOrigins(originList) {
  var urlParameters = "?origin=" + encodeURIComponent(originList.join(", "));
  var url = "http://{{host}}:{{ports[http][1]}}" + dirname(location.pathname) + RESOURCES_DIR + "preflight.py";

  promise_test(function(test) {
    return promise_rejects_js(test, TypeError, fetch(url + urlParameters));
  }, "Listing multiple origins is illegal: " + originList);
}
/* Actual origin */
var origin = "http://{{host}}:{{ports[http][0]}}";

corsMultipleOrigins(["\"\"", "http://example.com", origin]);
corsMultipleOrigins(["\"\"", "http://example.com", "*"]);
corsMultipleOrigins(["\"\"", origin, origin]);
corsMultipleOrigins(["*", "http://example.com", "*"]);
corsMultipleOrigins(["*", "http://example.com", origin]);
corsMultipleOrigins(["", "http://example.com", "https://example2.com"]);

done();
