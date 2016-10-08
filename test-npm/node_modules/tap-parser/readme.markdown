# tap-parser

parse the [test anything protocol](http://testanything.org/)

[![build status](https://secure.travis-ci.org/tapjs/tap-parser.png)](http://travis-ci.org/tapjs/tap-parser)

[![browser support](http://ci.testling.com/substack/tap-parser.png)](http://ci.testling.com/substack/tap-parser)

[![coverage status](https://coveralls.io/repos/tapjs/tap-parser/badge.svg?branch=master&service=github)](https://coveralls.io/github/tapjs/tap-parser?branch=master)

# example

``` js
var parser = require('tap-parser');
var p = parser(function (results) {
    console.dir(results);
});

process.stdin.pipe(p);
```

given some [TAP](http://testanything.org/)-formatted input:

```
$ node test.js
TAP version 13
# beep
ok 1 should be equal
ok 2 should be equivalent
# boop
ok 3 should be equal
ok 4 (unnamed assert)

1..4
# tests 4
# pass  4

# ok
```

parse the output:

```
$ node test.js | node parse.js
{ ok: true, count: 4, pass: 4, plan: { start: 1, end: 4 } }
```

# usage

This package also has a `tap-parser` command.

```
Usage:
  tap-parser [-j [<indent>] | --json[=indent]]

Parses TAP data from stdin, and outputs an object representing
the data found in the TAP stream to stdout.

If there are any failures in the TAP stream, then exits with a
non-zero status code.

Data is output by default using node's `util.format()` method, but
JSON can be specified using the `-j` or `--json` flag with a number
of spaces to use as the indent (default=2).
```

# methods

``` js
var parser = require('tap-parser')
```

## var p = parser(options, cb)

Return a writable stream `p` that emits parse events.

If `cb` is given it will listen for the `'complete'` event.

If `options` is given, it may contain the following flags:

- `preserveWhitespace` boolean which is `false` by default and will
  cause the parser to emit `line` events even for lines containing
  only whitespace.  (Whitespace lines in yaml blocks are always
  emitted, because whitespace is semantically relevant for yaml.)

- `strict` boolean which is `false` by default and causes the parser
  to treat non-TAP input as a failure.  Strictness is heritable to
  child subtests.  You can also turn strictness on or off by using the
  `pragma +strict` line in the TAP data to turn strictness on, or
  `pragma -strict` to turn strictness off.

The `parent`, `level` and `buffered` options are reserved for internal
use.

# events

## p.on('complete', function (results) {})

The `results` object contains a summary of the number of tests
skipped, failed, passed, etc., as well as a boolean `ok` member which
is true if and only if the planned test were all found, and either
"ok" or marked as "TODO".

## p.on('assert', function (assert) {})

Every `/^(not )?ok\b/` line will emit an `'assert'` event.

Every `assert` object has these keys:

* `assert.ok` - true if the assertion succeeded, false if failed
* `assert.id` - the assertion number
* `assert.name` - optional short description of the assertion

and may also have

* `assert.todo` - optional description of why the assertion failure is
  not a problem.  (Boolean `true` if no explaination provided)
* `assert.skip` - optional description of why this assertion was
  skipped (boolean `true` if no explanation provided)
* `assert.diag` - a diagnostic object with additional information
  about the test point.

## p.on('comment', function (comment) {})

Every `/^# (.+)/` line will emit the string contents of `comment`.

## p.on('plan', function (plan) {})

Every `/^\d+\.\.\d+/` line emits a `'plan'` event for the test numbers
`plan.start` through `plan.end`, inclusive.

If the test is [completely
skipped](http://podwiki.hexten.net/TAP/TAP.html?page=TAP#Skippingeverything)
the result will look like

```
{ ok: true,
  count: 0,
  pass: 0,
  plan:
   { start: 1,
     end: 0,
     skipAll: true,
     skipReason: 'This code has no seat belt' } }
```

## p.on('version', function (version) {})

A `/^TAP version (\d+)/` line emits a `'version'` event with a version
number or string.

## p.on('bailout', function (reason) {})

A `bail out!` line will cause the parser to completely stop doing
anything.  Child parser bailouts will bail out their parents as well.

## p.on('child', function (childParser) {})

If a child test set is embedded in the stream like this:

```
TAP Version 13
1..2
# nesting
    1..2
    ok 1 - true is ok
    ok 2 - doag is also okay
ok 1 - nesting
ok 2 - second
```

then the child stream will be parsed and events will be raised on the
`childParser` object.

Since TAP streams with child tests *should* follow child test sets
with a pass or fail assert based on the child test's results, failing
to handle child tests should always result in the same end result.
However, additional information from those child tests will obviously
be lost.

## p.on('extra', function (extra) {})

All other lines will trigger an `'extra'` event with the line text.

# install

With [npm](https://npmjs.org) do:

```
npm install tap-parser
```

You can use [browserify](http://browserify.org) to `require('tap-parser')` in
the browser.

# license

MIT
