3.5.0 / 2016-09-09
==================

  * deps: finalhandler@0.5.0
    - Change invalid or non-numeric status code to 500
    - Overwrite status message to match set status code
    - Prefer `err.statusCode` if `err.status` is invalid
    - Set response headers from `err.headers` object
    - Use `statuses` instead of `http` module for status messages

3.4.1 / 2016-01-23
==================

  * deps: finalhandler@0.4.1
    - deps: escape-html@~1.0.3
  * deps: parseurl@~1.3.1
    - perf: enable strict mode

3.4.0 / 2015-06-18
==================

  * deps: debug@~2.2.0
    - deps: ms@0.7.1
  * deps: finalhandler@0.4.0
    - Fix a false-positive when unpiping in Node.js 0.8
    - Support `statusCode` property on `Error` objects
    - Use `unpipe` module for unpiping requests
    - deps: debug@~2.2.0
    - deps: escape-html@1.0.2
    - deps: on-finished@~2.3.0
    - perf: enable strict mode
    - perf: remove argument reassignment
  * perf: enable strict mode
  * perf: remove argument reassignments

3.3.5 / 2015-03-16
==================

  * deps: debug@~2.1.3
    - Fix high intensity foreground color for bold
    - deps: ms@0.7.0
  * deps: finalhandler@0.3.4
    - deps: debug@~2.1.3

3.3.4 / 2015-01-07
==================

  * deps: debug@~2.1.1
  * deps: finalhandler@0.3.3
    - deps: debug@~2.1.1
    - deps: on-finished@~2.2.0

3.3.3 / 2014-11-09
==================

  * Correctly invoke async callback asynchronously

3.3.2 / 2014-10-28
==================

  * Fix handling of URLs containing `://` in the path

3.3.1 / 2014-10-22
==================

  * deps: finalhandler@0.3.2
    - deps: on-finished@~2.1.1

3.3.0 / 2014-10-17
==================

  * deps: debug@~2.1.0
    - Implement `DEBUG_FD` env variable support
  * deps: finalhandler@0.3.1
    - Terminate in progress response only on error
    - Use `on-finished` to determine request status
    - deps: debug@~2.1.0

3.2.0 / 2014-09-08
==================

  * deps: debug@~2.0.0
  * deps: finalhandler@0.2.0
    - Set `X-Content-Type-Options: nosniff` header
    - deps: debug@~2.0.0

3.1.1 / 2014-08-10
==================

  * deps: parseurl@~1.3.0

3.1.0 / 2014-07-22
==================

  * deps: debug@1.0.4
  * deps: finalhandler@0.1.0
    - Respond after request fully read
    - deps: debug@1.0.4
  * deps: parseurl@~1.2.0
    - Cache URLs based on original value
    - Remove no-longer-needed URL mis-parse work-around
    - Simplify the "fast-path" `RegExp`
  * perf: reduce executed logic in routing
  * perf: refactor location of `try` block

3.0.2 / 2014-07-10
==================

  * deps: debug@1.0.3
    - Add support for multiple wildcards in namespaces
  * deps: parseurl@~1.1.3
    - faster parsing of href-only URLs

3.0.1 / 2014-06-19
==================

  * use `finalhandler` for final response handling
  * deps: debug@1.0.2

3.0.0 / 2014-05-29
==================

  * No changes

3.0.0-rc.2 / 2014-05-04
=======================

  * Call error stack even when response has been sent
  * Prevent default 404 handler after response sent
  * dep: debug@0.8.1
  * encode stack in HTML for default error handler
  * remove `proto` export

3.0.0-rc.1 / 2014-03-06
=======================

  * move middleware to separate repos
  * remove docs
  * remove node patches
  * remove connect(middleware...)
  * remove the old `connect.createServer()` method
  * remove various private `connect.utils` functions
  * drop node.js 0.8 support

2.30.2 / 2015-07-31
===================

  * deps: body-parser@~1.13.3
    - deps: type-is@~1.6.6
  * deps: compression@~1.5.2
    - deps: accepts@~1.2.12
    - deps: compressible@~2.0.5
    - deps: vary@~1.0.1
  * deps: errorhandler@~1.4.2
    - deps: accepts@~1.2.12
  * deps: method-override@~2.3.5
    - deps: vary@~1.0.1
    - perf: enable strict mode
  * deps: serve-index@~1.7.2
    - deps: accepts@~1.2.12
    - deps: mime-types@~2.1.4
  * deps: type-is@~1.6.6
    - deps: mime-types@~2.1.4
  * deps: vhost@~3.0.1
    - perf: enable strict mode

2.30.1 / 2015-07-05
===================

  * deps: body-parser@~1.13.2
    - deps: iconv-lite@0.4.11
    - deps: qs@4.0.0
    - deps: raw-body@~2.1.2
    - deps: type-is@~1.6.4
  * deps: compression@~1.5.1
    - deps: accepts@~1.2.10
    - deps: compressible@~2.0.4
  * deps: errorhandler@~1.4.1
    - deps: accepts@~1.2.10
  * deps: qs@4.0.0
    - Fix dropping parameters like `hasOwnProperty`
    - Fix various parsing edge cases
  * deps: morgan@~1.6.1
    - deps: basic-auth@~1.0.3
  * deps: pause@0.1.0
    - Re-emit events with all original arguments
    - Refactor internals
    - perf: enable strict mode
  * deps: serve-index@~1.7.1
    - deps: accepts@~1.2.10
    - deps: mime-types@~2.1.2
  * deps: type-is@~1.6.4
    - deps: mime-types@~2.1.2
    - perf: enable strict mode
    - perf: remove argument reassignment

2.30.0 / 2015-06-18
===================

  * deps: body-parser@~1.13.1
    - Add `statusCode` property on `Error`s, in addition to `status`
    - Change `type` default to `application/json` for JSON parser
    - Change `type` default to `application/x-www-form-urlencoded` for urlencoded parser
    - Provide static `require` analysis
    - Use the `http-errors` module to generate errors
    - deps: bytes@2.1.0
    - deps: iconv-lite@0.4.10
    - deps: on-finished@~2.3.0
    - deps: raw-body@~2.1.1
    - deps: type-is@~1.6.3
    - perf: enable strict mode
    - perf: remove argument reassignment
    - perf: remove delete call
  * deps: bytes@2.1.0
    - Slight optimizations
    - Units no longer case sensitive when parsing
  * deps: compression@~1.5.0
    - Fix return value from `.end` and `.write` after end
    - Improve detection of zero-length body without `Content-Length`
    - deps: accepts@~1.2.9
    - deps: bytes@2.1.0
    - deps: compressible@~2.0.3
    - perf: enable strict mode
    - perf: remove flush reassignment
    - perf: simplify threshold detection
  * deps: cookie@0.1.3
    - Slight optimizations
  * deps: cookie-parser@~1.3.5
    - deps: cookie@0.1.3
  * deps: csurf@~1.8.3
    - Add `sessionKey` option
    - deps: cookie@0.1.3
    - deps: csrf@~3.0.0
  * deps: errorhandler@~1.4.0
    - Add charset to the `Content-Type` header
    - Support `statusCode` property on `Error` objects
    - deps: accepts@~1.2.9
    - deps: escape-html@1.0.2
  * deps: express-session@~1.11.3
    - Support an array in `secret` option for key rotation
    - deps: cookie@0.1.3
    - deps: crc@3.3.0
    - deps: debug@~2.2.0
    - deps: depd@~1.0.1
    - deps: uid-safe@~2.0.0
  * deps: finalhandler@0.4.0
    - Fix a false-positive when unpiping in Node.js 0.8
    - Support `statusCode` property on `Error` objects
    - Use `unpipe` module for unpiping requests
    - deps: escape-html@1.0.2
    - deps: on-finished@~2.3.0
    - perf: enable strict mode
    - perf: remove argument reassignment
  * deps: fresh@0.3.0
    - Add weak `ETag` matching support
  * deps: morgan@~1.6.0
    - Add `morgan.compile(format)` export
    - Do not color 1xx status codes in `dev` format
    - Fix `response-time` token to not include response latency
    - Fix `status` token incorrectly displaying before response in `dev` format
    - Fix token return values to be `undefined` or a string
    - Improve representation of multiple headers in `req` and `res` tokens
    - Use `res.getHeader` in `res` token
    - deps: basic-auth@~1.0.2
    - deps: on-finished@~2.3.0
    - pref: enable strict mode
    - pref: reduce function closure scopes
    - pref: remove dynamic compile on every request for `dev` format
    - pref: remove an argument reassignment
    - pref: skip function call without `skip` option
  * deps: serve-favicon@~2.3.0
    - Send non-chunked response for `OPTIONS`
    - deps: etag@~1.7.0
    - deps: fresh@0.3.0
    - perf: enable strict mode
    - perf: remove argument reassignment
    - perf: remove bitwise operations
  * deps: serve-index@~1.7.0
    - Accept `function` value for `template` option
    - Send non-chunked response for `OPTIONS`
    - Stat parent directory when necessary
    - Use `Date.prototype.toLocaleDateString` to format date
    - deps: accepts@~1.2.9
    - deps: escape-html@1.0.2
    - deps: mime-types@~2.1.1
    - perf: enable strict mode
    - perf: remove argument reassignment
  * deps: serve-static@~1.10.0
    - Add `fallthrough` option
    - Fix reading options from options prototype
    - Improve the default redirect response headers
    - Malformed URLs now `next()` instead of 400
    - deps: escape-html@1.0.2
    - deps: send@0.13.0
    - perf: enable strict mode
    - perf: remove argument reassignment
  * deps: type-is@~1.6.3
    - deps: mime-types@~2.1.1
    - perf: reduce try block size
    - perf: remove bitwise operations

