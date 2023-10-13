/*
In the first line of the test file you should
enable strict mode, unless you test something
that needs it disabled
*/
'use strict';

/*
the common package gives you some commonly
used testing methods, like mustCall
*/
const common = require('../common');

/*
a small description on what you are testing
*/
// This test ensures that the http-parser can handle UTF-8 characters
// in the http header.

const assert = require('assert');
const http = require('http');

/*
the body of the actual test - tests should exit with code 0 on success
*/
const server = http.createServer(
  common.mustCall((request, response) => {
    response.write('hello world');
    response.end();
  })
);

server.listen(3000, () => {});
