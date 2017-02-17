'use strict';

const common = require('../common');
const URL = require('url').URL;
const { test, assert_equals } = common.WPT;

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/02585db/url/url-tojson.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
test(() => {
  const a = new URL("https://example.com/")
  assert_equals(JSON.stringify(a), "\"https://example.com/\"")
})
/* eslint-enable */