2.29.2 / 2015-05-14
===================

  * deps: body-parser@~1.12.4
    - Slight efficiency improvement when not debugging
    - deps: debug@~2.2.0
    - deps: depd@~1.0.1
    - deps: iconv-lite@0.4.8
    - deps: on-finished@~2.2.1
    - deps: qs@2.4.2
    - deps: raw-body@~2.0.1
    - deps: type-is@~1.6.2
  * deps: compression@~1.4.4
    - deps: accepts@~1.2.7
    - deps: debug@~2.2.0
  * deps: connect-timeout@~1.6.2
    - deps: debug@~2.2.0
    - deps: ms@0.7.1
  * deps: debug@~2.2.0
    - deps: ms@0.7.1
  * deps: depd@~1.0.1
  * deps: errorhandler@~1.3.6
    - deps: accepts@~1.2.7
  * deps: finalhandler@0.3.6
    - deps: debug@~2.2.0
    - deps: on-finished@~2.2.1
  * deps: method-override@~2.3.3
    - deps: debug@~2.2.0
  * deps: morgan@~1.5.3
    - deps: basic-auth@~1.0.1
    - deps: debug@~2.2.0
    - deps: depd@~1.0.1
    - deps: on-finished@~2.2.1
  * deps: qs@2.4.2
   - Fix allowing parameters like `constructor`
  * deps: response-time@~2.3.1
    - deps: depd@~1.0.1
  * deps: serve-favicon@~2.2.1
    - deps: etag@~1.6.0
    - deps: ms@0.7.1
  * deps: serve-index@~1.6.4
    - deps: accepts@~1.2.7
    - deps: debug@~2.2.0
    - deps: mime-types@~2.0.11
  * deps: serve-static@~1.9.3
    - deps: send@0.12.3
  * deps: type-is@~1.6.2
    - deps: mime-types@~2.0.11

2.29.1 / 2015-03-16
===================

  * deps: body-parser@~1.12.2
    - deps: debug@~2.1.3
    - deps: qs@2.4.1
    - deps: type-is@~1.6.1
  * deps: compression@~1.4.3
    - Fix error when code calls `res.end(str, encoding)`
    - deps: accepts@~1.2.5
    - deps: debug@~2.1.3
  * deps: connect-timeout@~1.6.1
    - deps: debug@~2.1.3
  * deps: debug@~2.1.3
    - Fix high intensity foreground color for bold
    - deps: ms@0.7.0
  * deps: errorhandler@~1.3.5
    - deps: accepts@~1.2.5
  * deps: express-session@~1.10.4
    - deps: debug@~2.1.3
  * deps: finalhandler@0.3.4
    - deps: debug@~2.1.3
  * deps: method-override@~2.3.2
    - deps: debug@~2.1.3
  * deps: morgan@~1.5.2
    - deps: debug@~2.1.3
  * deps: qs@2.4.1
    - Fix error when parameter `hasOwnProperty` is present
  * deps: serve-index@~1.6.3
    - Properly escape file names in HTML
    - deps: accepts@~1.2.5
    - deps: debug@~2.1.3
    - deps: escape-html@1.0.1
    - deps: mime-types@~2.0.10
  * deps: serve-static@~1.9.2
    - deps: send@0.12.2
  * deps: type-is@~1.6.1
    - deps: mime-types@~2.0.10

2.29.0 / 2015-02-17
===================

  * Use `content-type` to parse `Content-Type` headers
  * deps: body-parser@~1.12.0
    - add `debug` messages
    - accept a function for the `type` option
    - make internal `extended: true` depth limit infinity
    - use `content-type` to parse `Content-Type` headers
    - deps: iconv-lite@0.4.7
    - deps: raw-body@1.3.3
    - deps: type-is@~1.6.0
  * deps: compression@~1.4.1
    - Prefer `gzip` over `deflate` on the server
    - deps: accepts@~1.2.4
  * deps: connect-timeout@~1.6.0
    - deps: http-errors@~1.3.1
  * deps: cookie-parser@~1.3.4
    - deps: cookie-signature@1.0.6
  * deps: cookie-signature@1.0.6
  * deps: csurf@~1.7.0
    - Accept `CSRF-Token` and `XSRF-Token` request headers
    - Default `cookie.path` to `'/'`, if using cookies
    - deps: cookie-signature@1.0.6
    - deps: csrf@~2.0.6
    - deps: http-errors@~1.3.1
  * deps: errorhandler@~1.3.4
    - deps: accepts@~1.2.4
  * deps: express-session@~1.10.3
    - deps: cookie-signature@1.0.6
    - deps: uid-safe@1.1.0
  * deps: http-errors@~1.3.1
    - Construct errors using defined constructors from `createError`
    - Fix error names that are not identifiers
    - Set a meaningful `name` property on constructed errors
  * deps: response-time@~2.3.0
    - Add function argument to support recording of response time
  * deps: serve-index@~1.6.2
    - deps: accepts@~1.2.4
    - deps: http-errors@~1.3.1
    - deps: mime-types@~2.0.9
  * deps: serve-static@~1.9.1
    - deps: send@0.12.1
  * deps: type-is@~1.6.0
    - fix argument reassignment
    - fix false-positives in `hasBody` `Transfer-Encoding` check
    - support wildcard for both type and subtype (`*/*`)
    - deps: mime-types@~2.0.9

2.28.3 / 2015-01-31
===================

  * deps: compression@~1.3.1
    - deps: accepts@~1.2.3
    - deps: compressible@~2.0.2
  * deps: csurf@~1.6.6
    - deps: csrf@~2.0.5
  * deps: errorhandler@~1.3.3
    - deps: accepts@~1.2.3
  * deps: express-session@~1.10.2
    - deps: uid-safe@1.0.3
  * deps: serve-index@~1.6.1
    - deps: accepts@~1.2.3
    - deps: mime-types@~2.0.8
  * deps: type-is@~1.5.6
    - deps: mime-types@~2.0.8

2.28.2 / 2015-01-20
===================

  * deps: body-parser@~1.10.2
    - deps: iconv-lite@0.4.6
    - deps: raw-body@1.3.2
  * deps: serve-static@~1.8.1
    - Fix redirect loop in Node.js 0.11.14
    - Fix root path disclosure
    - deps: send@0.11.1

2.28.1 / 2015-01-08
===================

  * deps: csurf@~1.6.5
    - deps: csrf@~2.0.4
  * deps: express-session@~1.10.1
    - deps: uid-safe@~1.0.2

2.28.0 / 2015-01-05
===================

  * deps: body-parser@~1.10.1
    - Make internal `extended: true` array limit dynamic
    - deps: on-finished@~2.2.0
    - deps: type-is@~1.5.5
  * deps: compression@~1.3.0
    - Export the default `filter` function for wrapping
    - deps: accepts@~1.2.2
    - deps: debug@~2.1.1
  * deps: connect-timeout@~1.5.0
    - deps: debug@~2.1.1
    - deps: http-errors@~1.2.8
    - deps: ms@0.7.0
  * deps: csurf@~1.6.4
    - deps: csrf@~2.0.3
    - deps: http-errors@~1.2.8
  * deps: debug@~2.1.1
  * deps: errorhandler@~1.3.2
    - Add `log` option
    - Fix heading content to not include stack
    - deps: accepts@~1.2.2
  * deps: express-session@~1.10.0
    - Add `store.touch` interface for session stores
    - Fix `MemoryStore` expiration with `resave: false`
    - deps: debug@~2.1.1
  * deps: finalhandler@0.3.3
    - deps: debug@~2.1.1
    - deps: on-finished@~2.2.0
  * deps: method-override@~2.3.1
    - deps: debug@~2.1.1
    - deps: methods@~1.1.1
  * deps: morgan@~1.5.1
    - Add multiple date formats `clf`, `iso`, and `web`
    - Deprecate `buffer` option
    - Fix date format in `common` and `combined` formats
    - Fix token arguments to accept values with `"`
    - deps: debug@~2.1.1
    - deps: on-finished@~2.2.0
  * deps: serve-favicon@~2.2.0
    - Support query string in the URL
    - deps: etag@~1.5.1
    - deps: ms@0.7.0
  * deps: serve-index@~1.6.0
    - Add link to root directory
    - deps: accepts@~1.2.2
    - deps: batch@0.5.2
    - deps: debug@~2.1.1
    - deps: mime-types@~2.0.7
  * deps: serve-static@~1.8.0
    - Fix potential open redirect when mounted at root
    - deps: send@0.11.0
  * deps: type-is@~1.5.5
    - deps: mime-types@~2.0.7

2.27.6 / 2014-12-10
===================

  * deps: serve-index@~1.5.3
    - deps: accepts@~1.1.4
    - deps: http-errors@~1.2.8
    - deps: mime-types@~2.0.4

2.27.5 / 2014-12-10
===================

  * deps: compression@~1.2.2
    - Fix `.end` to only proxy to `.end`
    - deps: accepts@~1.1.4
  * deps: express-session@~1.9.3
    - Fix error when `req.sessionID` contains a non-string value
  * deps: http-errors@~1.2.8
    - Fix stack trace from exported function
    - Remove `arguments.callee` usage
  * deps: serve-index@~1.5.2
    - Fix icon name background alignment on mobile view
  * deps: type-is@~1.5.4
    - deps: mime-types@~2.0.4

2.27.4 / 2014-11-23
===================

  * deps: body-parser@~1.9.3
    - deps: iconv-lite@0.4.5
    - deps: qs@2.3.3
    - deps: raw-body@1.3.1
    - deps: type-is@~1.5.3
  * deps: compression@~1.2.1
    - deps: accepts@~1.1.3
  * deps: errorhandler@~1.2.3
    - deps: accepts@~1.1.3
  * deps: express-session@~1.9.2
    - deps: crc@3.2.1
  * deps: qs@2.3.3
    - Fix `arrayLimit` behavior
  * deps: serve-favicon@~2.1.7
    - Avoid errors from enumerables on `Object.prototype`
  * deps: serve-index@~1.5.1
    - deps: accepts@~1.1.3
    - deps: mime-types@~2.0.3
  * deps: type-is@~1.5.3
    - deps: mime-types@~2.0.3

