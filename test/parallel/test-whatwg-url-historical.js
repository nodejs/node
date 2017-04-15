'use strict';
const common = require('../common');
const URL = require('url').URL;
const { test, assert_equals, assert_throws } = common.WPT;

if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl');
  return;
}

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/historical.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
// var objects = [
//   [function() { return window.location }, "location object"],
//   [function() { return document.createElement("a") }, "a element"],
//   [function() { return document.createElement("area") }, "area element"],
// ];

// objects.forEach(function(o) {
//   test(function() {
//     var object = o[0]();
//     assert_false("searchParams" in object,
//                  o[1] + " should not have a searchParams attribute");
//   }, "searchParams on " + o[1]);
// });

test(function() {
  var url = new URL("./foo", "http://www.example.org");
  assert_equals(url.href, "http://www.example.org/foo");
  assert_throws(new TypeError(), function() {
    url.href = "./bar";
  });
}, "Setting URL's href attribute and base URLs");

test(function() {
  assert_equals(URL.domainToASCII, undefined);
}, "URL.domainToASCII should be undefined");

test(function() {
  assert_equals(URL.domainToUnicode, undefined);
}, "URL.domainToUnicode should be undefined");
/* eslint-enable */
