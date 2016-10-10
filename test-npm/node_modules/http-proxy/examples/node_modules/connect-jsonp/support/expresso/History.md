
0.7.3 / 2011-03-02 
==================

  * Fixed server listening check. Closes #62. [Andrew Stone]

0.7.2 / 2010-12-29 
==================

  * Fixed problem with `listen()` sometimes firing on the same tick [guillermo]

0.7.1 / 2010-12-28 
==================

  * Fixed `assert.request()` client logic into an issue() function, fired upon the `listen()` callback if the server doesn't have an assigned fd. [guillermo]
  * Removed `--watch`

0.7.0 / 2010-11-19 
==================

  * Removed `assert` from test function signature
    Just use `require('assert')` :) this will make integration
    with libraries like [should](http://github.com/visionmedia/should) cleaner.

0.6.4 / 2010-11-02 
==================

  * Added regexp support to `assert.response()` headers
  * Removed `waitForExit` code, causing issues

0.6.3 / 2010-11-02 
==================

  * Added `assert.response()` body RegExp support
  * Fixed issue with _--serial_ not executing files sequentially. Closes #42
  * Fixed hang when modules use `setInterval` - monitor running tests & force the process to quit after all have completed + timeout [Steve Mason]

0.6.2 / 2010-09-17 
==================

  * Added _node-jsocoverage_ to package.json (aka will respect npm's binroot)
  * Added _-t, --timeout_ MS option, defaulting to 2000 ms
  * Added _-s, --serial_
  * __PREFIX__ clobberable
  * Fixed `assert.response()` for latest node
  * Fixed cov reporting from exploding on empty files

0.6.2 / 2010-08-03
==================

  * Added `assert.type()`
  * Renamed `assert.isNotUndefined()` to `assert.isDefined()`
  * Fixed `assert.includes()` param ordering

0.6.0 / 2010-07-31
==================

  * Added _docs/api.html_
  * Added -w, --watch
  * Added `Array` support to `assert.includes()`
  * Added; outputting exceptions immediately. Closes #19
  * Fixed `assert.includes()` param ordering
  * Fixed `assert.length()` param ordering
  * Fixed jscoverage links

0.5.0 / 2010-07-16
==================

  * Added support for async exports
  * Added timeout support to `assert.response()`. Closes #3
  * Added 4th arg callback support to `assert.response()`
  * Added `assert.length()`
  * Added `assert.match()`
  * Added `assert.isUndefined()`
  * Added `assert.isNull()`
  * Added `assert.includes()`
  * Added growlnotify support via -g, --growl
  * Added -o, --only TESTS. Ex: --only "test foo()" --only "test foo(), test bar()"
  * Removed profanity

0.4.0 / 2010-07-09
==================

  * Added reporting source coverage (respects --boring for color haters)
  * Added callback to assert.response(). Closes #12
  * Fixed; putting exceptions to stderr. Closes #13

0.3.1 / 2010-06-28
==================

  * Faster assert.response()

0.3.0 / 2010-06-28
==================

  * Added -p, --port NUM flags
  * Added assert.response(). Closes #11

0.2.1 / 2010-06-25
==================

  * Fixed issue with reporting object assertions

0.2.0 / 2010-06-21
==================

  * Added `make uninstall`
  * Added better readdir() failure message
  * Fixed `make install` for kiwi

0.1.0 / 2010-06-15
==================

  * Added better usage docs via --help
  * Added better conditional color support
  * Added pre exit assertion support

0.0.3 / 2010-06-02
==================

  * Added more room for filenames in test coverage
  * Added boring output support via --boring (suppress colored output)
  * Fixed async failure exit status

0.0.2 / 2010-05-30
==================

  * Fixed exit status for CI support

0.0.1 / 2010-05-30
==================

  * Initial release