2.27.3 / 2014-11-09
===================

  * Correctly invoke async callback asynchronously
  * deps: csurf@~1.6.3
    - bump csrf
    - bump http-errors

2.27.2 / 2014-10-28
===================

  * Fix handling of URLs containing `://` in the path
  * deps: body-parser@~1.9.2
    - deps: qs@2.3.2
  * deps: qs@2.3.2
    - Fix parsing of mixed objects and values

2.27.1 / 2014-10-22
===================

  * deps: body-parser@~1.9.1
    - deps: on-finished@~2.1.1
    - deps: qs@2.3.0
    - deps: type-is@~1.5.2
  * deps: express-session@~1.9.1
    - Remove unnecessary empty write call
  * deps: finalhandler@0.3.2
    - deps: on-finished@~2.1.1
  * deps: morgan@~1.4.1
    - deps: on-finished@~2.1.1
  * deps: qs@2.3.0
    - Fix parsing of mixed implicit and explicit arrays
  * deps: serve-static@~1.7.1
    - deps: send@0.10.1

2.27.0 / 2014-10-16
===================

  * Use `http-errors` module for creating errors
  * Use `utils-merge` module for merging objects
  * deps: body-parser@~1.9.0
    - include the charset in "unsupported charset" error message
    - include the encoding in "unsupported content encoding" error message
    - deps: depd@~1.0.0
  * deps: compression@~1.2.0
    - deps: debug@~2.1.0
  * deps: connect-timeout@~1.4.0
    - Create errors with `http-errors`
    - deps: debug@~2.1.0
  * deps: debug@~2.1.0
    - Implement `DEBUG_FD` env variable support
  * deps: depd@~1.0.0
  * deps: express-session@~1.9.0
    - deps: debug@~2.1.0
    - deps: depd@~1.0.0
  * deps: finalhandler@0.3.1
    - Terminate in progress response only on error
    - Use `on-finished` to determine request status
    - deps: debug@~2.1.0
  * deps: method-override@~2.3.0
    - deps: debug@~2.1.0
  * deps: morgan@~1.4.0
    - Add `debug` messages
    - deps: depd@~1.0.0
  * deps: response-time@~2.2.0
    - Add `header` option for custom header name
    - Add `suffix` option
    - Change `digits` argument to an `options` argument
    - deps: depd@~1.0.0
  * deps: serve-favicon@~2.1.6
    - deps: etag@~1.5.0
  * deps: serve-index@~1.5.0
    - Add `dir` argument to `filter` function
    - Add icon for mkv files
    - Create errors with `http-errors`
    - Fix incorrect 403 on Windows and Node.js 0.11
    - Lookup icon by mime type for greater icon support
    - Support using tokens multiple times
    - deps: accepts@~1.1.2
    - deps: debug@~2.1.0
    - deps: mime-types@~2.0.2
  * deps: serve-static@~1.7.0
    - deps: send@0.10.0

2.26.6 / 2014-10-15
===================

  * deps: compression@~1.1.2
    - deps: accepts@~1.1.2
    - deps: compressible@~2.0.1
  * deps: csurf@~1.6.2
    - bump http-errors
    - fix cookie name when using `cookie: true`
  * deps: errorhandler@~1.2.2
    - deps: accepts@~1.1.2

2.26.5 / 2014-10-08
===================

  * Fix accepting non-object arguments to `logger`
  * deps: serve-static@~1.6.4
    - Fix redirect loop when index file serving disabled

2.26.4 / 2014-10-02
===================

  * deps: morgan@~1.3.2
    - Fix `req.ip` integration when `immediate: false`
  * deps: type-is@~1.5.2
    - deps: mime-types@~2.0.2

2.26.3 / 2014-09-24
===================

  * deps: body-parser@~1.8.4
    - fix content encoding to be case-insensitive
  * deps: serve-favicon@~2.1.5
    - deps: etag@~1.4.0
  * deps: serve-static@~1.6.3
    - deps: send@0.9.3

2.26.2 / 2014-09-19
===================

  * deps: body-parser@~1.8.3
    - deps: qs@2.2.4
  * deps: qs@2.2.4
    - Fix issue with object keys starting with numbers truncated

2.26.1 / 2014-09-15
===================

  * deps: body-parser@~1.8.2
    - deps: depd@0.4.5
  * deps: depd@0.4.5
  * deps: express-session@~1.8.2
    - Use `crc` instead of `buffer-crc32` for speed
    - deps: depd@0.4.5
  * deps: morgan@~1.3.1
    - Remove un-used `bytes` dependency
    - deps: depd@0.4.5
  * deps: serve-favicon@~2.1.4
    - Fix content headers being sent in 304 response
    - deps: etag@~1.3.1
  * deps: serve-static@~1.6.2
    - deps: send@0.9.2

2.26.0 / 2014-09-08
===================

  * deps: body-parser@~1.8.1
    - add `parameterLimit` option to `urlencoded` parser
    - change `urlencoded` extended array limit to 100
    - make empty-body-handling consistent between chunked requests
    - respond with 415 when over `parameterLimit` in `urlencoded`
    - deps: media-typer@0.3.0
    - deps: qs@2.2.3
    - deps: type-is@~1.5.1
  * deps: compression@~1.1.0
    - deps: accepts@~1.1.0
    - deps: compressible@~2.0.0
    - deps: debug@~2.0.0
  * deps: connect-timeout@~1.3.0
    - deps: debug@~2.0.0
  * deps: cookie-parser@~1.3.3
    - deps: cookie-signature@1.0.5
  * deps: cookie-signature@1.0.5
  * deps: csurf@~1.6.1
    - add `ignoreMethods` option
    - bump cookie-signature
    - csrf-tokens -> csrf
    - set `code` property on CSRF token errors
  * deps: debug@~2.0.0
  * deps: errorhandler@~1.2.0
    - Display error using `util.inspect` if no other representation
    - deps: accepts@~1.1.0
  * deps: express-session@~1.8.1
    - Do not resave already-saved session at end of request
    - Prevent session prototype methods from being overwritten
    - deps: cookie-signature@1.0.5
    - deps: debug@~2.0.0
  * deps: finalhandler@0.2.0
    - Set `X-Content-Type-Options: nosniff` header
    - deps: debug@~2.0.0
  * deps: fresh@0.2.4
  * deps: media-typer@0.3.0
    - Throw error when parameter format invalid on parse
  * deps: method-override@~2.2.0
    - deps: debug@~2.0.0
  * deps: morgan@~1.3.0
    - Assert if `format` is not a function or string
  * deps: qs@2.2.3
    - Fix issue where first empty value in array is discarded
  * deps: serve-favicon@~2.1.3
    - Accept string for `maxAge` (converted by `ms`)
    - Use `etag` to generate `ETag` header
    - deps: fresh@0.2.4
  * deps: serve-index@~1.2.1
    - Add `debug` messages
    - Resolve relative paths at middleware setup
    - deps: accepts@~1.1.0
  * deps: serve-static@~1.6.1
    - Add `lastModified` option
    - deps: send@0.9.1
  * deps: type-is@~1.5.1
    - fix `hasbody` to be true for `content-length: 0`
    - deps: media-typer@0.3.0
    - deps: mime-types@~2.0.1
  * deps: vhost@~3.0.0

2.25.10 / 2014-09-04
====================

  * deps: serve-static@~1.5.4
    - deps: send@0.8.5

2.25.9 / 2014-08-29
===================

  * deps: body-parser@~1.6.7
    - deps: qs@2.2.2
  * deps: qs@2.2.2

2.25.8 / 2014-08-27
===================

  * deps: body-parser@~1.6.6
    - deps: qs@2.2.0
  * deps: csurf@~1.4.1
  * deps: qs@2.2.0
    - Array parsing fix
    - Performance improvements

2.25.7 / 2014-08-18
===================

  * deps: body-parser@~1.6.5
    - deps: on-finished@2.1.0
  * deps: express-session@~1.7.6
    - Fix exception on `res.end(null)` calls
  * deps: morgan@~1.2.3
    - deps: on-finished@2.1.0
  * deps: serve-static@~1.5.3
    - deps: send@0.8.3

2.25.6 / 2014-08-14
===================

  * deps: body-parser@~1.6.4
    - deps: qs@1.2.2
  * deps: qs@1.2.2
  * deps: serve-static@~1.5.2
    - deps: send@0.8.2

2.25.5 / 2014-08-11
===================

  * Fix backwards compatibility in `logger`

2.25.4 / 2014-08-10
===================

  * Fix `query` middleware breaking with argument
    - It never really took one in the first place
  * deps: body-parser@~1.6.3
    - deps: qs@1.2.1
  * deps: compression@~1.0.11
    - deps: on-headers@~1.0.0
    - deps: parseurl@~1.3.0
  * deps: connect-timeout@~1.2.2
    - deps: on-headers@~1.0.0
  * deps: express-session@~1.7.5
    - Fix parsing original URL
    - deps: on-headers@~1.0.0
    - deps: parseurl@~1.3.0
  * deps: method-override@~2.1.3
  * deps: on-headers@~1.0.0
  * deps: parseurl@~1.3.0
  * deps: qs@1.2.1
  * deps: response-time@~2.0.1
    - deps: on-headers@~1.0.0
  * deps: serve-index@~1.1.6
    - Fix URL parsing
  * deps: serve-static@~1.5.1
    - Fix parsing of weird `req.originalUrl` values
    - deps: parseurl@~1.3.0
    = deps: utils-merge@1.0.0

2.25.3 / 2014-08-07
===================

  * deps: multiparty@3.3.2
    - Fix potential double-callback

2.25.2 / 2014-08-07
===================

  * deps: body-parser@~1.6.2
    - deps: qs@1.2.0
  * deps: qs@1.2.0
    - Fix parsing array of objects

