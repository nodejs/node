// META: global=worker

const crossOrigin = "https://{{hosts[alt][]}}:{{ports[https][0]}}";
const redirectToCrossOrigin = "/common/redirect.py?location=" + crossOrigin;

test(function() {
  assert_throws_js(SyntaxError, function() {
    importScripts("/workers/modules/resources/syntax-error.js");
  });
}, "Same-origin syntax error");

test(function() {
  assert_throws_js(Error, function() {
    importScripts("/workers/modules/resources/throw.js");
  });
}, "Same-origin throw");

// https://html.spec.whatwg.org/C/#run-a-classic-script
// Step 8.2. If rethrow errors is true and script's muted errors is true, then:
// Step 8.2.2. Throw a "NetworkError" DOMException.
test(function() {
  assert_throws_dom("NetworkError", function() {
    importScripts(crossOrigin +
                  "/workers/modules/resources/syntax-error.js");
  });
}, "Cross-origin syntax error");

test(function() {
  assert_throws_dom("NetworkError", function() {
    importScripts(crossOrigin +
                  "/workers/modules/resources/throw.js");
  });
}, "Cross-origin throw");

test(function() {
  assert_throws_dom("NetworkError", function() {
    importScripts(redirectToCrossOrigin +
                  "/workers/modules/resources/syntax-error.js");
  });
}, "Redirect-to-cross-origin syntax error");

test(function() {
  assert_throws_dom("NetworkError", function() {
    importScripts(redirectToCrossOrigin +
                  "/workers/modules/resources/throw.js");
  });
}, "Redirect-to-Cross-origin throw");
