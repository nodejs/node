0.5.0 / 2016-06-15
==================

  * Change invalid or non-numeric status code to 500
  * Overwrite status message to match set status code
  * Prefer `err.statusCode` if `err.status` is invalid
  * Set response headers from `err.headers` object
  * Use `statuses` instead of `http` module for status messages
    - Includes all defined status messages

0.4.1 / 2015-12-02
==================

  * deps: escape-html@~1.0.3
    - perf: enable strict mode
    - perf: optimize string replacement
    - perf: use faster string coercion

0.4.0 / 2015-06-14
==================

  * Fix a false-positive when unpiping in Node.js 0.8
  * Support `statusCode` property on `Error` objects
  * Use `unpipe` module for unpiping requests
  * deps: escape-html@1.0.2
  * deps: on-finished@~2.3.0
    - Add defined behavior for HTTP `CONNECT` requests
    - Add defined behavior for HTTP `Upgrade` requests
    - deps: ee-first@1.1.1
  * perf: enable strict mode
  * perf: remove argument reassignment

0.3.6 / 2015-05-11
==================

  * deps: debug@~2.2.0
    - deps: ms@0.7.1

0.3.5 / 2015-04-22
==================

  * deps: on-finished@~2.2.1
    - Fix `isFinished(req)` when data buffered

0.3.4 / 2015-03-15
==================

  * deps: debug@~2.1.3
    - Fix high intensity foreground color for bold
    - deps: ms@0.7.0

0.3.3 / 2015-01-01
==================

  * deps: debug@~2.1.1
  * deps: on-finished@~2.2.0

0.3.2 / 2014-10-22
==================

  * deps: on-finished@~2.1.1
    - Fix handling of pipelined requests

0.3.1 / 2014-10-16
==================

  * deps: debug@~2.1.0
    - Implement `DEBUG_FD` env variable support

0.3.0 / 2014-09-17
==================

  * Terminate in progress response only on error
  * Use `on-finished` to determine request status

0.2.0 / 2014-09-03
==================

  * Set `X-Content-Type-Options: nosniff` header
  * deps: debug@~2.0.0

0.1.0 / 2014-07-16
==================

  * Respond after request fully read
    - prevents hung responses and socket hang ups
  * deps: debug@1.0.4

0.0.3 / 2014-07-11
==================

  * deps: debug@1.0.3
    - Add support for multiple wildcards in namespaces

0.0.2 / 2014-06-19
==================

  * Handle invalid status codes

0.0.1 / 2014-06-05
==================

  * deps: debug@1.0.2

0.0.0 / 2014-06-05
==================

  * Extracted from connect/express