2.25.1 / 2014-08-06
===================

  * deps: body-parser@~1.6.1
    - deps: qs@1.1.0
  * deps: qs@1.1.0
    - Accept urlencoded square brackets
    - Accept empty values in implicit array notation

2.25.0 / 2014-08-05
===================

  * deps: body-parser@~1.6.0
    - deps: qs@1.0.2
  * deps: compression@~1.0.10
    - Fix upper-case Content-Type characters prevent compression
    - deps: compressible@~1.1.1
  * deps: csurf@~1.4.0
    - Support changing `req.session` after `csurf` middleware
    - Calling `res.csrfToken()` after `req.session.destroy()` will now work
  * deps: express-session@~1.7.4
    - Fix `res.end` patch to call correct upstream `res.write`
    - Fix response end delay for non-chunked responses
  * deps: qs@1.0.2
    - Complete rewrite
    - Limits array length to 20
    - Limits object depth to 5
    - Limits parameters to 1,000
  * deps: serve-static@~1.5.0
    - Add `extensions` option
    - deps: send@0.8.1

2.24.3 / 2014-08-04
===================

  * deps: serve-index@~1.1.5
    - Fix Content-Length calculation for multi-byte file names
    - deps: accepts@~1.0.7
  * deps: serve-static@~1.4.4
    - Fix incorrect 403 on Windows and Node.js 0.11
    - deps: send@0.7.4

2.24.2 / 2014-07-27
===================

  * deps: body-parser@~1.5.2
  * deps: depd@0.4.4
    - Work-around v8 generating empty stack traces
  * deps: express-session@~1.7.2
  * deps: morgan@~1.2.2
  * deps: serve-static@~1.4.2

2.24.1 / 2014-07-26
===================

  * deps: body-parser@~1.5.1
  * deps: depd@0.4.3
    - Fix exception when global `Error.stackTraceLimit` is too low
  * deps: express-session@~1.7.1
  * deps: morgan@~1.2.1
  * deps: serve-index@~1.1.4
  * deps: serve-static@~1.4.1

2.24.0 / 2014-07-22
===================

  * deps: body-parser@~1.5.0
    - deps: depd@0.4.2
    - deps: iconv-lite@0.4.4
    - deps: raw-body@1.3.0
    - deps: type-is@~1.3.2
  * deps: compression@~1.0.9
    - Add `debug` messages
    - deps: accepts@~1.0.7
  * deps: connect-timeout@~1.2.1
    - Accept string for `time` (converted by `ms`)
    - deps: debug@1.0.4
  * deps: debug@1.0.4
  * deps: depd@0.4.2
    - Add `TRACE_DEPRECATION` environment variable
    - Remove non-standard grey color from color output
    - Support `--no-deprecation` argument
    - Support `--trace-deprecation` argument
  * deps: express-session@~1.7.0
    - Improve session-ending error handling
    - deps: debug@1.0.4
    - deps: depd@0.4.2
  * deps: finalhandler@0.1.0
    - Respond after request fully read
    - deps: debug@1.0.4
  * deps: method-override@~2.1.2
    - deps: debug@1.0.4
    - deps: parseurl@~1.2.0
  * deps: morgan@~1.2.0
    - Add `:remote-user` token
    - Add `combined` log format
    - Add `common` log format
    - Remove non-standard grey color from `dev` format
  * deps: multiparty@3.3.1
  * deps: parseurl@~1.2.0
    - Cache URLs based on original value
    - Remove no-longer-needed URL mis-parse work-around
    - Simplify the "fast-path" `RegExp`
  * deps: serve-static@~1.4.0
    - Add `dotfiles` option
    - deps: parseurl@~1.2.0
    - deps: send@0.7.0

2.23.0 / 2014-07-10
===================

  * deps: debug@1.0.3
    - Add support for multiple wildcards in namespaces
  * deps: express-session@~1.6.4
  * deps: method-override@~2.1.0
    - add simple debug output
    - deps: methods@1.1.0
    - deps: parseurl@~1.1.3
  * deps: parseurl@~1.1.3
    - faster parsing of href-only URLs
  * deps: serve-static@~1.3.1
    - deps: parseurl@~1.1.3

2.22.0 / 2014-07-03
===================

  * deps: csurf@~1.3.0
    - Fix `cookie.signed` option to actually sign cookie
  * deps: express-session@~1.6.1
    - Fix `res.end` patch to return correct value
    - Fix `res.end` patch to handle multiple `res.end` calls
    - Reject cookies with missing signatures
  * deps: multiparty@3.3.0
    - Always emit close after all parts ended
    - Fix callback hang in node.js 0.8 on errors
  * deps: serve-static@~1.3.0
    - Accept string for `maxAge` (converted by `ms`)
    - Add `setHeaders` option
    - Include HTML link in redirect response
    - deps: send@0.5.0

2.21.1 / 2014-06-26
===================

  * deps: cookie-parser@1.3.2
    - deps: cookie-signature@1.0.4
  * deps: cookie-signature@1.0.4
    - fix for timing attacks
  * deps: express-session@~1.5.2
    - deps: cookie-signature@1.0.4
  * deps: type-is@~1.3.2
    - more mime types

2.21.0 / 2014-06-20
===================

  * deprecate `connect(middleware)` -- use `app.use(middleware)` instead
  * deprecate `connect.createServer()` -- use `connect()` instead
  * fix `res.setHeader()` patch to work with get -> append -> set pattern
  * deps: compression@~1.0.8
  * deps: errorhandler@~1.1.1
  * deps: express-session@~1.5.0
    - Deprecate integration with `cookie-parser` middleware
    - Deprecate looking for secret in `req.secret`
    - Directly read cookies; `cookie-parser` no longer required
    - Directly set cookies; `res.cookie` no longer required
    - Generate session IDs with `uid-safe`, faster and even less collisions
  * deps: serve-index@~1.1.3

2.20.2 / 2014-06-19
===================

  * deps: body-parser@1.4.3
    - deps: type-is@1.3.1

2.20.1 / 2014-06-19
===================

  * deps: type-is@1.3.1
    - fix global variable leak

2.20.0 / 2014-06-19
===================

  * deprecate `verify` option to `json` -- use `body-parser` npm module instead
  * deprecate `verify` option to `urlencoded` -- use `body-parser` npm module instead
  * deprecate things with `depd` module
  * use `finalhandler` for final response handling
  * use `media-typer` to parse `content-type` for charset
  * deps: body-parser@1.4.2
    - check accepted charset in content-type (accepts utf-8)
    - check accepted encoding in content-encoding (accepts identity)
    - deprecate `urlencoded()` without provided `extended` option
    - lazy-load urlencoded parsers
    - support gzip and deflate bodies
    - set `inflate: false` to turn off
    - deps: raw-body@1.2.2
    - deps: type-is@1.3.0
    - Support all encodings from `iconv-lite`
  * deps: connect-timeout@1.1.1
    - deps: debug@1.0.2
  * deps: cookie-parser@1.3.1
    - export parsing functions
    - `req.cookies` and `req.signedCookies` are now plain objects
    - slightly faster parsing of many cookies
  * deps: csurf@1.2.2
  * deps: errorhandler@1.1.0
    - Display error on console formatted like `throw`
    - Escape HTML in stack trace
    - Escape HTML in title
    - Fix up edge cases with error sent in response
    - Set `X-Content-Type-Options: nosniff` header
    - Use accepts for negotiation
  * deps: express-session@1.4.0
    - Add `genid` option to generate custom session IDs
    - Add `saveUninitialized` option to control saving uninitialized sessions
    - Add `unset` option to control unsetting `req.session`
    - Generate session IDs with `rand-token` by default; reduce collisions
    - Integrate with express "trust proxy" by default
    - deps: buffer-crc32@0.2.3
    - deps: debug@1.0.2
  * deps: multiparty@3.2.9
  * deps: serve-index@1.1.2
    - deps: batch@0.5.1
  * deps: type-is@1.3.0
    - improve type parsing
  * deps: vhost@2.0.0
    - Accept `RegExp` object for `hostname`
    - Provide `req.vhost` object
    - Support IPv6 literal in `Host` header

2.19.6 / 2014-06-11
===================

  * deps: body-parser@1.3.1
    - deps: type-is@1.2.1
  * deps: compression@1.0.7
    - use vary module for better `Vary` behavior
    - deps: accepts@1.0.3
    - deps: compressible@1.1.0
  * deps: debug@1.0.2
  * deps: serve-index@1.1.1
    - deps: accepts@1.0.3
  * deps: serve-static@1.2.3
    - Do not throw un-catchable error on file open race condition
    - deps: send@0.4.3

2.19.5 / 2014-06-09
===================

  * deps: csurf@1.2.1
    - refactor to use csrf-tokens@~1.0.2
  * deps: debug@1.0.1
  * deps: serve-static@1.2.2
    - fix "event emitter leak" warnings
    - deps: send@0.4.2
  * deps: type-is@1.2.1
    - Switch dependency from `mime` to `mime-types@1.0.0`

2.19.4 / 2014-06-05
===================

  * deps: errorhandler@1.0.2
    - Pass on errors from reading error files
  * deps: method-override@2.0.2
    - use vary module for better `Vary` behavior
  * deps: serve-favicon@2.0.1
    - Reduce byte size of `ETag` header

2.19.3 / 2014-06-03
===================

  * deps: compression@1.0.6
    - fix listeners for delayed stream creation
    - fix regression for certain `stream.pipe(res)` situations
    - fix regression when negotiation fails

2.19.2 / 2014-06-03
===================

  * deps: compression@1.0.4
    - fix adding `Vary` when value stored as array
    - fix back-pressure behavior
    - fix length check for `res.end`

2.19.1 / 2014-06-02
===================

  * fix deprecated `utils.escape`

