'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const URL = require('url').URL;
const { test, assert_equals } = common.WPT;
const additionalTestCases = require(
    path.join(common.fixturesDir, 'url-setter-tests-additional.js'));

if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl');
  return;
}

const request = {
  response: require(path.join(common.fixturesDir, 'url-setter-tests.json'))
};

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/url-setters.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
function startURLSettersTests() {
//   var setup = async_test("Loading data…")
//   setup.step(function() {
//     var request = new XMLHttpRequest()
//     request.open("GET", "setters_tests.json")
//     request.send()
//     request.responseType = "json"
//     request.onload = setup.step_func(function() {
         runURLSettersTests(request.response)
//       setup.done()
//     })
//   })
}

function runURLSettersTests(all_test_cases) {
  for (var attribute_to_be_set in all_test_cases) {
    if (attribute_to_be_set == "comment") {
      continue;
    }
    var test_cases = all_test_cases[attribute_to_be_set];
    for(var i = 0, l = test_cases.length; i < l; i++) {
      var test_case = test_cases[i];
      var name = "Setting <" + test_case.href + ">." + attribute_to_be_set +
                 " = '" + test_case.new_value + "'";
      if ("comment" in test_case) {
        name += " " + test_case.comment;
      }
      test(function() {
        var url = new URL(test_case.href);
        url[attribute_to_be_set] = test_case.new_value;
        for (var attribute in test_case.expected) {
          assert_equals(url[attribute], test_case.expected[attribute])
        }
      }, "URL: " + name)
      // test(function() {
      //   var url = document.createElement("a");
      //   url.href = test_case.href;
      //   url[attribute_to_be_set] = test_case.new_value;
      //   for (var attribute in test_case.expected) {
      //     assert_equals(url[attribute], test_case.expected[attribute])
      //   }
      // }, "<a>: " + name)
      // test(function() {
      //   var url = document.createElement("area");
      //   url.href = test_case.href;
      //   url[attribute_to_be_set] = test_case.new_value;
      //   for (var attribute in test_case.expected) {
      //     assert_equals(url[attribute], test_case.expected[attribute])
      //   }
      // }, "<area>: " + name)
    }
  }
}

startURLSettersTests()
/* eslint-enable */

// Tests below are not from WPT.

{
  for (const attributeToBeSet in additionalTestCases) {
    if (attributeToBeSet === 'comment') {
      continue;
    }
    const testCases = additionalTestCases[attributeToBeSet];
    for (const testCase of testCases) {
      let name = `Setting <${testCase.href}>.${attributeToBeSet}` +
                 ` = "${testCase.new_value}"`;
      if ('comment' in testCase) {
        name += ' ' + testCase.comment;
      }
      test(function() {
        const url = new URL(testCase.href);
        url[attributeToBeSet] = testCase.new_value;
        for (const attribute in testCase.expected) {
          assert_equals(url[attribute], testCase.expected[attribute]);
        }
      }, 'URL: ' + name);
    }
  }
}

{
  const url = new URL('http://example.com/');
  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  const props = Object.getOwnPropertyDescriptors(Object.getPrototypeOf(url));
  for (const [name, { set }] of Object.entries(props)) {
    if (set) {
      assert.throws(() => url[name] = obj,
                    /^Error: toString$/,
                    `url.${name} = { toString() { throw ... } }`);
      assert.throws(() => url[name] = sym,
                    /^TypeError: Cannot convert a Symbol value to a string$/,
                    `url.${name} = ${String(sym)}`);
    }
  }
}
