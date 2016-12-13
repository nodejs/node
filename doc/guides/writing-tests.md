# How to write a test for the Node.js project

## What is a test?

A test must be a node script that exercises a specific functionality provided
by node and checks that it behaves as expected. It should exit with code `0` on success,
otherwise it will fail. A test will fail if:

- It exits by setting `process.exitCode` to a non-zero number.
  - This is most often done by having an assertion throw an uncaught
    Error.
  - Occasionally, using `process.exit(code)` may be appropriate.
- It never exits. In this case, the test runner will terminate the test because
  it sets a maximum time limit.

Tests can be added for multiple reasons:

- When adding new functionality.
- When fixing regressions and bugs.
- When expanding test coverage.

## Test structure

Let's analyze this very basic test from the Node.js test suite:

```javascript
1   'use strict';
2   const common = require('../common');
3
4   // This test ensures that the http-parser can handle UTF-8 characters
5   // in the http header.
6
7   const http = require('http');
8   const assert = require('assert');
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

**Lines 1-2**

```javascript
'use strict';
const common = require('../common');
```

The first line enables strict mode. All tests should be in strict mode unless
the nature of the test requires that the test run without it.

The second line loads the `common` module. The `common` module is a helper
module that provides useful tools for the tests.

Even if no functions or other properties exported by `common` are used in a
test, the `common` module should still be included. This is because the `common`
module includes code that will cause tests to fail if variables are leaked into
the global space. In situations where no functions or other properties exported
by `common` are used, it can be included without assigning it to an identifier:

```javascript
require('../common');
```

**Lines 4-5**

```javascript
// This test ensures that the http-parser can handle UTF-8 characters
// in the http header.
```

A test should start with a comment containing a brief description of what it is
designed to test.

**Lines 7-8**

```javascript
const http = require('http');
const assert = require('assert');
```

These modules are required for the test to run. Except for special cases, these
modules should only include core modules.
The `assert` module is used by most of the tests to check that the assumptions
for the test are met.

**Lines 10-21**

This is the body of the test. This test is quite simple, it just tests that an
HTTP server accepts `non-ASCII` characters in the headers of an incoming
request. Interesting things to notice:

- If the test doesn't depend on a specific port number then always use 0 instead
  of an arbitrary value, as it allows tests to be run in parallel safely, as the
  operating system will assign a random port. If the test requires a specific
  port, for example if the test checks that assigning a specific port works as
  expected, then it is ok to assign a specific port number.
- The use of `common.mustCall` to check that some callbacks/listeners are
  called.
- The HTTP server is closed once all the checks have run. This way, the test can
  exit gracefully. Remember that for a test to succeed, it must exit with a
  status code of 0.

## General recommendations

### Timers

The use of timers is discouraged, unless timers are being tested. There are
multiple reasons for this. Mainly, they are a source of flakiness. For a thorough
explanation go [here](https://github.com/nodejs/testing/issues/27).

In the event a timer is needed, it's recommended using the
`common.platformTimeout()` method, that allows setting specific timeouts
depending on the platform. For example:

```javascript
const timer = setTimeout(fail, common.platformTimeout(4000));
```

will create a 4-seconds timeout, except for some platforms where the delay will
be multiplied for some factor.

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
accomplish this, a `// Flags: ` comment should be added in the preamble of the
test followed by the flags. For example, to allow a test to require some of the
`internal/*` modules, the `--expose-internals` flag should be added.
A test that would require `internal/freelist` could start like this:

```javascript
'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const freelist = require('internal/freelist');
```
