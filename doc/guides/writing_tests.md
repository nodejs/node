# How to write a test for the Node.js project

## What is a test?

A test must be a node script that exercises a specific functionality provided
by node and checks that it behaves as expected. It should return 0 on success,
otherwise it will fail. A test will fail if:

- It exits by calling `process.exit(code)` where `code != 0`
- It exits due to an uncaught exception.
- It never exits. In this case, the test runner will terminate the test because
  it sets a maximum time limit.

Tests can be added for multiple reasons:

- When adding new functionality.
- When fixing regressions and bugs.
- When expanding test coverage.


## Test structure

Let's analyze this very basic test from the Node.js test suite:

```javascript
1  'use strict'; 
2  const common = require('../common');
3  const http = require('http');
4  const assert = require('assert');
5
6  const server = http.createServer(common.mustCall((req, res) => {
7    res.end('ok');
8  }));
9  server.listen(common.PORT, () => {
10   http.get({
11     port: common.PORT,
12     headers: {'Test': 'Düsseldorf'}
13   }, common.mustCall((res) => {
14     assert.equal(res.statusCode, 200);
15     server.close();
16   }));
17 });
```

**Lines 1-2**

```javascript
'use strict';
const common = require('../common');
```

These two lines are mandatory and should be included on every test.
The `common` module is a helper module that provides useful tools for the tests.
If for some reason, no functionality from `common` is used, it should still be
included like this:

```javascript
require('../common');
```

Why? It checks for leaks of globals.

**Lines 3-4**

```javascript
const http = require('http');
const assert = require('assert');
```

These modules are required for the test to run. Except for special cases, these
modules should only include core modules.
The `assert` module is used by most of the tests to check that the assumptions
for the test are met.

**Lines 6-17**

This is the body of the test. This test is quite simple, it just tests that an
HTTP server accepts `non-ASCII` characters in the headers of an incoming
request. Interesting things to notice:

- The use of `common.PORT` as the listening port. Always use `common.PORT`
  instead of using an arbitrary value, as it allows to run tests in parallel
  safely, as they are not trying to reuse the same port another test is already
  using.
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
}).listen(common.PORT, function() {
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
})).listen(common.PORT, function() {
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