2.19.0 / 2014-06-02
===================

  * deprecate `methodOverride()` -- use `method-override` npm module instead
  * deps: body-parser@1.3.0
    - add `extended` option to urlencoded parser
  * deps: method-override@2.0.1
    - set `Vary` header
    - deps: methods@1.0.1
  * deps: multiparty@3.2.8
  * deps: response-time@2.0.0
    - add `digits` argument
    - do not override existing `X-Response-Time` header
    - timer not subject to clock drift
    - timer resolution down to nanoseconds
  * deps: serve-static@1.2.1
    - send max-age in Cache-Control in correct format
    - use `escape-html` for escaping
    - deps: send@0.4.1

2.18.0 / 2014-05-29
===================

  * deps: compression@1.0.3
  * deps: serve-index@1.1.0
    - Fix content negotiation when no `Accept` header
    - Properly support all HTTP methods
    - Support vanilla node.js http servers
    - Treat `ENAMETOOLONG` as code 414
    - Use accepts for negotiation
  * deps: serve-static@1.2.0
    - Calculate ETag with md5 for reduced collisions
    - Fix wrong behavior when index file matches directory
    - Ignore stream errors after request ends
    - Skip directories in index file search
    - deps: send@0.4.0

2.17.3 / 2014-05-27
===================

  * deps: express-session@1.2.1
    - Fix `resave` such that `resave: true` works

2.17.2 / 2014-05-27
===================

  * deps: body-parser@1.2.2
    - invoke `next(err)` after request fully read
    - deps: raw-body@1.1.6
  * deps: method-override@1.0.2
    - Handle `req.body` key referencing array or object
    - Handle multiple HTTP headers

2.17.1 / 2014-05-21
===================

  * fix `res.charset` appending charset when `content-type` has one

2.17.0 / 2014-05-20
===================

  * deps: express-session@1.2.0
    - Add `resave` option to control saving unmodified sessions
  * deps: morgan@1.1.1
    - "dev" format will use same tokens as other formats
    - `:response-time` token is now empty when immediate used
    - `:response-time` token is now monotonic
    - `:response-time` token has precision to 1 μs
    - fix `:status` + immediate output in node.js 0.8
    - improve `buffer` option to prevent indefinite event loop holding
    - simplify method to get remote address
    - deps: bytes@1.0.0
  * deps: serve-index@1.0.3
    - Fix error from non-statable files in HTML view

2.16.2 / 2014-05-18
===================

  * fix edge-case in `res.appendHeader` that would append in wrong order
  * deps: method-override@1.0.1

2.16.1 / 2014-05-17
===================

  * remove usages of `res.headerSent` from core

2.16.0 / 2014-05-17
===================

  * deprecate `res.headerSent` -- use `res.headersSent`
  * deprecate `res.on("header")` -- use on-headers module instead
  * fix `connect.version` to reflect the actual version
  * json: use body-parser
    - add `type` option
    - fix repeated limit parsing with every request
    - improve parser speed
  * urlencoded: use body-parser
    - add `type` option
    - fix repeated limit parsing with every request
  * dep: bytes@1.0.0
    * add negative support
  * dep: cookie-parser@1.1.0
    - deps: cookie@0.1.2
  * dep: csurf@1.2.0
    - add support for double-submit cookie
  * dep: express-session@1.1.0
    - Add `name` option; replacement for `key` option
    - Use `setImmediate` in MemoryStore for node.js >= 0.10

2.15.0 / 2014-05-04
===================

  * Add simple `res.cookie` support
  * Add `res.appendHeader`
  * Call error stack even when response has been sent
  * Patch `res.headerSent` to return Boolean
  * Patch `res.headersSent` for node.js 0.8
  * Prevent default 404 handler after response sent
  * dep: compression@1.0.2
    * support headers given to `res.writeHead`
    * deps: bytes@0.3.0
    * deps: negotiator@0.4.3
  * dep: connect-timeout@1.1.0
    * Add `req.timedout` property
    * Add `respond` option to constructor
    * Clear timer on socket destroy
    * deps: debug@0.8.1
  * dep: debug@^0.8.0
    * add `enable()` method
    * change from stderr to stdout
  * dep: errorhandler@1.0.1
    * Clean up error CSS
    * Do not respond after headers sent
  * dep: express-session@1.0.4
    * Remove import of `setImmediate`
    * Use `res.cookie()` instead of `res.setHeader()`
    * deps: cookie@0.1.2
    * deps: debug@0.8.1
  * dep: morgan@1.0.1
    * Make buffer unique per morgan instance
    * deps: bytes@0.3.0
  * dep: serve-favicon@2.0.0
    * Accept `Buffer` of icon as first argument
    * Non-GET and HEAD requests are denied
    * Send valid max-age value
    * Support conditional requests
    * Support max-age=0
    * Support OPTIONS method
    * Throw if `path` argument is directory
  * dep: serve-index@1.0.2
    * Add stylesheet option
    * deps: negotiator@0.4.3

2.14.5 / 2014-04-24
===================

  * dep: raw-body@1.1.4
    * allow true as an option
    * deps: bytes@0.3.0
  * dep: serve-static@1.1.0
    * Accept options directly to `send` module
    * deps: send@0.3.0

2.14.4 / 2014-04-07
===================

  * dep: bytes@0.3.0
    * added terabyte support
  * dep: csurf@1.1.0
    * add constant-time string compare
  * dep: serve-static@1.0.4
    * Resolve relative paths at middleware setup
    * Use parseurl to parse the URL from request
  * fix node.js 0.8 compatibility with memory session

2.14.3 / 2014-03-18
===================

  * dep: static-favicon@1.0.2
    * Fixed content of default icon

2.14.2 / 2014-03-11
===================

  * dep: static-favicon@1.0.1
    * Fixed path to default icon

2.14.1 / 2014-03-06
===================

  * dep: fresh@0.2.2
    * no real changes
  * dep: serve-index@1.0.1
    * deps: negotiator@0.4.2
  * dep: serve-static@1.0.2
    * deps: send@0.2.0

2.14.0 / 2014-03-05
===================

 * basicAuth: use basic-auth-connect
 * cookieParser: use cookie-parser
 * compress: use compression
 * csrf: use csurf
 * dep: cookie-signature@1.0.3
 * directory: use serve-index
 * errorHandler: use errorhandler
 * favicon: use static-favicon
 * logger: use morgan
 * methodOverride: use method-override
 * responseTime: use response-time
 * session: use express-session
 * static: use serve-static
 * timeout: use connect-timeout
 * vhost: use vhost

2.13.1 / 2014-03-05
===================

 * cookieSession: compare full value rather than crc32
 * deps: raw-body@1.1.3

2.13.0 / 2014-02-14
===================

 * fix typo in memory store warning #974 @rvagg
 * compress: use compressible
 * directory: add template option #990 @gottaloveit @Earl-Brown
 * csrf: prevent deprecated warning with old sessions

2.12.0 / 2013-12-10
===================

 * bump qs
 * directory: sort folders before files
 * directory: add folder icons
 * directory: de-duplicate icons, details/mobile views #968 @simov
 * errorHandler: end default 404 handler with a newline #972 @rlidwka
 * session: remove long cookie expire check #870 @undoZen

2.11.2 / 2013-12-01
===================

 * bump raw-body

2.11.1 / 2013-11-27
===================

 * bump raw-body
 * errorHandler: use `res.setHeader()` instead of `res.writeHead()` #949 @lo1tuma

2.11.0 / 2013-10-29
===================

 * update bytes
 * update uid2
 * update negotiator
 * sessions: add rolling session option #944 @ilmeo
 * sessions: property set cookies when given FQDN
 * cookieSessions: properly set cookies when given FQDN #948 @bmancini55
 * proto: fix FQDN mounting when multiple handlers #945 @bmancini55

2.10.1 / 2013-10-23
===================

 * fixed; fixed a bug with static middleware at root and trailing slashes #942 (@dougwilson)

2.10.0 / 2013-10-22
===================

 * fixed: set headers written by writeHead before emitting 'header'
 * fixed: mounted path should ignore querystrings on FQDNs #940 (@dougwilson)
 * fixed: parsing protocol-relative URLs with @ as pathnames #938 (@dougwilson)
 * fixed: fix static directory redirect for mount's root #937 (@dougwilson)
 * fixed: setting set-cookie header when mixing arrays and strings #893 (@anuj123)
 * bodyParser: optional verify function for urlencoded and json parsers for signing request bodies
 * compress: compress checks content-length to check threshold
 * compress: expose `res.flush()` for flushing responses
 * cookieParser: pass options into node-cookie #803 (@cauldrath)
 * errorHandler: replace `\n`s with `<br/>`s in error handler

2.9.2 / 2013-10-18
==================

 * warn about multiparty and limit middleware deprecation for v3
 * fix fully qualified domain name mounting. #920 (@dougwilson)
 * directory: Fix potential security issue with serving files outside the root. #929 (@dougwilson)
 * logger: store IP at beginning in case socket prematurely closes #930 (@dougwilson)

2.9.1 / 2013-10-15
==================

 * update multiparty
 * compress: Set vary header only if Content-Type passes filter #904
 * directory: Fix directory middleware URI escaping #917 (@dougwilson)
 * directory: Fix directory seperators for Windows #914 (@dougwilson)
 * directory: Keep query string intact during directory redirect #913 (@dougwilson)
 * directory: Fix paths in links #730 (@JacksonTian)
 * errorHandler: Don't escape text/plain as HTML #875 (@johan)
 * logger: Write '0' instead of '-' when response time is zero #910 (@dougwilson)
 * logger: Log even when connections are aborted #760 (@dylanahsmith)
 * methodOverride: Check req.body is an object #907 (@kbjr)
 * multipart: Add .type back to file parts for backwards compatibility #912 (@dougwilson)
 * multipart: Allow passing options to the Multiparty constructor #902 (@niftylettuce)

2.9.0 / 2013-09-07
==================

 * multipart: add docs regarding tmpfiles
 * multipart: add .name back to file parts
 * multipart: use multiparty instead of formidable

2.8.8 / 2013-09-02
==================

 * csrf: change to math.random() salt and remove csrfToken() callback

