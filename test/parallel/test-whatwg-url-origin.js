'use strict';
const common = require('../common');
const path = require('path');
const URL = require('url').URL;
const { test, assert_equals } = common.WPT;

if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl');
  return;
}

const request = {
  response: require(path.join(common.fixturesDir, 'url-tests.json'))
};

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/url-origin.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
function runURLOriginTests() {
  // var setup = async_test("Loading data…")
  // setup.step(function() {
  //   var request = new XMLHttpRequest()
  //   request.open("GET", "urltestdata.json")
  //   request.send()
  //   request.responseType = "json"
  //   request.onload = setup.step_func(function() {
         runURLTests(request.response)
  //     setup.done()
  //   })
  // })
}

function bURL(url, base) {
  return new URL(url, base || "about:blank")
}

function runURLTests(urltests) {
  for(var i = 0, l = urltests.length; i < l; i++) {
    var expected = urltests[i]
    if (typeof expected === "string" || !("origin" in expected)) continue
    test(function() {
      var url = bURL(expected.input, expected.base)
      assert_equals(url.origin, expected.origin, "origin")
    }, "Origin parsing: <" + expected.input + "> against <" + expected.base + ">")
  }
}

runURLOriginTests()
/* eslint-enable */
