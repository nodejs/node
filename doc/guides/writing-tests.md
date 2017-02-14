# How to write a test for the Node.js project

## What is a test?

Most tests in Node.js core are JavaScript programs that exercise a functionality
provided by Node.js and check that it behaves as expected. Tests should exit
with code `0` on success. A test will fail if:

- It exits by setting `process.exitCode` to a non-zero number.
  - This is usually done by having an assertion throw an uncaught Error.
  - Occasionally, using `process.exit(code)` may be appropriate.
- It never exits. In this case, the test runner will terminate the test because
  it sets a maximum time limit.

Add tests when:

- Adding new functionality.
- Fixing regressions and bugs.
- Expanding test coverage.

## Test structure

Let's analyze this basic test from the Node.js test suite:

```javascript
1   'use strict';
2   const common = require('../common');
3
4   // This test ensures that the http-parser can handle UTF-8 characters
5   // in the http header.
6
7   const assert = require('assert');
8   const http = require('http');
9
10  const server = http.createServer(common.mustCall((req, res) => {
11    res.end('ok');
12  }));
13  server.listen(0, () => {
14   http.get({
15     port: server.address().port,
16     headers: {'Test': 'Düsseldorf'}
17   }, common.mustCall((res) => {
18     assert.strictEqual(res.statusCode, 200);
19     server.close();
20   }));
21 });
```

### **Lines 1-2**

```javascript
'use strict';
const common = require('../common');
```

The first line enables strict mode. All tests should be in strict mode unless
the nature of the test requires that the test run without it.

The second line loads the `common` module. The `common` module is a helper
module that provides useful tools for the tests.

Even if a test uses no functions or other properties exported by `common`,
the test should still include the `common` module before any other modules. This
is because the `common` module includes code that will cause a test to fail if
the test leaks variables into the global space. In situations where a test uses
no functions or other properties exported by `common`, include it without
assigning it to an identifier:

```javascript
require('../common');
```

### **Lines 4-5**

```javascript
// This test ensures that the http-parser can handle UTF-8 characters
// in the http header.
```

A test should start with a comment containing a brief description of what it is
designed to test.

### **Lines 7-8**

```javascript
const assert = require('assert');
const http = require('http');
```

The test checks functionality in the `http` module.

Most tests use the `assert` module to confirm expectations of the test.

The require statements are sorted in
[ASCII](http://man7.org/linux/man-pages/man7/ascii.7.html) order (digits, upper
case, `_`, lower case).

### **Lines 10-21**

This is the body of the test. This test is simple, it just tests that an
HTTP server accepts `non-ASCII` characters in the headers of an incoming
request. Interesting things to notice:

- If the test doesn't depend on a specific port number, then always use 0
  instead of an arbitrary value, as it allows tests to run in parallel safely,
  as the operating system will assign a random port. If the test requires a
  specific port, for example if the test checks that assigning a specific port
  works as expected, then it is ok to assign a specific port number.
- The use of `common.mustCall` to check that some callbacks/listeners are
  called.
- The HTTP server closes once all the checks have run. This way, the test can
  exit gracefully. Remember that for a test to succeed, it must exit with a
  status code of 0.

## General recommendations

### Timers

Avoid timers unless the test is specifically testing timers. There are multiple
reasons for this. Mainly, they are a source of flakiness. For a thorough
explanation go [here](https://github.com/nodejs/testing/issues/27).

In the event a test needs a timer, consider using the
`common.platformTimeout()` method. It allows setting specific timeouts
depending on the platform. For example:

```javascript
const timer = setTimeout(fail, common.platformTimeout(4000));
```

will create a 4-second timeout on most platforms but a longer timeout on slower
platforms.

### The *common* API

Make use of the helpers from the `common` module as much as possible.

One interesting case is `common.mustCall`. The use of `common.mustCall` may
avoid the use of extra variables and the corresponding assertions. Let's explain
this with a real test from the test suite.

```javascript
'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var request = 0;
var response = 0;
process.on('exit', function() {
  assert.equal(request, 1, 'http server "request" callback was not called');
  assert.equal(response, 1, 'http request "response" callback was not called');
});

var server = http.createServer(function(req, res) {
  request++;
  res.end();
}).listen(0, function() {
  var options = {
    agent: null,
    port: this.address().port
  };
  http.get(options, function(res) {
    response++;
    res.resume();
    server.close();
  });
});
```

This test could be greatly simplified by using `common.mustCall` like this:

```javascript
'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(common.mustCall(function(req, res) {
  res.end();
})).listen(0, function() {
  var options = {
    agent: null,
    port: this.address().port
  };
  http.get(options, common.mustCall(function(res) {
    res.resume();
    server.close();
  }));
});

```

### Flags

Some tests will require running Node.js with specific command line flags set. To
accomplish this, add a `// Flags: ` comment in the preamble of the
test followed by the flags. For example, to allow a test to require some of the
`internal/*` modules, add the `--expose-internals` flag.
A test that would require `internal/freelist` could start like this:

```javascript
'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const freelist = require('internal/freelist');
```

### Assertions

When writing assertions, prefer the strict versions:

* `assert.strictEqual()` over `assert.equal()`
* `assert.deepStrictEqual()` over `assert.deepEqual()`

When using `assert.throws()`, if possible, provide the full error message:

```js
assert.throws(
  () => {
    throw new Error('Wrong value');
  },
  /^Error: Wrong value$/ // Instead of something like /Wrong value/
);
```

### ES.Next features

For performance considerations, we only use a selected subset of ES.Next
features in JavaScript code in the `lib` directory. However, when writing
tests, it is encouraged to use ES.Next features that have already landed
in the ECMAScript specification. For example:

* `let` and `const` over `var`
* Template literals over string concatenation
* Arrow functions when appropriate

## Naming Test Files

Test files are named using kebab casing. The first component of the name is
`test`. The second is the module or subsystem being tested. The third is usually
the method or event name being tested. Subsequent components of the name add
more information about what is being tested.

For example, a test for the `beforeExit` event on the `process` object might be
named `test-process-before-exit.js`. If the test specifically checked that arrow
functions worked correctly with the `beforeExit` event, then it might be named
`test-process-before-exit-arrow-functions.js`.

## Imported Tests

### Web Platform Tests

Some of the tests for the WHATWG URL implementation (named
`test-whatwg-url-*.js`) are imported from the
[Web Platform Tests Project](https://github.com/w3c/web-platform-tests/tree/master/url).
These imported tests will be wrapped like this:

```js
/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/urlsearchparams-stringifier.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/

// Test code

/* eslint-enable */
```

If you want to improve tests that have been imported this way, please send
a PR to the upstream project first. When your proposed change is merged in
the upstream project, send another PR here to update Node.js accordingly.
Be sure to update the hash in the URL following `WPT Refs:`.