2.8.7 / 2013-08-28
==================

 * csrf: prevent salt generation on every request, and add async req.csrfToken(fn)

2.8.6 / 2013-08-28
==================

 * csrf: refactor to use HMAC tokens (BREACH attack)
 * compress: add compression of SVG and common font files by default.

2.8.5 / 2013-08-11
==================

 * add: compress Dart source files by default
 * update fresh

2.8.4 / 2013-07-08
==================

 * update send

2.8.3 / 2013-07-04
==================

 * add a name back to static middleware ("staticMiddleware")
 * fix .hasBody() utility to require transfer-encoding or content-length

2.8.2 / 2013-07-03
==================

 * update send
 * update cookie dep.
 * add better debug() for middleware
 * add whitelisting of supported methods to methodOverride()

2.8.1 / 2013-06-27
==================

 * fix: escape req.method in 404 response

2.8.0 / 2013-06-26
==================

 * add `threshold` option to `compress()` to prevent compression of small responses
 * add support for vendor JSON mime types in json()
 * add X-Forwarded-Proto initial https proxy support
 * change static redirect to 303
 * change octal escape sequences for strict mode
 * change: replace utils.uid() with uid2 lib
 * remove other "static" function name. Fixes #794
 * fix: hasBody() should return false if Content-Length: 0

2.7.11 / 2013-06-02
==================

 * update send

2.7.10 / 2013-05-21
==================

 * update qs
 * update formidable
 * fix: write/end to noop() when request aborted

2.7.9 / 2013-05-07
==================

  * update qs
  * drop support for node < v0.8

2.7.8 / 2013-05-03
==================

  * update qs

2.7.7 / 2013-04-29
==================

  * update qs dependency
  * remove "static" function name. Closes #794
  * update node-formidable
  * update buffer-crc32

2.7.6 / 2013-04-15
==================

  * revert cookie signature which was creating session race conditions

2.7.5 / 2013-04-12
==================

  * update cookie-signature
  * limit: do not consume request in node 0.10.x

2.7.4 / 2013-04-01
==================

  * session: add long expires check and prevent excess set-cookie
  * session: add console.error() of session#save() errors

2.7.3 / 2013-02-19
==================

  * add name to compress middleware
  * add appending Accept-Encoding to Vary when set but missing
  * add tests for csrf middleware
  * add 'next' support for connect() server handler
  * change utils.uid() to return url-safe chars. Closes #753
  * fix treating '.' as a regexp in vhost()
  * fix duplicate bytes dep in package.json. Closes #743
  * fix #733 - parse x-forwarded-proto in a more generally compatibly way
  * revert "add support for `next(status[, msg])`"; makes composition hard

2.7.2 / 2013-01-04
==================

  * add support for `next(status[, msg])` back
  * add utf-8 meta tag to support foreign characters in filenames/directories
  * change `timeout()` 408 to 503
  * replace 'node-crc' with 'buffer-crc32', fixes licensing
  * fix directory.html IE support

2.7.1 / 2012-12-05
==================

  * add directory() tests
  * add support for bodyParser to ignore Content-Type if no body is present (jquery primarily does this poorely)
  * fix errorHandler signature

2.7.0 / 2012-11-13
==================

  * add support for leading JSON whitespace
  * add logging of `req.ip` when present
  * add basicAuth support for `:`-delimited string
  * update cookie module. Closes #688

2.6.2 / 2012-11-01
==================

  * add `debug()` for disconnected session store
  * fix session regeneration bug. Closes #681

2.6.1 / 2012-10-25
==================

  * add passing of `connect.timeout()` errors to `next()`
  * replace signature utils with cookie-signature module

2.6.0 / 2012-10-09
==================

  * add `defer` option to `multipart()` [Blake Miner]
  * fix mount path case sensitivity. Closes #663
  * fix default of ascii encoding from `logger()`, now utf8. Closes #293

2.5.0 / 2012-09-27
==================

  * add `err.status = 400` to multipart() errors
  * add double-encoding protection to `compress()`. Closes #659
  * add graceful handling cookie parsing errors [shtylman]
  * fix typo X-Response-time to X-Response-Time

2.4.6 / 2012-09-18
==================

  * update qs

2.4.5 / 2012-09-03
==================

  * add session store "connect" / "disconnect" support [louischatriot]
  * fix `:url` log token

2.4.4 / 2012-08-21
==================

  * fix `static()` pause regression from "send" integration

2.4.3 / 2012-08-07
==================

  * fix `.write()` encoding for zlib inconstancy. Closes #561

2.4.2 / 2012-07-25
==================

  * remove limit default from `urlencoded()`
  * remove limit default from `json()`
  * remove limit default from `multipart()`
  * fix `cookieSession()` clear cookie path / domain bug. Closes #636

2.4.1 / 2012-07-24
==================

  * fix `options` mutation in `static()`

2.4.0 / 2012-07-23
==================

  * add `connect.timeout()`
  * add __GET__ / __HEAD__ check to `directory()`. Closes #634
  * add "pause" util dep
  * update send dep for normalization bug

2.3.9 / 2012-07-16
==================

  * add more descriptive invalid json error message
  * update send dep for root normalization regression
  * fix staticCache fresh dep

2.3.8 / 2012-07-12
==================

  * fix `connect.static()` 404 regression, pass `next()`. Closes #629

2.3.7 / 2012-07-05
==================

  * add `json()` utf-8 illustration test. Closes #621
  * add "send" dependency
  * change `connect.static()` internals to use "send"
  * fix `session()` req.session generation with pathname mismatch
  * fix `cookieSession()` req.session generation with pathname mismatch
  * fix mime export. Closes #618

2.3.6 / 2012-07-03
==================

  * Fixed cookieSession() with cookieParser() secret regression. Closes #602
  * Fixed set-cookie header fields on cookie.path mismatch. Closes #615

2.3.5 / 2012-06-28
==================

  * Remove `logger()` mount check
  * Fixed `staticCache()` dont cache responses with set-cookie. Closes #607
  * Fixed `staticCache()` when Cookie is present

2.3.4 / 2012-06-22
==================

  * Added `err.buf` to urlencoded() and json()
  * Update cookie to 0.0.4. Closes #604
  * Fixed: only send 304 if original response in 2xx or 304 [timkuijsten]

2.3.3 / 2012-06-11
==================

  * Added ETags back to `static()` [timkuijsten]
  * Replaced `utils.parseRange()` with `range-parser` module
  * Replaced `utils.parseBytes()` with `bytes` module
  * Replaced `utils.modified()` with `fresh` module
  * Fixed `cookieSession()` regression with invalid cookie signing [shtylman]

2.3.2 / 2012-06-08
==================

  * expose mime module
  * Update crc dep (which bundled nodeunit)

2.3.1 / 2012-06-06
==================

  * Added `secret` option to `cookieSession` middleware [shtylman]
  * Added `secret` option to `session` middleware [shtylman]
  * Added `req.remoteUser` back to `basicAuth()` as alias of `req.user`
  * Performance: improve signed cookie parsing
  * Update `cookie` dependency [shtylman]

2.3.0 / 2012-05-20
==================

  * Added limit option to `json()`
  * Added limit option to `urlencoded()`
  * Added limit option to `multipart()`
  * Fixed: remove socket error event listener on callback
  * Fixed __ENOTDIR__ error on `static` middleware

2.2.2 / 2012-05-07
==================

  * Added support to csrf middle for pre-flight CORS requests
  * Updated `engines` to allow newer version of node
  * Removed duplicate repo prop. Closes #560

2.2.1 / 2012-04-28
==================

  * Fixed `static()` redirect when mounted. Closes #554

2.2.0 / 2012-04-25
==================

  * Added `make benchmark`
  * Perf: memoize url parsing (~20% increase)
  * Fixed `connect(fn, fn2, ...)`. Closes #549

2.1.3 / 2012-04-20
==================

  * Added optional json() `reviver` function to be passed to JSON.parse [jed]
  * Fixed: emit drain in compress middleware [nsabovic]

2.1.2 / 2012-04-11
==================

  * Fixed cookieParser() `req.cookies` regression

2.1.1 / 2012-04-11
==================

  * Fixed `session()` browser-session length cookies & examples
  * Fixed: make `query()` "self-aware" [jed]

2.1.0 / 2012-04-05
==================

  * Added `debug()` calls to `.use()` (`DEBUG=connect:displatcher`)
  * Added `urlencoded()` support for GET
  * Added `json()` support for GET. Closes #497
  * Added `strict` option to `json()`
  * Changed: `session()` only set-cookie when modified
  * Removed `Session#lastAccess` property. Closes #399

2.0.3 / 2012-03-20
==================

  * Added: `cookieSession()` only sets cookie on change. Closes #442
  * Added `connect:dispatcher` debug() probes

2.0.2 / 2012-03-04
==================

  * Added test for __ENAMETOOLONG__ now that node is fixed
  * Fixed static() index "/" check on windows. Closes #498
  * Fixed Content-Range behaviour to match RFC2616 [matthiasdg / visionmedia]

2.0.1 / 2012-02-29
==================

  * Added test coverage for `vhost()` middleware
  * Changed `cookieParser()` signed cookie support to use SHA-2 [senotrusov]
  * Fixed `static()` Range: respond with 416 when unsatisfiable
  * Fixed `vhost()` middleware. Closes #494

2.0.0 / 2011-10-05
==================

  * Added `cookieSession()` middleware for cookie-only sessions
  * Added `compress()` middleware for gzip / deflate support
  * Added `session()` "proxy" setting to trust `X-Forwarded-Proto`
  * Added `json()` middleware to parse "application/json"
  * Added `urlencoded()` middleware to parse "application/x-www-form-urlencoded"
  * Added `multipart()` middleware to parse "multipart/form-data"
  * Added `cookieParser(secret)` support so anything using this middleware may access signed cookies
  * Added signed cookie support to `cookieParser()`
  * Added support for JSON-serialized cookies to `cookieParser()`
  * Added `err.status` support in Connect's default end-point
  * Added X-Cache MISS / HIT to `staticCache()`
  * Added public `res.headerSent` checking nodes `res._headerSent` until node does
  * Changed `basicAuth()` req.remoteUser to req.user
  * Changed: default `session()` to a browser-session cookie. Closes #475
  * Changed: no longer lowercase cookie names
  * Changed `bodyParser()` to use `json()`, `urlencoded()`, and `multipart()`
  * Changed: `errorHandler()` is now a development-only middleware
  * Changed middleware to `next()` errors when possible so applications can unify logging / handling
  * Removed `http[s].Server` inheritance, now just a function, making it easy to have an app providing both http and https
  * Removed `.createServer()` (use `connect()`)
  * Removed `secret` option from `session()`, use `cookieParser(secret)`
  * Removed `connect.session.ignore` array support
  * Removed `router()` middleware. Closes #262
  * Fixed: set-cookie only once for browser-session cookies
  * Fixed FQDN support. dont add leading "/"
  * Fixed 404 XSS attack vector. Closes #473
  * Fixed __HEAD__ support for 404s and 500s generated by Connect's end-point

1.8.5 / 2011-12-22
==================

  * Fixed: actually allow empty body for json

1.8.4 / 2011-12-22
==================

  * Changed: allow empty body for json/urlencoded requests. Backport for #443

1.8.3 / 2011-12-16
==================

  * Fixed `static()` _index.html_ support on windows

1.8.2 / 2011-12-03
==================

  * Fixed potential security issue, store files in req.files. Closes #431 [reported by dobesv]

1.8.1 / 2011-11-21
==================

  * Added nesting support for _multipart/form-data_ [jackyz]

1.8.0 / 2011-11-17
==================

  * Added _multipart/form-data_ support to `bodyParser()` using formidable

1.7.3 / 2011-11-11
==================

  * Fixed `req.body`, always default to {}
  * Fixed HEAD support for 404s and 500s

1.7.2 / 2011-10-24
==================

  * "node": ">= 0.4.1 < 0.7.0"
  * Added `static()` redirect option. Closes #398
  * Changed `limit()`: respond with 413 when content-length exceeds the limit
  * Removed socket error listener in static(). Closes #389
  * Fixed `staticCache()` Age header field
  * Fixed race condition causing errors reported in #329.

1.7.1 / 2011-09-12
==================

  * Added: make `Store` inherit from `EventEmitter`
  * Added session `Store#load(sess, fn)` to fetch a `Session` instance
  * Added backpressure support to `staticCache()`
  * Changed `res.socket.destroy()` to `req.socket.destroy()`

1.7.0 / 2011-08-31
==================

  * Added `staticCache()` middleware, a memory cache for `static()`
  * Added public `res.headerSent` checking nodes `res._headerSent` (remove when node adds this)
  * Changed: ignore error handling middleware when header is sent
  * Changed: dispatcher errors after header is sent destroy the sock

1.6.4 / 2011-08-26
==================

  * Revert "Added double-next reporting"

1.6.3 / 2011-08-26
==================

  * Added double-`next()` reporting
  * Added `immediate` option to `logger()`. Closes #321
  * Dependency `qs >= 0.3.1`

1.6.2 / 2011-08-11
==================

  * Fixed `connect.static()` null byte vulnerability
  * Fixed `connect.directory()` null byte vulnerability
  * Changed: 301 redirect in `static()` to postfix "/" on directory. Closes #289

1.6.1 / 2011-08-03
==================

  * Added: allow retval `== null` from logger callback to ignore line
  * Added `getOnly` option to `connect.static.send()`
  * Added response "header" event allowing augmentation
  * Added `X-CSRF-Token` header field check
  * Changed dep `qs >= 0.3.0`
  * Changed: persist csrf token. Closes #322
  * Changed: sort directory middleware files alphabetically

1.6.0 / 2011-07-10
==================

  * Added :response-time to "dev" logger format
  * Added simple `csrf()` middleware. Closes #315
  * Fixed `res._headers` logger regression. Closes #318
  * Removed support for multiple middleware being passed to `.use()`

1.5.2 / 2011-07-06
==================

  * Added `filter` function option to `directory()` [David Rio Deiros]
  * Changed: re-write of the `logger()` middleware, with extensible tokens and formats
  * Changed: `static.send()` ".." in path without root considered malicious
  * Fixed quotes in docs. Closes #312
  * Fixed urls when mounting `directory()`, use `originalUrl` [Daniel Dickison]


1.5.1 / 2011-06-20
==================

  * Added malicious path check to `directory()` middleware
  * Added `utils.forbidden(res)`
  * Added `connect.query()` middleware

1.5.0 / 2011-06-20
==================

  * Added `connect.directory()` middleware for serving directory listings

1.4.6 / 2011-06-18
==================

  * Fixed `connect.static()` root with `..`
  * Fixed `connect.static()` __EBADF__

1.4.5 / 2011-06-17
==================

  * Fixed EBADF in `connect.static()`. Closes #297

1.4.4 / 2011-06-16
==================

  * Changed `connect.static()` to check resolved dirname. Closes #294

1.4.3 / 2011-06-06
==================

  * Fixed fd leak in `connect.static()` when the socket is closed
  * Fixed; `bodyParser()` ignoring __GET/HEAD__. Closes #285

1.4.2 / 2011-05-27
==================

  * Changed to `devDependencies`
  * Fixed stream creation on `static()` __HEAD__ request. [Andreas Lind Petersen]
  * Fixed Win32 support for `static()`
  * Fixed monkey-patch issue. Closes #261

1.4.1 / 2011-05-08
==================

  * Added "hidden" option to `static()`. ignores hidden files by default. Closes   * Added; expose `connect.static.mime.define()`. Closes #251
  * Fixed `errorHandler` middleware for missing stack traces. [aseemk]
#274

1.4.0 / 2011-04-25
==================

  * Added route-middleware `next('route')` support to jump passed the route itself
  * Added Content-Length support to `limit()`
  * Added route-specific middleware support (used to be in express)
  * Changed; refactored duplicate session logic
  * Changed; prevent redefining `store.generate` per request
  * Fixed; `static()` does not set Content-Type when explicitly set [nateps]
  * Fixed escape `errorHandler()` {error} contents
  * NOTE: `router` will be removed in 2.0


1.3.0 / 2011-04-06
==================

  * Added `router.remove(path[, method])` to remove a route

1.2.3 / 2011-04-05
==================

  * Fixed basicAuth realm issue when passing strings. Closes #253

1.2.2 / 2011-04-05
==================

  * Added `basicAuth(username, password)` support
  * Added `errorHandler.title` defaulting to "Connect"
  * Changed `errorHandler` css

1.2.1 / 2011-03-30
==================

  * Fixed `logger()` https `remoteAddress` logging [Alexander Simmerl]

1.2.0 / 2011-03-30
==================

  * Added `router.lookup(path[, method])`
  * Added `router.match(url[, method])`
  * Added basicAuth async support. Closes #223

1.1.5 / 2011-03-27
==================

  * Added; allow `logger()` callback function to return an empty string to ignore logging
  * Fixed; utilizing `mime.charsets.lookup()` for `static()`. Closes 245

1.1.4 / 2011-03-23
==================

  * Added `logger()` support for format function
  * Fixed `logger()` to support mess of writeHead()/progressive api for node 0.4.x

1.1.3 / 2011-03-21
==================

  * Changed; `limit()` now calls `req.destroy()`

1.1.2 / 2011-03-21
==================

  * Added request "limit" event to `limit()` middleware
  * Changed; `limit()` middleware will `next(err)` on failure

1.1.1 / 2011-03-18
==================

  * Fixed session middleware for HTTPS. Closes #241 [reported by mt502]

1.1.0 / 2011-03-17
==================

  * Added `Session#reload(fn)`

1.0.6 / 2011-03-09
==================

  * Fixed `res.setHeader()` patch, preserve casing

1.0.5 / 2011-03-09
==================

  * Fixed; `logger()` using `req.originalUrl` instead of `req.url`

1.0.4 / 2011-03-09
==================

  * Added `res.charset`
  * Added conditional sessions example
  * Added support for `session.ignore` to be replaced. Closes #227
  * Fixed `Cache-Control` delimiters. Closes #228

1.0.3 / 2011-03-03
==================

  * Fixed; `static.send()` invokes callback with connection error

1.0.2 / 2011-03-02
==================

  * Fixed exported connect function
  * Fixed package.json; node ">= 0.4.1 < 0.5.0"

1.0.1 / 2011-03-02
==================

  * Added `Session#save(fn)`. Closes #213
  * Added callback support to `connect.static.send()` for express
  * Added `connect.static.send()` "path" option
  * Fixed content-type in `static()` for _index.html_

1.0.0 / 2011-03-01
==================

  * Added `stack`, `message`, and `dump` errorHandler option aliases
  * Added `req.originalMethod` to methodOverride
  * Added `favicon()` maxAge option support
  * Added `connect()` alternative to `connect.createServer()`
  * Added new [documentation](http://senchalabs.github.com/connect)
  * Added Range support to `static()`
  * Added HTTPS support
  * Rewrote session middleware. The session API now allows for
    session-specific cookies, so you may alter each individually.
    Click to view the new [session api](http://senchalabs.github.com/connect/middleware-session.html).
  * Added middleware self-awareness. This helps prevent
    middleware breakage when used within mounted servers.
    For example `cookieParser()` will not parse cookies more
    than once even when within a mounted server.
  * Added new examples in the `./examples` directory
  * Added [limit()](http://senchalabs.github.com/connect/middleware-limit.html) middleware
  * Added [profiler()](http://senchalabs.github.com/connect/middleware-profiler.html) middleware
  * Added [responseTime()](http://senchalabs.github.com/connect/middleware-responseTime.html) middleware
  * Renamed `staticProvider` to `static`
  * Renamed `bodyDecoder` to `bodyParser`
  * Renamed `cookieDecoder` to `cookieParser`
  * Fixed ETag quotes. [reported by papandreou]
  * Fixed If-None-Match comma-delimited ETag support. [reported by papandreou]
  * Fixed; only set req.originalUrl once. Closes #124
  * Fixed symlink support for `static()`. Closes #123

0.5.10 / 2011-02-14
==================

  * Fixed SID space issue. Closes #196
  * Fixed; proxy `res.end()` to commit session data
  * Fixed directory traversal attack in `staticProvider`. Closes #198

0.5.9 / 2011-02-09
==================

  * qs >= 0.0.4

0.5.8 / 2011-02-04
==================

  * Added `qs` dependency
  * Fixed router race-condition causing possible failure
    when `next()`ing to one or more routes with parallel
    requests

0.5.7 / 2011-02-01
==================

  * Added `onvhost()` call so Express (and others) can know when they are
  * Revert "Added stylus support" (use the middleware which ships with stylus)
  * Removed custom `Server#listen()` to allow regular `http.Server#listen()` args to work properly
  * Fixed long standing router issue (#83) that causes '.' to be disallowed within named placeholders in routes [Andreas Lind Petersen]
  * Fixed `utils.uid()` length error [Jxck]
mounted

0.5.6 / 2011-01-23
==================

  * Added stylus support to `compiler`
  * _favicon.js_ cleanup
  * _compiler.js_ cleanup
  * _bodyDecoder.js_ cleanup

0.5.5 / 2011-01-13
==================

  * Changed; using sha256 HMAC instead of md5. [Paul Querna]
  * Changed; generated a longer random UID, without time influence. [Paul Querna]
  * Fixed; session middleware throws when secret is not present. [Paul Querna]

0.5.4 / 2011-01-07
==================

  * Added; throw when router path or callback is missing
  * Fixed; `next(err)` on cookie parse exception instead of ignoring
  * Revert "Added utils.pathname(), memoized url.parse(str).pathname"

0.5.3 / 2011-01-05
==================

  * Added _docs/api.html_
  * Added `utils.pathname()`, memoized url.parse(str).pathname
  * Fixed `session.id` issue. Closes #183
  * Changed; Defaulting `staticProvider` maxAge to 0 not 1 year. Closes #179
  * Removed bad outdated docs, we need something new / automated eventually

0.5.2 / 2010-12-28
==================

  * Added default __OPTIONS__ support to _router_ middleware

0.5.1 / 2010-12-28
==================

  * Added `req.session.id` mirroring `req.sessionID`
  * Refactored router, exposing `connect.router.methods`
  * Exclude non-lib files from npm
  * Removed imposed headers `X-Powered-By`, `Server`, etc

0.5.0 / 2010-12-06
==================

  * Added _./index.js_
  * Added route segment precondition support and example
  * Added named capture group support to router

0.4.0 / 2010-11-29
==================

  * Added `basicAuth` middleware
  * Added more HTTP methods to the `router` middleware

0.3.0 / 2010-07-21
==================

  * Added _staticGzip_ middleware
  * Added `connect.utils` to expose utils
  * Added `connect.session.Session`
  * Added `connect.session.Store`
  * Added `connect.session.MemoryStore`
  * Added `connect.middleware` to expose the middleware getters
  * Added `buffer` option to _logger_ for performance increase
  * Added _favicon_ middleware for serving your own favicon or the connect default
  * Added option support to _staticProvider_, can now pass _root_ and _lifetime_.
  * Added; mounted `Server` instances now have the `route` property exposed for reflection
  * Added support for callback as first arg to `Server#use()`
  * Added support for `next(true)` in _router_ to bypass match attempts
  * Added `Server#listen()` _host_ support
  * Added `Server#route` when `Server#use()` is called with a route on a `Server` instance
  * Added _methodOverride_ X-HTTP-Method-Override support
  * Refactored session internals, adds _secret_ option
  * Renamed `lifetime` option to `maxAge` in _staticProvider_
  * Removed connect(1), it is now [spark(1)](http://github.com/senchalabs/spark)
  * Removed connect(1) dependency on examples, they can all now run with node(1)
  * Remove a typo that was leaking a global.
  * Removed `Object.prototype` forEach() and map() methods
  * Removed a few utils not used
  * Removed `connect.createApp()`
  * Removed `res.simpleBody()`
  * Removed _format_ middleware
  * Removed _flash_ middleware
  * Removed _redirect_ middleware
  * Removed _jsonrpc_ middleware, use [visionmedia/connect-jsonrpc](http://github.com/visionmedia/connect-jsonrpc)
  * Removed _pubsub_ middleware
  * Removed need for `params.{captures,splat}` in _router_ middleware, `params` is an array
  * Changed; _compiler_ no longer 404s
  * Changed; _router_ signature now matches connect middleware signature
  * Fixed a require in _session_ for default `MemoryStore`
  * Fixed nasty request body bug in _router_. Closes #54
  * Fixed _less_ support in _compiler_
  * Fixed bug preventing proper bubbling of exceptions in mounted servers
  * Fixed bug in `Server#use()` preventing `Server` instances as the first arg
  * Fixed **ENOENT** special case, is now treated as any other exception
  * Fixed spark env support

0.2.1 / 2010-07-09
==================

  * Added support for _router_ `next()` to continue calling matched routes
  * Added mime type for _cache.manifest_ files.
  * Changed _compiler_ middleware to use async require
  * Changed session api, stores now only require `#get()`, and `#set()`
  * Fixed _cacheManifest_ by adding `utils.find()` back

0.2.0 / 2010-07-01
==================

  * Added calls to `Session()` casts the given object as a `Session` instance
  * Added passing of `next()` to _router_ callbacks. Closes #46
  * Changed; `MemoryStore#destroy()` removes `req.session`
  * Changed `res.redirect("back")` to default to "/" when Referr?er is not present
  * Fixed _staticProvider_ urlencoded paths issue. Closes #47
  * Fixed _staticProvider_ middleware responding to **GET** requests
  * Fixed _jsonrpc_ middleware `Accept` header check. Closes #43
  * Fixed _logger_ format option
  * Fixed typo in _compiler_ middleware preventing the _dest_ option from working

0.1.0 / 2010-06-25
==================

  * Revamped the api, view the [Connect documentation](http://extjs.github.com/Connect/index.html#Middleware-Authoring) for more info (hover on the right for menu)
  * Added [extended api docs](http://extjs.github.com/Connect/api.html)
  * Added docs for several more middleware layers
  * Added `connect.Server#use()`
  * Added _compiler_ middleware which provides arbitrary static compilation
  * Added `req.originalUrl`
  * Removed _blog_ example
  * Removed _sass_ middleware (use _compiler_)
  * Removed _less_ middleware (use _compiler_)
  * Renamed middleware to be camelcase, _body-decoder_ is now _bodyDecoder_ etc.
  * Fixed `req.url` mutation bug when matching `connect.Server#use()` routes
  * Fixed `mkdir -p` implementation used in _bin/connect_. Closes #39
  * Fixed bug in _bodyDecoder_ throwing exceptions on request empty bodies
  * `make install` installing lib to $LIB_PREFIX aka $HOME/.node_libraries

0.0.6 / 2010-06-22
==================

  * Added _static_ middleware usage example
  * Added support for regular expressions as paths for _router_
  * Added `util.merge()`
  * Increased performance of _static_ by ~ 200 rps
  * Renamed the _rest_ middleware to _router_
  * Changed _rest_ api to accept a callback function
  * Removed _router_ middleware
  * Removed _proto.js_, only `Object#forEach()` remains

0.0.5 / 2010-06-21
==================

  * Added Server#use() which contains the Layer normalization logic
  * Added documentation for several middleware
  * Added several new examples
  * Added _less_ middleware
  * Added _repl_ middleware
  * Added _vhost_ middleware
  * Added _flash_ middleware
  * Added _cookie_ middleware
  * Added _session_ middleware
  * Added `utils.htmlEscape()`
  * Added `utils.base64Decode()`
  * Added `utils.base64Encode()`
  * Added `utils.uid()`
  * Added bin/connect app path and --config path support for .js suffix, although optional. Closes #26
  * Moved mime code to `utils.mime`, ex `utils.mime.types`, and `utils.mime.type()`
  * Renamed req.redirect() to res.redirect(). Closes #29
  * Fixed _sass_ 404 on **ENOENT**
  * Fixed +new Date duplication. Closes #24

0.0.4 / 2010-06-16
==================

  * Added workerPidfile() to bin/connect
  * Added --workers support to bin/connect stop and status commands
  * Added _redirect_ middleware
  * Added better --config support to bin/connect. All flags can be utilized
  * Added auto-detection of _./config.js_
  * Added config example
  * Added `net.Server` support to bin/connect
  * Writing worker pids relative to `env.pidfile`
  * s/parseQuery/parse/g
  * Fixed npm support

0.0.3 / 2010-06-16
==================

  * Fixed node dependency in package.json, now _">= 0.1.98-0"_ to support __HEAD__

0.0.2 / 2010-06-15
==================

  * Added `-V, --version` to bin/connect
  * Added `utils.parseCookie()`
  * Added `utils.serializeCookie()`
  * Added `utils.toBoolean()`
  * Added _sass_ middleware
  * Added _cookie_ middleware
  * Added _format_ middleware
  * Added _lint_ middleware
  * Added _rest_ middleware
  * Added _./package.json_ (npm install connect)
  * Added `handleError()` support
  * Added `process.connectEnv`
  * Added custom log format support to _log_ middleware
  * Added arbitrary env variable support to bin/connect (ext: --logFormat ":method :url")
  * Added -w, --workers to bin/connect
  * Added bin/connect support for --user NAME and --group NAME
  * Fixed url re-writing support

0.0.1 / 2010-06-03
==================

  * Initial release

