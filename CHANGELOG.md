# Node.js ChangeLog

## 2016-05-05, Version 6.1.0 (Current), @Fishrock123

### Notable Changes

* **assert**: `deep{Strict}Equal()` now works correctly with circular references. (Rich Trott) [#6432](https://github.com/nodejs/node/pull/6432)
* **debugger**: Arrays are now formatted correctly in the debugger repl. (cjihrig) [#6448](https://github.com/nodejs/node/pull/6448)
* **deps**: Upgrade OpenSSL sources to 1.0.2h (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
  - Please see our [blog post](https://nodejs.org/en/blog/vulnerability/openssl-may-2016/) for more info on the security contents of this release.
* **net**: Introduced a `Socket#connecting` property. (Fedor Indutny) [#6404](https://github.com/nodejs/node/pull/6404)
  - Previously this information was only available as the undocumented, internal `_connecting` property.
* **process**: Introduced `process.cpuUsage()`. (Patrick Mueller) [#6157](https://github.com/nodejs/node/pull/6157)
* **stream**: `Writable#setDefaultEncoding()` now returns `this`. (Alexander Makarenko) [#5040](https://github.com/nodejs/node/pull/5040)
* **util**: Two new additions to `util.inspect()`:
  - Added a `maxArrayLength` option to truncate the formatting of Arrays. (James M Snell) [#6334](https://github.com/nodejs/node/pull/6334)
    - This is set to `100` by default.
  - Added a `showProxy` option for formatting proxy intercepting handlers. (James M Snell) [#6465](https://github.com/nodejs/node/pull/6465)
    - Inspecting proxies is non-trivial and as such this is off by default.

### Commits

* [[`76c9ab5fcf`](https://github.com/nodejs/node/commit/76c9ab5fcf)] - **assert**: allow circular references (Rich Trott) [#6432](https://github.com/nodejs/node/pull/6432)
* [[`7b9ae70757`](https://github.com/nodejs/node/commit/7b9ae70757)] - **benchmark**: Fix crash in net benchmarks (Matt Loring) [#6407](https://www.github.com/nodejs/node/pull/6407)
* [[`0d1985358a`](https://github.com/nodejs/node/commit/0d1985358a)] - **build**: use shorthand lint target from test (Johan Bergström) [#6406](https://github.com/nodejs/node/pull/6406)
* [[`7153f96f0e`](https://github.com/nodejs/node/commit/7153f96f0e)] - **build**: unbreak -prof, disable PIE on OS X (Ben Noordhuis) [#6453](https://github.com/nodejs/node/pull/6453)
* [[`8956432e18`](https://github.com/nodejs/node/commit/8956432e18)] - **build**: exclude tap files from tarballs (Brian White) [#6348](https://github.com/nodejs/node/pull/6348)
* [[`11e7cc5310`](https://github.com/nodejs/node/commit/11e7cc5310)] - **build**: don't compile with -B (Ben Noordhuis) [#6393](https://github.com/nodejs/node/pull/6393)
* [[`1330496bbf`](https://github.com/nodejs/node/commit/1330496bbf)] - **cluster**: remove use of bind() in destroy() (yorkie) [#6502](https://github.com/nodejs/node/pull/6502)
* [[`fdde36909c`](https://github.com/nodejs/node/commit/fdde36909c)] - **crypto**: fix error in deprecation message (Rich Trott) [#6344](https://github.com/nodejs/node/pull/6344)
* [[`2d503b1d4b`](https://github.com/nodejs/node/commit/2d503b1d4b)] - **debugger**: display array contents in repl (cjihrig) [#6448](https://github.com/nodejs/node/pull/6448)
* [[`54f8600613`](https://github.com/nodejs/node/commit/54f8600613)] - **deps**: update openssl asm and asm_obsolete files (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
* [[`a5a2944877`](https://github.com/nodejs/node/commit/a5a2944877)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [nodejs/io.js#1836](https://github.com/nodejs/io.js/pull/1836)
* [[`3fe68129c8`](https://github.com/nodejs/node/commit/3fe68129c8)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`d159462fed`](https://github.com/nodejs/node/commit/d159462fed)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`3af28d3693`](https://github.com/nodejs/node/commit/3af28d3693)] - **deps**: copy all openssl header files to include dir (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
* [[`e6ab3ece65`](https://github.com/nodejs/node/commit/e6ab3ece65)] - **deps**: upgrade openssl sources to 1.0.2h (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
* [[`65b6574d59`](https://github.com/nodejs/node/commit/65b6574d59)] - **deps**: backport IsValid changes from 4e8736d in V8 (Michaël Zasso) [#6544](https://github.com/nodejs/node/pull/6544)
* [[`33f24c821b`](https://github.com/nodejs/node/commit/33f24c821b)] - **doc**: adds 'close' events to fs.ReadStream and fs.WriteStream (Jenna Vuong) [#6499](https://github.com/nodejs/node/pull/6499)
* [[`4f728df1bf`](https://github.com/nodejs/node/commit/4f728df1bf)] - **doc**: linkify remaining references to fs.Stats object (Kevin Donahue) [#6485](https://github.com/nodejs/node/pull/6485)
* [[`9a29b50c52`](https://github.com/nodejs/node/commit/9a29b50c52)] - **doc**: fix the lint of an example in cluster.md (yorkie) [#6516](https://github.com/nodejs/node/pull/6516)
* [[`d674493fa5`](https://github.com/nodejs/node/commit/d674493fa5)] - **doc**: add missing underscore for markdown italics (Kevin Donahue) [#6529](https://github.com/nodejs/node/pull/6529)
* [[`7c30f15e1b`](https://github.com/nodejs/node/commit/7c30f15e1b)] - **doc**: ensure consistent grammar in node.1 file (justshiv) [#6426](https://github.com/nodejs/node/pull/6426)
* [[`e5ce53a217`](https://github.com/nodejs/node/commit/e5ce53a217)] - **doc**: fix sentence fragment in fs doc (Rich Trott) [#6488](https://github.com/nodejs/node/pull/6488)
* [[`3e028a143c`](https://github.com/nodejs/node/commit/3e028a143c)] - **doc**: remove obsolete comment in isError() example (cjihrig) [#6486](https://github.com/nodejs/node/pull/6486)
* [[`969f96a019`](https://github.com/nodejs/node/commit/969f96a019)] - **doc**: fix a typo in `__dirname` section (William Luo) [#6473](https://github.com/nodejs/node/pull/6473)
* [[`ab7055b003`](https://github.com/nodejs/node/commit/ab7055b003)] - **doc**: fix fs.realpath man pg links (phette23) [#6451](https://github.com/nodejs/node/pull/6451)
* [[`13e660888f`](https://github.com/nodejs/node/commit/13e660888f)] - **doc**: extra clarification of historySize option (vsemozhetbyt) [#6397](https://github.com/nodejs/node/pull/6397)
* [[`3d5b732660`](https://github.com/nodejs/node/commit/3d5b732660)] - **doc**: clarifies http.serverResponse implementation (Allen Hernandez) [#6072](https://github.com/nodejs/node/pull/6072)
* [[`7034ebe2bc`](https://github.com/nodejs/node/commit/7034ebe2bc)] - **doc**: use `Node.js` in synopsis document (Rich Trott) [#6476](https://github.com/nodejs/node/pull/6476)
* [[`4ae39f9863`](https://github.com/nodejs/node/commit/4ae39f9863)] - **doc**: remove all scrollbar styling (Claudio Rodriguez) [#6479](https://github.com/nodejs/node/pull/6479)
* [[`e6c8da45b1`](https://github.com/nodejs/node/commit/e6c8da45b1)] - **(SEMVER-MINOR)** **doc**: make `writable.setDefaultEncoding()` return `this` (Alexander Makarenko) [#5040](https://github.com/nodejs/node/pull/5040)
* [[`4068d64f4f`](https://github.com/nodejs/node/commit/4068d64f4f)] - **doc**: fix EventEmitter#eventNames() example (Сковорода Никита Андреевич) [#6417](https://github.com/nodejs/node/pull/6417)
* [[`bfcde97251`](https://github.com/nodejs/node/commit/bfcde97251)] - **doc**: fix incorrect syntax in examples (Evan Lucas) [#6463](https://github.com/nodejs/node/pull/6463)
* [[`8eb87ee239`](https://github.com/nodejs/node/commit/8eb87ee239)] - **doc**: Remove extra space in REPL example (Juan) [#6447](https://github.com/nodejs/node/pull/6447)
* [[`fd37d54eb5`](https://github.com/nodejs/node/commit/fd37d54eb5)] - **doc**: added note warning about change to console.endTime() (Ben Page) [#6454](https://github.com/nodejs/node/pull/6454)
* [[`b3f75ec801`](https://github.com/nodejs/node/commit/b3f75ec801)] - **doc**: expand documentation for process.exit() (James M Snell) [#6410](https://github.com/nodejs/node/pull/6410)
* [[`fc0fbf1c63`](https://github.com/nodejs/node/commit/fc0fbf1c63)] - **doc**: subdivide TOC, add auxiliary links (Jeremiah Senkpiel) [#6167](https://github.com/nodejs/node/pull/6167)
* [[`150dd36503`](https://github.com/nodejs/node/commit/150dd36503)] - **doc**: no Node.js(1) (Jeremiah Senkpiel) [#6167](https://github.com/nodejs/node/pull/6167)
* [[`ab84d69048`](https://github.com/nodejs/node/commit/ab84d69048)] - **doc**: better example & synopsis (Jeremiah Senkpiel) [#6167](https://github.com/nodejs/node/pull/6167)
* [[`f6d72791a1`](https://github.com/nodejs/node/commit/f6d72791a1)] - **doc**: update build instructions for OS X (Rich Trott) [#6309](https://github.com/nodejs/node/pull/6309)
* [[`36207c6daf`](https://github.com/nodejs/node/commit/36207c6daf)] - **doc**: correctly document the behavior of ee.once(). (Lance Ball) [#6371](https://github.com/nodejs/node/pull/6371)
* [[`19fb1345ba`](https://github.com/nodejs/node/commit/19fb1345ba)] - **doc**: use Buffer.from() instead of new Buffer() (Jackson Tian) [#6367](https://github.com/nodejs/node/pull/6367)
* [[`fb6753c75c`](https://github.com/nodejs/node/commit/fb6753c75c)] - **doc**: fix v6 changelog (James M Snell) [#6435](https://github.com/nodejs/node/pull/6435)
* [[`2c92a1fe03`](https://github.com/nodejs/node/commit/2c92a1fe03)] - **events**: pass the original listener added by once (DavidCai) [#6394](https://github.com/nodejs/node/pull/6394)
* [[`9ea6b282e8`](https://github.com/nodejs/node/commit/9ea6b282e8)] - **meta**: split CHANGELOG into two files (Myles Borins) [#6337](https://github.com/nodejs/node/pull/6337)
* [[`cbbe95e1e1`](https://github.com/nodejs/node/commit/cbbe95e1e1)] - **(SEMVER-MINOR)** **net**: introduce `Socket#connecting` property (Fedor Indutny) [#6404](https://github.com/nodejs/node/pull/6404)
* [[`534f03c2f0`](https://github.com/nodejs/node/commit/534f03c2f0)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`1b9fdba04e`](https://github.com/nodejs/node/commit/1b9fdba04e)] - **(SEMVER-MINOR)** **process**: add process.cpuUsage() - implementation, doc, tests (Patrick Mueller) [#6157](https://github.com/nodejs/node/pull/6157)
* [[`fa9d82d120`](https://github.com/nodejs/node/commit/fa9d82d120)] - **src**: unify implementations of Utf8Value etc. (Anna Henningsen) [#6357](https://github.com/nodejs/node/pull/6357)
* [[`65030c77b7`](https://github.com/nodejs/node/commit/65030c77b7)] - **test**: fix alpn tests for openssl1.0.2h (Shigeki Ohtsu) [#6550](https://github.com/nodejs/node/pull/6550)
* [[`7641f9a6de`](https://github.com/nodejs/node/commit/7641f9a6de)] - **test**: refactor large event emitter tests (cjihrig) [#6446](https://github.com/nodejs/node/pull/6446)
* [[`5fe5fa2897`](https://github.com/nodejs/node/commit/5fe5fa2897)] - **test**: make addon testing part of `make test` (Ben Noordhuis) [#6232](https://github.com/nodejs/node/pull/6232)
* [[`457d12a0a1`](https://github.com/nodejs/node/commit/457d12a0a1)] - **test**: add failing url parse tests as known_issue (James M Snell) [#5885](https://github.com/nodejs/node/pull/5885)
* [[`089362f8b8`](https://github.com/nodejs/node/commit/089362f8b8)] - **test,tools**: limit lint tolerance of gc global (Rich Trott) [#6324](https://github.com/nodejs/node/pull/6324)
* [[`6d1606ee94`](https://github.com/nodejs/node/commit/6d1606ee94)] - **test,tools**: adjust function argument alignment (Rich Trott) [#6390](https://github.com/nodejs/node/pull/6390)
* [[`08e0884ae0`](https://github.com/nodejs/node/commit/08e0884ae0)] - **tools**: add -F flag for fixing lint issues (Rich Trott) [#6483](https://github.com/nodejs/node/pull/6483)
* [[`9f23cb24f2`](https://github.com/nodejs/node/commit/9f23cb24f2)] - **tools**: fix exit code when linting from CI (Brian White) [#6412](https://github.com/nodejs/node/pull/6412)
* [[`e62c42b8f4`](https://github.com/nodejs/node/commit/e62c42b8f4)] - **tools**: remove default parameters from lint rule (Rich Trott) [#6411](https://github.com/nodejs/node/pull/6411)
* [[`66903f6695`](https://github.com/nodejs/node/commit/66903f6695)] - **tools**: add tests for the doctool (Ian Kronquist) [#6031](https://github.com/nodejs/node/pull/6031)
* [[`3f608b16a7`](https://github.com/nodejs/node/commit/3f608b16a7)] - **tools**: lint for function argument alignment (Rich Trott) [#6390](https://github.com/nodejs/node/pull/6390)
* [[`91ab769940`](https://github.com/nodejs/node/commit/91ab769940)] - **(SEMVER-MINOR)** **util**: truncate inspect array and typed array (James M Snell) [#6334](https://github.com/nodejs/node/pull/6334)
* [[`0bca959617`](https://github.com/nodejs/node/commit/0bca959617)] - **(SEMVER-MINOR)** **util**: fix inspecting of proxy objects (James M Snell) [#6465](https://github.com/nodejs/node/pull/6465)

## 2016-04-26, Version 6.0.0 (Current), @jasnell

### Notable changes

The following significant changes have been made since the previous Node.js
v5.0.0 release.

* Buffer
  * New Buffer constructors have been added
    [#4682](https://github.com/nodejs/node/pull/4682) and
    [#5833](https://github.com/nodejs/node/pull/5833).
  * Existing `Buffer()` and `SlowBuffer()` constructors have been deprecated
    in docs [#4682](https://github.com/nodejs/node/pull/4682) and
    [#5833](https://github.com/nodejs/node/pull/5833).
  * Previously deprecated Buffer APIs are removed
    [#5048](https://github.com/nodejs/node/pull/5048),
    [#4594](https://github.com/nodejs/node/pull/4594).
  * Improved error handling [#4514](https://github.com/nodejs/node/pull/4514).
  * The `Buffer.prototype.lastIndexOf()` method has been added
    [#4846](https://github.com/nodejs/node/pull/4846).
* Cluster
  * Worker emitted as first argument in 'message' event
    [#5361](https://github.com/nodejs/node/pull/5361).
  * The `worker.exitedAfterDisconnect` property replaces `worker.suicide`
    [#3743](https://github.com/nodejs/node/pull/3743).
* Console
  * Calling `console.timeEnd()` with an unknown label now emits a process
    warning rather than throwing
    [#5901](https://github.com/nodejs/node/pull/5901).
* Crypto
  * Improved error handling [#3100](https://github.com/nodejs/node/pull/3100),
    [#5611](https://github.com/nodejs/node/pull/5611).
  * Simplified Certificate class bindings
    [#5382](https://github.com/nodejs/node/pull/5382).
  * Improved control over FIPS mode
    [#5181](https://github.com/nodejs/node/pull/5181).
  * pbkdf2 digest overloading is deprecated
    [#4047](https://github.com/nodejs/node/pull/4047).
* Dependencies
  * Reintroduce shared c-ares build support
    [#5775](https://github.com/nodejs/node/pull/5775).
  * V8 updated to 5.0.71.35 [#6372](https://github.com/nodejs/node/pull/6372).
* DNS
  * Add `dns.resolvePtr()` API to query plain DNS PTR records
    [#4921](https://github.com/nodejs/node/pull/4921).
* Domains
  * Clear stack when no error handler
  [#4659](https://github.com/nodejs/node/pull/4659).
* Events
  * The `EventEmitter.prototype._events` object no longer inherits from
    Object.prototype [#6092](https://github.com/nodejs/node/pull/6092).
  * The `EventEmitter.prototype.prependListener()` and
    `EventEmitter.prototype.prependOnceListener()` methods have been added
    [#6032](https://github.com/nodejs/node/pull/6032).
* File System
  * The `fs.realpath()` and `fs.realpathSync()` methods have been updated
    to use a more efficient libuv-based implementation. This change includes
    the removal of the `cache` argument and the method can throw new errors
    [#3594](https://github.com/nodejs/node/pull/3594).
  * FS apis can now accept and return paths as Buffers
    [#5616](https://github.com/nodejs/node/pull/5616).
  * Error handling and type checking improvements
    [#5616](https://github.com/nodejs/node/pull/5616),
    [#5590](https://github.com/nodejs/node/pull/5590),
    [#4518](https://github.com/nodejs/node/pull/4518),
    [#3917](https://github.com/nodejs/node/pull/3917).
  * fs.read's string interface is deprecated
    [#4525](https://github.com/nodejs/node/pull/4525).
* HTTP
  * 'clientError' can now be used to return custom errors from an
    HTTP server [#4557](https://github.com/nodejs/node/pull/4557).
* Modules
  * Current directory is now prioritized for local lookups
    [#5689](https://github.com/nodejs/node/pull/5689).
  * Symbolic links are preserved when requiring modules
    [#5950](https://github.com/nodejs/node/pull/5950).
* Net
  * DNS hints no longer implicitly set
    [#6021](https://github.com/nodejs/node/pull/6021).
  * Improved error handling and type checking
    [#5981](https://github.com/nodejs/node/pull/5981),
    [#5733](https://github.com/nodejs/node/pull/5733),
    [#2904](https://github.com/nodejs/node/pull/2904).
* npm
  * Running npm requires the node binary to be in the path
    [#6098](https://github.com/nodejs/node/pull/6098).
* OS X
  * MACOSX_DEPLOYMENT_TARGET has been bumped up to 10.7
    [#6402](https://github.com/nodejs/node/pull/6402).
* Path
  * Improved type checking [#5348](https://github.com/nodejs/node/pull/5348).
* Process
  * Introduce process warnings API
    [#4782](https://github.com/nodejs/node/pull/4782).
  * Throw exception when non-function passed to nextTick
    [#3860](https://github.com/nodejs/node/pull/3860).
* Querystring
  * The object returned by `querystring.parse()` no longer inherits from
    Object.prototype [#6055](https://github.com/nodejs/node/pull/6055).
* Readline
  * Key info is emitted unconditionally
    [#6024](https://github.com/nodejs/node/pull/6024).
  * History can now be explicitly disabled
    [#6352](https://github.com/nodejs/node/pull/6352).
* REPL
  * Assignment to `_` will emit a warning
    [#5535](https://github.com/nodejs/node/pull/5535).
  * Expressions will no longer be completed when eval fails
    [#6328](https://github.com/nodejs/node/pull/6328).
* Timers
  * Fail early when callback is not a function
    [#4362](https://github.com/nodejs/node/pull/4362).
* Streams
  * `null` is now an invalid chunk to write in object mode
    [#6170](https://github.com/nodejs/node/pull/6170).
* TLS
  * Rename 'clientError' to 'tlsClientError'
    [#4557](https://github.com/nodejs/node/pull/4557).
  * SHA1 used for sessionIdContext
    [#3866](https://github.com/nodejs/node/pull/3866).
* TTY
  * Previously deprecated setRawMode wrapper is removed
    [#2528](https://github.com/nodejs/node/pull/2528).
* URL
  * Username and password will be dropped by `url.resolve()` if the host
    changes [#1480](https://github.com/nodejs/node/pull/1480).
* Util
  * Changes to Error object formatting
    [#4582](https://github.com/nodejs/node/pull/4582).
  * The `util._extend()` method has been deprecated
    [#4903](https://github.com/nodejs/node/pull/4903)
  * The `util.log()` method has been deprecated
    [#6161](https://github.com/nodejs/node/pull/6161).
* Windows
  * Windows XP and Vista are no longer supported
    [#5167](https://github.com/nodejs/node/pull/5167).
* Zlib
  * Multiple improvements have been made to Zlib processing
    [#5883](https://github.com/nodejs/node/pull/5883) and
    [#5707](https://github.com/nodejs/node/pull/5707).


### Commits

Semver-major Changes since v5.0.0

* [[`829d613daa`](https://github.com/nodejs/node/commit/829d613daa)] - **(SEMVER-MAJOR)** **buffer**: docs-only deprecate SlowBuffer (James M Snell) [#5833](https://github.com/nodejs/node/pull/5833)
* [[`d0c6e6f7c4`](https://github.com/nodejs/node/commit/d0c6e6f7c4)] - **(SEMVER-MAJOR)** **buffer**: add Buffer.allocUnsafeSlow(size) (James M Snell) [#5833](https://github.com/nodejs/node/pull/5833)
* [[`85ab4a5f12`](https://github.com/nodejs/node/commit/85ab4a5f12)] - **(SEMVER-MAJOR)** **buffer**: add .from(), .alloc() and .allocUnsafe() (James M Snell) [#4682](https://github.com/nodejs/node/pull/4682)
* [[`2c55cc2d2c`](https://github.com/nodejs/node/commit/2c55cc2d2c)] - **(SEMVER-MAJOR)** **buffer**: remove deprecated Buffer.write branch (dcposch@dcpos.ch) [#5048](https://github.com/nodejs/node/pull/5048)
* [[`101bca988c`](https://github.com/nodejs/node/commit/101bca988c)] - **(SEMVER-MAJOR)** **buffer**: remove deprecated buffer.get/.set methods (Feross Aboukhadijeh) [#4594](https://github.com/nodejs/node/pull/4594)
* [[`3b27dd5ce1`](https://github.com/nodejs/node/commit/3b27dd5ce1)] - **(SEMVER-MAJOR)** **buffer**: throw if both length and enc are passed (Mathias Buus) [#4514](https://github.com/nodejs/node/pull/4514)
* [[`ce864ef502`](https://github.com/nodejs/node/commit/ce864ef502)] - **(SEMVER-MAJOR)** **build**: Bump MACOSX_DEPLOYMENT_TARGET to 10.7 (Сковорода Никита Андреевич) [#6402](https://github.com/nodejs/node/pull/6402)
* [[`90a5fc20be`](https://github.com/nodejs/node/commit/90a5fc20be)] - **(SEMVER-MAJOR)** **build**: remove lint/dotfiles from release tarball (Johan Bergström) [#5695](https://github.com/nodejs/node/pull/5695)
* [[`66f048af6f`](https://github.com/nodejs/node/commit/66f048af6f)] - **(SEMVER-MAJOR)** **cluster**: migrate from worker.suicide (Evan Lucas) [#3743](https://github.com/nodejs/node/pull/3743)
* [[`66f4586dd0`](https://github.com/nodejs/node/commit/66f4586dd0)] - **(SEMVER-MAJOR)** **cluster**: emit worker as first 'message' event arg (Ben Noordhuis) [#5361](https://github.com/nodejs/node/pull/5361)
* [[`30507951d9`](https://github.com/nodejs/node/commit/30507951d9)] - **(SEMVER-MAJOR)** **console**: timeEnd() with no label emits warning (Eugene Obrezkov) [#5901](https://github.com/nodejs/node/pull/5901)
* [[`a5cce79ec3`](https://github.com/nodejs/node/commit/a5cce79ec3)] - **(SEMVER-MAJOR)** **console**: delete timers that have ended (Vladimir Varankin) [#3562](https://github.com/nodejs/node/pull/3562)
* [[`47cca06412`](https://github.com/nodejs/node/commit/47cca06412)] - **(SEMVER-MAJOR)** **crypto**: better error message for createHash (Calvin Metcalf) [#6042](https://github.com/nodejs/node/pull/6042)
* [[`41feaa89e0`](https://github.com/nodejs/node/commit/41feaa89e0)] - **(SEMVER-MAJOR)** **crypto**: improve error messages (Sakthipriyan Vairamani) [#3100](https://github.com/nodejs/node/pull/3100)
* [[`f429fe1b88`](https://github.com/nodejs/node/commit/f429fe1b88)] - **(SEMVER-MAJOR)** **crypto**: fail early when loading crypto without openssl (James M Snell) [#5611](https://github.com/nodejs/node/pull/5611)
* [[`a37401e061`](https://github.com/nodejs/node/commit/a37401e061)] - **(SEMVER-MAJOR)** **crypto**: simplify Certificate class bindings (Alexander Makarenko) [#5382](https://github.com/nodejs/node/pull/5382)
* [[`7c48cb5601`](https://github.com/nodejs/node/commit/7c48cb5601)] - **(SEMVER-MAJOR)** **crypto**: Improve control of FIPS mode (Stefan Budeanu) [#5181](https://github.com/nodejs/node/pull/5181)
* [[`a1163582c5`](https://github.com/nodejs/node/commit/a1163582c5)] - **(SEMVER-MAJOR)** **crypto**: pbkdf2 deprecate digest overload. (Tom Gallacher) [#4047](https://github.com/nodejs/node/pull/4047)
* [[`b010c87164`](https://github.com/nodejs/node/commit/b010c87164)] - **(SEMVER-MAJOR)** **crypto, string_bytes**: treat `buffer` str as `utf8` (Fedor Indutny) [#5522](https://github.com/nodejs/node/pull/5522)
* [[`a42453b085`](https://github.com/nodejs/node/commit/a42453b085)] - **(SEMVER-MAJOR)** **deps**: upgrade V8 to 5.0.71.32 (Ali Ijaz Sheikh) [#6111](https://github.com/nodejs/node/pull/6111)
* [[`2253be95d0`](https://github.com/nodejs/node/commit/2253be95d0)] - **(SEMVER-MAJOR)** **deps**: reintroduce supporting shared c-ares builds (Johan Bergström) [#5775](https://github.com/nodejs/node/pull/5775)
* [[`4bc1cccb22`](https://github.com/nodejs/node/commit/4bc1cccb22)] - **(SEMVER-MAJOR)** **dgram**: pass null as error on successful send() (cjihrig) [#5929](https://github.com/nodejs/node/pull/5929)
* [[`dbdbdd4998`](https://github.com/nodejs/node/commit/dbdbdd4998)] - **(SEMVER-MAJOR)** **dns**: add resolvePtr to query plain DNS PTR records (Daniel Turing) [#4921](https://github.com/nodejs/node/pull/4921)
* [[`c4ab861a49`](https://github.com/nodejs/node/commit/c4ab861a49)] - **(SEMVER-MAJOR)** **dns**: add failure test for dns.resolveXXX (Daniel Turing) [#4921](https://github.com/nodejs/node/pull/4921)
* [[`f3be421c1c`](https://github.com/nodejs/node/commit/f3be421c1c)] - **(SEMVER-MAJOR)** **dns**: coerce port to number in lookupService (Evan Lucas) [#4883](https://github.com/nodejs/node/pull/4883)
* [[`fa74805bac`](https://github.com/nodejs/node/commit/fa74805bac)] - **(SEMVER-MAJOR)** **doc**: doc-only deprecation for util.log() (Jackson Tian) [#6161](https://github.com/nodejs/node/pull/6161)
* [[`34b1a64068`](https://github.com/nodejs/node/commit/34b1a64068)] - **(SEMVER-MAJOR)** **doc**: minor copy improvement in buffer.markdown (James M Snell) [#5833](https://github.com/nodejs/node/pull/5833)
* [[`4d4f3535a9`](https://github.com/nodejs/node/commit/4d4f3535a9)] - **(SEMVER-MAJOR)** **doc**: general improvements to fs.markdown (James M Snell) [#5616](https://github.com/nodejs/node/pull/5616)
* [[`d8290286b3`](https://github.com/nodejs/node/commit/d8290286b3)] - **(SEMVER-MAJOR)** **doc**: document deprecation of util._extend (Benjamin Gruenbaum) [#4903](https://github.com/nodejs/node/pull/4903)
* [[`3abf9d5df5`](https://github.com/nodejs/node/commit/3abf9d5df5)] - **(SEMVER-MAJOR)** **doc, tls**: deprecate createSecurePair (Yuval Brik) [#6063](https://github.com/nodejs/node/pull/6063)
* [[`90204cc468`](https://github.com/nodejs/node/commit/90204cc468)] - **(SEMVER-MAJOR)** **domains**: clear stack when no error handler (Julien Gilli) [#4659](https://github.com/nodejs/node/pull/4659)
* [[`c3ad97d3cc`](https://github.com/nodejs/node/commit/c3ad97d3cc)] - **(SEMVER-MAJOR)** **events**: don't inherit from Object.prototype (Brian White) [#6092](https://github.com/nodejs/node/pull/6092)
* [[`a2492f989f`](https://github.com/nodejs/node/commit/a2492f989f)] - **(SEMVER-MAJOR)** **fs**: optimize realpath using uv_fs_realpath() (Yuval Brik) [#3594](https://github.com/nodejs/node/pull/3594)
* [[`53a95a5b12`](https://github.com/nodejs/node/commit/53a95a5b12)] - **(SEMVER-MAJOR)** **fs**: make fs.watch error message more useful (James M Snell) [#5616](https://github.com/nodejs/node/pull/5616)
* [[`060e5f0c00`](https://github.com/nodejs/node/commit/060e5f0c00)] - **(SEMVER-MAJOR)** **fs**: Buffer and encoding enhancements to fs API (James M Snell) [#5616](https://github.com/nodejs/node/pull/5616)
* [[`8bb60e3c8d`](https://github.com/nodejs/node/commit/8bb60e3c8d)] - **(SEMVER-MAJOR)** **fs**: improve error message for invalid flag (James M Snell) [#5590](https://github.com/nodejs/node/pull/5590)
* [[`1124de2d76`](https://github.com/nodejs/node/commit/1124de2d76)] - **(SEMVER-MAJOR)** **fs**: deprecate fs.read's string interface (Sakthipriyan Vairamani) [#4525](https://github.com/nodejs/node/pull/4525)
* [[`2b15e68bbe`](https://github.com/nodejs/node/commit/2b15e68bbe)] - **(SEMVER-MAJOR)** **fs**: fs.read into zero buffer should not throw exception (Feross Aboukhadijeh) [#4518](https://github.com/nodejs/node/pull/4518)
* [[`8b97249893`](https://github.com/nodejs/node/commit/8b97249893)] - **(SEMVER-MAJOR)** **fs**: fix the error report on fs.link(Sync) (yorkie) [#3917](https://github.com/nodejs/node/pull/3917)
* [[`5f76b24e5e`](https://github.com/nodejs/node/commit/5f76b24e5e)] - **(SEMVER-MAJOR)** **http**: overridable `clientError` (Fedor Indutny) [#4557](https://github.com/nodejs/node/pull/4557)
* [[`d01eb6882f`](https://github.com/nodejs/node/commit/d01eb6882f)] - **(SEMVER-MAJOR)** **lib**: add 'pid' prefix in `internal/util` (Minwoo Jung) [#3878](https://github.com/nodejs/node/pull/3878)
* [[`20285ad177`](https://github.com/nodejs/node/commit/20285ad177)] - **(SEMVER-MAJOR)** **lib**: Consistent error messages in all modules (micnic) [#3374](https://github.com/nodejs/node/pull/3374)
* [[`94b9948d63`](https://github.com/nodejs/node/commit/94b9948d63)] - **(SEMVER-MAJOR)** **lib,src**: ensure '(node:pid)' prefix for stdout logging (Minwoo Jung) [#3833](https://github.com/nodejs/node/pull/3833)
* [[`b70dc67828`](https://github.com/nodejs/node/commit/b70dc67828)] - **(SEMVER-MAJOR)** **lib,test**: remove publicly exposed freelist (cjihrig) [#3738](https://github.com/nodejs/node/pull/3738)
* [[`c43b182b11`](https://github.com/nodejs/node/commit/c43b182b11)] - **(SEMVER-MAJOR)** **module**: preserve symlinks when requiring (Alex Lamar) [#5950](https://github.com/nodejs/node/pull/5950)
* [[`d38503ab01`](https://github.com/nodejs/node/commit/d38503ab01)] - **(SEMVER-MAJOR)** **module**: prioritize current dir for local lookups (Phillip Johnsen) [#5689](https://github.com/nodejs/node/pull/5689)
* [[`71470a8e45`](https://github.com/nodejs/node/commit/71470a8e45)] - **(SEMVER-MAJOR)** **module**: pass v8::Object to linked module initialization function (Phillip Kovalev) [#4771](https://github.com/nodejs/node/pull/4771)
* [[`18490d3d5a`](https://github.com/nodejs/node/commit/18490d3d5a)] - **(SEMVER-MAJOR)** **module**: always decorate thrown errors (Brian White) [#4287](https://github.com/nodejs/node/pull/4287)
* [[`c32c889c45`](https://github.com/nodejs/node/commit/c32c889c45)] - **(SEMVER-MAJOR)** **net**: Validate port in createServer().listen() (Dirceu Pereira Tiegs) [#5732](https://github.com/nodejs/node/pull/5732)
* [[`b85a50b6da`](https://github.com/nodejs/node/commit/b85a50b6da)] - **(SEMVER-MAJOR)** **net**: remove implicit setting of DNS hints (cjihrig) [#6021](https://github.com/nodejs/node/pull/6021)
* [[`ec49fc8229`](https://github.com/nodejs/node/commit/ec49fc8229)] - **(SEMVER-MAJOR)** **net**: improve socket.write() error message (Phillip Johnsen) [#5981](https://github.com/nodejs/node/pull/5981)
* [[`d0edabecbf`](https://github.com/nodejs/node/commit/d0edabecbf)] - **(SEMVER-MAJOR)** **net**: strict checking for internal/net isLegalPort (James M Snell) [#5733](https://github.com/nodejs/node/pull/5733)
* [[`a78b3344f8`](https://github.com/nodejs/node/commit/a78b3344f8)] - **(SEMVER-MAJOR)** **net**: type check createServer options object (Sam Roberts) [#2904](https://github.com/nodejs/node/pull/2904)
* [[`25751bedfe`](https://github.com/nodejs/node/commit/25751bedfe)] - **(SEMVER-MAJOR)** **node**: deprecate process.EventEmitter (Evan Lucas) [#5049](https://github.com/nodejs/node/pull/5049)
* [[`08085c49b6`](https://github.com/nodejs/node/commit/08085c49b6)] - **(SEMVER-MAJOR)** **path**: assert inputs are strings (Brian White) [#5348](https://github.com/nodejs/node/pull/5348)
* [[`d1000b4137`](https://github.com/nodejs/node/commit/d1000b4137)] - **(SEMVER-MAJOR)** **path**: make format() consistent and more functional (Nathan Woltman) [#2408](https://github.com/nodejs/node/pull/2408)
* [[`c6656db352`](https://github.com/nodejs/node/commit/c6656db352)] - **(SEMVER-MAJOR)** **process**: add 'warning' event and process.emitWarning() (James M Snell) [#4782](https://github.com/nodejs/node/pull/4782)
* [[`72e3dd9f43`](https://github.com/nodejs/node/commit/72e3dd9f43)] - **(SEMVER-MAJOR)** **process**: throw on non-function to nextTick() (yorkie) [#3860](https://github.com/nodejs/node/pull/3860)
* [[`33c286154b`](https://github.com/nodejs/node/commit/33c286154b)] - **(SEMVER-MAJOR)** **querystring**: don't inherit from Object.prototype (Brian White) [#6055](https://github.com/nodejs/node/pull/6055)
* [[`5c8b5971eb`](https://github.com/nodejs/node/commit/5c8b5971eb)] - **(SEMVER-MAJOR)** **querystring**: using toString for objects on querystring.escape (Igor Kalashnikov) [#5341](https://github.com/nodejs/node/pull/5341)
* [[`9c6ef5b3e1`](https://github.com/nodejs/node/commit/9c6ef5b3e1)] - **(SEMVER-MAJOR)** **readline**: allow history to be disabled (surya panikkal) [#6352](https://github.com/nodejs/node/pull/6352)
* [[`0a62f929da`](https://github.com/nodejs/node/commit/0a62f929da)] - **(SEMVER-MAJOR)** **readline**: emit key info unconditionally (cjihrig) [#6024](https://github.com/nodejs/node/pull/6024)
* [[`3de9bc9429`](https://github.com/nodejs/node/commit/3de9bc9429)] - **(SEMVER-MAJOR)** **readline**: document emitKeypressEvents() (cjihrig) [#6024](https://github.com/nodejs/node/pull/6024)
* [[`ca2e8b292f`](https://github.com/nodejs/node/commit/ca2e8b292f)] - **(SEMVER-MAJOR)** **readline**: deprecate undocumented exports (cjihrig) [#3862](https://github.com/nodejs/node/pull/3862)
* [[`bd2cee1b10`](https://github.com/nodejs/node/commit/bd2cee1b10)] - **(SEMVER-MAJOR)** **repl**: don’t complete expressions when eval fails (Anna Henningsen) [#6328](https://github.com/nodejs/node/pull/6328)
* [[`ad8257fa5b`](https://github.com/nodejs/node/commit/ad8257fa5b)] - **(SEMVER-MAJOR)** **repl**: Assignment of _ allowed with warning (Lance Ball) [#5535](https://github.com/nodejs/node/pull/5535)
* [[`757fbac64b`](https://github.com/nodejs/node/commit/757fbac64b)] - **(SEMVER-MAJOR)** **src**: remove deprecated internal functions (Ben Noordhuis) [#6053](https://github.com/nodejs/node/pull/6053)
* [[`57003520f8`](https://github.com/nodejs/node/commit/57003520f8)] - **(SEMVER-MAJOR)** **src**: attach error to stack on displayErrors (cjihrig) [#4874](https://github.com/nodejs/node/pull/4874)
* [[`4e46931406`](https://github.com/nodejs/node/commit/4e46931406)] - **(SEMVER-MAJOR)** **src**: deprecate undocumented variables (Jackson Tian) [#1838](https://github.com/nodejs/node/pull/1838)
* [[`c0abecdaf3`](https://github.com/nodejs/node/commit/c0abecdaf3)] - **(SEMVER-MAJOR)** **stream**: make null an invalid chunk to write in object mode (Calvin Metcalf) [#6170](https://github.com/nodejs/node/pull/6170)
* [[`652782d137`](https://github.com/nodejs/node/commit/652782d137)] - **(SEMVER-MAJOR)** **test**: update test-repl-require for local paths (Myles Borins) [#5689](https://github.com/nodejs/node/pull/5689)
* [[`a5aa7c1713`](https://github.com/nodejs/node/commit/a5aa7c1713)] - **(SEMVER-MAJOR)** **test**: expand test case for unknown file open flags (James M Snell) [#5590](https://github.com/nodejs/node/pull/5590)
* [[`2c33819370`](https://github.com/nodejs/node/commit/2c33819370)] - **(SEMVER-MAJOR)** **test**: fix tests that check error messages (cjihrig) [#3727](https://github.com/nodejs/node/pull/3727)
* [[`ac153bd2a6`](https://github.com/nodejs/node/commit/ac153bd2a6)] - **(SEMVER-MAJOR)** **timers**: fail early when callback is not a function (Anna Henningsen) [#4362](https://github.com/nodejs/node/pull/4362)
* [[`1ab6b21360`](https://github.com/nodejs/node/commit/1ab6b21360)] - **(SEMVER-MAJOR)** **tls**: rename `clientError` to `tlsClientError` (Fedor Indutny) [#4557](https://github.com/nodejs/node/pull/4557)
* [[`df268f97bc`](https://github.com/nodejs/node/commit/df268f97bc)] - **(SEMVER-MAJOR)** **tls**: use SHA1 for sessionIdContext (Stefan Budeanu) [#3866](https://github.com/nodejs/node/pull/3866)
* [[`102a77655a`](https://github.com/nodejs/node/commit/102a77655a)] - **(SEMVER-MAJOR)** **tools**: do not rewrite npm shebang in install.py (Evan Lucas) [#6098](https://github.com/nodejs/node/pull/6098)
* [[`a2c0aa84e0`](https://github.com/nodejs/node/commit/a2c0aa84e0)] - **(SEMVER-MAJOR)** **tty**: Remove deprecated setRawMode wrapper (Wyatt Preul) [#2528](https://github.com/nodejs/node/pull/2528)
* [[`1b50b80e06`](https://github.com/nodejs/node/commit/1b50b80e06)] - **(SEMVER-MAJOR)** **url**: drop auth in `url.resolve()` if host changes (Alex Kocharin) [#1480](https://github.com/nodejs/node/pull/1480)
* [[`e2f47f5698`](https://github.com/nodejs/node/commit/e2f47f5698)] - **(SEMVER-MAJOR)** **util**: Change how Error objects are formatted (Mudit Ameta) [#4582](https://github.com/nodejs/node/pull/4582)
* [[`93d6b5fb68`](https://github.com/nodejs/node/commit/93d6b5fb68)] - **(SEMVER-MAJOR)** **util**: use consistent Dates in inspect() (Xotic750) [#4318](https://github.com/nodejs/node/pull/4318)
* [[`24012a879d`](https://github.com/nodejs/node/commit/24012a879d)] - **(SEMVER-MAJOR)** **util**: make inspect more reliable (Evan Lucas) [#4098](https://github.com/nodejs/node/pull/4098)
* [[`007cfea308`](https://github.com/nodejs/node/commit/007cfea308)] - **(SEMVER-MAJOR)** **util**: remove pump (Wyatt Preul) [#2531](https://github.com/nodejs/node/pull/2531)
* [[`4cf19ad1bb`](https://github.com/nodejs/node/commit/4cf19ad1bb)] - **(SEMVER-MAJOR)** **util**: Remove exec, has been deprecated for years (Wyatt Preul) [#2530](https://github.com/nodejs/node/pull/2530)
* [[`34a35919e1`](https://github.com/nodejs/node/commit/34a35919e1)] - **(SEMVER-MAJOR)** **util**: improve typed array formatting (Ben Noordhuis) [#3793](https://github.com/nodejs/node/pull/3793)
* [[`1cf26c036d`](https://github.com/nodejs/node/commit/1cf26c036d)] - **(SEMVER-MAJOR)** **win**: prevent node from running on Windows Vista or earlier (Alexis Campailla) [#5167](https://github.com/nodejs/node/pull/5167)
* [[`55db19074d`](https://github.com/nodejs/node/commit/55db19074d)] - **(SEMVER-MAJOR)** **win,msi**: prevent from installing on Windows Vista or earlier (Alexis Campailla) [#5167](https://github.com/nodejs/node/pull/5167)
* [[`54a5287e3e`](https://github.com/nodejs/node/commit/54a5287e3e)] - **(SEMVER-MAJOR)** **zlib**: fix gzip member head/buffer boundary issue (Anna Henningsen) [#5883](https://github.com/nodejs/node/pull/5883)
* [[`8b43d3f52d`](https://github.com/nodejs/node/commit/8b43d3f52d)] - **(SEMVER-MAJOR)** **zlib**: do not emit event on *Sync() methods (Rich Trott) [#5707](https://github.com/nodejs/node/pull/5707)


Semver-minor and patch commits since v5.11.0

* [[`7d4f652ced`](https://github.com/nodejs/node/commit/7d4f652ced)] - **benchmarks**: fix lint error (Myles Borins) [#6314](https://github.com/nodejs/node/pull/6314)
* [[`451f4fc88e`](https://github.com/nodejs/node/commit/451f4fc88e)] - **benchmarks**: add microbenchmarks for new ES6 features (James M Snell) [#6222](https://github.com/nodejs/node/pull/6222)
* [[`962c1e1cac`](https://github.com/nodejs/node/commit/962c1e1cac)] - **benchmarks**: add common.v8ForceOptimization (James M Snell) [#6222](https://github.com/nodejs/node/pull/6222)
* [[`d866438ce1`](https://github.com/nodejs/node/commit/d866438ce1)] - **buffer**: safeguard against accidental kNoZeroFill (Сковорода Никита Андреевич) [nodejs/node-private#30](https://github.com/nodejs/node-private/pull/30)
* [[`3350b4712b`](https://github.com/nodejs/node/commit/3350b4712b)] - **(SEMVER-MINOR)** **buffer**: add Buffer.prototype.lastIndexOf() (dcposch@dcpos.ch) [#4846](https://github.com/nodejs/node/pull/4846)
* [[`afd821a91d`](https://github.com/nodejs/node/commit/afd821a91d)] - **buffer**: faster case for create buffer from empty string (Jackson Tian) [#4414](https://github.com/nodejs/node/pull/4414)
* [[`73fc440870`](https://github.com/nodejs/node/commit/73fc440870)] - **buffer**: fix buffer alignment restriction (Ali Ijaz Sheikh) [#5752](https://github.com/nodejs/node/pull/5752)
* [[`206a81fb14`](https://github.com/nodejs/node/commit/206a81fb14)] - **buffer**: avoid undefined behaviour (Ali Ijaz Sheikh) [#5494](https://github.com/nodejs/node/pull/5494)
* [[`ebbbc5a790`](https://github.com/nodejs/node/commit/ebbbc5a790)] - **buffer**: replace deprecated SetWeak usage (Ali Ijaz Sheikh) [#5204](https://github.com/nodejs/node/pull/5204)
* [[`34aac23d0b`](https://github.com/nodejs/node/commit/34aac23d0b)] - **buffer**: cleanup CallbackInfo (Ali Ijaz Sheikh) [#5204](https://github.com/nodejs/node/pull/5204)
* [[`d7259280ee`](https://github.com/nodejs/node/commit/d7259280ee)] - **build**: update android-configure script for npm (Robert Chiras) [#6349](https://github.com/nodejs/node/pull/6349)
* [[`8cf94006c9`](https://github.com/nodejs/node/commit/8cf94006c9)] - **build**: do not ignore errors during build-addons (Ali Ijaz Sheikh) [#5494](https://github.com/nodejs/node/pull/5494)
* [[`704b68aeb9`](https://github.com/nodejs/node/commit/704b68aeb9)] - **cares**: Support malloc(0) scenarios for AIX (Gireesh Punathil) [#6305](https://github.com/nodejs/node/pull/6305)
* [[`77e2efec8c`](https://github.com/nodejs/node/commit/77e2efec8c)] - **console**: refactor to use rest params and template strings (James M Snell) [#6233](https://github.com/nodejs/node/pull/6233)
* [[`a53b2ac4b1`](https://github.com/nodejs/node/commit/a53b2ac4b1)] - **contextify**: tie lifetimes of context & sandbox (Ali Ijaz Sheikh) [#5786](https://github.com/nodejs/node/pull/5786)
* [[`d6237aa7c6`](https://github.com/nodejs/node/commit/d6237aa7c6)] - **(SEMVER-MINOR)** **crypto**: Read OpenSSL config before init (Stefan Budeanu) [#6374](https://github.com/nodejs/node/pull/6374)
* [[`c1729b0c38`](https://github.com/nodejs/node/commit/c1729b0c38)] - **deps**: upgrade to V8 5.0.71.35 (Ali Ijaz Sheikh) [#6372](https://github.com/nodejs/node/pull/6372)
* [[`7337693c33`](https://github.com/nodejs/node/commit/7337693c33)] - **deps**: upgrade to V8 5.0.71.34 (Ali Ijaz Sheikh) [#6320](https://github.com/nodejs/node/pull/6320)
* [[`809ed52147`](https://github.com/nodejs/node/commit/809ed52147)] - **deps**: upgrade to V8 5.0.71.33 (Ali Ijaz Sheikh) [#6290](https://github.com/nodejs/node/pull/6290)
* [[`19867cedfe`](https://github.com/nodejs/node/commit/19867cedfe)] - **deps**: exclude tests on ppc for v8 5.0 (Michael Dawson) [#6267](https://github.com/nodejs/node/pull/6267)
* [[`450418286a`](https://github.com/nodejs/node/commit/450418286a)] - **deps**: cherry-pick 1ef7487b from v8 upstream (Michael Dawson) [#6218](https://github.com/nodejs/node/pull/6218)
* [[`c3cec1eefc`](https://github.com/nodejs/node/commit/c3cec1eefc)] - **deps**: upgrade libuv to 1.9.0 (Saúl Ibarra Corretgé) [#5994](https://github.com/nodejs/node/pull/5994)
* [[`3a8e8230ee`](https://github.com/nodejs/node/commit/3a8e8230ee)] - **deps**: upgrade to V8 4.9.385.35 (Ben Noordhuis) [#6077](https://github.com/nodejs/node/pull/6077)
* [[`f8e8075a62`](https://github.com/nodejs/node/commit/f8e8075a62)] - **deps**: upgrade to V8 4.9.385.27 (Ali Ijaz Sheikh) [#5494](https://github.com/nodejs/node/pull/5494)
* [[`079973b96a`](https://github.com/nodejs/node/commit/079973b96a)] - **deps**: add V8 DEP trace_event (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`89f234300a`](https://github.com/nodejs/node/commit/89f234300a)] - **deps**: edit v8 gitignore to allow trace_event copy (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`069e02ab47`](https://github.com/nodejs/node/commit/069e02ab47)] - **deps**: upgrade to V8 4.9.385.18 (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`674e5131ed`](https://github.com/nodejs/node/commit/674e5131ed)] - **deps**: backport bc2e393 from v8 upstream (evan.lucas) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`113d1f3821`](https://github.com/nodejs/node/commit/113d1f3821)] - **deps**: cherry-pick 68e89fb from v8's upstream (Fedor Indutny) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`ef4170ea03`](https://github.com/nodejs/node/commit/ef4170ea03)] - **deps**: upgrade to V8 4.8.271.17 (Ali Ijaz Sheikh) [#4785](https://github.com/nodejs/node/pull/4785)
* [[`384b20362c`](https://github.com/nodejs/node/commit/384b20362c)] - **deps**: upgrade to V8 4.7.80.32 (Ali Ijaz Sheikh) [#4699](https://github.com/nodejs/node/pull/4699)
* [[`36ac3d642e`](https://github.com/nodejs/node/commit/36ac3d642e)] - **deps**: backport 8d6a228 from the v8's upstream (Fedor Indutny) [#4259](https://github.com/nodejs/node/pull/4259)
* [[`e2dec98837`](https://github.com/nodejs/node/commit/e2dec98837)] - **deps**: upgrade to V8 4.7.80.25 (Ali Ijaz Sheikh) [#4160](https://github.com/nodejs/node/pull/4160)
* [[`1e324d883e`](https://github.com/nodejs/node/commit/1e324d883e)] - **deps**: backport bc2e393 from v8 upstream (evan.lucas) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`d9d050d396`](https://github.com/nodejs/node/commit/d9d050d396)] - **deps**: cherry-pick 68e89fb from v8's upstream (Fedor Indutny) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`edfc8cde04`](https://github.com/nodejs/node/commit/edfc8cde04)] - **deps**: backport 1ee712a from V8 upstream (Julien Gilli) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`8a43a3d761`](https://github.com/nodejs/node/commit/8a43a3d761)] - **deps**: upgrade V8 to 4.7.80.24 (Ali Ijaz Sheikh) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`61be0004a6`](https://github.com/nodejs/node/commit/61be0004a6)] - **doc**: fix position of `fs.readSync()` (Jeremiah Senkpiel) [#6399](https://github.com/nodejs/node/pull/6399)
* [[`27ce7fe953`](https://github.com/nodejs/node/commit/27ce7fe953)] - **doc**: change references to Stable to Current (Myles Borins) [#6318](https://github.com/nodejs/node/pull/6318)
* [[`547a6fecb9`](https://github.com/nodejs/node/commit/547a6fecb9)] - **doc**: update authors (James M Snell) [#6373](https://github.com/nodejs/node/pull/6373)
* [[`af8dd44d43`](https://github.com/nodejs/node/commit/af8dd44d43)] - **doc**: add JacksonTian to collaborators (Jackson Tian) [#6388](https://github.com/nodejs/node/pull/6388)
* [[`09f7b8a818`](https://github.com/nodejs/node/commit/09f7b8a818)] - **doc**: add Minqi Pan to collaborators (Minqi Pan) [#6387](https://github.com/nodejs/node/pull/6387)
* [[`dfc7ffd24d`](https://github.com/nodejs/node/commit/dfc7ffd24d)] - **doc**: add eljefedelrodeodeljefe to collaborators (Robert Jefe Lindstaedt) [#6389](https://github.com/nodejs/node/pull/6389)
* [[`e45dbd3089`](https://github.com/nodejs/node/commit/e45dbd3089)] - **doc**: add ronkorving to collaborators (ronkorving) [#6385](https://github.com/nodejs/node/pull/6385)
* [[`96f6af7b26`](https://github.com/nodejs/node/commit/96f6af7b26)] - **doc**: add estliberitas to collaborators (Alexander Makarenko) [#6386](https://github.com/nodejs/node/pull/6386)
* [[`8def5fa4e7`](https://github.com/nodejs/node/commit/8def5fa4e7)] - **doc**: fix broken references (Alexander Gromnitsky) [#6350](https://github.com/nodejs/node/pull/6350)
* [[`11d71da558`](https://github.com/nodejs/node/commit/11d71da558)] - **doc**: add note for platform specific flags `fs.open()` (Robert Jefe Lindstaedt) [#6136](https://github.com/nodejs/node/pull/6136)
* [[`e6d4f3befb`](https://github.com/nodejs/node/commit/e6d4f3befb)] - **doc**: improvements to child_process, process docs (Alexander Makarenko)
* [[`7c03595b2a`](https://github.com/nodejs/node/commit/7c03595b2a)] - **doc**: fix a typo in the CONTRIBUTING.md (vsemozhetbyt) [#6343](https://github.com/nodejs/node/pull/6343)
* [[`557c309447`](https://github.com/nodejs/node/commit/557c309447)] - **doc**: add vm example, be able to require modules (Robert Jefe Lindstaedt) [#5323](https://github.com/nodejs/node/pull/5323)
* [[`61a214e957`](https://github.com/nodejs/node/commit/61a214e957)] - **doc**: note that process.config can and will be changed (James M Snell) [#6266](https://github.com/nodejs/node/pull/6266)
* [[`09ec1cc1c7`](https://github.com/nodejs/node/commit/09ec1cc1c7)] - **doc**: path.resolve ignores zero-length strings (Igor Klopov) [#5928](https://github.com/nodejs/node/pull/5928)
* [[`b743d82ffc`](https://github.com/nodejs/node/commit/b743d82ffc)] - **doc**: improve github templates by using comments (Johan Bergström) [#5710](https://github.com/nodejs/node/pull/5710)
* [[`d939152230`](https://github.com/nodejs/node/commit/d939152230)] - **doc**: fix typo in fs writeSync param list (firedfox) [#5984](https://github.com/nodejs/node/pull/5984)
* [[`4039ef1a58`](https://github.com/nodejs/node/commit/4039ef1a58)] - **doc**: remove redundant parameter comments from fs (firedfox) [#5952](https://github.com/nodejs/node/pull/5952)
* [[`9b6440a86c`](https://github.com/nodejs/node/commit/9b6440a86c)] - **doc**: add simple http clientError example (James M Snell) [#5248](https://github.com/nodejs/node/pull/5248)
* [[`de91e9a8a7`](https://github.com/nodejs/node/commit/de91e9a8a7)] - **doc**: fix v4.3.0 changelog commit entries (James M Snell) [#5164](https://github.com/nodejs/node/pull/5164)
* [[`e31bda81dd`](https://github.com/nodejs/node/commit/e31bda81dd)] - **doc**: add CTC meeting minutes 2016-01-20 (Rod Vagg) [#4904](https://github.com/nodejs/node/pull/4904)
* [[`ce11a37c4a`](https://github.com/nodejs/node/commit/ce11a37c4a)] - **doc**: add CTC meeting minutes 2016-01-27 (Rod Vagg) [#5057](https://github.com/nodejs/node/pull/5057)
* [[`3c70dc83ed`](https://github.com/nodejs/node/commit/3c70dc83ed)] - **doc**: add CTC meeting minutes 2016-01-06 (Rod Vagg) [#4667](https://github.com/nodejs/node/pull/4667)
* [[`55326f5488`](https://github.com/nodejs/node/commit/55326f5488)] - **doc**: add CTC meeting minutes 2015-12-16 (Rod Vagg) [#4666](https://github.com/nodejs/node/pull/4666)
* [[`1a346840bf`](https://github.com/nodejs/node/commit/1a346840bf)] - **doc**: add CTC meeting minutes 2015-12-02 (Rod Vagg) [#4665](https://github.com/nodejs/node/pull/4665)
* [[`3ac5f8dcba`](https://github.com/nodejs/node/commit/3ac5f8dcba)] - **doc**: add CTC meeting minutes 2015-11-11 (Rod Vagg) [#4664](https://github.com/nodejs/node/pull/4664)
* [[`db39625a44`](https://github.com/nodejs/node/commit/db39625a44)] - **doc**: add CTC meeting minutes 2015-11-04 (Rod Vagg) [#4663](https://github.com/nodejs/node/pull/4663)
* [[`aa53bf2faa`](https://github.com/nodejs/node/commit/aa53bf2faa)] - **(SEMVER-MINOR)** **events**: add prependListener() and prependOnceListener() (James M Snell) [#6032](https://github.com/nodejs/node/pull/6032)
* [[`c9628c55fa`](https://github.com/nodejs/node/commit/c9628c55fa)] - **events**: make eventNames() use Reflect.ownKeys() (Luigi Pinca) [#5822](https://github.com/nodejs/node/pull/5822)
* [[`b6e3fa745b`](https://github.com/nodejs/node/commit/b6e3fa745b)] - **(SEMVER-MINOR)** **events**: add eventNames() method (James M Snell) [#5617](https://github.com/nodejs/node/pull/5617)
* [[`1d79787e2e`](https://github.com/nodejs/node/commit/1d79787e2e)] - **fs**: add a temporary fix for re-evaluation support (Сковорода Никита Андреевич) [#5102](https://github.com/nodejs/node/pull/5102)
* [[`3441a4178b`](https://github.com/nodejs/node/commit/3441a4178b)] - **fs**: revert "change statSync to accessSync..." (Rich Trott) [#4679](https://github.com/nodejs/node/pull/4679)
* [[`809bf5e38c`](https://github.com/nodejs/node/commit/809bf5e38c)] - **fs**: change statSync to accessSync in realpathSync (Ben Noordhuis) [#4575](https://github.com/nodejs/node/pull/4575)
* [[`4bd8b20f20`](https://github.com/nodejs/node/commit/4bd8b20f20)] - **github**: put description of PR to the end of tmpl (Fedor Indutny) [#5378](https://github.com/nodejs/node/pull/5378)
* [[`81e35b5a29`](https://github.com/nodejs/node/commit/81e35b5a29)] - **github**: add issue and pull request templates (Fedor Indutny) [#5291](https://github.com/nodejs/node/pull/5291)
* [[`7cbd9fe39a`](https://github.com/nodejs/node/commit/7cbd9fe39a)] - **gyp**: inherit parent for `*.host` (Johan Bergström) [#6173](https://github.com/nodejs/node/pull/6173)
* [[`c29b3c1808`](https://github.com/nodejs/node/commit/c29b3c1808)] - **lib**: improve module loading performance (Brian White) [#5172](https://github.com/nodejs/node/pull/5172)
* [[`953173b0d7`](https://github.com/nodejs/node/commit/953173b0d7)] - **lib**: wrap tick_processor scripts in IIFE (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`f3d9de772c`](https://github.com/nodejs/node/commit/f3d9de772c)] - **module**: fix resolution of filename with trailing slash (Michaël Zasso) [#6215](https://github.com/nodejs/node/pull/6215)
* [[`5c14efbbe9`](https://github.com/nodejs/node/commit/5c14efbbe9)] - **module**: return correct exports from linked binding (Phillip Kovalev) [#5337](https://github.com/nodejs/node/pull/5337)
* [[`19f700859b`](https://github.com/nodejs/node/commit/19f700859b)] - **module**: revert "optimize js and json file i/o" (Rich Trott) [#4679](https://github.com/nodejs/node/pull/4679)
* [[`7c60328002`](https://github.com/nodejs/node/commit/7c60328002)] - **module**: optimize js and json file i/o (Ben Noordhuis) [#4575](https://github.com/nodejs/node/pull/4575)
* [[`aa395b78fe`](https://github.com/nodejs/node/commit/aa395b78fe)] - **net**: set ADDRCONFIG DNS hint in connections (cjihrig) [#6281](https://github.com/nodejs/node/pull/6281)
* [[`e052096ee0`](https://github.com/nodejs/node/commit/e052096ee0)] - **(SEMVER-MINOR)** **os**: add userInfo() method (cjihrig) [#6104](https://github.com/nodejs/node/pull/6104)
* [[`d18daaa4e7`](https://github.com/nodejs/node/commit/d18daaa4e7)] - **querystring**: fix comments (Brian White) [#6365](https://github.com/nodejs/node/pull/6365)
* [[`633268c6df`](https://github.com/nodejs/node/commit/633268c6df)] - **src**: fix check-imports.py linter errors (Sakthipriyan Vairamani) [#6105](https://github.com/nodejs/node/pull/6105)
* [[`b0b7f6a12b`](https://github.com/nodejs/node/commit/b0b7f6a12b)] - **src**: squelch -Wunused-variable in non-icu builds (Ben Noordhuis) [#6351](https://github.com/nodejs/node/pull/6351)
* [[`b6a41d6120`](https://github.com/nodejs/node/commit/b6a41d6120)] - **src**: fix out-of-bounds write in TwoByteValue (Anna Henningsen) [#6330](https://github.com/nodejs/node/pull/6330)
* [[`86ff8447f3`](https://github.com/nodejs/node/commit/86ff8447f3)] - **src**: add intl and icu configs to process.binding('config') (James M Snell) [#6266](https://github.com/nodejs/node/pull/6266)
* [[`5b7a011e1b`](https://github.com/nodejs/node/commit/5b7a011e1b)] - **src**: add process.binding('config') (James M Snell) [#6266](https://github.com/nodejs/node/pull/6266)
* [[`f286e1e1ad`](https://github.com/nodejs/node/commit/f286e1e1ad)] - **src**: fix -Wunused-result warning in e38bade (Sakthipriyan Vairamani) [#6276](https://github.com/nodejs/node/pull/6276)
* [[`2a4998c5c0`](https://github.com/nodejs/node/commit/2a4998c5c0)] - **src**: don't set non-primitive values on templates (Ben Noordhuis) [#6228](https://github.com/nodejs/node/pull/6228)
* [[`00c876dec5`](https://github.com/nodejs/node/commit/00c876dec5)] - **src**: fix ARRAY_SIZE() logic error (Ben Noordhuis) [#5969](https://github.com/nodejs/node/pull/5969)
* [[`ad0ce57432`](https://github.com/nodejs/node/commit/ad0ce57432)] - **src**: fix NewInstance deprecation warnings (Michaël Zasso) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`67b5a8a936`](https://github.com/nodejs/node/commit/67b5a8a936)] - **src**: replace usage of deprecated ForceSet (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`536cfecd71`](https://github.com/nodejs/node/commit/536cfecd71)] - **src**: replace deprecated SetWeak in object_wrap (Ali Ijaz Sheikh) [#5494](https://github.com/nodejs/node/pull/5494)
* [[`492fbfbff8`](https://github.com/nodejs/node/commit/492fbfbff8)] - **src**: update uses of deprecated NewExternal (Ali Ijaz Sheikh) [#5462](https://github.com/nodejs/node/pull/5462)
* [[`a45e1f9f98`](https://github.com/nodejs/node/commit/a45e1f9f98)] - **src**: fix deprecated SetWeak usage in base-object (Ali Ijaz Sheikh) [#5204](https://github.com/nodejs/node/pull/5204)
* [[`c1649a7e4e`](https://github.com/nodejs/node/commit/c1649a7e4e)] - **src**: replace usage of deprecated SetAccessor (Ali Ijaz Sheikh) [#5204](https://github.com/nodejs/node/pull/5204)
* [[`cddd5347f6`](https://github.com/nodejs/node/commit/cddd5347f6)] - **src**: replace deprecated ProcessDebugMessages (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`d515a3f4b4`](https://github.com/nodejs/node/commit/d515a3f4b4)] - **src**: replace usage of deprecated GetDebugContext (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`4417f99432`](https://github.com/nodejs/node/commit/4417f99432)] - **src**: replace usage of deprecated SetMessageHandler (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`8894f2745f`](https://github.com/nodejs/node/commit/8894f2745f)] - **src**: replace deprecated CancelTerminateExecution (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`f50ef1a46f`](https://github.com/nodejs/node/commit/f50ef1a46f)] - **src**: replace deprecated TerminateExecution (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`ac32340997`](https://github.com/nodejs/node/commit/ac32340997)] - **src**: replace usage of deprecated HasOwnProperty (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`a729208808`](https://github.com/nodejs/node/commit/a729208808)] - **src**: replace usage of deprecated Has (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`3d7fd9aa22`](https://github.com/nodejs/node/commit/3d7fd9aa22)] - **src**: replace usage of deprecated GetEndColumn (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`d45045f318`](https://github.com/nodejs/node/commit/d45045f318)] - **src**: replace usage of deprecated Delete (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`16b0a8c1ac`](https://github.com/nodejs/node/commit/16b0a8c1ac)] - **src**: replace usage of deprecated CompileUnbound (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`023c317173`](https://github.com/nodejs/node/commit/023c317173)] - **src**: replace usage of deprecated Compile (Michaël Zasso) [#5159](https://github.com/nodejs/node/pull/5159)
* [[`79d7475604`](https://github.com/nodejs/node/commit/79d7475604)] - **src**: fix TryCatch deprecation warnings (Michaël Zasso) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`81f1732a05`](https://github.com/nodejs/node/commit/81f1732a05)] - **src**: replace deprecated String::NewFromOneByte (Michaël Zasso) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`924cc6c633`](https://github.com/nodejs/node/commit/924cc6c633)] - **src**: upgrade to new v8::Private api (Ben Noordhuis) [#5045](https://github.com/nodejs/node/pull/5045)
* [[`79dc1d7635`](https://github.com/nodejs/node/commit/79dc1d7635)] - **src**: remove forwards for v8::GC*logueCallback (Ali Ijaz Sheikh) [#4381](https://github.com/nodejs/node/pull/4381)
* [[`89abe86808`](https://github.com/nodejs/node/commit/89abe86808)] - ***Revert*** "**stream**: emit 'pause' on nextTick" (Evan Lucas) [#5947](https://github.com/nodejs/node/pull/5947)
* [[`ace1009456`](https://github.com/nodejs/node/commit/ace1009456)] - **stream**: emit 'pause' on nextTick (Alexis Campailla) [#5776](https://github.com/nodejs/node/pull/5776)
* [[`eee9dc7e9d`](https://github.com/nodejs/node/commit/eee9dc7e9d)] - ***Revert*** "**stream**: add bytesRead property for readable" (cjihrig) [#4746](https://github.com/nodejs/node/pull/4746)
* [[`bfb2cd0bfd`](https://github.com/nodejs/node/commit/bfb2cd0bfd)] - **stream**: add bytesRead property for readable (Jackson Tian) [#4372](https://github.com/nodejs/node/pull/4372)
* [[`0ed85e35a9`](https://github.com/nodejs/node/commit/0ed85e35a9)] - **test**: increase the platform timeout for AIX (Michael Dawson) [#6342](https://github.com/nodejs/node/pull/6342)
* [[`591ebe7d82`](https://github.com/nodejs/node/commit/591ebe7d82)] - **test**: add tests for console.assert (Evan Lucas) [#6302](https://github.com/nodejs/node/pull/6302)
* [[`a9bab5fddb`](https://github.com/nodejs/node/commit/a9bab5fddb)] - **test**: v8-flags is sensitive to script caching (Ali Ijaz Sheikh) [#6316](https://github.com/nodejs/node/pull/6316)
* [[`c83204e2d9`](https://github.com/nodejs/node/commit/c83204e2d9)] - **test**: don't assume IPv6 in test-regress-GH-5727 (cjihrig) [#6319](https://github.com/nodejs/node/pull/6319)
* [[`6e57d2d7c4`](https://github.com/nodejs/node/commit/6e57d2d7c4)] - **test**: spawn new processes in vm-cached-data (Fedor Indutny) [#6280](https://github.com/nodejs/node/pull/6280)
* [[`3ebabc95aa`](https://github.com/nodejs/node/commit/3ebabc95aa)] - **test**: update error message for JSON.parse (Michaël Zasso) [#5945](https://github.com/nodejs/node/pull/5945)
* [[`b73e1b3c5a`](https://github.com/nodejs/node/commit/b73e1b3c5a)] - **test**: fix another flaky stringbytes test (Ali Ijaz Sheikh) [#6073](https://github.com/nodejs/node/pull/6073)
* [[`f4ebd5989a`](https://github.com/nodejs/node/commit/f4ebd5989a)] - **test**: fix flakiness of stringbytes-external (Ali Ijaz Sheikh) [#6039](https://github.com/nodejs/node/pull/6039)
* [[`be97db92af`](https://github.com/nodejs/node/commit/be97db92af)] - **test**: add buffer alignment regression tests (Anna Henningsen) [#5752](https://github.com/nodejs/node/pull/5752)
* [[`01f82f0685`](https://github.com/nodejs/node/commit/01f82f0685)] - **test**: fix proxy tab-completion test (Ali Ijaz Sheikh) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`9968941797`](https://github.com/nodejs/node/commit/9968941797)] - **test**: fix tests after V8 upgrade (Michaël Zasso) [#4722](https://github.com/nodejs/node/pull/4722)
* [[`db9e122182`](https://github.com/nodejs/node/commit/db9e122182)] - **test**: update ArrayBuffer alloc failure message (Ali Ijaz Sheikh) [#4785](https://github.com/nodejs/node/pull/4785)
* [[`dc09bbe3ee`](https://github.com/nodejs/node/commit/dc09bbe3ee)] - **test**: fix test-repl-tab-complete after V8 upgrade (Ali Ijaz Sheikh) [#4106](https://github.com/nodejs/node/pull/4106)
* [[`3a0eef7b99`](https://github.com/nodejs/node/commit/3a0eef7b99)] - **test,benchmark**: use deepStrictEqual() (Rich Trott) [#6213](https://github.com/nodejs/node/pull/6213)
* [[`e928edad65`](https://github.com/nodejs/node/commit/e928edad65)] - **tools**: rewrite check-install.sh in python (Sakthipriyan Vairamani) [#6105](https://github.com/nodejs/node/pull/6105)
* [[`96978768c9`](https://github.com/nodejs/node/commit/96978768c9)] - **tools**: enforce deepStrictEqual over deepEqual (Rich Trott) [#6213](https://github.com/nodejs/node/pull/6213)
* [[`9e30129fa7`](https://github.com/nodejs/node/commit/9e30129fa7)] - **(SEMVER-MINOR)** **tools**: add buffer-constructor eslint rule (James M Snell) [#5740](https://github.com/nodejs/node/pull/5740)
* [[`b6475b9a9d`](https://github.com/nodejs/node/commit/b6475b9a9d)] - ***Revert*** "**tty**: don't read from console stream upon creation" (Evan Lucas) [#5947](https://github.com/nodejs/node/pull/5947)
* [[`4611389294`](https://github.com/nodejs/node/commit/4611389294)] - **tty**: don't read from console stream upon creation (Alexis Campailla) [#5776](https://github.com/nodejs/node/pull/5776)
* [[`59737b29c7`](https://github.com/nodejs/node/commit/59737b29c7)] - **url**: use "empty" object for empty query strings (Brian White) [#6289](https://github.com/nodejs/node/pull/6289)
* [[`0b43c08f44`](https://github.com/nodejs/node/commit/0b43c08f44)] - **util**: pass on additional error() args (Brian White) [#4279](https://github.com/nodejs/node/pull/4279)
* [[`967a15541b`](https://github.com/nodejs/node/commit/967a15541b)] - **v8**: warn in Template::Set() on improper use (Ben Noordhuis) [#6277](https://github.com/nodejs/node/pull/6277)


## 2016-04-20, Version 5.11.0 (Stable), @thealphanerd

## Notable Changes

* **Buffer**:
  * `Buffer.prototype.compare` can now compare sub-ranges of two Buffers. (James M Snell) [#5880](https://github.com/nodejs/node/pull/5880)
* **deps**:
  * update to http-parser 2.7.0 (Fedor Indutny) [#6279](https://github.com/nodejs/node/pull/6279)
  * update ESLint to 2.7.0 (silverwind) [#6132](https://github.com/nodejs/node/pull/6132)
* **net**:
  * adds support for passing DNS lookup hints to `createConnection()` (Colin Ihrig) [#6000](https://github.com/nodejs/node/pull/6000)
* **node**:
  * Make the builtin libraries available for the `--eval` and `--print` CLI options (Anna Henningsen) [#6207](https://github.com/nodejs/node/pull/6207)
* **npm**:
  * upgrade npm to 3.8.6 (Kat Marchán) [#6153](https://github.com/nodejs/node/pull/6153)
* **repl**:
  * Pressing enter in the repl will repeat the last command by default if no input has been received. This behaviour was in node previously and was not removed intentionally. (Rich Trott) [#6090](https://github.com/nodejs/node/pull/6090)
* **src**:
  * add SIGINFO to supported signals (James Reggio) [#6093](https://github.com/nodejs/node/pull/6093)
* **streams**:
  * Fix a regression that caused by net streams requesting multiple chunks synchronously when combined with cork/uncork (Matteo Collina) [#6164](https://github.com/nodejs/node/pull/6164)
* **zlib**:
  * The flushing flag is now configurable allowing for decompression of partial data (Anna Henningsen) [#6069](https://github.com/nodejs/node/pull/6069)

### Commits

* [[`14fcb1dded`](https://github.com/nodejs/node/commit/14fcb1dded)] - **assert**: respect assert.doesNotThrow message. (Ilya Shaisultanov) [#2407](https://github.com/nodejs/node/pull/2407)
* [[`332f7382bb`](https://github.com/nodejs/node/commit/332f7382bb)] - **benchmark**: add module loader benchmark parameter (Brian White) [#5172](https://github.com/nodejs/node/pull/5172)
* [[`473f086a94`](https://github.com/nodejs/node/commit/473f086a94)] - **(SEMVER-MINOR)** **buffer**: add Buffer.prototype.compare by offset (James M Snell) [#5880](https://github.com/nodejs/node/pull/5880)
* [[`d44540f5af`](https://github.com/nodejs/node/commit/d44540f5af)] - **buffer**: standardize array index check (Trevor Norris) [#6084](https://github.com/nodejs/node/pull/6084)
* [[`bd12d72e0c`](https://github.com/nodejs/node/commit/bd12d72e0c)] - **build**: fix make tar-headers for Linux (Gibson Fahnestock) [#5978](https://github.com/nodejs/node/pull/5978)
* [[`3c8d404a82`](https://github.com/nodejs/node/commit/3c8d404a82)] - **build**: allow test-ci to run tests in parallel (Johan Bergström) [#6208](https://github.com/nodejs/node/pull/6208)
* [[`a5f8d0c6ef`](https://github.com/nodejs/node/commit/a5f8d0c6ef)] - **build**: remove -f{data,function}-sections flags (Ben Noordhuis) [#6077](https://github.com/nodejs/node/pull/6077)
* [[`adfb1a4bb0`](https://github.com/nodejs/node/commit/adfb1a4bb0)] - **child_process**: add nullptr checks after allocs (Anna Henningsen) [#6256](https://github.com/nodejs/node/pull/6256)
* [[`1fb40524ee`](https://github.com/nodejs/node/commit/1fb40524ee)] - **(SEMVER-MINOR)** **debugger**: run last command on presssing enter (Rich Trott) [#6090](https://github.com/nodejs/node/pull/6090)
* [[`5305831184`](https://github.com/nodejs/node/commit/5305831184)] - **deps**: update to http-parser 2.7.0 (Fedor Indutny) [#6279](https://github.com/nodejs/node/pull/6279)
* [[`8ae200c768`](https://github.com/nodejs/node/commit/8ae200c768)] - **deps**: floating fix for npm's test-node script (Kat Marchán) [#6153](https://github.com/nodejs/node/pull/6153)
* [[`e3e544eb96`](https://github.com/nodejs/node/commit/e3e544eb96)] - **deps**: upgrade npm to 3.8.6 (Kat Marchán) [#6153](https://github.com/nodejs/node/pull/6153)
* [[`a7104e4516`](https://github.com/nodejs/node/commit/a7104e4516)] - **deps**: cherry-pick 1383d00 from v8 upstream (Fedor Indutny) [#6179](https://github.com/nodejs/node/pull/6179)
* [[`632e6b9617`](https://github.com/nodejs/node/commit/632e6b9617)] - **deps**: backport 125ac66 from v8 upstream (Myles Borins) [#6086](https://github.com/nodejs/node/pull/6086)
* [[`4b8376986a`](https://github.com/nodejs/node/commit/4b8376986a)] - **doc**: git mv to .md (Robert Jefe Lindstaedt) [#4747](https://github.com/nodejs/node/pull/4747)
* [[`e6f4a175d4`](https://github.com/nodejs/node/commit/e6f4a175d4)] - **doc**: add full example for zlib.flush() (Anna Henningsen) [#6172](https://github.com/nodejs/node/pull/6172)
* [[`50f3f10ce6`](https://github.com/nodejs/node/commit/50f3f10ce6)] - **doc**: note that zlib.flush acts after pending writes (Anna Henningsen) [#6172](https://github.com/nodejs/node/pull/6172)
* [[`985685d170`](https://github.com/nodejs/node/commit/985685d170)] - **doc**: fix broken references (Alexander Gromnitsky) [#6100](https://github.com/nodejs/node/pull/6100)
* [[`d66d883a85`](https://github.com/nodejs/node/commit/d66d883a85)] - **doc**: path.format provide more examples (John Eversole) [#5838](https://github.com/nodejs/node/pull/5838)
* [[`dc1552e321`](https://github.com/nodejs/node/commit/dc1552e321)] - **doc**: replace functions with arrow functions (abouthiroppy) [#6203](https://github.com/nodejs/node/pull/6203)
* [[`fa04dfc307`](https://github.com/nodejs/node/commit/fa04dfc307)] - **doc**: DCO anchor that doesn't change (William Kapke) [#6257](https://github.com/nodejs/node/pull/6257)
* [[`b49a5b33b5`](https://github.com/nodejs/node/commit/b49a5b33b5)] - **doc**: fix http response event, Agent#getName (Matthew Douglass) [#5993](https://github.com/nodejs/node/pull/5993)
* [[`3b00d7a5b1`](https://github.com/nodejs/node/commit/3b00d7a5b1)] - **doc**: document intention and dangers of fs module Buffer API (Nikolai Vavilov) [#6020](https://github.com/nodejs/node/pull/6020)
* [[`3bc31526bb`](https://github.com/nodejs/node/commit/3bc31526bb)] - **doc**: explain differences in console.assert between node and browsers (James M Snell) [#6169](https://github.com/nodejs/node/pull/6169)
* [[`3f73502662`](https://github.com/nodejs/node/commit/3f73502662)] - **doc**: native module reloading is not supported (Bryan English) [#6168](https://github.com/nodejs/node/pull/6168)
* [[`5f9c8297f1`](https://github.com/nodejs/node/commit/5f9c8297f1)] - **doc**: clarify fs.watch() and inodes on linux, os x (Joran Dirk Greef) [#6099](https://github.com/nodejs/node/pull/6099)
* [[`f3c0b78ae4`](https://github.com/nodejs/node/commit/f3c0b78ae4)] - **doc**: add domain postmortem (Trevor Norris) [#6159](https://github.com/nodejs/node/pull/6159)
* [[`a91834e743`](https://github.com/nodejs/node/commit/a91834e743)] - **doc**: add stefanmb to collaborators (Stefan Budeanu) [#6227](https://github.com/nodejs/node/pull/6227)
* [[`117348d082`](https://github.com/nodejs/node/commit/117348d082)] - **doc**: add iWuzHere to collaborators (Imran Iqbal) [#6226](https://github.com/nodejs/node/pull/6226)
* [[`a1c46b63e8`](https://github.com/nodejs/node/commit/a1c46b63e8)] - **doc**: add santigimeno to collaborators (Santiago Gimeno) [#6225](https://github.com/nodejs/node/pull/6225)
* [[`976e4bb3da`](https://github.com/nodejs/node/commit/976e4bb3da)] - **doc**: add addaleax to collaborators (Anna Henningsen) [#6224](https://github.com/nodejs/node/pull/6224)
* [[`4fa949ef75`](https://github.com/nodejs/node/commit/4fa949ef75)] - **doc**: fix incorrect references in buffer docs (Amery) [#6194](https://github.com/nodejs/node/pull/6194)
* [[`b26fea1595`](https://github.com/nodejs/node/commit/b26fea1595)] - **doc**: add copy about how to curl SHA256.txt (Myles Borins) [#6120](https://github.com/nodejs/node/pull/6120)
* [[`daaad47099`](https://github.com/nodejs/node/commit/daaad47099)] - **doc**: clarification for maxBuffer and Unicode output (James M Snell) [#6030](https://github.com/nodejs/node/pull/6030)
* [[`5e6915f374`](https://github.com/nodejs/node/commit/5e6915f374)] - **doc**: describe child.kill() pitfalls on linux (Robert Jefe Lindstaedt) [#2098](https://github.com/nodejs/node/issues/2098)
* [[`a40d0e8f9d`](https://github.com/nodejs/node/commit/a40d0e8f9d)] - **doc**: fix scrolling on iOS devices (Luigi Pinca) [#5878](https://github.com/nodejs/node/pull/5878)
* [[`a81fca4f99`](https://github.com/nodejs/node/commit/a81fca4f99)] - **doc**: add topic - event loop, timers, `nextTick()` (Jeff Harris) [#4936](https://github.com/nodejs/node/pull/4936)
* [[`440d1172fd`](https://github.com/nodejs/node/commit/440d1172fd)] - **doc**: add example using algorithms not directly exposed (Brad Hill) [#6108](https://github.com/nodejs/node/pull/6108)
* [[`96ad5c5303`](https://github.com/nodejs/node/commit/96ad5c5303)] - **doc**: update openssl LICENSE using license-builder.sh (Steven R. Loomis) [#6065](https://github.com/nodejs/node/pull/6065)
* [[`07829b0bc9`](https://github.com/nodejs/node/commit/07829b0bc9)] - **doc**: simple doc typo fix (Brendon Pierson) [#6041](https://github.com/nodejs/node/pull/6041)
* [[`bc0ee06226`](https://github.com/nodejs/node/commit/bc0ee06226)] - **doc**: note about Android support (Rich Trott) [#6040](https://github.com/nodejs/node/pull/6040)
* [[`60a73a2ed2`](https://github.com/nodejs/node/commit/60a73a2ed2)] - **doc**: fix a typo in 5.10.1's changelog (Vladimir Varankin) [#6076](https://github.com/nodejs/node/pull/6076)
* [[`b57be92c1b`](https://github.com/nodejs/node/commit/b57be92c1b)] - **gitignore**: adding .vs/ directory to .gitignore (Mike Kaufman) [#6070](https://github.com/nodejs/node/pull/6070)
* [[`6e891c7ad4`](https://github.com/nodejs/node/commit/6e891c7ad4)] - **gitignore**: ignore VS 2015 *.VC.opendb files (Mike Kaufman) [#6070](https://github.com/nodejs/node/pull/6070)
* [[`abd101be1a`](https://github.com/nodejs/node/commit/abd101be1a)] - **http**: disallow sending obviously invalid status codes (Brian White) [#6291](https://github.com/nodejs/node/pull/6291)
* [[`16b23b2c28`](https://github.com/nodejs/node/commit/16b23b2c28)] - **http**: skip body and next message of CONNECT res (Fedor Indutny) [#6279](https://github.com/nodejs/node/pull/6279)
* [[`a259ee4018`](https://github.com/nodejs/node/commit/a259ee4018)] - **http**: unref socket timer on parser execute (Fedor Indutny) [#6286](https://github.com/nodejs/node/pull/6286)
* [[`d4abca5b27`](https://github.com/nodejs/node/commit/d4abca5b27)] - **lib**: remove bootstrap global context indirection (Jeremiah Senkpiel) [#5881](https://github.com/nodejs/node/pull/5881)
* [[`c8783aff21`](https://github.com/nodejs/node/commit/c8783aff21)] - **lib,test,tools**: alignment on variable assignments (Rich Trott) [#6242](https://github.com/nodejs/node/pull/6242)
* [[`d5d4f194f1`](https://github.com/nodejs/node/commit/d5d4f194f1)] - **net**: replace __defineGetter__ with defineProperty (Fedor Indutny) [#6284](https://github.com/nodejs/node/pull/6284)
* [[`6d9c0c9aa7`](https://github.com/nodejs/node/commit/6d9c0c9aa7)] - **(SEMVER-MINOR)** **net**: support DNS hints in createConnection() (Colin Ihrig) [#6000](https://github.com/nodejs/node/pull/6000)
* [[`457f24f19c`](https://github.com/nodejs/node/commit/457f24f19c)] - **(SEMVER-MINOR)** **node**: make builtin libs available for `--eval` (Anna Henningsen) [#6207](https://github.com/nodejs/node/pull/6207)
* [[`fc89d17656`](https://github.com/nodejs/node/commit/fc89d17656)] - **path**: fixing a test that breaks on some machines. (Mike Kaufman) [#6067](https://github.com/nodejs/node/pull/6067)
* [[`1d408099b7`](https://github.com/nodejs/node/commit/1d408099b7)] - **process**: fix incorrect usage of assert.fail() (Rich Trott) [#6211](https://github.com/nodejs/node/pull/6211)
* [[`07c9f981d6`](https://github.com/nodejs/node/commit/07c9f981d6)] - **(SEMVER-MINOR)** **repl**: keep the built-in modules non-enumerable (Anna Henningsen) [#6207](https://github.com/nodejs/node/pull/6207)
* [[`5382deaa18`](https://github.com/nodejs/node/commit/5382deaa18)] - **repl**: don’t complete non-simple expressions (Anna Henningsen) [#6192](https://github.com/nodejs/node/pull/6192)
* [[`2254f1a175`](https://github.com/nodejs/node/commit/2254f1a175)] - **repl**: refactor repl.js (Rich Trott) [#6071](https://github.com/nodejs/node/pull/6071)
* [[`7d54d85269`](https://github.com/nodejs/node/commit/7d54d85269)] - **(SEMVER-MINOR)** **src**: add SIGINFO to supported signals (James Reggio) [#6093](https://github.com/nodejs/node/pull/6093)
* [[`fbc99ba4f1`](https://github.com/nodejs/node/commit/fbc99ba4f1)] - **src**: add missing 'inline' keywords (Ben Noordhuis) [#6056](https://github.com/nodejs/node/pull/6056)
* [[`20bb92f5c8`](https://github.com/nodejs/node/commit/20bb92f5c8)] - **src**: use size_t for http parser array size fields (Ben Noordhuis) [#5969](https://github.com/nodejs/node/pull/5969)
* [[`2fd8be2dbe`](https://github.com/nodejs/node/commit/2fd8be2dbe)] - **src**: replace ARRAY_SIZE with typesafe arraysize (Ben Noordhuis) [#5969](https://github.com/nodejs/node/pull/5969)
* [[`4392b4aee0`](https://github.com/nodejs/node/commit/4392b4aee0)] - **stream**: Fix readableState.awaitDrain mechanism (Anna Henningsen) [#6023](https://github.com/nodejs/node/pull/6023)
* [[`20dcdd365b`](https://github.com/nodejs/node/commit/20dcdd365b)] - **stream_base**: expose `bytesRead` getter (Fedor Indutny) [#6284](https://github.com/nodejs/node/pull/6284)
* [[`f69416c06e`](https://github.com/nodejs/node/commit/f69416c06e)] - **streams**: support unlimited synchronous cork/uncork cycles (Matteo Collina) [#6164](https://github.com/nodejs/node/pull/6164)
* [[`4bfed26d1a`](https://github.com/nodejs/node/commit/4bfed26d1a)] - **test**: add zlib close-after-error regression test (Anna Henningsen) [#6270](https://github.com/nodejs/node/pull/6270)
* [[`99d0a61441`](https://github.com/nodejs/node/commit/99d0a61441)] - **test**: move more tests from sequential to parallel (Santiago Gimeno) [#6187](https://github.com/nodejs/node/pull/6187)
* [[`96be986a77`](https://github.com/nodejs/node/commit/96be986a77)] - **test**: assert - fixed error messages to match the tests (surya panikkal) [#6241](https://github.com/nodejs/node/pull/6241)
* [[`4e4efb756e`](https://github.com/nodejs/node/commit/4e4efb756e)] - **test**: add test for responses to HTTP CONNECT req (Josh Leder) [#6279](https://github.com/nodejs/node/pull/6279)
* [[`5b42ef5dfe`](https://github.com/nodejs/node/commit/5b42ef5dfe)] - **test**: move debugger tests to sequential (Rich Trott) [#6205](https://github.com/nodejs/node/pull/6205)
* [[`9856b804e9`](https://github.com/nodejs/node/commit/9856b804e9)] - **test**: move some test from sequential to parallel (Santiago Gimeno) [#6087](https://github.com/nodejs/node/pull/6087)
* [[`1d130d0203`](https://github.com/nodejs/node/commit/1d130d0203)] - **test**: move the debugger tests back to parallel (Santiago Gimeno) [#6246](https://github.com/nodejs/node/pull/6246)
* [[`c0e9c94868`](https://github.com/nodejs/node/commit/c0e9c94868)] - **test**: fix issues for ESLint 2.7.0 (silverwind) [#6132](https://github.com/nodejs/node/pull/6132)
* [[`056a258173`](https://github.com/nodejs/node/commit/056a258173)] - **test**: fix flaky test-http-set-timeout-server (Santiago Gimeno) [#6248](https://github.com/nodejs/node/pull/6248)
* [[`be993fcf6c`](https://github.com/nodejs/node/commit/be993fcf6c)] - **test**: fix test-net-settimeout flakiness (Santiago Gimeno) [#6166](https://github.com/nodejs/node/pull/6166)
* [[`a38b614ae9`](https://github.com/nodejs/node/commit/a38b614ae9)] - **test**: fix flaky test-child-process-fork-net (Rich Trott) [#6138](https://github.com/nodejs/node/pull/6138)
* [[`476535cc0e`](https://github.com/nodejs/node/commit/476535cc0e)] - **test**: fix flaky test-http-client-abort (Rich Trott) [#6124](https://github.com/nodejs/node/pull/6124)
* [[`6bb7999bd6`](https://github.com/nodejs/node/commit/6bb7999bd6)] - **test**: refactor test-file-write-stream3 (Rich Trott) [#6050](https://github.com/nodejs/node/pull/6050)
* [[`a27e95231e`](https://github.com/nodejs/node/commit/a27e95231e)] - **test**: enforce strict mode for test-domain-crypto (Rich Trott) [#6047](https://github.com/nodejs/node/pull/6047)
* [[`8da4bad1c9`](https://github.com/nodejs/node/commit/8da4bad1c9)] - **test**: fix pummel test failures (Rich Trott) [#6012](https://github.com/nodejs/node/pull/6012)
* [[`edd8a15508`](https://github.com/nodejs/node/commit/edd8a15508)] - **test,repl**: use deepStrictEqual for false-y values (Jeremiah Senkpiel) [#6196](https://github.com/nodejs/node/pull/6196)
* [[`48ecc0b6b5`](https://github.com/nodejs/node/commit/48ecc0b6b5)] - **test,tools**: enable linting for undefined vars (Rich Trott) [#6255](https://github.com/nodejs/node/pull/6255)
* [[`d809c84bf8`](https://github.com/nodejs/node/commit/d809c84bf8)] - **test,vm**: enable strict mode for vm tests (Rich Trott) [#6209](https://github.com/nodejs/node/pull/6209)
* [[`4a1dfdcc0f`](https://github.com/nodejs/node/commit/4a1dfdcc0f)] - **tools**: lint rule for assert.fail() (Rich Trott) [#6261](https://github.com/nodejs/node/pull/6261)
* [[`fff6a84da5`](https://github.com/nodejs/node/commit/fff6a84da5)] - **tools**: enable linting for v8_prof_processor.js (Rich Trott) [#6262](https://github.com/nodejs/node/pull/6262)
* [[`a2ca347803`](https://github.com/nodejs/node/commit/a2ca347803)] - **tools**: move message listener to worker objects (Brian White) [#6212](https://github.com/nodejs/node/pull/6212)
* [[`f201b01bf7`](https://github.com/nodejs/node/commit/f201b01bf7)] - **tools**: improve js linter (Brian White) [#5638](https://github.com/nodejs/node/pull/5638)
* [[`be070d775f`](https://github.com/nodejs/node/commit/be070d775f)] - **tools**: lint for alignment of variable assignments (Rich Trott) [#6242](https://github.com/nodejs/node/pull/6242)
* [[`d9b8758f47`](https://github.com/nodejs/node/commit/d9b8758f47)] - **tools**: update ESLint to 2.7.0 (silverwind) [#6132](https://github.com/nodejs/node/pull/6132)
* [[`a6056c453e`](https://github.com/nodejs/node/commit/a6056c453e)] - **tools**: fix license-builder.sh again for ICU (Steven R. Loomis) [#6068](https://github.com/nodejs/node/pull/6068)
* [[`fabc33a075`](https://github.com/nodejs/node/commit/fabc33a075)] - **tools**: remove simplejson dependency (Sakthipriyan Vairamani) [#6101](https://github.com/nodejs/node/pull/6101)
* [[`d516412cd5`](https://github.com/nodejs/node/commit/d516412cd5)] - **tools,doc**: parse types in braces everywhere (Alexander Makarenko) [#5329](https://github.com/nodejs/node/pull/5329)
* [[`69eb4a6834`](https://github.com/nodejs/node/commit/69eb4a6834)] - **tools,doc**: fix json for grouped optional params (firedfox) [#5977](https://github.com/nodejs/node/pull/5977)
* [[`a2dd848764`](https://github.com/nodejs/node/commit/a2dd848764)] - **tools,doc**: fix incomplete json produced by doctool (firedfox) [#5966](https://github.com/nodejs/node/pull/5966)
* [[`bad006f2e1`](https://github.com/nodejs/node/commit/bad006f2e1)] - **zlib**: fix use after null when calling .close (James Lal) [#5982](https://github.com/nodejs/node/pull/5982)
* [[`83bc0a2ac9`](https://github.com/nodejs/node/commit/83bc0a2ac9)] - **(SEMVER-MINOR)** **zlib**: Make the finish flush flag configurable (Anna Henningsen) [#6069](https://github.com/nodejs/node/pull/6069)
* [[`2c23e14d5d`](https://github.com/nodejs/node/commit/2c23e14d5d)] - **(SEMVER-MINOR)** **zlib**: detect gzip files when using unzip* (Anna Henningsen) [#5884](https://github.com/nodejs/node/pull/5884)
* [[`61167c3e23`](https://github.com/nodejs/node/commit/61167c3e23)] - **zlib**: fix gzip member head/buffer boundary issue (Anna Henningsen) [#5973](https://github.com/nodejs/node/pull/5973)

## 2016-04-05, Version 5.10.1 (Stable), @thealphanerd

### Notable changes

**http**:
  * Enclose IPv6 Host header in square brackets. This will enable proper separation of the host adress from any port reference (Mihai Potra) [#5314](https://github.com/nodejs/node/pull/5314)

**path**:
  * Make win32.isAbsolute more consistent (Brian White) [#6028](https://github.com/nodejs/node/pull/6028)

### Commits

* [[`0f5a51ae4b`](https://github.com/nodejs/node/commit/0f5a51ae4b)] - **assert**: Check typed array view type in deepEqual (Anna Henningsen) [#5910](https://github.com/nodejs/node/pull/5910)
* [[`e966d1f5db`](https://github.com/nodejs/node/commit/e966d1f5db)] - **buffer**: don't set `kNoZeroFill` flag in allocUnsafe (Vladimir Kurchatkin) [#6007](https://github.com/nodejs/node/pull/6007)
* [[`3f75751c2e`](https://github.com/nodejs/node/commit/3f75751c2e)] - **build**: introduce ci targets for lint/benchmark (Johan Bergström) [#5921](https://github.com/nodejs/node/pull/5921)
* [[`781290b61d`](https://github.com/nodejs/node/commit/781290b61d)] - **doc**: refine child_process detach behaviour (Robert Jefe Lindstaedt) [#5330](https://github.com/nodejs/node/pull/5330)
* [[`aa9fb03202`](https://github.com/nodejs/node/commit/aa9fb03202)] - **doc**: use HTTPS for links where possible (Rich Trott) [#6019](https://github.com/nodejs/node/pull/6019)
* [[`dd25984838`](https://github.com/nodejs/node/commit/dd25984838)] - **doc**: note assert.throws() pitfall (Rich Trott) [#6029](https://github.com/nodejs/node/pull/6029)
* [[`f879f5e68a`](https://github.com/nodejs/node/commit/f879f5e68a)] - **doc**: document unspecified behavior for buf.write* methods (James M Snell) [#5925](https://github.com/nodejs/node/pull/5925)
* [[`f12c3861e0`](https://github.com/nodejs/node/commit/f12c3861e0)] - **doc**: clarify stdout/stderr arguments to callback (James M Snell) [#6015](https://github.com/nodejs/node/pull/6015)
* [[`ce173716be`](https://github.com/nodejs/node/commit/ce173716be)] - **doc**: add 'Command Line Options' to 'View on single page' (firedfox) [#6011](https://github.com/nodejs/node/pull/6011)
* [[`7337ef6422`](https://github.com/nodejs/node/commit/7337ef6422)] - **doc**: minor argument formatting in stream.markdown (James M Snell) [#6016](https://github.com/nodejs/node/pull/6016)
* [[`0ae5d027c6`](https://github.com/nodejs/node/commit/0ae5d027c6)] - **doc**: clarify that __dirname is module local (James M Snell) [#6018](https://github.com/nodejs/node/pull/6018)
* [[`8bec8aa41f`](https://github.com/nodejs/node/commit/8bec8aa41f)] - **doc**: consolidate timers docs in timers.markdown (Bryan English) [#5837](https://github.com/nodejs/node/pull/5837)
* [[`0a13099c42`](https://github.com/nodejs/node/commit/0a13099c42)] - **etw**: add event messages (João Reis) [#5936](https://github.com/nodejs/node/pull/5936)
* [[`c6ac6f2ea1`](https://github.com/nodejs/node/commit/c6ac6f2ea1)] - **http**: Corrects IPv6 address in Host header (Mihai Potra) [#5314](https://github.com/nodejs/node/pull/5314)
* [[`8317778925`](https://github.com/nodejs/node/commit/8317778925)] - **meta**: add "joining a wg" section to WORKING_GROUPS.md (Matteo Collina) [#5488](https://github.com/nodejs/node/pull/5488)
* [[`f3f19ee5e2`](https://github.com/nodejs/node/commit/f3f19ee5e2)] - **net**: refactor self=this to arrow functions (Benjamin Gruenbaum) [#5857](https://github.com/nodejs/node/pull/5857)
* [[`1c4007927d`](https://github.com/nodejs/node/commit/1c4007927d)] - **path**: fix win32.isAbsolute() inconsistency (Brian White) [#6028](https://github.com/nodejs/node/pull/6028)
* [[`059b607a4f`](https://github.com/nodejs/node/commit/059b607a4f)] - **test**: make use of globals explicit (Rich Trott) [#6014](https://github.com/nodejs/node/pull/6014)
* [[`cc8fcc5a07`](https://github.com/nodejs/node/commit/cc8fcc5a07)] - **test**: be explicit about polluting of `global` (Rich Trott) [#6017](https://github.com/nodejs/node/pull/6017)
* [[`7db7a820b9`](https://github.com/nodejs/node/commit/7db7a820b9)] - **test**: make arch available in status files (Santiago Gimeno) [#5997](https://github.com/nodejs/node/pull/5997)
* [[`02f2ebd9b4`](https://github.com/nodejs/node/commit/02f2ebd9b4)] - **test**: explicitly set global in test-repl (Rich Trott) [#6026](https://github.com/nodejs/node/pull/6026)
* [[`2ab1237137`](https://github.com/nodejs/node/commit/2ab1237137)] - **test**: fix flaky test-net-socket-timeout-unref (Rich Trott) [#6003](https://github.com/nodejs/node/pull/6003)
* [[`0127c2bd39`](https://github.com/nodejs/node/commit/0127c2bd39)] - **test**: fix test-dns.js flakiness (Rich Trott) [#5996](https://github.com/nodejs/node/pull/5996)
* [[`6052ced37f`](https://github.com/nodejs/node/commit/6052ced37f)] - **test**: fix error message checks in test-module-loading (James M Snell) [#5986](https://github.com/nodejs/node/pull/5986)
* [[`a40b0cb673`](https://github.com/nodejs/node/commit/a40b0cb673)] - **test**: refactor http-end-throw-socket-handling (Santiago Gimeno) [#5676](https://github.com/nodejs/node/pull/5676)
* [[`96bb315262`](https://github.com/nodejs/node/commit/96bb315262)] - **test**: ensure _handle property existence (Rich Trott) [#5916](https://github.com/nodejs/node/pull/5916)
* [[`4f1fa2adeb`](https://github.com/nodejs/node/commit/4f1fa2adeb)] - **test**: fix offending max-len linter error (Sakthipriyan Vairamani) [#5980](https://github.com/nodejs/node/pull/5980)
* [[`f14d71ccea`](https://github.com/nodejs/node/commit/f14d71ccea)] - **test**: stdin is not always a net.Socket (Jeremiah Senkpiel) [#5935](https://github.com/nodejs/node/pull/5935)
* [[`50a062e691`](https://github.com/nodejs/node/commit/50a062e691)] - **tools**: remove obsolete lint config file (Rich Trott) [#5959](https://github.com/nodejs/node/pull/5959)
* [[`7491fdcfe9`](https://github.com/nodejs/node/commit/7491fdcfe9)] - **tools**: remove disabling of already-disabled rule (Rich Trott) [#6013](https://github.com/nodejs/node/pull/6013)

## 2016-03-31, Version 0.10.44 (Maintenance), @rvagg

### Notable changes

* npm: Upgrade to v2.15.1. Fixes a security flaw in the use of authentication tokens in HTTP requests that would allow an attacker to set up a server that could collect tokens from users of the command-line interface. Authentication tokens have previously been sent with every request made by the CLI for logged-in users, regardless of the destination of the request. This update fixes this by only including those tokens for requests made against the registry or registries used for the current install. IMPORTANT: This is a major upgrade to npm v2 LTS from the previously deprecated npm v1. (Forrest L Norvell) https://github.com/nodejs/node/pull/5967
* openssl: OpenSSL v1.0.1s disables the EXPORT and LOW ciphers as they are obsolete and not considered safe. This release of Node.js turns on `OPENSSL_NO_WEAK_SSL_CIPHERS` to fully disable the 27 ciphers included in these lists which can be used in SSLv3 and higher. Full details can be found in our LTS discussion on the matter (https://github.com/nodejs/LTS/issues/85). (Shigeki Ohtsu) https://github.com/nodejs/node/pull/5712

### Commits

* [feceb77d7e] - deps: upgrade npm in LTS to 2.15.1 (Forrest L Norvell) https://github.com/nodejs/node/pull/5968
* [0847954331] - deps: Disable EXPORT and LOW ciphers in openssl (Shigeki Ohtsu) https://github.com/nodejs/node/pull/5712
* [6bb86e727a] - test: change tls tests not to use LOW cipher (Shigeki Ohtsu) https://github.com/nodejs/node/pull/5712
* [905bec29ad] - win,build: support Visual C++ Build Tools 2015 (João Reis) https://github.com/nodejs/node/pull/5627

## 2016-03-31, Version 5.10.0 (Stable), @evanlucas

### Notable changes

* **buffer**:
  * make byteLength work with ArrayBuffer & DataView (Jackson Tian) [#5255](https://github.com/nodejs/node/pull/5255)
  * backport --zero-fill-buffers command line option (James M Snell) [#5744](https://github.com/nodejs/node/pull/5744)
  * backport new buffer constructor APIs (James M Snell) [#5763](https://github.com/nodejs/node/pull/5763)
  * add swap16() and swap32() methods (James M Snell) [#5724](https://github.com/nodejs/node/pull/5724)
* **fs**: add the fs.mkdtemp() function. (Florian MARGAINE) [#5333](https://github.com/nodejs/node/pull/5333)
* **net**: emit host in lookup event (HUANG Wei) [#5598](https://github.com/nodejs/node/pull/5598)
* **node**: --no-browser-globals configure flag (Fedor Indutny) [#5853](https://github.com/nodejs/node/pull/5853)
* **npm**: Upgrade to v3.8.3. Fixes a security flaw in the use of authentication tokens in HTTP requests that
  would allow an attacker to set up a server that could collect tokens from users of the command-line interface.
  Authentication tokens have previously been sent with every request made by the CLI for logged-in users,
  regardless of the destination of the request. This update fixes this by only including those tokens for requests
  made against the registry or registries used for the current install. (Forrest L Norvell) [npm/node#6](https://github.com/npm/node/pull/6)
* **repl**: support standalone blocks (Prince J Wesley) [#5581](https://github.com/nodejs/node/pull/5581)
* **src**: override v8 thread defaults using cli options (Tom Gallacher) [#4344](https://github.com/nodejs/node/pull/4344)

### Commits

* [[`2cbbaafca9`](https://github.com/nodejs/node/commit/2cbbaafca9)] - **async_wrap**: don't abort on callback exception (Trevor Norris) [#5756](https://github.com/nodejs/node/pull/5756)
* [[`6f16882733`](https://github.com/nodejs/node/commit/6f16882733)] - **async_wrap**: notify post if intercepted exception (Trevor Norris) [#5756](https://github.com/nodejs/node/pull/5756)
* [[`a4856122d3`](https://github.com/nodejs/node/commit/a4856122d3)] - **async_wrap**: setupHooks now accepts object (Trevor Norris) [#5756](https://github.com/nodejs/node/pull/5756)
* [[`ee83c956c5`](https://github.com/nodejs/node/commit/ee83c956c5)] - **(SEMVER-MINOR)** **buffer**: make byteLength work with ArrayBuffer & DataView (Jackson Tian) [#5255](https://github.com/nodejs/node/pull/5255)
* [[`1f8e4b54ce`](https://github.com/nodejs/node/commit/1f8e4b54ce)] - **(SEMVER-MINOR)** **buffer**: add swap16() and swap32() methods (James M Snell) [#5724](https://github.com/nodejs/node/pull/5724)
* [[`bdf933bece`](https://github.com/nodejs/node/commit/bdf933bece)] - **buffer**: changing let in for loops back to var (Gareth Ellis) [#5819](https://github.com/nodejs/node/pull/5819)
* [[`c1534e7eaf`](https://github.com/nodejs/node/commit/c1534e7eaf)] - **(SEMVER-MINOR)** **buffer**: backport new buffer constructor APIs (James M Snell) [#5763](https://github.com/nodejs/node/pull/5763)
* [[`3c02727055`](https://github.com/nodejs/node/commit/3c02727055)] - **(SEMVER-MINOR)** **buffer**: backport --zero-fill-buffers command line option (James M Snell) [#5744](https://github.com/nodejs/node/pull/5744)
* [[`58b5c1e19f`](https://github.com/nodejs/node/commit/58b5c1e19f)] - **build**: add suport for x86 architecture (Robert Chiras) [#5544](https://github.com/nodejs/node/pull/5544)
* [[`389f5a85e6`](https://github.com/nodejs/node/commit/389f5a85e6)] - **build**: add script to create Android .mk files (Robert Chiras) [#5544](https://github.com/nodejs/node/pull/5544)
* [[`5ee5fa292f`](https://github.com/nodejs/node/commit/5ee5fa292f)] - **build**: add missing `openssl_fips%` to common.gypi (Fedor Indutny) [#5919](https://github.com/nodejs/node/pull/5919)
* [[`5681ffecf7`](https://github.com/nodejs/node/commit/5681ffecf7)] - **build**: enable compilation for linuxOne (Michael Dawson) [#5941](https://github.com/nodejs/node/pull/5941)
* [[`660ec9f889`](https://github.com/nodejs/node/commit/660ec9f889)] - **child_process**: refactor self=this in socket_list (Benjamin Gruenbaum) [#5860](https://github.com/nodejs/node/pull/5860)
* [[`e1a012f277`](https://github.com/nodejs/node/commit/e1a012f277)] - **deps**: upgrade npm to 3.8.3 (Forrest L Norvell)
* [[`ec1813199d`](https://github.com/nodejs/node/commit/ec1813199d)] - **deps**: backport 8d00c2c from v8 upstream (Ben Noordhuis) [#5577](https://github.com/nodejs/node/pull/5577)
* [[`2a5c6d7006`](https://github.com/nodejs/node/commit/2a5c6d7006)] - **dns**: Refactor forEach to map (Benjamin Gruenbaum) [#5803](https://github.com/nodejs/node/pull/5803)
* [[`6a6112a2f3`](https://github.com/nodejs/node/commit/6a6112a2f3)] - **dns**: Use object without protoype for map (Benjamin Gruenbaum) [#5843](https://github.com/nodejs/node/pull/5843)
* [[`8fa0b5c1da`](https://github.com/nodejs/node/commit/8fa0b5c1da)] - **doc**: Add @mhdawson back to the CTC (James M Snell) [#5633](https://github.com/nodejs/node/pull/5633)
* [[`858a524325`](https://github.com/nodejs/node/commit/858a524325)] - **doc**: typo: interal->internal. (Corey Kosak) [#5849](https://github.com/nodejs/node/pull/5849)
* [[`5676a35bd9`](https://github.com/nodejs/node/commit/5676a35bd9)] - **doc**: explain path.format expected properties (John Eversole) [#5801](https://github.com/nodejs/node/pull/5801)
* [[`29778393a0`](https://github.com/nodejs/node/commit/29778393a0)] - **doc**: use consistent event name parameter (Benjamin Gruenbaum) [#5850](https://github.com/nodejs/node/pull/5850)
* [[`949b17ff6d`](https://github.com/nodejs/node/commit/949b17ff6d)] - **doc**: fix order of end tags of list after heading (firedfox) [#5874](https://github.com/nodejs/node/pull/5874)
* [[`8e790b7a0c`](https://github.com/nodejs/node/commit/8e790b7a0c)] - **doc**: add instructions to only sign a release (Jeremiah Senkpiel) [#5876](https://github.com/nodejs/node/pull/5876)
* [[`f1f9aff855`](https://github.com/nodejs/node/commit/f1f9aff855)] - **doc**: fix doc for Buffer.readInt32LE() (ghaiklor) [#5890](https://github.com/nodejs/node/pull/5890)
* [[`731f7b8055`](https://github.com/nodejs/node/commit/731f7b8055)] - **etw**: fix descriptors of events 9 and 23 (João Reis) [#5742](https://github.com/nodejs/node/pull/5742)
* [[`aac9ead379`](https://github.com/nodejs/node/commit/aac9ead379)] - **etw,build**: always generate .rc and .h files (João Reis) [#5657](https://github.com/nodejs/node/pull/5657)
* [[`80155d398c`](https://github.com/nodejs/node/commit/80155d398c)] - **(SEMVER-MINOR)** **fs**: add the fs.mkdtemp() function. (Florian MARGAINE) [#5333](https://github.com/nodejs/node/pull/5333)
* [[`ae15d68ad1`](https://github.com/nodejs/node/commit/ae15d68ad1)] - **governance**: remove target size for CTC (Rich Trott) [#5879](https://github.com/nodejs/node/pull/5879)
* [[`63c601bc15`](https://github.com/nodejs/node/commit/63c601bc15)] - **http**: speed up checkIsHttpToken (Jackson Tian) [#4790](https://github.com/nodejs/node/pull/4790)
* [[`40847b0b8b`](https://github.com/nodejs/node/commit/40847b0b8b)] - **lib**: rename /node.js to /bootstrap_node.js (Jeremiah Senkpiel) [#5103](https://github.com/nodejs/node/pull/5103)
* [[`e644eb3d69`](https://github.com/nodejs/node/commit/e644eb3d69)] - **lib**: refactor code with startsWith/endsWith (Jackson Tian) [#5753](https://github.com/nodejs/node/pull/5753)
* [[`a757e0583c`](https://github.com/nodejs/node/commit/a757e0583c)] - **lib,src**: move src/node.js to lib/internal/node.js (Jeremiah Senkpiel) [#5103](https://github.com/nodejs/node/pull/5103)
* [[`e3c7b46326`](https://github.com/nodejs/node/commit/e3c7b46326)] - **lib,src**: refactor src/node.js into internal files (Jeremiah Senkpiel) [#5103](https://github.com/nodejs/node/pull/5103)
* [[`b07bc5d996`](https://github.com/nodejs/node/commit/b07bc5d996)] - **(SEMVER-MINOR)** **net**: emit host in lookup event (HUANG Wei) [#5598](https://github.com/nodejs/node/pull/5598)
* [[`2fa959be15`](https://github.com/nodejs/node/commit/2fa959be15)] - **(SEMVER-MINOR)** **node**: --no-browser-globals configure flag (Fedor Indutny) [#5853](https://github.com/nodejs/node/pull/5853)
* [[`a2ad21645f`](https://github.com/nodejs/node/commit/a2ad21645f)] - **querystring**: don't stringify bad surrogate pair (Brian White) [#5858](https://github.com/nodejs/node/pull/5858)
* [[`427173204e`](https://github.com/nodejs/node/commit/427173204e)] - **(SEMVER-MINOR)** **repl**: support standalone blocks (Prince J Wesley) [#5581](https://github.com/nodejs/node/pull/5581)
* [[`d044898495`](https://github.com/nodejs/node/commit/d044898495)] - **src**: Add missing `using v8::MaybeLocal` (Anna Henningsen) [#5974](https://github.com/nodejs/node/pull/5974)
* [[`0d0c57ff5e`](https://github.com/nodejs/node/commit/0d0c57ff5e)] - **(SEMVER-MINOR)** **src**: override v8 thread defaults using cli options (Tom Gallacher) [#4344](https://github.com/nodejs/node/pull/4344)
* [[`f9d0166291`](https://github.com/nodejs/node/commit/f9d0166291)] - **src**: reword command and add ternary (Trevor Norris) [#5756](https://github.com/nodejs/node/pull/5756)
* [[`f1488bb24c`](https://github.com/nodejs/node/commit/f1488bb24c)] - **src,http_parser**: remove KickNextTick call (Trevor Norris) [#5756](https://github.com/nodejs/node/pull/5756)
* [[`8e8768ecbb`](https://github.com/nodejs/node/commit/8e8768ecbb)] - **test**: add known_issues test for GH-2148 (Rich Trott) [#5920](https://github.com/nodejs/node/pull/5920)
* [[`bf94b5a1b9`](https://github.com/nodejs/node/commit/bf94b5a1b9)] - **test**: mitigate flaky test-https-agent (Rich Trott) [#5939](https://github.com/nodejs/node/pull/5939)
* [[`2192528326`](https://github.com/nodejs/node/commit/2192528326)] - **test**: fix flaky test-repl (Brian White) [#5914](https://github.com/nodejs/node/pull/5914)
* [[`aebe6245b7`](https://github.com/nodejs/node/commit/aebe6245b7)] - **test**: add test for piping large input from stdin (Anna Henningsen) [#5949](https://github.com/nodejs/node/pull/5949)
* [[`a19de97d2f`](https://github.com/nodejs/node/commit/a19de97d2f)] - **test**: remove the use of curl in the test suite (Santiago Gimeno) [#5750](https://github.com/nodejs/node/pull/5750)
* [[`6928a17aa3`](https://github.com/nodejs/node/commit/6928a17aa3)] - **test**: exclude new fs watch test for AIX (Michael Dawson) [#5937](https://github.com/nodejs/node/pull/5937)
* [[`3238bff3b3`](https://github.com/nodejs/node/commit/3238bff3b3)] - **test**: confirm globals not used internally (Rich Trott) [#5882](https://github.com/nodejs/node/pull/5882)
* [[`a41fd93f68`](https://github.com/nodejs/node/commit/a41fd93f68)] - **test**: fix flaky test-net-socket-timeout (Brian White) [#5902](https://github.com/nodejs/node/pull/5902)
* [[`82a50d3def`](https://github.com/nodejs/node/commit/82a50d3def)] - **test**: move dns test to test/internet (Ben Noordhuis) [#5905](https://github.com/nodejs/node/pull/5905)
* [[`fb0c5bcac2`](https://github.com/nodejs/node/commit/fb0c5bcac2)] - **test**: fix flaky test-http-set-timeout (Rich Trott) [#5856](https://github.com/nodejs/node/pull/5856)
* [[`8344a522a8`](https://github.com/nodejs/node/commit/8344a522a8)] - **test**: fix test-debugger-client.js (Rich Trott) [#5851](https://github.com/nodejs/node/pull/5851)
* [[`7ec5397954`](https://github.com/nodejs/node/commit/7ec5397954)] - **timers**: fixing API refs to use safe internal refs (Kyle Simpson) [#5882](https://github.com/nodejs/node/pull/5882)
* [[`cb676cf3e7`](https://github.com/nodejs/node/commit/cb676cf3e7)] - **tools**: fix json doc generation (firedfox) [#5943](https://github.com/nodejs/node/pull/5943)
* [[`77bed269ad`](https://github.com/nodejs/node/commit/77bed269ad)] - **win,build**: build and test add-ons on test-ci (Bogdan Lobor) [#5886](https://github.com/nodejs/node/pull/5886)
* [[`afcd276ecc`](https://github.com/nodejs/node/commit/afcd276ecc)] - **zlib**: Fix handling of gzip magic bytes mid-file (Anna Henningsen) [#5863](https://github.com/nodejs/node/pull/5863)

## 2016-03-31, Version 4.4.2 'Argon' (LTS), @thealphanerd

### Notable Changes

* **https**:
  - Under certain conditions ssl sockets may have been causing a memory leak when keepalive is enabled. This is no longer the case. (Alexander Penev) [#5713](https://github.com/nodejs/node/pull/5713)
* **lib**:
  - The way that we were internally passing arguments was causing a potential leak. By copying the arguments into an array we can avoid this. (Nathan Woltman) [#4361](https://github.com/nodejs/node/pull/4361)
* **npm**:
  - Upgrade to v2.15.1. Fixes a security flaw in the use of authentication tokens in HTTP requests that would allow an attacker to set up a server that could collect tokens from users of the command-line interface. Authentication tokens have previously been sent with every request made by the CLI for logged-in users, regardless of the destination of the request. This update fixes this by only including those tokens for requests made against the registry or registries used for the current install. (Forrest L Norvell)
* **repl**:
  - Previously if you were using the repl in strict mode the column number would be wrong in a stack trace. This is no longer an issue. (Prince J Wesley) [#5416](https://github.com/nodejs/node/pull/5416)

### Commits

* [[`96e163a79f`](https://github.com/nodejs/node/commit/96e163a79f)] - **buffer**: changing let in for loops back to var (Gareth Ellis) [#5819](https://github.com/nodejs/node/pull/5819)
* [[`0c6f6742f2`](https://github.com/nodejs/node/commit/0c6f6742f2)] - **console**: check that stderr is writable (Rich Trott) [#5635](https://github.com/nodejs/node/pull/5635)
* [[`55c3f804c4`](https://github.com/nodejs/node/commit/55c3f804c4)] - **deps**: upgrade npm in LTS to 2.15.1 (Forrest L Norvell)
* [[`1d0e4a987d`](https://github.com/nodejs/node/commit/1d0e4a987d)] - **deps**: remove unused openssl files (Ben Noordhuis) [#5619](https://github.com/nodejs/node/pull/5619)
* [[`d55599f4d8`](https://github.com/nodejs/node/commit/d55599f4d8)] - **dns**: use template literals (Benjamin Gruenbaum) [#5809](https://github.com/nodejs/node/pull/5809)
* [[`42bbdc9dd1`](https://github.com/nodejs/node/commit/42bbdc9dd1)] - **doc** Add @mhdawson back to the CTC (James M Snell) [#5633](https://github.com/nodejs/node/pull/5633)
* [[`8d86d232e7`](https://github.com/nodejs/node/commit/8d86d232e7)] - **doc**: typo: interal->internal. (Corey Kosak) [#5849](https://github.com/nodejs/node/pull/5849)
* [[`60ddab841e`](https://github.com/nodejs/node/commit/60ddab841e)] - **doc**: add instructions to only sign a release (Jeremiah Senkpiel) [#5876](https://github.com/nodejs/node/pull/5876)
* [[`040263e0f3`](https://github.com/nodejs/node/commit/040263e0f3)] - **doc**: grammar, clarity and links in timers doc (Bryan English) [#5792](https://github.com/nodejs/node/pull/5792)
* [[`8c24bd25a6`](https://github.com/nodejs/node/commit/8c24bd25a6)] - **doc**: fix order of end tags of list after heading (firedfox) [#5874](https://github.com/nodejs/node/pull/5874)
* [[`7c837028da`](https://github.com/nodejs/node/commit/7c837028da)] - **doc**: use consistent event name parameter (Benjamin Gruenbaum) [#5850](https://github.com/nodejs/node/pull/5850)
* [[`20faf9097d`](https://github.com/nodejs/node/commit/20faf9097d)] - **doc**: explain error message on missing main file (Wolfgang Steiner) [#5812](https://github.com/nodejs/node/pull/5812)
* [[`79d26ae196`](https://github.com/nodejs/node/commit/79d26ae196)] - **doc**: explain path.format expected properties (John Eversole) [#5801](https://github.com/nodejs/node/pull/5801)
* [[`e43e8e3a31`](https://github.com/nodejs/node/commit/e43e8e3a31)] - **doc**: add a cli options doc page (Jeremiah Senkpiel) [#5787](https://github.com/nodejs/node/pull/5787)
* [[`c0a24e4a1d`](https://github.com/nodejs/node/commit/c0a24e4a1d)] - **doc**: fix multiline return comments in querystring (Claudio Rodriguez) [#5705](https://github.com/nodejs/node/pull/5705)
* [[`bf1fe4693c`](https://github.com/nodejs/node/commit/bf1fe4693c)] - **doc**: Add windows example for Path.format (Mithun Patel) [#5700](https://github.com/nodejs/node/pull/5700)
* [[`3b8fc4fddc`](https://github.com/nodejs/node/commit/3b8fc4fddc)] - **doc**: update crypto docs to use good defaults (Bill Automata) [#5505](https://github.com/nodejs/node/pull/5505)
* [[`a6ec8a6cb7`](https://github.com/nodejs/node/commit/a6ec8a6cb7)] - **doc**: fix crypto update() signatures (Brian White) [#5500](https://github.com/nodejs/node/pull/5500)
* [[`eb0ed46665`](https://github.com/nodejs/node/commit/eb0ed46665)] - **doc**: reformat & improve node.1 manual page (Jeremiah Senkpiel) [#5497](https://github.com/nodejs/node/pull/5497)
* [[`b70ca4a4b4`](https://github.com/nodejs/node/commit/b70ca4a4b4)] - **doc**: updated fs #5862 removed irrelevant data in fs.markdown (topal) [#5877](https://github.com/nodejs/node/pull/5877)
* [[`81876612f7`](https://github.com/nodejs/node/commit/81876612f7)] - **https**: fix ssl socket leak when keepalive is used (Alexander Penev) [#5713](https://github.com/nodejs/node/pull/5713)
* [[`6daebdbd9b`](https://github.com/nodejs/node/commit/6daebdbd9b)] - **lib**: simplify code with String.prototype.repeat() (Jackson Tian) [#5359](https://github.com/nodejs/node/pull/5359)
* [[`108fc90dd7`](https://github.com/nodejs/node/commit/108fc90dd7)] - **lib**: reduce usage of `self = this` (Jackson Tian) [#5231](https://github.com/nodejs/node/pull/5231)
* [[`3c8e59c396`](https://github.com/nodejs/node/commit/3c8e59c396)] - **lib**: copy arguments object instead of leaking it (Nathan Woltman) [#4361](https://github.com/nodejs/node/pull/4361)
* [[`8648420586`](https://github.com/nodejs/node/commit/8648420586)] - **net**: make `isIPv4` and `isIPv6` more efficient (Vladimir Kurchatkin) [#5478](https://github.com/nodejs/node/pull/5478)
* [[`07b7172d76`](https://github.com/nodejs/node/commit/07b7172d76)] - **net**: remove unused `var self = this` from old code (Benjamin Gruenbaum) [#5224](https://github.com/nodejs/node/pull/5224)
* [[`acbce4b72b`](https://github.com/nodejs/node/commit/acbce4b72b)] - **repl**: fix stack trace column number in strict mode (Prince J Wesley) [#5416](https://github.com/nodejs/node/pull/5416)
* [[`0a1eb168e0`](https://github.com/nodejs/node/commit/0a1eb168e0)] - **test**: fix `test-cluster-worker-kill` (Santiago Gimeno) [#5814](https://github.com/nodejs/node/pull/5814)
* [[`86b876fe7b`](https://github.com/nodejs/node/commit/86b876fe7b)] - **test**: smaller chunk size for smaller person.jpg (Jérémy Lal) [#5813](https://github.com/nodejs/node/pull/5813)
* [[`1135ee97e7`](https://github.com/nodejs/node/commit/1135ee97e7)] - **test**: strip non-free icc profile from person.jpg (Jérémy Lal) [#5813](https://github.com/nodejs/node/pull/5813)
* [[`0836d7e2fb`](https://github.com/nodejs/node/commit/0836d7e2fb)] - **test**: fix flaky test-cluster-shared-leak (Claudio Rodriguez) [#5802](https://github.com/nodejs/node/pull/5802)
* [[`e57355c2f4`](https://github.com/nodejs/node/commit/e57355c2f4)] - **test**: make test-net-connect-options-ipv6.js better (Michael Dawson) [#5791](https://github.com/nodejs/node/pull/5791)
* [[`1b266fc15c`](https://github.com/nodejs/node/commit/1b266fc15c)] - **test**: remove the use of curl in the test suite (Santiago Gimeno) [#5750](https://github.com/nodejs/node/pull/5750)
* [[`7e45d4f076`](https://github.com/nodejs/node/commit/7e45d4f076)] - **test**: minimize test-http-get-pipeline-problem (Rich Trott) [#5728](https://github.com/nodejs/node/pull/5728)
* [[`78effc3484`](https://github.com/nodejs/node/commit/78effc3484)] - **test**: add batch of known issue tests (cjihrig) [#5653](https://github.com/nodejs/node/pull/5653)
* [[`d506eea4b7`](https://github.com/nodejs/node/commit/d506eea4b7)] - **test**: improve test-npm-install (Santiago Gimeno) [#5613](https://github.com/nodejs/node/pull/5613)
* [[`7520100e8b`](https://github.com/nodejs/node/commit/7520100e8b)] - **test**: add test-npm-install to parallel tests suite (Myles Borins) [#5166](https://github.com/nodejs/node/pull/5166)
* [[`b258dddb8c`](https://github.com/nodejs/node/commit/b258dddb8c)] - **test**: repl tab completion test (Santiago Gimeno) [#5534](https://github.com/nodejs/node/pull/5534)
* [[`f209effe8b`](https://github.com/nodejs/node/commit/f209effe8b)] - **test**: remove timer from test-http-1.0 (Santiago Gimeno) [#5129](https://github.com/nodejs/node/pull/5129)
* [[`3a901b0e3e`](https://github.com/nodejs/node/commit/3a901b0e3e)] - **tools**: remove unused imports (Sakthipriyan Vairamani) [#5765](https://github.com/nodejs/node/pull/5765)

## 2016-03-31, Version 0.12.13 (LTS), @rvagg

### Notable changes

* npm: Upgrade to v2.15.1. (Forrest L Norvell)
* openssl: OpenSSL v1.0.1s disables the EXPORT and LOW ciphers as they are obsolete and not considered safe. This release of Node.js turns on `OPENSSL_NO_WEAK_SSL_CIPHERS` to fully disable the 27 ciphers included in these lists which can be used in SSLv3 and higher. Full details can be found in our LTS discussion on the matter (https://github.com/nodejs/LTS/issues/85). (Shigeki Ohtsu) https://github.com/nodejs/node/pull/5712

### Commits

* [4041ea6bc5] - deps: upgrade npm in LTS to 2.15.1 (Forrest L Norvell)
* [a115779026] - deps: Disable EXPORT and LOW ciphers in openssl (Shigeki Ohtsu) https://github.com/nodejs/node/pull/5712
* [ab907eb5a8] - test: skip cluster-disconnect-race on Windows (Gibson Fahnestock) https://github.com/nodejs/node/pull/5621
* [9c06db7444] - test: change tls tests not to use LOW cipher (Shigeki Ohtsu) https://github.com/nodejs/node/pull/5712
* [154098a3dc] - test: bp fix for test-http-get-pipeline-problem.js (Michael Dawson) https://github.com/nodejs/node/pull/3013
* [ff2bed6e86] - win,build: support Visual C++ Build Tools 2015 (João Reis) https://github.com/nodejs/node/pull/5627

## 2016-03-23, Version 5.9.1 (Stable), @Fishrock123

### Notable changes

* **buffer**: Now properly throws `RangeError`s on out-of-bounds writes (Matt Loring) [#5605](https://github.com/nodejs/node/pull/5605).
  - This effects `write{Float|Double}` when the `noAssert` option is not used.
* **timers**:
  - Returned timeout objects now have a `Timeout` constructor name (Jeremiah Senkpiel) [#5793](https://github.com/nodejs/node/pull/5793).
  - Performance of `Immediate` processing is now ~20-40% faster (Brian White) [#4169](https://github.com/nodejs/node/pull/4169).
* **vm**: Fixed a contextify regression introduced in v5.9.0 (Ali Ijaz Sheikh) [#5800](https://github.com/nodejs/node/pull/5800).

### Commits

* [[`341b3d01c8`](https://github.com/nodejs/node/commit/341b3d01c8)] - **benchmark**: fix linting errors (Rich Trott) [#5840](https://github.com/nodejs/node/pull/5840)
* [[`72fb796bed`](https://github.com/nodejs/node/commit/72fb796bed)] - **buffer**: throw range error before truncating write (Matt Loring) [#5605](https://github.com/nodejs/node/pull/5605)
* [[`c5d83695e1`](https://github.com/nodejs/node/commit/c5d83695e1)] - **contextify**: tie lifetimes of context & sandbox (Ali Ijaz Sheikh) [#5800](https://github.com/nodejs/node/pull/5800)
* [[`ae24d05451`](https://github.com/nodejs/node/commit/ae24d05451)] - **deps**: remove unused openssl files (Ben Noordhuis) [#5619](https://github.com/nodejs/node/pull/5619)
* [[`54abbe7e6f`](https://github.com/nodejs/node/commit/54abbe7e6f)] - **dns**: use template literals (Benjamin Gruenbaum) [#5809](https://github.com/nodejs/node/pull/5809)
* [[`3fef69bf15`](https://github.com/nodejs/node/commit/3fef69bf15)] - **dns**: use isIp consistently (Benjamin Gruenbaum) [#5804](https://github.com/nodejs/node/pull/5804)
* [[`d2d0fe9d34`](https://github.com/nodejs/node/commit/d2d0fe9d34)] - **doc**: update crypto docs to use good defaults (Bill Automata) [#5505](https://github.com/nodejs/node/pull/5505)
* [[`1631f06477`](https://github.com/nodejs/node/commit/1631f06477)] - **doc**: add CTC meeting minutes 2016-02-10 (Rod Vagg) [#5273](https://github.com/nodejs/node/pull/5273)
* [[`7ab597d646`](https://github.com/nodejs/node/commit/7ab597d646)] - **doc**: add CTC meeting minutes 2016-02-03 (Rod Vagg) [#5272](https://github.com/nodejs/node/pull/5272)
* [[`e20d0b8802`](https://github.com/nodejs/node/commit/e20d0b8802)] - **doc**: explain error message on missing main file (Wolfgang Steiner) [#5812](https://github.com/nodejs/node/pull/5812)
* [[`e99082e32d`](https://github.com/nodejs/node/commit/e99082e32d)] - **doc**: add a cli options doc page (Jeremiah Senkpiel) [#5787](https://github.com/nodejs/node/pull/5787)
* [[`0ffd794b27`](https://github.com/nodejs/node/commit/0ffd794b27)] - **doc**: Add windows example for Path.format (Mithun Patel) [#5700](https://github.com/nodejs/node/pull/5700)
* [[`f53cc37578`](https://github.com/nodejs/node/commit/f53cc37578)] - **doc**: grammar, clarity and links in timers doc (Bryan English) [#5792](https://github.com/nodejs/node/pull/5792)
* [[`3ada8cc09a`](https://github.com/nodejs/node/commit/3ada8cc09a)] - **doc**: align doc/api/tls.markdown with style guide (Stefano Vozza) [#5706](https://github.com/nodejs/node/pull/5706)
* [[`5d28ce3942`](https://github.com/nodejs/node/commit/5d28ce3942)] - **doc**: topic blocking vs non-blocking (Jarrett Widman) [#5326](https://github.com/nodejs/node/pull/5326)
* [[`d9b4e15f75`](https://github.com/nodejs/node/commit/d9b4e15f75)] - **doc**: fix typo in synchronous randomBytes example (Andrea Giammarchi) [#5781](https://github.com/nodejs/node/pull/5781)
* [[`d8318c2226`](https://github.com/nodejs/node/commit/d8318c2226)] - **doc**: fix crypto update() signatures (Brian White) [#5500](https://github.com/nodejs/node/pull/5500)
* [[`15c5662959`](https://github.com/nodejs/node/commit/15c5662959)] - **doc**: fix multiline return comments in querystring (Claudio Rodriguez) [#5705](https://github.com/nodejs/node/pull/5705)
* [[`75f723c0aa`](https://github.com/nodejs/node/commit/75f723c0aa)] - **doc**: fix invalid path doc comments (Rich Trott) [#5670](https://github.com/nodejs/node/pull/5670)
* [[`724b87d75c`](https://github.com/nodejs/node/commit/724b87d75c)] - **doc**: explain path.format() algorithm (Rich Trott) [#5688](https://github.com/nodejs/node/pull/5688)
* [[`89df17ed0b`](https://github.com/nodejs/node/commit/89df17ed0b)] - **doc**: fix return value of write methods (Felix Böhm) [#5736](https://github.com/nodejs/node/pull/5736)
* [[`5ab51ee151`](https://github.com/nodejs/node/commit/5ab51ee151)] - **doc**: reformat & improve node.1 manual page (Jeremiah Senkpiel) [#5497](https://github.com/nodejs/node/pull/5497)
* [[`f34a00cee2`](https://github.com/nodejs/node/commit/f34a00cee2)] - **docs**: fix man pages link if tok type is code (Mithun Patel) [#5721](https://github.com/nodejs/node/pull/5721)
* [[`3bff3111f4`](https://github.com/nodejs/node/commit/3bff3111f4)] - **https**: fix ssl socket leak when keepalive is used (Alexander Penev) [#5713](https://github.com/nodejs/node/pull/5713)
* [[`7b21c09b73`](https://github.com/nodejs/node/commit/7b21c09b73)] - **lib**: simplify code with String.prototype.repeat() (Jackson Tian) [#5359](https://github.com/nodejs/node/pull/5359)
* [[`c75f97f43b`](https://github.com/nodejs/node/commit/c75f97f43b)] - **lib**: reduce usage of `self = this` (Jackson Tian) [#5231](https://github.com/nodejs/node/pull/5231)
* [[`1ccf9b4a56`](https://github.com/nodejs/node/commit/1ccf9b4a56)] - **net**: remove unused `var self = this` from old code (Benjamin Gruenbaum) [#5224](https://github.com/nodejs/node/pull/5224)
* [[`6e5835b8cd`](https://github.com/nodejs/node/commit/6e5835b8cd)] - **path**: refactor path.format() repeated code (Rich Trott) [#5673](https://github.com/nodejs/node/pull/5673)
* [[`15c7b3a127`](https://github.com/nodejs/node/commit/15c7b3a127)] - **src,tools**: use template literals (Rich Trott) [#5778](https://github.com/nodejs/node/pull/5778)
* [[`ca971b0d77`](https://github.com/nodejs/node/commit/ca971b0d77)] - **test**: smaller chunk size for smaller person.jpg (Jérémy Lal) [#5813](https://github.com/nodejs/node/pull/5813)
* [[`f95fc175eb`](https://github.com/nodejs/node/commit/f95fc175eb)] - **test**: strip non-free icc profile from person.jpg (Jérémy Lal) [#5813](https://github.com/nodejs/node/pull/5813)
* [[`7c2c7b0577`](https://github.com/nodejs/node/commit/7c2c7b0577)] - **test**: remove timer from test-http-1.0 (Santiago Gimeno) [#5129](https://github.com/nodejs/node/pull/5129)
* [[`70512e51a4`](https://github.com/nodejs/node/commit/70512e51a4)] - **test**: repl tab completion test (Santiago Gimeno) [#5534](https://github.com/nodejs/node/pull/5534)
* [[`89f091d621`](https://github.com/nodejs/node/commit/89f091d621)] - **test**: make test-net-connect-options-ipv6.js better (Michael Dawson) [#5791](https://github.com/nodejs/node/pull/5791)
* [[`d2fa64490f`](https://github.com/nodejs/node/commit/d2fa64490f)] - **test**: fix `test-cluster-worker-kill` (Santiago Gimeno) [#5814](https://github.com/nodejs/node/pull/5814)
* [[`f0d885a0a9`](https://github.com/nodejs/node/commit/f0d885a0a9)] - **test**: fix flaky test-cluster-shared-leak (Claudio Rodriguez) [#5802](https://github.com/nodejs/node/pull/5802)
* [[`b352cc7db4`](https://github.com/nodejs/node/commit/b352cc7db4)] - **test**: minimize test-http-get-pipeline-problem (Rich Trott) [#5728](https://github.com/nodejs/node/pull/5728)
* [[`21770c3806`](https://github.com/nodejs/node/commit/21770c3806)] - **test**: reduce brittleness of tab complete test (Matt Loring) [#5772](https://github.com/nodejs/node/pull/5772)
* [[`46f0e02620`](https://github.com/nodejs/node/commit/46f0e02620)] - **timers**: fix lint from 4fe02e2 (Jeremiah Senkpiel) [#5825](https://github.com/nodejs/node/pull/5825)
* [[`20a68e9eef`](https://github.com/nodejs/node/commit/20a68e9eef)] - **timers**: give Timeouts a constructor name (Jeremiah Senkpiel) [#5793](https://github.com/nodejs/node/pull/5793)
* [[`d3654d80f3`](https://github.com/nodejs/node/commit/d3654d80f3)] - **timers**: improve setImmediate() performance (Brian White) [#4169](https://github.com/nodejs/node/pull/4169)
* [[`b1a4870200`](https://github.com/nodejs/node/commit/b1a4870200)] - **tools**: remove unused imports (Sakthipriyan Vairamani) [#5765](https://github.com/nodejs/node/pull/5765)

## 2016-03-22, Version 4.4.1 'Argon' (LTS), @thealphanerd

This LTS release comes with 113 commits, 56 of which are doc related,
18 of which are build / tooling related, 16 of which are test related
and 7 which are benchmark related.

### Notable Changes
* **build**:
  - Updated Logos for the OSX + Windows installers
    - (Rod Vagg) [#5401](https://github.com/nodejs/node/pull/5401)
    - (Robert Jefe Lindstaedt) [#5531](https://github.com/nodejs/node/pull/5531)
  - New option to select your VS Version in the Windows installer
    - (julien.waechter) [#4645](https://github.com/nodejs/node/pull/4645)
  - Support Visual C++ Build Tools 2015
    - (João Reis) [#5627](https://github.com/nodejs/node/pull/5627)
* **tools**:
  - Gyp now works on OSX without XCode
    - (Shigeki Ohtsu) [nodejs/node#1325](https://github.com/nodejs/node/pull/1325)

### Commits

* [[`df283f8a03`](https://github.com/nodejs/node/commit/df283f8a03)] - **benchmark**: fix linting issues (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`c901741c60`](https://github.com/nodejs/node/commit/c901741c60)] - **benchmark**: use strict mode (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`4be2065dbc`](https://github.com/nodejs/node/commit/4be2065dbc)] - **benchmark**: refactor to eliminate redeclared vars (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`ddac368533`](https://github.com/nodejs/node/commit/ddac368533)] - **benchmark**: fix lint errors (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`03b20a73b9`](https://github.com/nodejs/node/commit/03b20a73b9)] - **benchmark**: add benchmark for buf.compare() (Rich Trott) [#5441](https://github.com/nodejs/node/pull/5441)
* [[`b816044845`](https://github.com/nodejs/node/commit/b816044845)] - **buffer**: remove duplicated code in fromObject (HUANG Wei) [#4948](https://github.com/nodejs/node/pull/4948)
* [[`067ce9b905`](https://github.com/nodejs/node/commit/067ce9b905)] - **build**: don't install github templates (Johan Bergström) [#5612](https://github.com/nodejs/node/pull/5612)
* [[`a1772dc515`](https://github.com/nodejs/node/commit/a1772dc515)] - **build**: update Node.js logo on OSX installer (Rod Vagg) [#5401](https://github.com/nodejs/node/pull/5401)
* [[`9058fc0383`](https://github.com/nodejs/node/commit/9058fc0383)] - **build**: correctly detect clang version (Stefan Budeanu) [#5553](https://github.com/nodejs/node/pull/5553)
* [[`1165ecc6f7`](https://github.com/nodejs/node/commit/1165ecc6f7)] - **build**: update Node.js logo on Win installer (Robert Jefe Lindstaedt) [#5531](https://github.com/nodejs/node/pull/5531)
* [[`4990ddad72`](https://github.com/nodejs/node/commit/4990ddad72)] - **build**: remove --quiet from eslint invocation (firedfox) [#5519](https://github.com/nodejs/node/pull/5519)
* [[`46a5d519dd`](https://github.com/nodejs/node/commit/46a5d519dd)] - **build**: skip msi build if WiX is not found (Tsarevich Dmitry) [#5220](https://github.com/nodejs/node/pull/5220)
* [[`dac4e64491`](https://github.com/nodejs/node/commit/dac4e64491)] - **build**: add option to select VS version (julien.waechter) [#4645](https://github.com/nodejs/node/pull/4645)
* [[`7a10fd3a56`](https://github.com/nodejs/node/commit/7a10fd3a56)] - **collaborator_guide**: clarify commit message rules (Wyatt Preul) [#5661](https://github.com/nodejs/node/pull/5661)
* [[`97e95d04c2`](https://github.com/nodejs/node/commit/97e95d04c2)] - **crypto**: PBKDF2 works with `int` not `ssize_t` (Fedor Indutny) [#5397](https://github.com/nodejs/node/pull/5397)
* [[`57b02e6a3e`](https://github.com/nodejs/node/commit/57b02e6a3e)] - **debugger**: remove unneeded callback check (Rich Trott) [#5319](https://github.com/nodejs/node/pull/5319)
* [[`19ae308867`](https://github.com/nodejs/node/commit/19ae308867)] - **deps**: update openssl config (Shigeki Ohtsu) [#5630](https://github.com/nodejs/node/pull/5630)
* [[`d7b81b5bc7`](https://github.com/nodejs/node/commit/d7b81b5bc7)] - **deps**: cherry-pick 2e4da65 from v8's 4.8 upstream (Michael Dawson) [#5293](https://github.com/nodejs/node/pull/5293)
* [[`1e05f371d6`](https://github.com/nodejs/node/commit/1e05f371d6)] - **doc**: fix typo in synchronous randomBytes example (Andrea Giammarchi) [#5781](https://github.com/nodejs/node/pull/5781)
* [[`5f54bd2088`](https://github.com/nodejs/node/commit/5f54bd2088)] - **doc**: topic blocking vs non-blocking (Jarrett Widman) [#5326](https://github.com/nodejs/node/pull/5326)
* [[`0943001563`](https://github.com/nodejs/node/commit/0943001563)] - **doc**: fix invalid path doc comments (Rich Trott) [#5797](https://github.com/nodejs/node/pull/5797)
* [[`bb423bb1e6`](https://github.com/nodejs/node/commit/bb423bb1e6)] - **doc**: update release tweet template (Jeremiah Senkpiel) [#5628](https://github.com/nodejs/node/pull/5628)
* [[`1e877f10aa`](https://github.com/nodejs/node/commit/1e877f10aa)] - **doc**: fix typo in child_process docs (Benjamin Gruenbaum) [#5681](https://github.com/nodejs/node/pull/5681)
* [[`d53dcc599b`](https://github.com/nodejs/node/commit/d53dcc599b)] - **doc**: update fansworld-claudio username on README (Claudio Rodriguez) [#5680](https://github.com/nodejs/node/pull/5680)
* [[`4332f8011e`](https://github.com/nodejs/node/commit/4332f8011e)] - **doc**: fix return value of write methods (Felix Böhm) [#5736](https://github.com/nodejs/node/pull/5736)
* [[`e572542de5`](https://github.com/nodejs/node/commit/e572542de5)] - **doc**: Add note about use of JSON.stringify() (Mithun Patel) [#5723](https://github.com/nodejs/node/pull/5723)
* [[`daf3ef66ef`](https://github.com/nodejs/node/commit/daf3ef66ef)] - **doc**: explain path.format() algorithm (Rich Trott) [#5688](https://github.com/nodejs/node/pull/5688)
* [[`f6d4982aa0`](https://github.com/nodejs/node/commit/f6d4982aa0)] - **doc**: clarify type of first argument in zlib (Kirill Fomichev) [#5685](https://github.com/nodejs/node/pull/5685)
* [[`07e71b2d44`](https://github.com/nodejs/node/commit/07e71b2d44)] - **doc**: fix typo in api/addons (Daijiro Wachi) [#5678](https://github.com/nodejs/node/pull/5678)
* [[`c6dc56175b`](https://github.com/nodejs/node/commit/c6dc56175b)] - **doc**: remove non-standard use of hyphens (Stefano Vozza)
* [[`4c92316972`](https://github.com/nodejs/node/commit/4c92316972)] - **doc**: add fansworld-claudio to collaborators (Claudio Rodriguez) [#5668](https://github.com/nodejs/node/pull/5668)
* [[`0a6e883f85`](https://github.com/nodejs/node/commit/0a6e883f85)] - **doc**: add thekemkid to collaborators (Glen Keane) [#5667](https://github.com/nodejs/node/pull/5667)
* [[`39c7d8a972`](https://github.com/nodejs/node/commit/39c7d8a972)] - **doc**: add AndreasMadsen to collaborators (Andreas Madsen) [#5666](https://github.com/nodejs/node/pull/5666)
* [[`eec3008970`](https://github.com/nodejs/node/commit/eec3008970)] - **doc**: add whitlockjc to collaborators (Jeremy Whitlock) [#5665](https://github.com/nodejs/node/pull/5665)
* [[`e5f254d83c`](https://github.com/nodejs/node/commit/e5f254d83c)] - **doc**: add benjamingr to collaborator list (Benjamin Gruenbaum) [#5664](https://github.com/nodejs/node/pull/5664)
* [[`3f718643c9`](https://github.com/nodejs/node/commit/3f718643c9)] - **doc**: add phillipj to collaborators (Phillip Johnsen) [#5663](https://github.com/nodejs/node/pull/5663)
* [[`2d5527fe69`](https://github.com/nodejs/node/commit/2d5527fe69)] - **doc**: add mattloring to collaborators (Matt Loring) [#5662](https://github.com/nodejs/node/pull/5662)
* [[`51763462bc`](https://github.com/nodejs/node/commit/51763462bc)] - **doc**: include typo in 'unhandledRejection' example (Robert C Jensen) [#5654](https://github.com/nodejs/node/pull/5654)
* [[`cae5da2f0a`](https://github.com/nodejs/node/commit/cae5da2f0a)] - **doc**: fix markdown links (Steve Mao) [#5641](https://github.com/nodejs/node/pull/5641)
* [[`b1b17efcb7`](https://github.com/nodejs/node/commit/b1b17efcb7)] - **doc**: move build instructions to a new document (Johan Bergström) [#5634](https://github.com/nodejs/node/pull/5634)
* [[`13a8bde1fa`](https://github.com/nodejs/node/commit/13a8bde1fa)] - **doc**: fix dns.resolveCname description typo (axvm) [#5622](https://github.com/nodejs/node/pull/5622)
* [[`1faea43c40`](https://github.com/nodejs/node/commit/1faea43c40)] - **doc**: fix typo in fs.symlink (Michaël Zasso) [#5560](https://github.com/nodejs/node/pull/5560)
* [[`98a1bb6989`](https://github.com/nodejs/node/commit/98a1bb6989)] - **doc**: document directories in test directory (Michael Barrett) [#5557](https://github.com/nodejs/node/pull/5557)
* [[`04d3f8a741`](https://github.com/nodejs/node/commit/04d3f8a741)] - **doc**: update link green to match homepage (silverwind) [#5548](https://github.com/nodejs/node/pull/5548)
* [[`1afab6ac9c`](https://github.com/nodejs/node/commit/1afab6ac9c)] - **doc**: add clarification on birthtime in fs stat (Kári Tristan Helgason) [#5479](https://github.com/nodejs/node/pull/5479)
* [[`d871ae2349`](https://github.com/nodejs/node/commit/d871ae2349)] - **doc**: fix typo in child_process documentation (Evan Lucas) [#5474](https://github.com/nodejs/node/pull/5474)
* [[`97a18bdbad`](https://github.com/nodejs/node/commit/97a18bdbad)] - **doc**: update NAN urls in ROADMAP.md and doc/releases.md (ronkorving) [#5472](https://github.com/nodejs/node/pull/5472)
* [[`d4a1fc7acd`](https://github.com/nodejs/node/commit/d4a1fc7acd)] - **doc**: add Testing WG (Rich Trott) [#5461](https://github.com/nodejs/node/pull/5461)
* [[`1642078580`](https://github.com/nodejs/node/commit/1642078580)] - **doc**: fix crypto function indentation level (Brian White) [#5460](https://github.com/nodejs/node/pull/5460)
* [[`2b0c7ad985`](https://github.com/nodejs/node/commit/2b0c7ad985)] - **doc**: fix links in tls, cluster docs (Alexander Makarenko) [#5364](https://github.com/nodejs/node/pull/5364)
* [[`901dbabea6`](https://github.com/nodejs/node/commit/901dbabea6)] - **doc**: fix relative links in net docs (Evan Lucas) [#5358](https://github.com/nodejs/node/pull/5358)
* [[`38d429172d`](https://github.com/nodejs/node/commit/38d429172d)] - **doc**: fix typo in pbkdf2Sync code sample (Marc Cuva) [#5306](https://github.com/nodejs/node/pull/5306)
* [[`d4cfc6f97c`](https://github.com/nodejs/node/commit/d4cfc6f97c)] - **doc**: add missing property in cluster example (Rafael Cepeda) [#5305](https://github.com/nodejs/node/pull/5305)
* [[`b66d6b1458`](https://github.com/nodejs/node/commit/b66d6b1458)] - **doc**: improve httpVersionMajor / httpVersionMajor (Jackson Tian) [#5296](https://github.com/nodejs/node/pull/5296)
* [[`70c872c9c4`](https://github.com/nodejs/node/commit/70c872c9c4)] - **doc**: improve unhandledException doc copy (James M Snell) [#5287](https://github.com/nodejs/node/pull/5287)
* [[`ba5e0b6110`](https://github.com/nodejs/node/commit/ba5e0b6110)] - **doc**: fix buf.readInt16LE output (Chinedu Francis Nwafili) [#5282](https://github.com/nodejs/node/pull/5282)
* [[`1624d5b049`](https://github.com/nodejs/node/commit/1624d5b049)] - **doc**: document base64url encoding support (Tristan Slominski) [#5243](https://github.com/nodejs/node/pull/5243)
* [[`b1d580c9d2`](https://github.com/nodejs/node/commit/b1d580c9d2)] - **doc**: update removeListener behaviour (Vaibhav) [#5201](https://github.com/nodejs/node/pull/5201)
* [[`ca17f91ba8`](https://github.com/nodejs/node/commit/ca17f91ba8)] - **doc**: add note for binary safe string reading (Anton Andesen) [#5155](https://github.com/nodejs/node/pull/5155)
* [[`0830bb4950`](https://github.com/nodejs/node/commit/0830bb4950)] - **doc**: clarify when writable.write callback is called (Kevin Locke) [#4810](https://github.com/nodejs/node/pull/4810)
* [[`17a74305c8`](https://github.com/nodejs/node/commit/17a74305c8)] - **doc**: add info to docs on how to submit docs patch (Sequoia McDowell) [#4591](https://github.com/nodejs/node/pull/4591)
* [[`470a9ca909`](https://github.com/nodejs/node/commit/470a9ca909)] - **doc**: add onboarding resources (Jeremiah Senkpiel) [#3726](https://github.com/nodejs/node/pull/3726)
* [[`3168e6b486`](https://github.com/nodejs/node/commit/3168e6b486)] - **doc**: update V8 URL (Craig Akimoto) [#5530](https://github.com/nodejs/node/pull/5530)
* [[`04d16eb7e8`](https://github.com/nodejs/node/commit/04d16eb7e8)] - **doc**: document fs.datasync(Sync) (Ron Korving) [#5402](https://github.com/nodejs/node/pull/5402)
* [[`29646200f8`](https://github.com/nodejs/node/commit/29646200f8)] - **doc**: add Evan Lucas to the CTC (Rod Vagg)
* [[`a2a32b7810`](https://github.com/nodejs/node/commit/a2a32b7810)] - **doc**: add Rich Trott to the CTC (Rod Vagg) [#5276](https://github.com/nodejs/node/pull/5276)
* [[`4e469d5e47`](https://github.com/nodejs/node/commit/4e469d5e47)] - **doc**: add Ali Ijaz Sheikh to the CTC (Rod Vagg) [#5277](https://github.com/nodejs/node/pull/5277)
* [[`d09b44f59b`](https://github.com/nodejs/node/commit/d09b44f59b)] - **doc**: add Сковорода Никита Андреевич to the CTC (Rod Vagg) [#5278](https://github.com/nodejs/node/pull/5278)
* [[`ebbc64bc97`](https://github.com/nodejs/node/commit/ebbc64bc97)] - **doc**: add "building node with ninja" guide (Jeremiah Senkpiel) [#4767](https://github.com/nodejs/node/pull/4767)
* [[`67245fa0e3`](https://github.com/nodejs/node/commit/67245fa0e3)] - **doc**: clarify code of conduct reporting (Julie Pagano) [#5107](https://github.com/nodejs/node/pull/5107)
* [[`cd78ff9706`](https://github.com/nodejs/node/commit/cd78ff9706)] - **doc**: fix links in Addons docs (Alexander Makarenko) [#5072](https://github.com/nodejs/node/pull/5072)
* [[`20539954ff`](https://github.com/nodejs/node/commit/20539954ff)] - **docs**: fix man pages link if tok type is code (Mithun Patel) [#5721](https://github.com/nodejs/node/pull/5721)
* [[`38d7b0b6ea`](https://github.com/nodejs/node/commit/38d7b0b6ea)] - **docs**: update link to iojs+release ci job (Myles Borins) [#5632](https://github.com/nodejs/node/pull/5632)
* [[`f982632f90`](https://github.com/nodejs/node/commit/f982632f90)] - **http**: remove old, confusing comment (Brian White) [#5233](https://github.com/nodejs/node/pull/5233)
* [[`ca5d7a8bb6`](https://github.com/nodejs/node/commit/ca5d7a8bb6)] - **http**: remove unnecessary check (Brian White) [#5233](https://github.com/nodejs/node/pull/5233)
* [[`2ce83bd8f9`](https://github.com/nodejs/node/commit/2ce83bd8f9)] - **http,util**: fix typos in comments (Alexander Makarenko) [#5279](https://github.com/nodejs/node/pull/5279)
* [[`b690916e5a`](https://github.com/nodejs/node/commit/b690916e5a)] - **lib**: freelist: use .pop() for allocation (Anton Khlynovskiy) [#2174](https://github.com/nodejs/node/pull/2174)
* [[`e7f45f0a17`](https://github.com/nodejs/node/commit/e7f45f0a17)] - **repl**: handle quotes within regexp literal (Prince J Wesley) [#5117](https://github.com/nodejs/node/pull/5117)
* [[`7c3b844f78`](https://github.com/nodejs/node/commit/7c3b844f78)] - **src**: return UV_EAI_NODATA on empty lookup (cjihrig) [#4715](https://github.com/nodejs/node/pull/4715)
* [[`242a65e930`](https://github.com/nodejs/node/commit/242a65e930)] - **stream**: prevent object map change in TransformState (Evan Lucas) [#5032](https://github.com/nodejs/node/pull/5032)
* [[`fb5ba6b928`](https://github.com/nodejs/node/commit/fb5ba6b928)] - **stream**: prevent object map change in ReadableState (Evan Lucas) [#4761](https://github.com/nodejs/node/pull/4761)
* [[`04db9efd78`](https://github.com/nodejs/node/commit/04db9efd78)] - **stream**: fix no data on partial decode (Brian White) [#5226](https://github.com/nodejs/node/pull/5226)
* [[`cc0e36ff98`](https://github.com/nodejs/node/commit/cc0e36ff98)] - **string_decoder**: fix performance regression (Brian White) [#5134](https://github.com/nodejs/node/pull/5134)
* [[`666d3690d8`](https://github.com/nodejs/node/commit/666d3690d8)] - **test**: eval a strict function (Kári Tristan Helgason) [#5250](https://github.com/nodejs/node/pull/5250)
* [[`9952bcf203`](https://github.com/nodejs/node/commit/9952bcf203)] - **test**: bug repro for vm function redefinition (cjihrig) [#5528](https://github.com/nodejs/node/pull/5528)
* [[`063f22f1f0`](https://github.com/nodejs/node/commit/063f22f1f0)] - **test**: check memoryUsage properties The properties on memoryUsage were not checked before, this commit checks them. (Wyatt Preul) [#5546](https://github.com/nodejs/node/pull/5546)
* [[`7a0fcfc127`](https://github.com/nodejs/node/commit/7a0fcfc127)] - **test**: remove broken debugger scenarios (Rich Trott) [#5532](https://github.com/nodejs/node/pull/5532)
* [[`ba9ad2662c`](https://github.com/nodejs/node/commit/ba9ad2662c)] - **test**: apply Linux workaround to Linux only (Rich Trott) [#5471](https://github.com/nodejs/node/pull/5471)
* [[`4aa2c03d31`](https://github.com/nodejs/node/commit/4aa2c03d31)] - **test**: increase timeout for test-tls-fast-writing (Rich Trott) [#5466](https://github.com/nodejs/node/pull/5466)
* [[`b4ef644ce4`](https://github.com/nodejs/node/commit/b4ef644ce4)] - **test**: retry on known SmartOS bug (Rich Trott) [#5454](https://github.com/nodejs/node/pull/5454)
* [[`d681bf24b5`](https://github.com/nodejs/node/commit/d681bf24b5)] - **test**: fix flaky child-process-fork-regr-gh-2847 (Santiago Gimeno) [#5422](https://github.com/nodejs/node/pull/5422)
* [[`b4fbe04514`](https://github.com/nodejs/node/commit/b4fbe04514)] - **test**: fix test-timers.reliability on OS X (Rich Trott) [#5379](https://github.com/nodejs/node/pull/5379)
* [[`99269ffdbf`](https://github.com/nodejs/node/commit/99269ffdbf)] - **test**: increase timeouts on some unref timers tests (Jeremiah Senkpiel) [#5352](https://github.com/nodejs/node/pull/5352)
* [[`85f927a774`](https://github.com/nodejs/node/commit/85f927a774)] - **test**: prevent flakey test on pi2 (Trevor Norris) [#5537](https://github.com/nodejs/node/pull/5537)
* [[`c86902d800`](https://github.com/nodejs/node/commit/c86902d800)] - **test**: mitigate flaky test-http-agent (Rich Trott) [#5346](https://github.com/nodejs/node/pull/5346)
* [[`f242e62817`](https://github.com/nodejs/node/commit/f242e62817)] - **test**: remove flaky designation from fixed tests (Rich Trott) [#5459](https://github.com/nodejs/node/pull/5459)
* [[`a39aacf035`](https://github.com/nodejs/node/commit/a39aacf035)] - **test**: refactor test-dgram-udp4 (Santiago Gimeno) [#5339](https://github.com/nodejs/node/pull/5339)
* [[`6386f62221`](https://github.com/nodejs/node/commit/6386f62221)] - **test**: remove unneeded bind() and related comments (Aayush Naik) [#5023](https://github.com/nodejs/node/pull/5023)
* [[`068b0cbd12`](https://github.com/nodejs/node/commit/068b0cbd12)] - **test**: move cluster tests to parallel (Rich Trott) [#4774](https://github.com/nodejs/node/pull/4774)
* [[`a673c9ae2d`](https://github.com/nodejs/node/commit/a673c9ae2d)] - **tls**: fix assert in context._external accessor (Ben Noordhuis) [#5521](https://github.com/nodejs/node/pull/5521)
* [[`8ffef48fee`](https://github.com/nodejs/node/commit/8ffef48fee)] - **tools**: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) [nodejs/node#1325](https://github.com/nodejs/node/pull/1325)
* [[`4b6a8f4321`](https://github.com/nodejs/node/commit/4b6a8f4321)] - **tools**: update gyp to b3cef02 (Imran Iqbal) [#3487](https://github.com/nodejs/node/pull/3487)
* [[`7501ddc878`](https://github.com/nodejs/node/commit/7501ddc878)] - **tools**: support testing known issues (cjihrig) [#5528](https://github.com/nodejs/node/pull/5528)
* [[`10ec1d2a6b`](https://github.com/nodejs/node/commit/10ec1d2a6b)] - **tools**: enable linting for benchmarks (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`deec8bc5f5`](https://github.com/nodejs/node/commit/deec8bc5f5)] - **tools**: reduce verbosity of cpplint (Sakthipriyan Vairamani) [#5578](https://github.com/nodejs/node/pull/5578)
* [[`64d5752711`](https://github.com/nodejs/node/commit/64d5752711)] - **tools**: enable no-self-assign ESLint rule (Rich Trott) [#5552](https://github.com/nodejs/node/pull/5552)
* [[`131ed494e2`](https://github.com/nodejs/node/commit/131ed494e2)] - **tools**: enable no-extra-parens in ESLint (Rich Trott) [#5512](https://github.com/nodejs/node/pull/5512)
* [[`d4b9f02fdc`](https://github.com/nodejs/node/commit/d4b9f02fdc)] - **tools**: apply custom buffer lint rule to /lib only (Rich Trott) [#5371](https://github.com/nodejs/node/pull/5371)
* [[`6867bed4c4`](https://github.com/nodejs/node/commit/6867bed4c4)] - **tools**: enable additional lint rules (Rich Trott) [#5357](https://github.com/nodejs/node/pull/5357)
* [[`5e6b7605ee`](https://github.com/nodejs/node/commit/5e6b7605ee)] - **tools**: add Node.js-specific ESLint rules (Rich Trott) [#5320](https://github.com/nodejs/node/pull/5320)
* [[`6dc49ae203`](https://github.com/nodejs/node/commit/6dc49ae203)] - **tools,benchmark**: increase lint compliance (Rich Trott) [#5773](https://github.com/nodejs/node/pull/5773)
* [[`dff7091fce`](https://github.com/nodejs/node/commit/dff7091fce)] - **url**: group slashed protocols by protocol name (nettofarah) [#5380](https://github.com/nodejs/node/pull/5380)
* [[`0e97a3ea51`](https://github.com/nodejs/node/commit/0e97a3ea51)] - **win,build**: support Visual C++ Build Tools 2015 (João Reis) [#5627](https://github.com/nodejs/node/pull/5627)

## 2016-03-16, Version 5.9.0 (Stable), @evanlucas

### Notable changes

* **contextify**: Fixed a memory consumption issue related to heavy use of `vm.createContext` and `vm.runInNewContext`. (Ali Ijaz Sheikh)
https://github.com/nodejs/node/pull/5392
* **governance**: The following members have been added as collaborators:
 - Andreas Madsen (@AndreasMadsen)
 - Benjamin Gruenbaum (@benjamingr)
 - Claudio Rodriguez (@claudiorodriguez)
 - Glen Keane (@thekemkid)
 - Jeremy Whitlock (@whitlockjc)
 - Matt Loring (@matthewloring)
 - Phillip Johnsen (@phillipj)
* **lib**: copy arguments object instead of leaking it (Nathan Woltman)
https://github.com/nodejs/node/pull/4361
* **src**: allow both -i and -e flags to be used at the same time (Rich Trott)
https://github.com/nodejs/node/pull/5655
* **timers**: Internal Node.js timeouts now use the same logic path as those created with `setTimeout()` (Jeremiah Senkpiel) [#4007](https://github.com/nodejs/node/pull/4007)
  * This may cause a slightly different performance profile in some situations. So far, it has shown to be positive in most cases.
* **v8**: backport fb4ccae from v8 upstream (Vladimir Krivosheev) #4231
  -  breakout events from v8 to offer better support for external debuggers
* **zlib**: add support for concatenated members (Kári Tristan Helgason)
https://github.com/nodejs/node/pull/5120
  * Previously, if multiple members were in the same archive, only the first would be read. The others are no longer thrown away.

### Commits

* [[`03b99bf8b9`](https://github.com/nodejs/node/commit/03b99bf8b9)] - **build**: don't install github templates (Johan Bergström) [#5612](https://github.com/nodejs/node/pull/5612)
* [[`a7819da15a`](https://github.com/nodejs/node/commit/a7819da15a)] - ***Revert*** "**build**: run lint before tests" (Rich Trott) [#5602](https://github.com/nodejs/node/pull/5602)
* [[`5e9cac4333`](https://github.com/nodejs/node/commit/5e9cac4333)] - **console**: check that stderr is writable (Rich Trott) [#5635](https://github.com/nodejs/node/pull/5635)
* [[`0662fcf209`](https://github.com/nodejs/node/commit/0662fcf209)] - **contextify**: cache sandbox and context in locals (Ali Ijaz Sheikh) [#5392](https://github.com/nodejs/node/pull/5392)
* [[`4f2c839d46`](https://github.com/nodejs/node/commit/4f2c839d46)] - **contextify**: replace deprecated SetWeak usage (Ali Ijaz Sheikh) [#5392](https://github.com/nodejs/node/pull/5392)
* [[`bfff07b4dd`](https://github.com/nodejs/node/commit/bfff07b4dd)] - **contextify**: cleanup weak ref for sandbox (Ali Ijaz Sheikh) [#5392](https://github.com/nodejs/node/pull/5392)
* [[`93f60cdc54`](https://github.com/nodejs/node/commit/93f60cdc54)] - **contextify**: cleanup weak ref for global proxy (Ali Ijaz Sheikh) [#5392](https://github.com/nodejs/node/pull/5392)
* [[`b6c355de0d`](https://github.com/nodejs/node/commit/b6c355de0d)] - **(SEMVER-MINOR)** **deps**: backport fb4ccae from v8 upstream (develar) [#4231](https://github.com/nodejs/node/pull/4231)
* [[`29510aa4fd`](https://github.com/nodejs/node/commit/29510aa4fd)] - **deps**: update openssl config (Shigeki Ohtsu) [#5630](https://github.com/nodejs/node/pull/5630)
* [[`532d1bf9ce`](https://github.com/nodejs/node/commit/532d1bf9ce)] - **deps**: sync deps/http_parser with nodejs/http_parser (James M Snell) [#5600](https://github.com/nodejs/node/pull/5600)
* [[`d5d64c327b`](https://github.com/nodejs/node/commit/d5d64c327b)] - **doc**: clarify commit message rules (Wyatt Preul) [#5661](https://github.com/nodejs/node/pull/5661)
* [[`8c4c84fe5b`](https://github.com/nodejs/node/commit/8c4c84fe5b)] - **doc**: add Testing WG (Rich Trott) [#5461](https://github.com/nodejs/node/pull/5461)
* [[`434af03825`](https://github.com/nodejs/node/commit/434af03825)] - **doc**: Add note about use of JSON.stringify() (Mithun Patel) [#5723](https://github.com/nodejs/node/pull/5723)
* [[`62926d85bd`](https://github.com/nodejs/node/commit/62926d85bd)] - **doc**: clarify type of first argument in zlib (Kirill Fomichev) [#5685](https://github.com/nodejs/node/pull/5685)
* [[`eb73574349`](https://github.com/nodejs/node/commit/eb73574349)] - **doc**: clarify when writable.write callback is called (Kevin Locke) [#4810](https://github.com/nodejs/node/pull/4810)
* [[`c579507034`](https://github.com/nodejs/node/commit/c579507034)] - **doc**: fix typo in api/addons (Daijiro Wachi) [#5678](https://github.com/nodejs/node/pull/5678)
* [[`8e45c9d9ea`](https://github.com/nodejs/node/commit/8e45c9d9ea)] - **doc**: fix typo in api/dgram (Daijiro Wachi) [#5678](https://github.com/nodejs/node/pull/5678)
* [[`44a9b100c5`](https://github.com/nodejs/node/commit/44a9b100c5)] - **doc**: fix typo in api/fs (Daijiro Wachi) [#5678](https://github.com/nodejs/node/pull/5678)
* [[`b667573bcb`](https://github.com/nodejs/node/commit/b667573bcb)] - **doc**: update fansworld-claudio username on README (Claudio Rodriguez) [#5680](https://github.com/nodejs/node/pull/5680)
* [[`9794abb5d1`](https://github.com/nodejs/node/commit/9794abb5d1)] - **doc**: add onboarding resources (Jeremiah Senkpiel) [#3726](https://github.com/nodejs/node/pull/3726)
* [[`31e39fbd7a`](https://github.com/nodejs/node/commit/31e39fbd7a)] - **doc**: remove non-standard use of hyphens (Stefano Vozza)
* [[`f3e9daa825`](https://github.com/nodejs/node/commit/f3e9daa825)] - **doc**: add clarification on birthtime in fs stat (Kári Tristan Helgason) [#5479](https://github.com/nodejs/node/pull/5479)
* [[`c379ec6522`](https://github.com/nodejs/node/commit/c379ec6522)] - **doc**: move build instructions to a new document (Johan Bergström) [#5634](https://github.com/nodejs/node/pull/5634)
* [[`2a442b3dfc`](https://github.com/nodejs/node/commit/2a442b3dfc)] - **doc**: update removeListener behaviour (Vaibhav) [#5201](https://github.com/nodejs/node/pull/5201)
* [[`f6ee0996e0`](https://github.com/nodejs/node/commit/f6ee0996e0)] - **doc**: fix typo in child_process docs (Benjamin Gruenbaum) [#5681](https://github.com/nodejs/node/pull/5681)
* [[`dd12661173`](https://github.com/nodejs/node/commit/dd12661173)] - **doc**: include typo in 'unhandledRejection' example (Robert C Jensen) [#5654](https://github.com/nodejs/node/pull/5654)
* [[`f7aecd6e94`](https://github.com/nodejs/node/commit/f7aecd6e94)] - **doc**: add thekemkid to collaborators (Glen Keane) [#5667](https://github.com/nodejs/node/pull/5667)
* [[`b81711acfb`](https://github.com/nodejs/node/commit/b81711acfb)] - **doc**: add phillipj to collaborators (Phillip Johnsen) [#5663](https://github.com/nodejs/node/pull/5663)
* [[`a33f2486f0`](https://github.com/nodejs/node/commit/a33f2486f0)] - **doc**: add fansworld-claudio to collaborators (Claudio Rodriguez) [#5668](https://github.com/nodejs/node/pull/5668)
* [[`285d5e7ba6`](https://github.com/nodejs/node/commit/285d5e7ba6)] - **doc**: add AndreasMadsen to collaborators (Andreas Madsen) [#5666](https://github.com/nodejs/node/pull/5666)
* [[`8e1f6706e3`](https://github.com/nodejs/node/commit/8e1f6706e3)] - **doc**: add benjamingr to collaborator list (Benjamin Gruenbaum) [#5664](https://github.com/nodejs/node/pull/5664)
* [[`f7842cbb24`](https://github.com/nodejs/node/commit/f7842cbb24)] - **doc**: add whitlockjc to collaborators (Jeremy Whitlock) [#5665](https://github.com/nodejs/node/pull/5665)
* [[`dd6f4ec2e4`](https://github.com/nodejs/node/commit/dd6f4ec2e4)] - **doc**: add mattloring to collaborators (Matt Loring) [#5662](https://github.com/nodejs/node/pull/5662)
* [[`9ebd559a55`](https://github.com/nodejs/node/commit/9ebd559a55)] - **doc**: fix markdown links (Steve Mao) [#5641](https://github.com/nodejs/node/pull/5641)
* [[`62d267e1ff`](https://github.com/nodejs/node/commit/62d267e1ff)] - **doc**: fix dns.resolveCname description typo (axvm) [#5622](https://github.com/nodejs/node/pull/5622)
* [[`9f8e2e2979`](https://github.com/nodejs/node/commit/9f8e2e2979)] - **doc**: update release tweet template (Jeremiah Senkpiel) [#5628](https://github.com/nodejs/node/pull/5628)
* [[`4d6fe300fe`](https://github.com/nodejs/node/commit/4d6fe300fe)] - **doc**: fix v5.8.0 changelog heading (Jeremiah Senkpiel) [#5559](https://github.com/nodejs/node/pull/5559)
* [[`4c1fdaeb2a`](https://github.com/nodejs/node/commit/4c1fdaeb2a)] - **docs**: update link to iojs+release ci job (Myles Borins) [#5632](https://github.com/nodejs/node/pull/5632)
* [[`205bed0bec`](https://github.com/nodejs/node/commit/205bed0bec)] - **lib**: copy arguments object instead of leaking it (Nathan Woltman) [#4361](https://github.com/nodejs/node/pull/4361)
* [[`b16f67a0b9`](https://github.com/nodejs/node/commit/b16f67a0b9)] - **net**: make `isIPv4` and `isIPv6` more efficient (Vladimir Kurchatkin) [#5478](https://github.com/nodejs/node/pull/5478)
* [[`4ecd996baa`](https://github.com/nodejs/node/commit/4ecd996baa)] - **(SEMVER-MINOR)** **src**: allow combination of -i and -e cli flags (Rich Trott) [#5655](https://github.com/nodejs/node/pull/5655)
* [[`f225459496`](https://github.com/nodejs/node/commit/f225459496)] - **test**: improve test-npm-install (Santiago Gimeno) [#5613](https://github.com/nodejs/node/pull/5613)
* [[`cceae5ae78`](https://github.com/nodejs/node/commit/cceae5ae78)] - **test**: eval a strict function (Kári Tristan Helgason) [#5250](https://github.com/nodejs/node/pull/5250)
* [[`9a44c8c337`](https://github.com/nodejs/node/commit/9a44c8c337)] - **test**: add batch of known issue tests (cjihrig) [#5653](https://github.com/nodejs/node/pull/5653)
* [[`1b7b1ed2c9`](https://github.com/nodejs/node/commit/1b7b1ed2c9)] - **timers**: greatly improve code comments (Jeremiah Senkpiel) [#4007](https://github.com/nodejs/node/pull/4007)
* [[`769254b0ba`](https://github.com/nodejs/node/commit/769254b0ba)] - **timers**: refactor timers (Jeremiah Senkpiel) [#4007](https://github.com/nodejs/node/pull/4007)
* [[`0b545fb3f8`](https://github.com/nodejs/node/commit/0b545fb3f8)] - **win,build**: support Visual C++ Build Tools 2015 (João Reis) [#5627](https://github.com/nodejs/node/pull/5627)
* [[`ef774ff9a8`](https://github.com/nodejs/node/commit/ef774ff9a8)] - **(SEMVER-MINOR)** **zlib**: add support for concatenated members (Kári Tristan Helgason) [#5120](https://github.com/nodejs/node/pull/5120)

## 2016-03-08, Version 5.8.0 (Stable), @Fishrock123

### Notable changes

* **child_process**: `send()` now accepts an options parameter (cjihrig) [#5283](https://github.com/nodejs/node/pull/5283).
  - Currently the only option is `keepOpen`, which keeps the underlying socket open after the message is sent.
* **constants**: `ENGINE_METHOD_RSA` is now correctly exposed (Sam Roberts) [#5463](https://github.com/nodejs/node/pull/5463).
* Fixed two regressions which originated in v5.7.0:
  - **http**: Errors inside of http client callbacks now propagate correctly (Trevor Norris) [#5591](https://github.com/nodejs/node/pull/5591).
  - **path**: Fixed normalization of absolute paths (Evan Lucas) [#5589](https://github.com/nodejs/node/pull/5589).
* **repl**: `start()` no longer requires an options parameter (cjihrig) [#5388](https://github.com/nodejs/node/pull/5388).
* **util**: Improved `format()` performance 50-300% (Evan Lucas) [#5360](https://github.com/nodejs/node/pull/5360).

### Commits

* [[`12ca84fc7f`](https://github.com/nodejs/node/commit/12ca84fc7f)] - **benchmark**: add util.format benchmark (Evan Lucas) [#5360](https://github.com/nodejs/node/pull/5360)
* [[`b955d02266`](https://github.com/nodejs/node/commit/b955d02266)] - **benchmark**: fix lint errors (Rich Trott) [#5517](https://github.com/nodejs/node/pull/5517)
* [[`2abf866b6e`](https://github.com/nodejs/node/commit/2abf866b6e)] - **build**: update Node.js logo on Win installer (Robert Jefe Lindstaedt) [#5531](https://github.com/nodejs/node/pull/5531)
* [[`86900f8f2b`](https://github.com/nodejs/node/commit/86900f8f2b)] - **build**: correctly detect clang version (Stefan Budeanu) [#5553](https://github.com/nodejs/node/pull/5553)
* [[`a3017992e4`](https://github.com/nodejs/node/commit/a3017992e4)] - **(SEMVER-MINOR)** **child_process**: add keepOpen option to send() (cjihrig) [#5283](https://github.com/nodejs/node/pull/5283)
* [[`6d4887ccc2`](https://github.com/nodejs/node/commit/6d4887ccc2)] - **(SEMVER-MINOR)** **child_process**: support options in send() (cjihrig) [#5283](https://github.com/nodejs/node/pull/5283)
* [[`9db827c7aa`](https://github.com/nodejs/node/commit/9db827c7aa)] - **(SEMVER-MINOR)** **constants**: define ENGINE_METHOD_RSA (Sam Roberts) [#5463](https://github.com/nodejs/node/pull/5463)
* [[`85013456cd`](https://github.com/nodejs/node/commit/85013456cd)] - **deps**: upgrade to npm 3.7.3 (Kat Marchán) [#5369](https://github.com/nodejs/node/pull/5369)
* [[`67e9f65958`](https://github.com/nodejs/node/commit/67e9f65958)] - **dgram**: default send address to 127.0.0.1 or ::1 (Matteo Collina) [#5493](https://github.com/nodejs/node/pull/5493)
* [[`3c92352c8c`](https://github.com/nodejs/node/commit/3c92352c8c)] - **doc**: document directories in test directory (Michael Barrett) [#5557](https://github.com/nodejs/node/pull/5557)
* [[`7be726f86a`](https://github.com/nodejs/node/commit/7be726f86a)] - **doc**: add info to docs on how to submit docs patch (Sequoia McDowell) [#4591](https://github.com/nodejs/node/pull/4591)
* [[`eb5a95e04a`](https://github.com/nodejs/node/commit/eb5a95e04a)] - **doc**: fix typo in fs.symlink (Michaël Zasso) [#5560](https://github.com/nodejs/node/pull/5560)
* [[`9ad901ef44`](https://github.com/nodejs/node/commit/9ad901ef44)] - **doc**: improve unhandledException doc copy (James M Snell) [#5287](https://github.com/nodejs/node/pull/5287)
* [[`3bd96fdb0f`](https://github.com/nodejs/node/commit/3bd96fdb0f)] - **doc**: update link green to match homepage (silverwind) [#5548](https://github.com/nodejs/node/pull/5548)
* [[`cb7e4fbac9`](https://github.com/nodejs/node/commit/cb7e4fbac9)] - **doc**: update V8 URL (Craig Akimoto) [#5530](https://github.com/nodejs/node/pull/5530)
* [[`b54a26fa61`](https://github.com/nodejs/node/commit/b54a26fa61)] - **(SEMVER-MINOR)** **doc**: correct name of engine methods (Sam Roberts) [#5463](https://github.com/nodejs/node/pull/5463)
* [[`f3971f5817`](https://github.com/nodejs/node/commit/f3971f5817)] - **path**: fix normalize for absolutes (Evan Lucas) [#5589](https://github.com/nodejs/node/pull/5589)
* [[`e572e421b4`](https://github.com/nodejs/node/commit/e572e421b4)] - **(SEMVER-MINOR)** **repl**: accept no arguments to start() (cjihrig) [#5388](https://github.com/nodejs/node/pull/5388)
* [[`5e6d706758`](https://github.com/nodejs/node/commit/5e6d706758)] - **src,http**: fix uncaughtException miss in http (Trevor Norris) [#5591](https://github.com/nodejs/node/pull/5591)
* [[`9dc94d7b09`](https://github.com/nodejs/node/commit/9dc94d7b09)] - **test**: add test-npm-install to parallel tests suite (Myles Borins) [#5166](https://github.com/nodejs/node/pull/5166)
* [[`4f20f31b3e`](https://github.com/nodejs/node/commit/4f20f31b3e)] - **test**: remove broken debugger scenarios (Rich Trott) [#5532](https://github.com/nodejs/node/pull/5532)
* [[`29e26b38c5`](https://github.com/nodejs/node/commit/29e26b38c5)] - **test**: bug repro for vm function redefinition (cjihrig) [#5528](https://github.com/nodejs/node/pull/5528)
* [[`e6210d5f50`](https://github.com/nodejs/node/commit/e6210d5f50)] - **test**: prevent flakey test on pi2 (Trevor Norris) [#5537](https://github.com/nodejs/node/pull/5537)
* [[`40b36baa2f`](https://github.com/nodejs/node/commit/40b36baa2f)] - **test**: check memoryUsage properties (Wyatt Preul) [#5546](https://github.com/nodejs/node/pull/5546)
* [[`048c0f4738`](https://github.com/nodejs/node/commit/048c0f4738)] - **tools**: reduce verbosity of cpplint (Sakthipriyan Vairamani) [#5578](https://github.com/nodejs/node/pull/5578)
* [[`7965c897e0`](https://github.com/nodejs/node/commit/7965c897e0)] - **tools**: enable no-self-assign ESLint rule (Rich Trott) [#5552](https://github.com/nodejs/node/pull/5552)
* [[`5aa17dc136`](https://github.com/nodejs/node/commit/5aa17dc136)] - **tools**: support testing known issues (cjihrig) [#5528](https://github.com/nodejs/node/pull/5528)
* [[`9a3e87e9a8`](https://github.com/nodejs/node/commit/9a3e87e9a8)] - **tools**: enable linting for benchmarks (Rich Trott) [#5517](https://github.com/nodejs/node/pull/5517)
* [[`c4fa2a6715`](https://github.com/nodejs/node/commit/c4fa2a6715)] - **tools**: enable no-extra-parens in ESLint (Rich Trott) [#5512](https://github.com/nodejs/node/pull/5512)
* [[`971edde0cb`](https://github.com/nodejs/node/commit/971edde0cb)] - **util**: improve format() performance further (Brian White) [#5360](https://github.com/nodejs/node/pull/5360)
* [[`c32d460747`](https://github.com/nodejs/node/commit/c32d460747)] - **util**: improve util.format performance (Evan Lucas) [#5360](https://github.com/nodejs/node/pull/5360)

## 2016-03-08, Version 4.4.0 'Argon' (LTS), @thealphanerd

In December we announced that we would be doing a minor release in order to
get a number of voted on SEMVER-MINOR changes into LTS. Our ability to release this
was delayed due to the unforeseen security release v4.3. We are quickly bumping to
v4.4 in order to bring you the features that we had committed to releasing.

This release also includes over 70 fixes to our docs and over 50 fixes to tests.

### Notable changes

The SEMVER-MINOR changes include:
  * **deps**:
    - An update to v8 that introduces a new flag --perf_basic_prof_only_functions (Ali Ijaz Sheikh) [#3609](https://github.com/nodejs/node/pull/3609)
  * **http**:
    - A new feature in http(s) agent that catches errors on *keep alived* connections (José F. Romaniello) [#4482](https://github.com/nodejs/node/pull/4482)
  * **src**:
    - Better support for Big-Endian systems (Bryon Leung) [#3410](https://github.com/nodejs/node/pull/3410)
  * **tls**:
    - A new feature that allows you to pass common SSL options to `tls.createSecurePair` (Коренберг Марк) [#2441](https://github.com/nodejs/node/pull/2441)
  * **tools**:
    - a new flag `--prof-process` which will execute the tick processor on the provided isolate files (Matt Loring) [#4021](https://github.com/nodejs/node/pull/4021)

Notable semver patch changes include:

  * **buld**:
    - Support python path that includes spaces. This should be of particular interest to our Windows users who may have python living in `c:/Program Files` (Felix Becker) [#4841](https://github.com/nodejs/node/pull/4841)
  * **https**:
    - A potential fix for [#3692](https://github.com/nodejs/node/issues/3692) HTTP/HTTPS client requests throwing EPROTO (Fedor Indutny) [#4982](https://github.com/nodejs/node/pull/4982)
  * **installer**:
    - More readable profiling information from isolate tick logs (Matt Loring) [#3032](https://github.com/nodejs/node/pull/3032)
  * **npm**:
    - upgrade to npm 2.14.20 (Kat Marchán) [#5510](https://github.com/nodejs/node/pull/5510)
  * **process**:
    - Add support for symbols in event emitters. Symbols didn't exist when it was written ¯\_(ツ)_/¯ (cjihrig) [#4798](https://github.com/nodejs/node/pull/4798)
  * **querystring**:
    - querystring.parse() is now 13-22% faster! (Brian White) [#4675](https://github.com/nodejs/node/pull/4675)
  * **streams**:
    - performance improvements for moving small buffers that shows a 5% throughput gain. IoT projects have been seen to be as much as 10% faster with this change! (Matteo Collina) [#4354](https://github.com/nodejs/node/pull/4354)
  * **tools**:
    - eslint has been updated to version 2.1.0 (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)

### Commits

* [[`360e04fd5a`](https://github.com/nodejs/node/commit/360e04fd5a)] - internal/child_process: call postSend on error (Fedor Indutny) [#4752](https://github.com/nodejs/node/pull/4752)
* [[`a29f501aa2`](https://github.com/nodejs/node/commit/a29f501aa2)] - **benchmark**: add a constant declaration for `net` (Minwoo Jung) [#3950](https://github.com/nodejs/node/pull/3950)
* [[`85e06a2e34`](https://github.com/nodejs/node/commit/85e06a2e34)] - **(SEMVER-MINOR)** **buffer**: allow encoding param to collapse (Trevor Norris) [#4803](https://github.com/nodejs/node/pull/4803)
* [[`fe893a8ebc`](https://github.com/nodejs/node/commit/fe893a8ebc)] - **(SEMVER-MINOR)** **buffer**: properly retrieve binary length of needle (Trevor Norris) [#4803](https://github.com/nodejs/node/pull/4803)
* [[`fae7c9db3f`](https://github.com/nodejs/node/commit/fae7c9db3f)] - **buffer**: refactor redeclared variables (Rich Trott) [#4886](https://github.com/nodejs/node/pull/4886)
* [[`4a6e2b26f7`](https://github.com/nodejs/node/commit/4a6e2b26f7)] - **build**: treat aarch64 as arm64 (Johan Bergström) [#5191](https://github.com/nodejs/node/pull/5191)
* [[`bc2536dfc6`](https://github.com/nodejs/node/commit/bc2536dfc6)] - **build**: add a help message and removed a TODO. (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`f6416be5d2`](https://github.com/nodejs/node/commit/f6416be5d2)] - **build**: remove redundant TODO in configure (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`6deb7a6eb8`](https://github.com/nodejs/node/commit/6deb7a6eb8)] - **build**: remove Makefile.build (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`66d1115555`](https://github.com/nodejs/node/commit/66d1115555)] - **build**: fix build when python path contains spaces (Felix Becker) [#4841](https://github.com/nodejs/node/pull/4841)
* [[`29951cf36a`](https://github.com/nodejs/node/commit/29951cf36a)] - **child_process**: fix data loss with readable event (Brian White) [#5036](https://github.com/nodejs/node/pull/5036)
* [[`81d4127279`](https://github.com/nodejs/node/commit/81d4127279)] - **cluster**: dont rely on `this` in `fork` (Igor Klopov) [#5216](https://github.com/nodejs/node/pull/5216)
* [[`de4c07b29e`](https://github.com/nodejs/node/commit/de4c07b29e)] - **console**: apply null as `this` for util.format (Jackson Tian) [#5222](https://github.com/nodejs/node/pull/5222)
* [[`4e0755cab3`](https://github.com/nodejs/node/commit/4e0755cab3)] - **crypto**: have fixed NodeBIOs return EOF (Adam Langley) [#5105](https://github.com/nodejs/node/pull/5105)
* [[`a7955d5071`](https://github.com/nodejs/node/commit/a7955d5071)] - **crypto**: fix memory leak in LoadPKCS12 (Fedor Indutny) [#5109](https://github.com/nodejs/node/pull/5109)
* [[`5d9c1cf001`](https://github.com/nodejs/node/commit/5d9c1cf001)] - **crypto**: add `pfx` certs as CA certs too (Fedor Indutny) [#5109](https://github.com/nodejs/node/pull/5109)
* [[`ab5cb0539b`](https://github.com/nodejs/node/commit/ab5cb0539b)] - **crypto**: use SSL_CTX_clear_extra_chain_certs. (Adam Langley) [#4919](https://github.com/nodejs/node/pull/4919)
* [[`198928eb9f`](https://github.com/nodejs/node/commit/198928eb9f)] - **crypto**: fix build when OCSP-stapling not provided (Adam Langley) [#4914](https://github.com/nodejs/node/pull/4914)
* [[`b8e1089df0`](https://github.com/nodejs/node/commit/b8e1089df0)] - **crypto**: use a const SSL_CIPHER (Adam Langley) [#4913](https://github.com/nodejs/node/pull/4913)
* [[`139d6d9284`](https://github.com/nodejs/node/commit/139d6d9284)] - **debugger**: assert test before accessing this.binding (Prince J Wesley) [#5145](https://github.com/nodejs/node/pull/5145)
* [[`9c8f2ab546`](https://github.com/nodejs/node/commit/9c8f2ab546)] - **deps**: upgrade to npm 2.14.20 (Kat Marchán) [#5510](https://github.com/nodejs/node/pull/5510)
* [[`e591a0927f`](https://github.com/nodejs/node/commit/e591a0927f)] - **deps**: upgrade to npm 2.14.19 (Kat Marchán) [#5335](https://github.com/nodejs/node/pull/5335)
* [[`a5ce67a0aa`](https://github.com/nodejs/node/commit/a5ce67a0aa)] - **deps**: upgrade to npm 2.14.18 (Kat Marchán) [#5245](https://github.com/nodejs/node/pull/5245)
* [[`469db021f7`](https://github.com/nodejs/node/commit/469db021f7)] - **(SEMVER-MINOR)** **deps**: backport 9da3ab6 from V8 upstream (Ali Ijaz Sheikh) [#3609](https://github.com/nodejs/node/pull/3609)
* [[`3ca04a5de9`](https://github.com/nodejs/node/commit/3ca04a5de9)] - **deps**: backport 8d00c2c from v8 upstream (Gibson Fahnestock) [#5024](https://github.com/nodejs/node/pull/5024)
* [[`60e0bd4be9`](https://github.com/nodejs/node/commit/60e0bd4be9)] - **deps**: upgrade to npm 2.14.17 (Kat Marchán) [#5110](https://github.com/nodejs/node/pull/5110)
* [[`976b9a9ab3`](https://github.com/nodejs/node/commit/976b9a9ab3)] - **deps**: upgrade to npm 2.14.16 (Kat Marchán) [#4960](https://github.com/nodejs/node/pull/4960)
* [[`38b370abea`](https://github.com/nodejs/node/commit/38b370abea)] - **deps**: upgrade to npm 2.14.15 (Kat Marchán) [#4872](https://github.com/nodejs/node/pull/4872)
* [[`82f549ef81`](https://github.com/nodejs/node/commit/82f549ef81)] - **dgram**: scope redeclared variables (Rich Trott) [#4940](https://github.com/nodejs/node/pull/4940)
* [[`063e14b568`](https://github.com/nodejs/node/commit/063e14b568)] - **dns**: throw a TypeError in lookupService with invalid port (Evan Lucas) [#4839](https://github.com/nodejs/node/pull/4839)
* [[`a2613aefae`](https://github.com/nodejs/node/commit/a2613aefae)] - **doc**: remove out-of-date matter from internal docs (Rich Trott) [#5421](https://github.com/nodejs/node/pull/5421)
* [[`394743f4b3`](https://github.com/nodejs/node/commit/394743f4b3)] - **doc**: explicit about VS 2015 support in readme (Phillip Johnsen) [#5406](https://github.com/nodejs/node/pull/5406)
* [[`da6b26fbfb`](https://github.com/nodejs/node/commit/da6b26fbfb)] - **doc**: copyedit util doc (Rich Trott) [#5399](https://github.com/nodejs/node/pull/5399)
* [[`7070ad0cc0`](https://github.com/nodejs/node/commit/7070ad0cc0)] - **doc**: mention prototype check in deepStrictEqual() (cjihrig) [#5367](https://github.com/nodejs/node/pull/5367)
* [[`d4789fc5fd`](https://github.com/nodejs/node/commit/d4789fc5fd)] - **doc**: s/http/https in Myles Borins' GitHub link (Rod Vagg) [#5356](https://github.com/nodejs/node/pull/5356)
* [[`b86540d1eb`](https://github.com/nodejs/node/commit/b86540d1eb)] - **doc**: clarify error handling in net.createServer (Dirceu Pereira Tiegs) [#5353](https://github.com/nodejs/node/pull/5353)
* [[`3106297037`](https://github.com/nodejs/node/commit/3106297037)] - **doc**: `require` behavior on case-insensitive systems (Hugo Wood)
* [[`e0b45e4315`](https://github.com/nodejs/node/commit/e0b45e4315)] - **doc**: update repo docs to use 'CTC' (Alexis Campailla) [#5304](https://github.com/nodejs/node/pull/5304)
* [[`e355f13989`](https://github.com/nodejs/node/commit/e355f13989)] - **doc**: improvements to crypto.markdown copy (Alexander Makarenko) [#5230](https://github.com/nodejs/node/pull/5230)
* [[`a9035b5e1d`](https://github.com/nodejs/node/commit/a9035b5e1d)] - **doc**: link to man pages (dcposch@dcpos.ch) [#5073](https://github.com/nodejs/node/pull/5073)
* [[`2043e6a63c`](https://github.com/nodejs/node/commit/2043e6a63c)] - **doc**: clarify child_process.execFile{,Sync} file arg (Kevin Locke) [#5310](https://github.com/nodejs/node/pull/5310)
* [[`8c732ad1e1`](https://github.com/nodejs/node/commit/8c732ad1e1)] - **doc**: fix buf.length slice example (Chinedu Francis Nwafili) [#5259](https://github.com/nodejs/node/pull/5259)
* [[`6c27c78b8b`](https://github.com/nodejs/node/commit/6c27c78b8b)] - **doc**: fix buffer\[index\] example (Chinedu Francis Nwafili) [#5253](https://github.com/nodejs/node/pull/5253)
* [[`7765f99683`](https://github.com/nodejs/node/commit/7765f99683)] - **doc**: fix template string (Rafael Cepeda) [#5240](https://github.com/nodejs/node/pull/5240)
* [[`d15ef20162`](https://github.com/nodejs/node/commit/d15ef20162)] - **doc**: improvements to console.markdown copy (Alexander Makarenko) [#5225](https://github.com/nodejs/node/pull/5225)
* [[`593206a752`](https://github.com/nodejs/node/commit/593206a752)] - **doc**: fix net.createConnection() example (Brian White) [#5219](https://github.com/nodejs/node/pull/5219)
* [[`464636b5c5`](https://github.com/nodejs/node/commit/464636b5c5)] - **doc**: improve scrolling, various CSS tweaks (Roman Reiss) [#5198](https://github.com/nodejs/node/pull/5198)
* [[`f615cd5b0b`](https://github.com/nodejs/node/commit/f615cd5b0b)] - **doc**: console is asynchronous unless it's a file (Ben Noordhuis) [#5133](https://github.com/nodejs/node/pull/5133)
* [[`fbed0d11f1`](https://github.com/nodejs/node/commit/fbed0d11f1)] - **doc**: merging behavior of writeHead vs setHeader (Alejandro Oviedo) [#5081](https://github.com/nodejs/node/pull/5081)
* [[`b0bb42bd7d`](https://github.com/nodejs/node/commit/b0bb42bd7d)] - **doc**: fix reference to API `hash.final` (Minwoo Jung) [#5050](https://github.com/nodejs/node/pull/5050)
* [[`dee5045221`](https://github.com/nodejs/node/commit/dee5045221)] - **doc**: uppercase 'RSA-SHA256' in crypto.markdown (Rainer Oviir) [#5044](https://github.com/nodejs/node/pull/5044)
* [[`498052a017`](https://github.com/nodejs/node/commit/498052a017)] - **doc**: consistent styling for functions in TLS docs (Alexander Makarenko) [#5000](https://github.com/nodejs/node/pull/5000)
* [[`031277e6f8`](https://github.com/nodejs/node/commit/031277e6f8)] - **doc**: apply consistent styling for functions (Rich Trott) [#4974](https://github.com/nodejs/node/pull/4974)
* [[`808fe0ea48`](https://github.com/nodejs/node/commit/808fe0ea48)] - **doc**: fix `notDeepEqual` API (Minwoo Jung) [#4971](https://github.com/nodejs/node/pull/4971)
* [[`5b9025689f`](https://github.com/nodejs/node/commit/5b9025689f)] - **doc**: show links consistently in deprecations (Sakthipriyan Vairamani) [#4907](https://github.com/nodejs/node/pull/4907)
* [[`3a1865db5e`](https://github.com/nodejs/node/commit/3a1865db5e)] - **doc**: don't use "interface" as a variable name (ChALkeR) [#4900](https://github.com/nodejs/node/pull/4900)
* [[`90715c3d68`](https://github.com/nodejs/node/commit/90715c3d68)] - **doc**: keep the names in sorted order (Sakthipriyan Vairamani) [#4876](https://github.com/nodejs/node/pull/4876)
* [[`d8b3b25c9c`](https://github.com/nodejs/node/commit/d8b3b25c9c)] - **doc**: fix JSON generation for aliased methods (Timothy Gu) [#4871](https://github.com/nodejs/node/pull/4871)
* [[`7b763c8d25`](https://github.com/nodejs/node/commit/7b763c8d25)] - **doc**: fix code type of markdowns (Jackson Tian) [#4858](https://github.com/nodejs/node/pull/4858)
* [[`37d4e7afc2`](https://github.com/nodejs/node/commit/37d4e7afc2)] - **doc**: check for errors in 'listen' event (Benjamin Gruenbaum) [#4834](https://github.com/nodejs/node/pull/4834)
* [[`3f876b104c`](https://github.com/nodejs/node/commit/3f876b104c)] - **doc**: Examples work when data exceeds buffer size (Glen Arrowsmith) [#4811](https://github.com/nodejs/node/pull/4811)
* [[`e3e20422a7`](https://github.com/nodejs/node/commit/e3e20422a7)] - **doc**: harmonize $ node command line notation (Robert Jefe Lindstaedt) [#4806](https://github.com/nodejs/node/pull/4806)
* [[`73e0195cef`](https://github.com/nodejs/node/commit/73e0195cef)] - **doc**: fix type references for link gen, link css (Claudio Rodriguez) [#4741](https://github.com/nodejs/node/pull/4741)
* [[`0bdac429e1`](https://github.com/nodejs/node/commit/0bdac429e1)] - **doc**: multiple improvements in Stream docs (Alexander Makarenko) [#5009](https://github.com/nodejs/node/pull/5009)
* [[`693c16fb6b`](https://github.com/nodejs/node/commit/693c16fb6b)] - **doc**: fix anchor links from stream to http and events (piepmatz) [#5007](https://github.com/nodejs/node/pull/5007)
* [[`5fb533522c`](https://github.com/nodejs/node/commit/5fb533522c)] - **doc**: replace function expressions with arrows (Benjamin Gruenbaum) [#4832](https://github.com/nodejs/node/pull/4832)
* [[`e3572fb809`](https://github.com/nodejs/node/commit/e3572fb809)] - **doc**: fix links order in Buffer doc (Alexander Makarenko) [#5076](https://github.com/nodejs/node/pull/5076)
* [[`5c936ab765`](https://github.com/nodejs/node/commit/5c936ab765)] - **doc**: clarify optional arguments of Buffer methods (Michaël Zasso) [#5008](https://github.com/nodejs/node/pull/5008)
* [[`6df350c2b3`](https://github.com/nodejs/node/commit/6df350c2b3)] - **doc**: improve styling consistency in Buffer docs (Alexander Makarenko) [#5001](https://github.com/nodejs/node/pull/5001)
* [[`047f4a157f`](https://github.com/nodejs/node/commit/047f4a157f)] - **doc**: make buffer methods styles consistent (Timothy Gu) [#4873](https://github.com/nodejs/node/pull/4873)
* [[`4cfc017b90`](https://github.com/nodejs/node/commit/4cfc017b90)] - **doc**: fix nonsensical grammar in Buffer::write (Jimb Esser) [#4863](https://github.com/nodejs/node/pull/4863)
* [[`9087f6daca`](https://github.com/nodejs/node/commit/9087f6daca)] - **doc**: fix named anchors in addons.markdown and http.markdown (Michael Theriot) [#4708](https://github.com/nodejs/node/pull/4708)
* [[`4c8713ce58`](https://github.com/nodejs/node/commit/4c8713ce58)] - **doc**: add buf.indexOf encoding param with example (Karl Skomski) [#3373](https://github.com/nodejs/node/pull/3373)
* [[`1819d74491`](https://github.com/nodejs/node/commit/1819d74491)] - **doc**: fenced all code blocks, typo fixes (Robert Jefe Lindstaedt) [#4733](https://github.com/nodejs/node/pull/4733)
* [[`961735e645`](https://github.com/nodejs/node/commit/961735e645)] - **doc**: make references clickable (Roman Klauke) [#4654](https://github.com/nodejs/node/pull/4654)
* [[`7e80442483`](https://github.com/nodejs/node/commit/7e80442483)] - **doc**: improve child_process.execFile() code example (Ryan Sobol) [#4504](https://github.com/nodejs/node/pull/4504)
* [[`de9ad5b39d`](https://github.com/nodejs/node/commit/de9ad5b39d)] - **doc**: remove "above" and "below" references (Richard Sun) [#4499](https://github.com/nodejs/node/pull/4499)
* [[`c549ca3b69`](https://github.com/nodejs/node/commit/c549ca3b69)] - **doc**: fix heading level error in Buffer doc (Shigeki Ohtsu) [#4537](https://github.com/nodejs/node/pull/4537)
* [[`a613bae14c`](https://github.com/nodejs/node/commit/a613bae14c)] - **doc**: improvements to crypto.markdown copy (James M Snell) [#4435](https://github.com/nodejs/node/pull/4435)
* [[`18f580d0c1`](https://github.com/nodejs/node/commit/18f580d0c1)] - **doc**: improve child_process.markdown copy (James M Snell) [#4383](https://github.com/nodejs/node/pull/4383)
* [[`a929837311`](https://github.com/nodejs/node/commit/a929837311)] - **doc**: improvements to buffer.markdown copy (James M Snell) [#4370](https://github.com/nodejs/node/pull/4370)
* [[`a22f688407`](https://github.com/nodejs/node/commit/a22f688407)] - **doc**: improve addons.markdown copy (James M Snell) [#4320](https://github.com/nodejs/node/pull/4320)
* [[`94c2de47b1`](https://github.com/nodejs/node/commit/94c2de47b1)] - **doc**: update process.send() signature (cjihrig) [#5284](https://github.com/nodejs/node/pull/5284)
* [[`4e1926cb08`](https://github.com/nodejs/node/commit/4e1926cb08)] - **doc**: replace node-forward link in CONTRIBUTING.md (Ben Noordhuis) [#5227](https://github.com/nodejs/node/pull/5227)
* [[`e1713e81e5`](https://github.com/nodejs/node/commit/e1713e81e5)] - **doc**: fix minor inconsistencies in repl doc (Rich Trott) [#5193](https://github.com/nodejs/node/pull/5193)
* [[`b2e72c0d92`](https://github.com/nodejs/node/commit/b2e72c0d92)] - **doc**: clarify exceptions during uncaughtException (Noah Rose) [#5180](https://github.com/nodejs/node/pull/5180)
* [[`c3c549836a`](https://github.com/nodejs/node/commit/c3c549836a)] - **doc**: update DCO to v1.1 (Mikeal Rogers) [#5170](https://github.com/nodejs/node/pull/5170)
* [[`9dd35ad594`](https://github.com/nodejs/node/commit/9dd35ad594)] - **doc**: fix dgram doc indentation (Rich Trott) [#5118](https://github.com/nodejs/node/pull/5118)
* [[`eed830702c`](https://github.com/nodejs/node/commit/eed830702c)] - **doc**: fix typo in dgram doc (Rich Trott) [#5114](https://github.com/nodejs/node/pull/5114)
* [[`abfb2f5864`](https://github.com/nodejs/node/commit/abfb2f5864)] - **doc**: fix link in cluster documentation (Timothy Gu) [#5068](https://github.com/nodejs/node/pull/5068)
* [[`8b040b5bb2`](https://github.com/nodejs/node/commit/8b040b5bb2)] - **doc**: fix minor typo in process doc (Prayag Verma) [#5018](https://github.com/nodejs/node/pull/5018)
* [[`47eebe1d80`](https://github.com/nodejs/node/commit/47eebe1d80)] - **doc**: fix typo in Readme.md (Prayag Verma) [#5017](https://github.com/nodejs/node/pull/5017)
* [[`2b97ff89a6`](https://github.com/nodejs/node/commit/2b97ff89a6)] - **doc**: minor improvement in OS docs (Alexander Makarenko) [#5006](https://github.com/nodejs/node/pull/5006)
* [[`9a5d58b89e`](https://github.com/nodejs/node/commit/9a5d58b89e)] - **doc**: improve styling consistency in VM docs (Alexander Makarenko) [#5005](https://github.com/nodejs/node/pull/5005)
* [[`960e1bab98`](https://github.com/nodejs/node/commit/960e1bab98)] - **doc**: minor improvement to HTTPS doc (Alexander Makarenko) [#5002](https://github.com/nodejs/node/pull/5002)
* [[`6048b011e8`](https://github.com/nodejs/node/commit/6048b011e8)] - **doc**: spell writable consistently (Peter Lyons) [#4954](https://github.com/nodejs/node/pull/4954)
* [[`7b8f904167`](https://github.com/nodejs/node/commit/7b8f904167)] - **doc**: update eol handling in readline (Kári Tristan Helgason) [#4927](https://github.com/nodejs/node/pull/4927)
* [[`83efd0d4d1`](https://github.com/nodejs/node/commit/83efd0d4d1)] - **doc**: add more details to process.env (Evan Lucas) [#4924](https://github.com/nodejs/node/pull/4924)
* [[`b2d2c0b588`](https://github.com/nodejs/node/commit/b2d2c0b588)] - **doc**: undo move http.IncomingMessage.statusMessage (Jeff Harris) [#4822](https://github.com/nodejs/node/pull/4822)
* [[`b091c41b53`](https://github.com/nodejs/node/commit/b091c41b53)] - **doc**: proper markdown escaping -> \_\_, \*, \_ (Robert Jefe Lindstaedt) [#4805](https://github.com/nodejs/node/pull/4805)
* [[`0887208290`](https://github.com/nodejs/node/commit/0887208290)] - **doc**: remove unnecessary bind(this) (Dmitriy Lazarev) [#4797](https://github.com/nodejs/node/pull/4797)
* [[`f3e3c70bca`](https://github.com/nodejs/node/commit/f3e3c70bca)] - **doc**: Update small error in LICENSE for npm (Kat Marchán) [#4872](https://github.com/nodejs/node/pull/4872)
* [[`e703b180b3`](https://github.com/nodejs/node/commit/e703b180b3)] - **doc,tools,test**: lint doc-based addon tests (Rich Trott) [#5427](https://github.com/nodejs/node/pull/5427)
* [[`0f3b8ca192`](https://github.com/nodejs/node/commit/0f3b8ca192)] - **fs**: refactor redeclared variables (Rich Trott) [#4959](https://github.com/nodejs/node/pull/4959)
* [[`152c6b6b8d`](https://github.com/nodejs/node/commit/152c6b6b8d)] - **http**: remove reference to onParserExecute (Tom Atkinson) [#4773](https://github.com/nodejs/node/pull/4773)
* [[`6a0571cd72`](https://github.com/nodejs/node/commit/6a0571cd72)] - **http**: do not emit `upgrade` on advertisement (Fedor Indutny) [#4337](https://github.com/nodejs/node/pull/4337)
* [[`567ced9ef0`](https://github.com/nodejs/node/commit/567ced9ef0)] - **(SEMVER-MINOR)** **http**: handle errors on idle sockets (José F. Romaniello) [#4482](https://github.com/nodejs/node/pull/4482)
* [[`de5177ccb8`](https://github.com/nodejs/node/commit/de5177ccb8)] - **https**: evict cached sessions on error (Fedor Indutny) [#4982](https://github.com/nodejs/node/pull/4982)
* [[`77a6036264`](https://github.com/nodejs/node/commit/77a6036264)] - **installer**: install the tick processor (Matt Loring) [#3032](https://github.com/nodejs/node/pull/3032)
* [[`ea16d8d7c5`](https://github.com/nodejs/node/commit/ea16d8d7c5)] - **lib**: remove string_decoder.js var redeclarations (Rich Trott) [#4978](https://github.com/nodejs/node/pull/4978)
* [[`1389660ab3`](https://github.com/nodejs/node/commit/1389660ab3)] - **lib**: scope loop variables (Rich Trott) [#4965](https://github.com/nodejs/node/pull/4965)
* [[`59255d7218`](https://github.com/nodejs/node/commit/59255d7218)] - **lib**: use arrow functions instead of bind (Minwoo Jung) [#3622](https://github.com/nodejs/node/pull/3622)
* [[`fd26960aab`](https://github.com/nodejs/node/commit/fd26960aab)] - **lib,test**: remove extra semicolons (Michaël Zasso) [#2205](https://github.com/nodejs/node/pull/2205)
* [[`9646d26ffd`](https://github.com/nodejs/node/commit/9646d26ffd)] - **module**: refactor redeclared variable (Rich Trott) [#4962](https://github.com/nodejs/node/pull/4962)
* [[`09311128e8`](https://github.com/nodejs/node/commit/09311128e8)] - **net**: use `_server` for internal book-keeping (Fedor Indutny) [#5262](https://github.com/nodejs/node/pull/5262)
* [[`824c402174`](https://github.com/nodejs/node/commit/824c402174)] - **net**: refactor redeclared variables (Rich Trott) [#4963](https://github.com/nodejs/node/pull/4963)
* [[`96f306f3cf`](https://github.com/nodejs/node/commit/96f306f3cf)] - **net**: move isLegalPort to internal/net (Evan Lucas) [#4882](https://github.com/nodejs/node/pull/4882)
* [[`78d64889bd`](https://github.com/nodejs/node/commit/78d64889bd)] - **node**: set process._eventsCount to 0 on startup (Evan Lucas) [#5208](https://github.com/nodejs/node/pull/5208)
* [[`7a2e8f4356`](https://github.com/nodejs/node/commit/7a2e8f4356)] - **process**: support symbol events (cjihrig) [#4798](https://github.com/nodejs/node/pull/4798)
* [[`c9e2dce247`](https://github.com/nodejs/node/commit/c9e2dce247)] - **querystring**: improve parse() performance (Brian White) [#4675](https://github.com/nodejs/node/pull/4675)
* [[`18542c41fe`](https://github.com/nodejs/node/commit/18542c41fe)] - **repl**: remove variable redeclaration (Rich Trott) [#4977](https://github.com/nodejs/node/pull/4977)
* [[`10be8dc360`](https://github.com/nodejs/node/commit/10be8dc360)] - **src**: force line buffering for stderr (Rich Trott) [#3701](https://github.com/nodejs/node/pull/3701)
* [[`7958664e85`](https://github.com/nodejs/node/commit/7958664e85)] - **src**: clean up usage of __proto__ (Jackson Tian) [#5069](https://github.com/nodejs/node/pull/5069)
* [[`4e0a0d51b3`](https://github.com/nodejs/node/commit/4e0a0d51b3)] - **src**: remove no longer relevant comments (Chris911) [#4843](https://github.com/nodejs/node/pull/4843)
* [[`51c8bc8abc`](https://github.com/nodejs/node/commit/51c8bc8abc)] - **src**: remove __builtin_bswap16 call (Ben Noordhuis) [#4290](https://github.com/nodejs/node/pull/4290)
* [[`5e1976e37c`](https://github.com/nodejs/node/commit/5e1976e37c)] - **src**: remove unused BITS_PER_LONG macro (Ben Noordhuis) [#4290](https://github.com/nodejs/node/pull/4290)
* [[`c18ef54d88`](https://github.com/nodejs/node/commit/c18ef54d88)] - **(SEMVER-MINOR)** **src**: add BE support to StringBytes::Encode() (Bryon Leung) [#3410](https://github.com/nodejs/node/pull/3410)
* [[`be9e7610b5`](https://github.com/nodejs/node/commit/be9e7610b5)] - **src,test,tools**: modify for more stringent linting (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`538c4756a7`](https://github.com/nodejs/node/commit/538c4756a7)] - **stream**: refactor redeclared variables (Rich Trott) [#4816](https://github.com/nodejs/node/pull/4816)
* [[`4fa22e4126`](https://github.com/nodejs/node/commit/4fa22e4126)] - **streams**: 5% throughput gain when sending small chunks (Matteo Collina) [#4354](https://github.com/nodejs/node/pull/4354)
* [[`b6bd87495f`](https://github.com/nodejs/node/commit/b6bd87495f)] - **test**: remove flaky mark for test-debug-no-context (Rich Trott) [#5317](https://github.com/nodejs/node/pull/5317)
* [[`7705360e35`](https://github.com/nodejs/node/commit/7705360e35)] - **test**: add test for https server close event (Braydon Fuller) [#5106](https://github.com/nodejs/node/pull/5106)
* [[`9d6623e1d1`](https://github.com/nodejs/node/commit/9d6623e1d1)] - **test**: use String.prototype.repeat() for clarity (Rich Trott) [#5311](https://github.com/nodejs/node/pull/5311)
* [[`18e3987e2e`](https://github.com/nodejs/node/commit/18e3987e2e)] - **test**: mitigate flaky test-debug-no-context (Rich Trott) [#5269](https://github.com/nodejs/node/pull/5269)
* [[`058db07ce8`](https://github.com/nodejs/node/commit/058db07ce8)] - **test**: refactor test-dgram-send-callback-recursive (Santiago Gimeno) [#5079](https://github.com/nodejs/node/pull/5079)
* [[`1647113d7a`](https://github.com/nodejs/node/commit/1647113d7a)] - **test**: refactor test-http-destroyed-socket-write2 (Santiago Gimeno) [#4970](https://github.com/nodejs/node/pull/4970)
* [[`07dc2b50e2`](https://github.com/nodejs/node/commit/07dc2b50e2)] - **test**: shorten path for bogus socket (Rich Trott) [#4478](https://github.com/nodejs/node/pull/4478)
* [[`47e7c8c359`](https://github.com/nodejs/node/commit/47e7c8c359)] - **test**: mark test-http-regr-gh-2928 flaky (Rich Trott) [#5280](https://github.com/nodejs/node/pull/5280)
* [[`9dbd66f7ef`](https://github.com/nodejs/node/commit/9dbd66f7ef)] - **test**: mark test-http-agent flaky (Rich Trott) [#5209](https://github.com/nodejs/node/pull/5209)
* [[`98049876b5`](https://github.com/nodejs/node/commit/98049876b5)] - **test**: minimal repl eval option test (Rich Trott) [#5192](https://github.com/nodejs/node/pull/5192)
* [[`ae3185b8ac`](https://github.com/nodejs/node/commit/ae3185b8ac)] - **test**: disable fs watch tests for AIX (Michael Dawson) [#5187](https://github.com/nodejs/node/pull/5187)
* [[`b639c3345b`](https://github.com/nodejs/node/commit/b639c3345b)] - **test**: fix child-process-fork-regr-gh-2847 again (Santiago Gimeno) [#5179](https://github.com/nodejs/node/pull/5179)
* [[`8be3afc474`](https://github.com/nodejs/node/commit/8be3afc474)] - **test**: fix flaky test-http-regr-gh-2928 (Rich Trott) [#5154](https://github.com/nodejs/node/pull/5154)
* [[`46dc12bdcc`](https://github.com/nodejs/node/commit/46dc12bdcc)] - **test**: enable to work pkcs12 test in FIPS mode (Shigeki Ohtsu) [#5150](https://github.com/nodejs/node/pull/5150)
* [[`e19b8ea692`](https://github.com/nodejs/node/commit/e19b8ea692)] - **test**: remove unneeded common.indirectInstanceOf() (Rich Trott) [#5149](https://github.com/nodejs/node/pull/5149)
* [[`6072d2e15e`](https://github.com/nodejs/node/commit/6072d2e15e)] - **test**: disable gh-5100 test when in FIPS mode (Fedor Indutny) [#5144](https://github.com/nodejs/node/pull/5144)
* [[`a8417a2787`](https://github.com/nodejs/node/commit/a8417a2787)] - **test**: fix flaky test-dgram-pingpong (Rich Trott) [#5125](https://github.com/nodejs/node/pull/5125)
* [[`9db67a6a44`](https://github.com/nodejs/node/commit/9db67a6a44)] - **test**: fix child-process-fork-regr-gh-2847 (Santiago Gimeno) [#5121](https://github.com/nodejs/node/pull/5121)
* [[`69150caedc`](https://github.com/nodejs/node/commit/69150caedc)] - **test**: don't run test-tick-processor.js on Aix (Michael Dawson) [#5093](https://github.com/nodejs/node/pull/5093)
* [[`4a492b96b1`](https://github.com/nodejs/node/commit/4a492b96b1)] - **test**: mark flaky tests on Raspberry Pi (Rich Trott) [#5082](https://github.com/nodejs/node/pull/5082)
* [[`4301f2cdc2`](https://github.com/nodejs/node/commit/4301f2cdc2)] - **test**: fix inconsistent styling in test-url (Brian White) [#5014](https://github.com/nodejs/node/pull/5014)
* [[`865baaed60`](https://github.com/nodejs/node/commit/865baaed60)] - **test**: fix redeclared vars in sequential tests (Rich Trott) [#4999](https://github.com/nodejs/node/pull/4999)
* [[`663e852c1b`](https://github.com/nodejs/node/commit/663e852c1b)] - **test**: pummel test fixes (Rich Trott) [#4998](https://github.com/nodejs/node/pull/4998)
* [[`72d38a4a38`](https://github.com/nodejs/node/commit/72d38a4a38)] - **test**: fix redeclared vars in test-vm-* (Rich Trott) [#4997](https://github.com/nodejs/node/pull/4997)
* [[`97ddfa2b6e`](https://github.com/nodejs/node/commit/97ddfa2b6e)] - **test**: fix redeclared vars in test-url (Rich Trott) [#4993](https://github.com/nodejs/node/pull/4993)
* [[`43d4db4314`](https://github.com/nodejs/node/commit/43d4db4314)] - **test**: fix redeclared test-util-* vars (Rich Trott) [#4994](https://github.com/nodejs/node/pull/4994)
* [[`88fae38d0c`](https://github.com/nodejs/node/commit/88fae38d0c)] - **test**: fix variable redeclarations (Rich Trott) [#4992](https://github.com/nodejs/node/pull/4992)
* [[`58595f146a`](https://github.com/nodejs/node/commit/58595f146a)] - **test**: fix redeclared test-path vars (Rich Trott) [#4991](https://github.com/nodejs/node/pull/4991)
* [[`2b711d51fa`](https://github.com/nodejs/node/commit/2b711d51fa)] - **test**: fix var redeclarations in test-os (Rich Trott) [#4990](https://github.com/nodejs/node/pull/4990)
* [[`bd9e2c31d6`](https://github.com/nodejs/node/commit/bd9e2c31d6)] - **test**: fix test-net-* variable redeclarations (Rich Trott) [#4989](https://github.com/nodejs/node/pull/4989)
* [[`d67ab81882`](https://github.com/nodejs/node/commit/d67ab81882)] - **test**: fix redeclared test-intl var (Rich Trott) [#4988](https://github.com/nodejs/node/pull/4988)
* [[`d6dbb2fae7`](https://github.com/nodejs/node/commit/d6dbb2fae7)] - **test**: fix redeclared test-http-* vars (Rich Trott) [#4987](https://github.com/nodejs/node/pull/4987)
* [[`ecaa89a8cb`](https://github.com/nodejs/node/commit/ecaa89a8cb)] - **test**: fix redeclared test-event-emitter-* vars (Rich Trott) [#4985](https://github.com/nodejs/node/pull/4985)
* [[`299c729371`](https://github.com/nodejs/node/commit/299c729371)] - **test**: remove redeclared var in test-domain (Rich Trott) [#4984](https://github.com/nodejs/node/pull/4984)
* [[`35a4a203bf`](https://github.com/nodejs/node/commit/35a4a203bf)] - **test**: remove var redeclarations in test-crypto-* (Rich Trott) [#4981](https://github.com/nodejs/node/pull/4981)
* [[`1d56b74af0`](https://github.com/nodejs/node/commit/1d56b74af0)] - **test**: remove test-cluster-* var redeclarations (Rich Trott) [#4980](https://github.com/nodejs/node/pull/4980)
* [[`0ce12cc1ec`](https://github.com/nodejs/node/commit/0ce12cc1ec)] - **test**: fix test-http-extra-response flakiness (Santiago Gimeno) [#4979](https://github.com/nodejs/node/pull/4979)
* [[`c6b4bf138c`](https://github.com/nodejs/node/commit/c6b4bf138c)] - **test**: scope redeclared vars in test-child-process* (Rich Trott) [#4944](https://github.com/nodejs/node/pull/4944)
* [[`7654c171c7`](https://github.com/nodejs/node/commit/7654c171c7)] - **test**: refactor switch (Rich Trott) [#4870](https://github.com/nodejs/node/pull/4870)
* [[`226dfef690`](https://github.com/nodejs/node/commit/226dfef690)] - **test**: add common.platformTimeout() to dgram test (Rich Trott) [#4938](https://github.com/nodejs/node/pull/4938)
* [[`fb14bac662`](https://github.com/nodejs/node/commit/fb14bac662)] - **test**: fix flaky cluster test on Windows 10 (Rich Trott) [#4934](https://github.com/nodejs/node/pull/4934)
* [[`f5d29d7ac4`](https://github.com/nodejs/node/commit/f5d29d7ac4)] - **test**: Add assertion for TLS peer certificate fingerprint (Alan Cohen) [#4923](https://github.com/nodejs/node/pull/4923)
* [[`618427cea6`](https://github.com/nodejs/node/commit/618427cea6)] - **test**: fix test-tls-zero-clear-in flakiness (Santiago Gimeno) [#4888](https://github.com/nodejs/node/pull/4888)
* [[`8700c39c70`](https://github.com/nodejs/node/commit/8700c39c70)] - **test**: fix irregular whitespace issue (Roman Reiss) [#4864](https://github.com/nodejs/node/pull/4864)
* [[`2b026c9d5a`](https://github.com/nodejs/node/commit/2b026c9d5a)] - **test**: fs.link() test runs on same device (Drew Folta) [#4861](https://github.com/nodejs/node/pull/4861)
* [[`80a637ac4d`](https://github.com/nodejs/node/commit/80a637ac4d)] - **test**: scope redeclared variable (Rich Trott) [#4854](https://github.com/nodejs/node/pull/4854)
* [[`8c4903d4ef`](https://github.com/nodejs/node/commit/8c4903d4ef)] - **test**: update arrow function style (cjihrig) [#4813](https://github.com/nodejs/node/pull/4813)
* [[`0a44e6a447`](https://github.com/nodejs/node/commit/0a44e6a447)] - **test**: mark test-tick-processor flaky (Rich Trott) [#4809](https://github.com/nodejs/node/pull/4809)
* [[`363460616c`](https://github.com/nodejs/node/commit/363460616c)] - **test**: refactor test-net-settimeout (Rich Trott) [#4799](https://github.com/nodejs/node/pull/4799)
* [[`6841d82c22`](https://github.com/nodejs/node/commit/6841d82c22)] - **test**: remove race condition in http flood test (Rich Trott) [#4793](https://github.com/nodejs/node/pull/4793)
* [[`b5bae32847`](https://github.com/nodejs/node/commit/b5bae32847)] - **test**: remove test-http-exit-delay (Rich Trott) [#4786](https://github.com/nodejs/node/pull/4786)
* [[`60514f9521`](https://github.com/nodejs/node/commit/60514f9521)] - **test**: refactor test-fs-watch (Rich Trott) [#4776](https://github.com/nodejs/node/pull/4776)
* [[`2a3a431119`](https://github.com/nodejs/node/commit/2a3a431119)] - **test**: fix `net-socket-timeout-unref` flakiness (Santiago Gimeno) [#4772](https://github.com/nodejs/node/pull/4772)
* [[`9e6f3632a1`](https://github.com/nodejs/node/commit/9e6f3632a1)] - **test**: remove Object.observe from tests (Vladimir Kurchatkin) [#4769](https://github.com/nodejs/node/pull/4769)
* [[`f78daa67b8`](https://github.com/nodejs/node/commit/f78daa67b8)] - **test**: make npm tests work on prerelease node versions (Kat Marchán) [#4960](https://github.com/nodejs/node/pull/4960)
* [[`1c03191b6a`](https://github.com/nodejs/node/commit/1c03191b6a)] - **test**: make npm tests work on prerelease node versions (Kat Marchán) [#4872](https://github.com/nodejs/node/pull/4872)
* [[`d9c22cc896`](https://github.com/nodejs/node/commit/d9c22cc896)] - **test,buffer**: refactor redeclarations (Rich Trott) [#4893](https://github.com/nodejs/node/pull/4893)
* [[`5c4960468a`](https://github.com/nodejs/node/commit/5c4960468a)] - **tls**: nullify `.ssl` on handle close (Fedor Indutny) [#5168](https://github.com/nodejs/node/pull/5168)
* [[`c0f5f01c9c`](https://github.com/nodejs/node/commit/c0f5f01c9c)] - **tls**: scope loop vars with let (Rich Trott) [#4853](https://github.com/nodejs/node/pull/4853)
* [[`c86627e0d1`](https://github.com/nodejs/node/commit/c86627e0d1)] - **(SEMVER-MINOR)** **tls**: add `options` argument to createSecurePair (Коренберг Марк) [#2441](https://github.com/nodejs/node/pull/2441)
* [[`c908ff36f4`](https://github.com/nodejs/node/commit/c908ff36f4)] - **tls_wrap**: reach error reporting for UV_EPROTO (Fedor Indutny) [#4885](https://github.com/nodejs/node/pull/4885)
* [[`cebe3b95e3`](https://github.com/nodejs/node/commit/cebe3b95e3)] - **tools**: run tick processor without forking (Matt Loring) [#4224](https://github.com/nodejs/node/pull/4224)
* [[`70d8827714`](https://github.com/nodejs/node/commit/70d8827714)] - **(SEMVER-MINOR)** **tools**: add --prof-process flag to node binary (Matt Loring) [#4021](https://github.com/nodejs/node/pull/4021)
* [[`a43b9291c7`](https://github.com/nodejs/node/commit/a43b9291c7)] - **tools**: replace obsolete ESLint rules (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`a89c6f58f1`](https://github.com/nodejs/node/commit/a89c6f58f1)] - **tools**: update ESLint to version 2.1.0 (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`789f62196a`](https://github.com/nodejs/node/commit/789f62196a)] - **tools**: remove obsolete lint rules (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`154772cfa8`](https://github.com/nodejs/node/commit/154772cfa8)] - **tools**: parse types into links in doc html gen (Claudio Rodriguez) [#4741](https://github.com/nodejs/node/pull/4741)
* [[`9237b6e38a`](https://github.com/nodejs/node/commit/9237b6e38a)] - **tools**: fix warning in doc parsing (Shigeki Ohtsu) [#4537](https://github.com/nodejs/node/pull/4537)
* [[`c653cc0c03`](https://github.com/nodejs/node/commit/c653cc0c03)] - **tools**: add recommended ES6 lint rules (Rich Trott) [#5210](https://github.com/nodejs/node/pull/5210)
* [[`993d9b7df0`](https://github.com/nodejs/node/commit/993d9b7df0)] - **tools**: add recommended linting rules (Rich Trott) [#5188](https://github.com/nodejs/node/pull/5188)
* [[`8423125223`](https://github.com/nodejs/node/commit/8423125223)] - **tools**: remove excessive comments from .eslintrc (Rich Trott) [#5151](https://github.com/nodejs/node/pull/5151)
* [[`4c687c98e4`](https://github.com/nodejs/node/commit/4c687c98e4)] - **tools**: enable no-proto rule for linter (Jackson Tian) [#5140](https://github.com/nodejs/node/pull/5140)
* [[`28e4e6f312`](https://github.com/nodejs/node/commit/28e4e6f312)] - **tools**: disallow mixed spaces and tabs for indents (Rich Trott) [#5135](https://github.com/nodejs/node/pull/5135)
* [[`50c6fe8604`](https://github.com/nodejs/node/commit/50c6fe8604)] - **tools**: alphabetize eslint stylistic issues section (Rich Trott)
* [[`ee594f1ed7`](https://github.com/nodejs/node/commit/ee594f1ed7)] - **tools**: lint for empty character classes in regex (Rich Trott) [#5115](https://github.com/nodejs/node/pull/5115)
* [[`bf0e239e99`](https://github.com/nodejs/node/commit/bf0e239e99)] - **tools**: lint for spacing around unary operators (Rich Trott) [#5063](https://github.com/nodejs/node/pull/5063)
* [[`6345acb792`](https://github.com/nodejs/node/commit/6345acb792)] - **tools**: enable no-redeclare rule for linter (Rich Trott) [#5047](https://github.com/nodejs/node/pull/5047)
* [[`1dae175b62`](https://github.com/nodejs/node/commit/1dae175b62)] - **tools**: fix redeclared vars in doc/json.js (Rich Trott) [#5047](https://github.com/nodejs/node/pull/5047)
* [[`d1d220a1cf`](https://github.com/nodejs/node/commit/d1d220a1cf)] - **tools**: apply linting to doc tools (Rich Trott) [#4973](https://github.com/nodejs/node/pull/4973)
* [[`eddde1f60c`](https://github.com/nodejs/node/commit/eddde1f60c)] - **tools**: fix detecting constructor for JSON doc (Timothy Gu) [#4966](https://github.com/nodejs/node/pull/4966)
* [[`bcb327c8dd`](https://github.com/nodejs/node/commit/bcb327c8dd)] - **tools**: add property types in JSON documentation (Timothy Gu) [#4884](https://github.com/nodejs/node/pull/4884)
* [[`9a06a4c116`](https://github.com/nodejs/node/commit/9a06a4c116)] - **tools**: enable assorted ESLint error rules (Roman Reiss) [#4864](https://github.com/nodejs/node/pull/4864)
* [[`38474cfd49`](https://github.com/nodejs/node/commit/38474cfd49)] - **tools**: add arrow function rules to eslint (cjihrig) [#4813](https://github.com/nodejs/node/pull/4813)
* [[`f898abaa4f`](https://github.com/nodejs/node/commit/f898abaa4f)] - **tools**: fix setting path containing an ampersand (Brian White) [#4804](https://github.com/nodejs/node/pull/4804)
* [[`d10bee8e79`](https://github.com/nodejs/node/commit/d10bee8e79)] - **tools**: enable no-extra-semi rule in eslint (Michaël Zasso) [#2205](https://github.com/nodejs/node/pull/2205)
* [[`01006392cf`](https://github.com/nodejs/node/commit/01006392cf)] - **tools,doc**: fix linting errors (Rich Trott) [#5161](https://github.com/nodejs/node/pull/5161)
* [[`57a5f8731a`](https://github.com/nodejs/node/commit/57a5f8731a)] - **url**: change scoping of variables with let (Kári Tristan Helgason) [#4867](https://github.com/nodejs/node/pull/4867)

## 2016-03-08, Version 0.12.12 (LTS), @rvagg

### Notable changes:

* openssl: Fully remove SSLv2 support, the `--enable-ssl2` command line argument will now produce an error. The DROWN Attack (https://drownattack.com/) creates a vulnerability where SSLv2 is enabled by a server, even if a client connection is not using SSLv2. The SSLv2 protocol is widely considered unacceptably broken and should not be supported. More information is available at https://www.openssl.org/news/vulnerabilities.html#2016-0800

Note that the upgrade to OpenSSL 1.0.1s in Node.js v0.12.11 removed internal SSLv2 support. The change in this release was originally intended for v0.12.11. The `--enable-ssl2` command line argument now produces an error rather than being a no-op.

### Commits:

* [dbfc9d9241] - crypto,tls: remove SSLv2 support (Ben Noordhuis) https://github.com/nodejs/node/pull/5536

## 2016-03-04, Version 0.10.43 (Maintenance), @rvagg

### Notable changes:

* http_parser: Update to http-parser 1.2 to fix an unintentionally strict limitation of allowable header characters. (James M Snell) https://github.com/nodejs/node/pull/5242
* domains:
  - Prevent an exit due to an exception being thrown rather than emitting an `'uncaughtException'` event on the `process` object when no error handler is set on the domain within which an error is thrown and an `'uncaughtException'` event listener is set on `process`. (Julien Gilli) https://github.com/nodejs/node/pull/3887
  - Fix an issue where the process would not abort in the proper function call if an error is thrown within a domain with no error handler and `--abort-on-uncaught-exception` is used. (Julien Gilli) https://github.com/nodejs/node/pull/3887
* openssl: Upgrade from 1.0.1r to 1.0.1s (Ben Noordhuis) https://github.com/nodejs/node/pull/5508
  - Fix a double-free defect in parsing malformed DSA keys that may potentially be used for DoS or memory corruption attacks. It is likely to be very difficult to use this defect for a practical attack and is therefore considered low severity for Node.js users. More info is available at https://www.openssl.org/news/vulnerabilities.html#2016-0705
  - Fix a defect that can cause memory corruption in certain very rare cases relating to the internal `BN_hex2bn()` and `BN_dec2bn()` functions. It is believed that Node.js is not invoking the code paths that use these functions so practical attacks via Node.js using this defect are _unlikely_ to be possible. More info is available at https://www.openssl.org/news/vulnerabilities.html#2016-0797
  - Fix a defect that makes the CacheBleed Attack (https://ssrg.nicta.com.au/projects/TS/cachebleed/) possible. This defect enables attackers to execute side-channel attacks leading to the potential recovery of entire RSA private keys. It only affects the Intel Sandy Bridge (and possibly older) microarchitecture when using hyper-threading. Newer microarchitectures, including Haswell, are unaffected. More info is available at https://www.openssl.org/news/vulnerabilities.html#2016-0702
  - Remove SSLv2 support, the `--enable-ssl2` command line argument will now produce an error. The DROWN Attack (https://drownattack.com/) creates a vulnerability where SSLv2 is enabled by a server, even if a client connection is not using SSLv2. The SSLv2 protocol is widely considered unacceptably broken and should not be supported. More information is available at https://www.openssl.org/news/vulnerabilities.html#2016-0800

### Commits:

* [164157abbb] - build: update Node.js logo on OSX installer (Rod Vagg) https://github.com/nodejs/node/pull/5401
* [f8cb0dcf67] - crypto,tls: remove SSLv2 support (Ben Noordhuis) https://github.com/nodejs/node/pull/5529
* [42ded2a590] - deps: upgrade openssl to 1.0.1s (Ben Noordhuis) https://github.com/nodejs/node/pull/5508
* [1e45a6111c] - deps: update http-parser to version 1.2 (James M Snell) https://github.com/nodejs/node/pull/5242
* [6db377b2f4] - doc: remove SSLv2 descriptions (Shigeki Ohtsu) https://github.com/nodejs/node/pull/5541
* [563c359f5c] - domains: fix handling of uncaught exceptions (Julien Gilli) https://github.com/nodejs/node/pull/3887
* [e483f3fd26] - test: fix hanging http obstext test (Ben Noordhuis) https://github.com/nodejs/node/pull/5511

## 2016-03-03, Version 0.12.11 (LTS), @rvagg

### Notable changes:

* http_parser: Update to http-parser 2.3.2 to fix an unintentionally strict limitation of allowable header characters. (James M Snell) https://github.com/nodejs/node/pull/5241
* domains:
  - Prevent an exit due to an exception being thrown rather than emitting an 'uncaughtException' event on the `process` object when no error handler is set on the domain within which an error is thrown and an 'uncaughtException' event listener is set on `process`. (Julien Gilli) https://github.com/nodejs/node/pull/3885
  - Fix an issue where the process would not abort in the proper function call if an error is thrown within a domain with no error handler and `--abort-on-uncaught-exception` is used. (Julien Gilli) https://github.com/nodejs/node/pull/3885
* openssl: Upgrade from 1.0.1r to 1.0.1s (Ben Noordhuis) https://github.com/nodejs/node/pull/5509
  - Fix a double-free defect in parsing malformed DSA keys that may potentially be used for DoS or memory corruption attacks. It is likely to be very difficult to use this defect for a practical attack and is therefore considered low severity for Node.js users. More info is available at https://www.openssl.org/news/vulnerabilities.html#2016-0705
  - Fix a defect that can cause memory corruption in certain very rare cases relating to the internal `BN_hex2bn()` and `BN_dec2bn()` functions. It is believed that Node.js is not invoking the code paths that use these functions so practical attacks via Node.js using this defect are _unlikely_ to be possible. More info is available at https://www.openssl.org/news/vulnerabilities.html#2016-0797
  - Fix a defect that makes the CacheBleed Attack (https://ssrg.nicta.com.au/projects/TS/cachebleed/) possible. This defect enables attackers to execute side-channel attacks leading to the potential recovery of entire RSA private keys. It only affects the Intel Sandy Bridge (and possibly older) microarchitecture when using hyper-threading. Newer microarchitectures, including Haswell, are unaffected. More info is available at https://www.openssl.org/news/vulnerabilities.html#2016-0702

### Commits:

* [1ab6653db9] - build: update Node.js logo on OSX installer (Rod Vagg) https://github.com/nodejs/node/pull/5401
* [fcc64792ae] - child_process: guard against race condition (Rich Trott) https://github.com/nodejs/node/pull/5153
* [6c468df9af] - child_process: fix data loss with readable event (Brian White) https://github.com/nodejs/node/pull/5037
* [61a22019c2] - deps: upgrade openssl to 1.0.1s (Ben Noordhuis) https://github.com/nodejs/node/pull/5509
* [fa26b13df7] - deps: update to http-parser 2.3.2 (James M Snell) https://github.com/nodejs/node/pull/5241
* [46c8e2165f] - deps: backport 1f8555 from v8's upstream (Trevor Norris) https://github.com/nodejs/node/pull/3945
* [ce58c2c31a] - doc: remove SSLv2 descriptions (Shigeki Ohtsu) https://github.com/nodejs/node/pull/5541
* [018e4e0b1a] - domains: fix handling of uncaught exceptions (Julien Gilli) https://github.com/nodejs/node/pull/3885
* [d421e85dc9] - lib: fix cluster handle leak (Rich Trott) https://github.com/nodejs/node/pull/5152
* [3a48f0022f] - node: fix leaking Context handle (Trevor Norris) https://github.com/nodejs/node/pull/3945
* [28dddabf6a] - src: fix build error without OpenSSL support (Jörg Krause) https://github.com/nodejs/node/pull/4201
* [a79baf03cd] - src: use global SealHandleScope (Trevor Norris) https://github.com/nodejs/node/pull/3945
* [be39f30447] - test: add test-domain-exit-dispose-again back (Julien Gilli) https://github.com/nodejs/node/pull/4278
* [da66166b9a] - test: fix test-domain-exit-dispose-again (Julien Gilli) https://github.com/nodejs/node/pull/3991

## 2016-03-02, Version 5.7.1 (Stable), @Fishrock123

### Notable changes

* **governance**: The Core Technical Committee (CTC) added four new members to help guide Node.js core development: Evan Lucas, Rich Trott, Ali Ijaz Sheikh and Сковорода Никита Андреевич (Nikita Skovoroda).
* **openssl**: Upgrade from 1.0.2f to 1.0.2g (Ben Noordhuis) [#5507](https://github.com/nodejs/node/pull/5507).
  - Fix a double-free defect in parsing malformed DSA keys that may potentially be used for DoS or memory corruption attacks. It is likely to be very difficult to use this defect for a practical attack and is therefore considered low severity for Node.js users. More info is available at [CVE-2016-0705](https://www.openssl.org/news/vulnerabilities.html#2016-0705).
  - Fix a defect that can cause memory corruption in certain very rare cases relating to the internal `BN_hex2bn()` and `BN_dec2bn()` functions. It is believed that Node.js is not invoking the code paths that use these functions so practical attacks via Node.js using this defect are _unlikely_ to be possible. More info is available at [CVE-2016-0797](https://www.openssl.org/news/vulnerabilities.html#2016-0797).
  - Fix a defect that makes the _[CacheBleed Attack](https://ssrg.nicta.com.au/projects/TS/cachebleed/)_ possible. This defect enables attackers to execute side-channel attacks leading to the potential recovery of entire RSA private keys. It only affects the Intel Sandy Bridge (and possibly older) microarchitecture when using hyper-threading. Newer microarchitectures, including Haswell, are unaffected. More info is available at [CVE-2016-0702](https://www.openssl.org/news/vulnerabilities.html#2016-0702).
* Fixed several regressions that appeared in v5.7.0:
  - **`path.relative()`**:
    - Output is no longer unnecessarily verbose (Brian White) [#5389](https://github.com/nodejs/node/pull/5389).
    - Resolving UNC paths on Windows now works correctly (Owen Smith) [#5456](https://github.com/nodejs/node/pull/5456).
    - Resolving paths with prefixes now works correctly from the root directory (Owen Smith) [#5490](https://github.com/nodejs/node/pull/5490).
  - **url**: Fixed an off-by-one error with `parse()` (Brian White) [#5394](https://github.com/nodejs/node/pull/5394).
  - **dgram**: Now correctly handles a default address case when offset and length are specified (Matteo Collina) [#5407](https://github.com/nodejs/node/pull/5407).

### Commits

* [[`7cae774d9b`](https://github.com/nodejs/node/commit/7cae774d9b)] - **benchmark**: refactor to eliminate redeclared vars (Rich Trott) [#5468](https://github.com/nodejs/node/pull/5468)
* [[`6aebe16669`](https://github.com/nodejs/node/commit/6aebe16669)] - **benchmark**: add benchmark for buf.compare() (Rich Trott) [#5441](https://github.com/nodejs/node/pull/5441)
* [[`00660f55c8`](https://github.com/nodejs/node/commit/00660f55c8)] - **benchmark**: move string-decoder to its own category (Andreas Madsen) [#5177](https://github.com/nodejs/node/pull/5177)
* [[`4650cb3818`](https://github.com/nodejs/node/commit/4650cb3818)] - **benchmark**: fix configuation parameters (Andreas Madsen) [#5177](https://github.com/nodejs/node/pull/5177)
* [[`3ccb275139`](https://github.com/nodejs/node/commit/3ccb275139)] - **benchmark**: merge url.js with url-resolve.js (Andreas Madsen) [#5177](https://github.com/nodejs/node/pull/5177)
* [[`c1e7dbffaa`](https://github.com/nodejs/node/commit/c1e7dbffaa)] - **benchmark**: move misc to categorized directories (Andreas Madsen) [#5177](https://github.com/nodejs/node/pull/5177)
* [[`2f9fee6e8e`](https://github.com/nodejs/node/commit/2f9fee6e8e)] - **benchmark**: use strict mode (Rich Trott) [#5336](https://github.com/nodejs/node/pull/5336)
* [[`4c09e7f359`](https://github.com/nodejs/node/commit/4c09e7f359)] - **build**: remove --quiet from eslint invocation (firedfox) [#5519](https://github.com/nodejs/node/pull/5519)
* [[`2c619f2012`](https://github.com/nodejs/node/commit/2c619f2012)] - **build**: run lint before tests (Rich Trott) [#5470](https://github.com/nodejs/node/pull/5470)
* [[`f349a9a2cf`](https://github.com/nodejs/node/commit/f349a9a2cf)] - **build**: update Node.js logo on OSX installer (Rod Vagg) [#5401](https://github.com/nodejs/node/pull/5401)
* [[`88f393588a`](https://github.com/nodejs/node/commit/88f393588a)] - **crypto**: PBKDF2 works with `int` not `ssize_t` (Fedor Indutny) [#5397](https://github.com/nodejs/node/pull/5397)
* [[`1e86804503`](https://github.com/nodejs/node/commit/1e86804503)] - **deps**: upgrade openssl to 1.0.2g (Ben Noordhuis) [#5507](https://github.com/nodejs/node/pull/5507)
* [[`d3f9b84be8`](https://github.com/nodejs/node/commit/d3f9b84be8)] - **dgram**: handle default address case when offset and length are specified (Matteo Collina)
* [[`f1f3832934`](https://github.com/nodejs/node/commit/f1f3832934)] - **doc**: update NAN urls in ROADMAP.md and doc/releases.md (ronkorving) [#5472](https://github.com/nodejs/node/pull/5472)
* [[`51bc062dab`](https://github.com/nodejs/node/commit/51bc062dab)] - **doc**: add CTC meeting minutes 2016-02-17 (Rod Vagg) [#5410](https://github.com/nodejs/node/pull/5410)
* [[`795c85ba1c`](https://github.com/nodejs/node/commit/795c85ba1c)] - **doc**: fix typo in child_process documentation (Evan Lucas) [#5474](https://github.com/nodejs/node/pull/5474)
* [[`0a56e9690b`](https://github.com/nodejs/node/commit/0a56e9690b)] - **doc**: add note for binary safe string reading (Anton Andesen) [#5155](https://github.com/nodejs/node/pull/5155)
* [[`ea8331e15f`](https://github.com/nodejs/node/commit/ea8331e15f)] - **doc**: improvements to crypto.markdown copy (Alexander Makarenko) [#5230](https://github.com/nodejs/node/pull/5230)
* [[`378a772034`](https://github.com/nodejs/node/commit/378a772034)] - **doc**: `require` behavior on case-insensitive systems (Hugo Wood)
* [[`06b7eb6636`](https://github.com/nodejs/node/commit/06b7eb6636)] - **doc**: document base64url encoding support (Tristan Slominski) [#5243](https://github.com/nodejs/node/pull/5243)
* [[`8ec3d904cb`](https://github.com/nodejs/node/commit/8ec3d904cb)] - **doc**: improve httpVersionMajor / httpVersionMajor (Jackson Tian) [#5296](https://github.com/nodejs/node/pull/5296)
* [[`534e88f56c`](https://github.com/nodejs/node/commit/534e88f56c)] - **doc**: fix relative links in net docs (Evan Lucas) [#5358](https://github.com/nodejs/node/pull/5358)
* [[`7b98a30976`](https://github.com/nodejs/node/commit/7b98a30976)] - **doc**: fix crypto function indentation level (Brian White) [#5460](https://github.com/nodejs/node/pull/5460)
* [[`c0fd802cc2`](https://github.com/nodejs/node/commit/c0fd802cc2)] - **doc**: link to man pages (dcposch@dcpos.ch) [#5073](https://github.com/nodejs/node/pull/5073)
* [[`f8c6701e22`](https://github.com/nodejs/node/commit/f8c6701e22)] - **doc**: add missing property in cluster example (Rafael Cepeda) [#5305](https://github.com/nodejs/node/pull/5305)
* [[`3bfe0483f0`](https://github.com/nodejs/node/commit/3bfe0483f0)] - **doc**: corrected name of argument in socket.send (Chris Dew) [#5449](https://github.com/nodejs/node/pull/5449)
* [[`c8725f5e95`](https://github.com/nodejs/node/commit/c8725f5e95)] - **doc**: fix links in tls, cluster docs (Alexander Makarenko) [#5364](https://github.com/nodejs/node/pull/5364)
* [[`7f2cf9af5c`](https://github.com/nodejs/node/commit/7f2cf9af5c)] - **doc**: explicit about VS 2015 support in readme (Phillip Johnsen) [#5406](https://github.com/nodejs/node/pull/5406)
* [[`12d3cdbfea`](https://github.com/nodejs/node/commit/12d3cdbfea)] - **doc**: remove out-of-date matter from internal docs (Rich Trott) [#5421](https://github.com/nodejs/node/pull/5421)
* [[`43853679f7`](https://github.com/nodejs/node/commit/43853679f7)] - **doc**: copyedit util doc (Rich Trott) [#5399](https://github.com/nodejs/node/pull/5399)
* [[`903e8d09e1`](https://github.com/nodejs/node/commit/903e8d09e1)] - **doc**: fix typo in pbkdf2Sync code sample (Marc Cuva) [#5306](https://github.com/nodejs/node/pull/5306)
* [[`79b1c22c9f`](https://github.com/nodejs/node/commit/79b1c22c9f)] - **doc**: fix buf.readInt16LE output (Chinedu Francis Nwafili) [#5282](https://github.com/nodejs/node/pull/5282)
* [[`e46915f2f3`](https://github.com/nodejs/node/commit/e46915f2f3)] - **doc**: note util.isError() @@toStringTag limitations (cjihrig) [#5414](https://github.com/nodejs/node/pull/5414)
* [[`935fd21fff`](https://github.com/nodejs/node/commit/935fd21fff)] - **doc**: clarify error handling in net.createServer (Dirceu Pereira Tiegs) [#5353](https://github.com/nodejs/node/pull/5353)
* [[`93dce6d4fe`](https://github.com/nodejs/node/commit/93dce6d4fe)] - **doc**: document fs.datasync(Sync) (Ron Korving) [#5402](https://github.com/nodejs/node/pull/5402)
* [[`96daf51358`](https://github.com/nodejs/node/commit/96daf51358)] - **doc**: add Evan Lucas to the CTC (Rod Vagg) [#5275](https://github.com/nodejs/node/pull/5275)
* [[`31b405d0cf`](https://github.com/nodejs/node/commit/31b405d0cf)] - **doc**: add Rich Trott to the CTC (Rod Vagg) [#5276](https://github.com/nodejs/node/pull/5276)
* [[`bcd154e402`](https://github.com/nodejs/node/commit/bcd154e402)] - **doc**: add Ali Ijaz Sheikh to the CTC (Rod Vagg) [#5277](https://github.com/nodejs/node/pull/5277)
* [[`9d0330c804`](https://github.com/nodejs/node/commit/9d0330c804)] - **doc**: add Сковорода Никита Андреевич to the CTC (Rod Vagg) [#5278](https://github.com/nodejs/node/pull/5278)
* [[`365cc63783`](https://github.com/nodejs/node/commit/365cc63783)] - **doc**: add "building node with ninja" guide (Jeremiah Senkpiel) [#4767](https://github.com/nodejs/node/pull/4767)
* [[`2b00c315e1`](https://github.com/nodejs/node/commit/2b00c315e1)] - **doc**: mention prototype check in deepStrictEqual() (cjihrig) [#5367](https://github.com/nodejs/node/pull/5367)
* [[`ff988b3ee6`](https://github.com/nodejs/node/commit/ff988b3ee6)] - **doc,tools,test**: lint doc-based addon tests (Rich Trott) [#5427](https://github.com/nodejs/node/pull/5427)
* [[`d77c3bf204`](https://github.com/nodejs/node/commit/d77c3bf204)] - **http_parser**: use `MakeCallback` (Trevor Norris) [#5419](https://github.com/nodejs/node/pull/5419)
* [[`e3421ac296`](https://github.com/nodejs/node/commit/e3421ac296)] - **lib**: freelist: use .pop() for allocation (Anton Khlynovskiy) [#2174](https://github.com/nodejs/node/pull/2174)
* [[`91d218d096`](https://github.com/nodejs/node/commit/91d218d096)] - **path**: fix path.relative() for prefixes at root (Owen Smith) [#5490](https://github.com/nodejs/node/pull/5490)
* [[`ef7a088906`](https://github.com/nodejs/node/commit/ef7a088906)] - **path**: fix win32 parse() (Zheng Chaoping) [#5484](https://github.com/nodejs/node/pull/5484)
* [[`871396ce8f`](https://github.com/nodejs/node/commit/871396ce8f)] - **path**: fix win32 relative() for UNC paths (Owen Smith) [#5456](https://github.com/nodejs/node/pull/5456)
* [[`91782f1888`](https://github.com/nodejs/node/commit/91782f1888)] - **path**: fix win32 relative() when "to" is a prefix (Owen Smith) [#5456](https://github.com/nodejs/node/pull/5456)
* [[`30cec18eeb`](https://github.com/nodejs/node/commit/30cec18eeb)] - **path**: fix verbose relative() output (Brian White) [#5389](https://github.com/nodejs/node/pull/5389)
* [[`2b88523836`](https://github.com/nodejs/node/commit/2b88523836)] - **repl**: fix stack trace column number in strict mode (Prince J Wesley) [#5416](https://github.com/nodejs/node/pull/5416)
* [[`51db48f741`](https://github.com/nodejs/node/commit/51db48f741)] - **src,tools**: remove null sentinel from source array (Ben Noordhuis) [#5418](https://github.com/nodejs/node/pull/5418)
* [[`03a5daba55`](https://github.com/nodejs/node/commit/03a5daba55)] - **src,tools**: drop nul byte from built-in source code (Ben Noordhuis) [#5418](https://github.com/nodejs/node/pull/5418)
* [[`17d14f3346`](https://github.com/nodejs/node/commit/17d14f3346)] - **src,tools**: allow utf-8 in built-in js source code (Ben Noordhuis) [#5418](https://github.com/nodejs/node/pull/5418)
* [[`12ae6abc69`](https://github.com/nodejs/node/commit/12ae6abc69)] - **test**: increase timeout for test-tls-fast-writing (Rich Trott) [#5466](https://github.com/nodejs/node/pull/5466)
* [[`81348e8855`](https://github.com/nodejs/node/commit/81348e8855)] - **test**: apply Linux workaround to Linux only (Rich Trott) [#5471](https://github.com/nodejs/node/pull/5471)
* [[`c4d9cdb7d0`](https://github.com/nodejs/node/commit/c4d9cdb7d0)] - **test**: allow options for v8 testing (Michael Dawson) [#5502](https://github.com/nodejs/node/pull/5502)
* [[`d1a82c6824`](https://github.com/nodejs/node/commit/d1a82c6824)] - **test**: retry on known SmartOS bug (Rich Trott) [#5454](https://github.com/nodejs/node/pull/5454)
* [[`c7f8a13043`](https://github.com/nodejs/node/commit/c7f8a13043)] - **test**: remove unneeded bind() and related comments (Aayush Naik) [#5023](https://github.com/nodejs/node/pull/5023)
* [[`cc4cbb10df`](https://github.com/nodejs/node/commit/cc4cbb10df)] - **test**: fix flaky child-process-fork-regr-gh-2847 (Santiago Gimeno) [#5422](https://github.com/nodejs/node/pull/5422)
* [[`0ebbf6cd53`](https://github.com/nodejs/node/commit/0ebbf6cd53)] - **test**: remove flaky designation from fixed tests (Rich Trott) [#5459](https://github.com/nodejs/node/pull/5459)
* [[`c83725c604`](https://github.com/nodejs/node/commit/c83725c604)] - **test**: add test-cases for posix path.relative() (Owen Smith) [#5456](https://github.com/nodejs/node/pull/5456)
* [[`22bb7c9d27`](https://github.com/nodejs/node/commit/22bb7c9d27)] - **test**: fix test runner arg regression (Stefan Budeanu) [#5446](https://github.com/nodejs/node/pull/5446)
* [[`8c67b94b11`](https://github.com/nodejs/node/commit/8c67b94b11)] - **test**: refactor test-dgram-send-callback-recursive (Santiago Gimeno) [#5079](https://github.com/nodejs/node/pull/5079)
* [[`2c21d34a2f`](https://github.com/nodejs/node/commit/2c21d34a2f)] - **test**: refactor test-dgram-udp4 (Santiago Gimeno) [#5339](https://github.com/nodejs/node/pull/5339)
* [[`479a43c876`](https://github.com/nodejs/node/commit/479a43c876)] - **test**: allow passing args to executable (Stefan Budeanu) [#5376](https://github.com/nodejs/node/pull/5376)
* [[`ff75023812`](https://github.com/nodejs/node/commit/ff75023812)] - **test**: fix test-timers.reliability on OS X (Rich Trott) [#5379](https://github.com/nodejs/node/pull/5379)
* [[`991f82b4bd`](https://github.com/nodejs/node/commit/991f82b4bd)] - **test**: mitigate flaky test-http-agent (Rich Trott) [#5346](https://github.com/nodejs/node/pull/5346)
* [[`0f54553a99`](https://github.com/nodejs/node/commit/0f54553a99)] - **test**: increase timeouts on some unref timers tests (Jeremiah Senkpiel) [#5352](https://github.com/nodejs/node/pull/5352)
* [[`25c01cd779`](https://github.com/nodejs/node/commit/25c01cd779)] - **tls**: fix assert in context.\_external accessor (Ben Noordhuis) [#5521](https://github.com/nodejs/node/pull/5521)
* [[`5ffd7430d1`](https://github.com/nodejs/node/commit/5ffd7430d1)] - **tools**: apply custom buffer lint rule to /lib only (Rich Trott) [#5371](https://github.com/nodejs/node/pull/5371)
* [[`fa5d28f246`](https://github.com/nodejs/node/commit/fa5d28f246)] - **tools**: enable additional lint rules (Rich Trott) [#5357](https://github.com/nodejs/node/pull/5357)
* [[`b44b701e5b`](https://github.com/nodejs/node/commit/b44b701e5b)] - **tools,benchmark**: increase lint compliance (Rich Trott) [#5429](https://github.com/nodejs/node/pull/5429)
* [[`9424fa5732`](https://github.com/nodejs/node/commit/9424fa5732)] - **url**: group slashed protocols by protocol name (nettofarah) [#5380](https://github.com/nodejs/node/pull/5380)
* [[`dfe45f13e7`](https://github.com/nodejs/node/commit/dfe45f13e7)] - **url**: fix off-by-one error with parse() (Brian White) [#5394](https://github.com/nodejs/node/pull/5394)

## 2016-03-02, Version 4.3.2 'Argon' (LTS), @thealphanerd

This is a security release with only a single commit, an update to openssl due to a recent security advisory. You can read more about the security advisory on [the Node.js website](https://nodejs.org/en/blog/vulnerability/openssl-march-2016/)

### Notable changes

* **openssl**: Upgrade from 1.0.2f to 1.0.2g (Ben Noordhuis) [#5507](https://github.com/nodejs/node/pull/5507)
  - Fix a double-free defect in parsing malformed DSA keys that may potentially be used for DoS or memory corruption attacks. It is likely to be very difficult to use this defect for a practical attack and is therefore considered low severity for Node.js users. More info is available at [CVE-2016-0705](https://www.openssl.org/news/vulnerabilities.html#2016-0705).
  - Fix a defect that can cause memory corruption in certain very rare cases relating to the internal `BN_hex2bn()` and `BN_dec2bn()` functions. It is believed that Node.js is not invoking the code paths that use these functions so practical attacks via Node.js using this defect are _unlikely_ to be possible. More info is available at [CVE-2016-0797](https://www.openssl.org/news/vulnerabilities.html#2016-0797).
  - Fix a defect that makes the _[CacheBleed Attack](https://ssrg.nicta.com.au/projects/TS/cachebleed/)_ possible. This defect enables attackers to execute side-channel attacks leading to the potential recovery of entire RSA private keys. It only affects the Intel Sandy Bridge (and possibly older) microarchitecture when using hyper-threading. Newer microarchitectures, including Haswell, are unaffected. More info is available at [CVE-2016-0702](https://www.openssl.org/news/vulnerabilities.html#2016-0702).

## Commits

* [[`c133797d09`](https://github.com/nodejs/node/commit/c133797d09)] - **deps**: upgrade openssl to 1.0.2g (Ben Noordhuis) [#5507](https://github.com/nodejs/node/pull/5507)

## 2016-02-23, Version 5.7.0 (Stable), @rvagg

### Notable changes

* **buffer**:
  - You can now supply an `encoding` argument when filling a Buffer `Buffer#fill(string[, start[, end]][, encoding])`, supplying an existing Buffer will also work with `Buffer#fill(buffer[, start[, end]])`. See the [API documentation](https://nodejs.org/api/buffer.html#buffer_buf_fill_value_offset_end_encoding) for details on how this works. (Trevor Norris) [#4935](https://github.com/nodejs/node/pull/4935)
  - `Buffer#indexOf()` no longer requires a `byteOffset` argument if you also wish to specify an `encoding`: `Buffer#indexOf(val[, byteOffset][, encoding])`. (Trevor Norris) [#4803](https://github.com/nodejs/node/pull/4803)
* **child_process**: `spawn()` and `spawnSync()` now support a `'shell'` option to allow for optional execution of the given command inside a shell. If set to `true`, `cmd.exe` will be used on Windows and `/bin/sh` elsewhere. A path to a custom shell can also be passed to override these defaults. On Windows, this option allows `.bat.` and `.cmd` files to be executed with `spawn()` and `spawnSync()`. (Colin Ihrig) [#4598](https://github.com/nodejs/node/pull/4598)
* **http_parser**: Update to http-parser 2.6.2 to fix an unintentionally strict limitation of allowable header characters (James M Snell) [#5237](https://github.com/nodejs/node/pull/5237)
* **dgram**: `socket.send()` now supports accepts an array of Buffers or Strings as the first argument. See the [API docs](https://nodejs.org/download/nightly/v6.0.0-nightly201602102848f84332/docs/api/dgram.html#dgram_socket_send_msg_offset_length_port_address_callback) for details on how this works. (Matteo Collina) [#4374](https://github.com/nodejs/node/pull/4374)
* **http**: Fix a bug where handling headers will mistakenly trigger an `'upgrade'` event where the server is just advertising its protocols. This bug can prevent HTTP clients from communicating with HTTP/2 enabled servers. (Fedor Indutny) [#4337](https://github.com/nodejs/node/pull/4337)
* **net**: Added a `listening` Boolean property to `net` and `http` servers to indicate whether the server is listening for connections. (José Moreira) [#4743](https://github.com/nodejs/node/pull/4743)
* **node**: The C++ `node::MakeCallback()` API is now reentrant and calling it from inside another `MakeCallback()` call no longer causes the `nextTick` queue or Promises microtask queue to be processed out of order. (Trevor Norris) [#4507](https://github.com/nodejs/node/pull/4507)
* **tls**: Add a new `tlsSocket.getProtocol()` method to get the negotiated TLS protocol version of the current connection. (Brian White) [#4995](https://github.com/nodejs/node/pull/4995)
* **vm**: Introduce new `'produceCachedData'` and `'cachedData'` options to `new vm.Script()` to interact with V8's code cache. When a new `vm.Script` object is created with the `'produceCachedData'` set to `true` a `Buffer` with V8's code cache data will be produced and stored in `cachedData` property of the returned object. This data in turn may be supplied back to another `vm.Script()` object with a `'cachedData'` option if the supplied source is the same. Successfully executing a script from cached data can speed up instantiation time. See the [API docs](https://nodejs.org/api/vm.html#vm_new_vm_script_code_options) for details. (Fedor Indutny) [#4777](https://github.com/nodejs/node/pull/4777)
* **performance**: Improvements in:
  - `process.nextTick()` (Ruben Bridgewater) [#5092](https://github.com/nodejs/node/pull/5092)
  - `path` module (Brian White) [#5123](https://github.com/nodejs/node/pull/5123)
  - `querystring` module (Brian White) [#5012](https://github.com/nodejs/node/pull/5012)
  - `streams` module when processing small chunks (Matteo Collina) [#4354](https://github.com/nodejs/node/pull/4354)

### Commits

* [[`3a96fa0030`](https://github.com/nodejs/node/commit/3a96fa0030)] - **async_wrap**: add parent uid to init hook (Andreas Madsen) [#4600](https://github.com/nodejs/node/pull/4600)
* [[`4ef04c7c4c`](https://github.com/nodejs/node/commit/4ef04c7c4c)] - **async_wrap**: make uid the first argument in init (Andreas Madsen) [#4600](https://github.com/nodejs/node/pull/4600)
* [[`4afe801f90`](https://github.com/nodejs/node/commit/4afe801f90)] - **async_wrap**: add uid to all asyncWrap hooks (Andreas Madsen) [#4600](https://github.com/nodejs/node/pull/4600)
* [[`edf8f8a7da`](https://github.com/nodejs/node/commit/edf8f8a7da)] - **benchmark**: split path benchmarks (Brian White) [#5123](https://github.com/nodejs/node/pull/5123)
* [[`8d713d8d51`](https://github.com/nodejs/node/commit/8d713d8d51)] - **benchmark**: allow empty parameters (Brian White) [#5123](https://github.com/nodejs/node/pull/5123)
* [[`eb6d07327a`](https://github.com/nodejs/node/commit/eb6d07327a)] - **(SEMVER-MINOR)** **buffer**: add encoding parameter to fill() (Trevor Norris) [#4935](https://github.com/nodejs/node/pull/4935)
* [[`60d2048b6c`](https://github.com/nodejs/node/commit/60d2048b6c)] - **(SEMVER-MINOR)** **buffer**: properly retrieve binary length of needle (Trevor Norris) [#4803](https://github.com/nodejs/node/pull/4803)
* [[`4c67d74607`](https://github.com/nodejs/node/commit/4c67d74607)] - **(SEMVER-MINOR)** **buffer**: allow encoding param to collapse (Trevor Norris) [#4803](https://github.com/nodejs/node/pull/4803)
* [[`5fa4117bfc`](https://github.com/nodejs/node/commit/5fa4117bfc)] - **build**: add a help message and removed a TODO. (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`09bfb865af`](https://github.com/nodejs/node/commit/09bfb865af)] - **build**: remove redundant TODO in configure (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`3dfc11c516`](https://github.com/nodejs/node/commit/3dfc11c516)] - **build**: remove Makefile.build (Ojas Shirekar) [#5080](https://github.com/nodejs/node/pull/5080)
* [[`fc78d3d6a7`](https://github.com/nodejs/node/commit/fc78d3d6a7)] - **build**: skip msi build if WiX is not found (Tsarevich Dmitry) [#5220](https://github.com/nodejs/node/pull/5220)
* [[`356acb39d7`](https://github.com/nodejs/node/commit/356acb39d7)] - **build**: treat aarch64 as arm64 (Johan Bergström) [#5191](https://github.com/nodejs/node/pull/5191)
* [[`3b83d42b4a`](https://github.com/nodejs/node/commit/3b83d42b4a)] - **build**: fix build when python path contains spaces (Felix Becker) [#4841](https://github.com/nodejs/node/pull/4841)
* [[`9e6ad2d8ff`](https://github.com/nodejs/node/commit/9e6ad2d8ff)] - **child_process**: fix data loss with readable event (Brian White) [#5036](https://github.com/nodejs/node/pull/5036)
* [[`ecc797600f`](https://github.com/nodejs/node/commit/ecc797600f)] - **(SEMVER-MINOR)** **child_process**: add shell option to spawn() (cjihrig) [#4598](https://github.com/nodejs/node/pull/4598)
* [[`efd6f68dce`](https://github.com/nodejs/node/commit/efd6f68dce)] - **cluster**: dont rely on `this` in `fork` (Igor Klopov) [#5216](https://github.com/nodejs/node/pull/5216)
* [[`df93d60caf`](https://github.com/nodejs/node/commit/df93d60caf)] - **console**: apply null as `this` for util.format (Jackson Tian) [#5222](https://github.com/nodejs/node/pull/5222)
* [[`c397ba8fa3`](https://github.com/nodejs/node/commit/c397ba8fa3)] - **contextify**: use offset/length from Uint8Array (Fedor Indutny) [#4947](https://github.com/nodejs/node/pull/4947)
* [[`3048ac0b57`](https://github.com/nodejs/node/commit/3048ac0b57)] - **crypto**: have fixed NodeBIOs return EOF (Adam Langley) [#5105](https://github.com/nodejs/node/pull/5105)
* [[`af074846f5`](https://github.com/nodejs/node/commit/af074846f5)] - **debugger**: remove unneeded callback check (Rich Trott) [#5319](https://github.com/nodejs/node/pull/5319)
* [[`7bac743f36`](https://github.com/nodejs/node/commit/7bac743f36)] - **debugger**: assert test before accessing this.binding (Prince J Wesley) [#5145](https://github.com/nodejs/node/pull/5145)
* [[`18c94e5a8d`](https://github.com/nodejs/node/commit/18c94e5a8d)] - **deps**: remove unnecessary files (Brian White) [#5212](https://github.com/nodejs/node/pull/5212)
* [[`967cf97bf0`](https://github.com/nodejs/node/commit/967cf97bf0)] - **deps**: cherry-pick 2e4da65 from v8's 4.8 upstream (Michael Dawson) [#5293](https://github.com/nodejs/node/pull/5293)
* [[`bbdf2684d5`](https://github.com/nodejs/node/commit/bbdf2684d5)] - **deps**: update to http-parser 2.6.2 (James M Snell) [#5237](https://github.com/nodejs/node/pull/5237)
* [[`127dd6275a`](https://github.com/nodejs/node/commit/127dd6275a)] - ***Revert*** "**deps**: sync with upstream c-ares/c-ares@4ef6817" (Ben Noordhuis) [#5199](https://github.com/nodejs/node/pull/5199)
* [[`35c3832994`](https://github.com/nodejs/node/commit/35c3832994)] - **deps**: sync with upstream c-ares/c-ares@4ef6817 (Fedor Indutny) [#5199](https://github.com/nodejs/node/pull/5199)
* [[`b4db31822f`](https://github.com/nodejs/node/commit/b4db31822f)] - **dgram**: scope redeclared variables (Rich Trott) [#4940](https://github.com/nodejs/node/pull/4940)
* [[`368c1d1098`](https://github.com/nodejs/node/commit/368c1d1098)] - **(SEMVER-MINOR)** **dgram**: support dgram.send with multiple buffers (Matteo Collina) [#4374](https://github.com/nodejs/node/pull/4374)
* [[`a8862f59eb`](https://github.com/nodejs/node/commit/a8862f59eb)] - **doc**: update repo docs to use 'CTC' (Alexis Campailla) [#5304](https://github.com/nodejs/node/pull/5304)
* [[`6cf8ec5bd1`](https://github.com/nodejs/node/commit/6cf8ec5bd1)] - **doc**: s/http/https in Myles Borins' GitHub link (Rod Vagg) [#5356](https://github.com/nodejs/node/pull/5356)
* [[`0389e3803c`](https://github.com/nodejs/node/commit/0389e3803c)] - **doc**: clarify child_process.execFile{,Sync} file arg (Kevin Locke) [#5310](https://github.com/nodejs/node/pull/5310)
* [[`c48290d9b7`](https://github.com/nodejs/node/commit/c48290d9b7)] - **doc**: fix buf.length slice example (Chinedu Francis Nwafili) [#5259](https://github.com/nodejs/node/pull/5259)
* [[`a6e437c619`](https://github.com/nodejs/node/commit/a6e437c619)] - **doc**: fix buffer\[index\] example (Chinedu Francis Nwafili) [#5253](https://github.com/nodejs/node/pull/5253)
* [[`73ef1bd423`](https://github.com/nodejs/node/commit/73ef1bd423)] - **doc**: fix template string (Rafael Cepeda) [#5240](https://github.com/nodejs/node/pull/5240)
* [[`fa04daa384`](https://github.com/nodejs/node/commit/fa04daa384)] - **doc**: clarify exceptions during uncaughtException (Noah Rose) [#5180](https://github.com/nodejs/node/pull/5180)
* [[`22f132e61d`](https://github.com/nodejs/node/commit/22f132e61d)] - **doc**: improvements to console.markdown copy (Alexander Makarenko) [#5225](https://github.com/nodejs/node/pull/5225)
* [[`48fa6f6063`](https://github.com/nodejs/node/commit/48fa6f6063)] - **doc**: update process.send() signature (cjihrig) [#5284](https://github.com/nodejs/node/pull/5284)
* [[`35d89d4662`](https://github.com/nodejs/node/commit/35d89d4662)] - **doc**: fix net.createConnection() example (Brian White) [#5219](https://github.com/nodejs/node/pull/5219)
* [[`149007c9f0`](https://github.com/nodejs/node/commit/149007c9f0)] - **doc**: replace node-forward link in CONTRIBUTING.md (Ben Noordhuis) [#5227](https://github.com/nodejs/node/pull/5227)
* [[`a6aaf2caab`](https://github.com/nodejs/node/commit/a6aaf2caab)] - **doc**: improve scrolling, various CSS tweaks (Roman Reiss) [#5198](https://github.com/nodejs/node/pull/5198)
* [[`18b00deeac`](https://github.com/nodejs/node/commit/18b00deeac)] - **doc**: update DCO to v1.1 (Mikeal Rogers) [#5170](https://github.com/nodejs/node/pull/5170)
* [[`3955bc4cd0`](https://github.com/nodejs/node/commit/3955bc4cd0)] - **doc**: fix minor inconsistencies in repl doc (Rich Trott) [#5193](https://github.com/nodejs/node/pull/5193)
* [[`287bce7b48`](https://github.com/nodejs/node/commit/287bce7b48)] - **doc**: merging behavior of writeHead vs setHeader (Alejandro Oviedo) [#5081](https://github.com/nodejs/node/pull/5081)
* [[`529e749d88`](https://github.com/nodejs/node/commit/529e749d88)] - **doc**: fix type references for link gen, link css (Claudio Rodriguez) [#4741](https://github.com/nodejs/node/pull/4741)
* [[`275f6dbcbb`](https://github.com/nodejs/node/commit/275f6dbcbb)] - **(SEMVER-MINOR)** **doc**: correct tlsSocket.getCipher() description (Brian White) [#4995](https://github.com/nodejs/node/pull/4995)
* [[`b706b0c2c5`](https://github.com/nodejs/node/commit/b706b0c2c5)] - **http**: remove old, confusing comment (Brian White) [#5233](https://github.com/nodejs/node/pull/5233)
* [[`ed36235248`](https://github.com/nodejs/node/commit/ed36235248)] - **http**: remove unnecessary check (Brian White) [#5233](https://github.com/nodejs/node/pull/5233)
* [[`7e82a566b3`](https://github.com/nodejs/node/commit/7e82a566b3)] - **(SEMVER-MINOR)** **http**: allow async createConnection() (Brian White) [#4638](https://github.com/nodejs/node/pull/4638)
* [[`411d813323`](https://github.com/nodejs/node/commit/411d813323)] - **http**: do not emit `upgrade` on advertisement (Fedor Indutny) [#4337](https://github.com/nodejs/node/pull/4337)
* [[`bbc786b50f`](https://github.com/nodejs/node/commit/bbc786b50f)] - **http,util**: fix typos in comments (Alexander Makarenko) [#5279](https://github.com/nodejs/node/pull/5279)
* [[`a2d198c702`](https://github.com/nodejs/node/commit/a2d198c702)] - **net**: use `_server` for internal book-keeping (Fedor Indutny) [#5262](https://github.com/nodejs/node/pull/5262)
* [[`18d24e60c5`](https://github.com/nodejs/node/commit/18d24e60c5)] - **(SEMVER-MINOR)** **net**: add net.listening boolean property over a getter (José Moreira) [#4743](https://github.com/nodejs/node/pull/4743)
* [[`9cee86e3e9`](https://github.com/nodejs/node/commit/9cee86e3e9)] - **node**: set process._eventsCount to 0 on startup (Evan Lucas) [#5208](https://github.com/nodejs/node/pull/5208)
* [[`f2e4f621c5`](https://github.com/nodejs/node/commit/f2e4f621c5)] - **node**: improve process.nextTick performance (Ruben Bridgewater) [#5092](https://github.com/nodejs/node/pull/5092)
* [[`1c6f927bd1`](https://github.com/nodejs/node/commit/1c6f927bd1)] - **path**: fix input type checking regression (Brian White) [#5244](https://github.com/nodejs/node/pull/5244)
* [[`4dae8caf7a`](https://github.com/nodejs/node/commit/4dae8caf7a)] - **path**: performance improvements on all platforms (Brian White) [#5123](https://github.com/nodejs/node/pull/5123)
* [[`46be1f4d0c`](https://github.com/nodejs/node/commit/46be1f4d0c)] - **querystring**: improve escape() performance (Brian White) [#5012](https://github.com/nodejs/node/pull/5012)
* [[`27e323e8c1`](https://github.com/nodejs/node/commit/27e323e8c1)] - **querystring**: improve unescapeBuffer() performance (Brian White) [#5012](https://github.com/nodejs/node/pull/5012)
* [[`301023b2b4`](https://github.com/nodejs/node/commit/301023b2b4)] - **querystring**: improve parse() performance (Brian White) [#5012](https://github.com/nodejs/node/pull/5012)
* [[`98907c716b`](https://github.com/nodejs/node/commit/98907c716b)] - **(SEMVER-MINOR)** **repl**: allow multiline function call (Zirak) [#3823](https://github.com/nodejs/node/pull/3823)
* [[`c551da8cb4`](https://github.com/nodejs/node/commit/c551da8cb4)] - **repl**: handle quotes within regexp literal (Prince J Wesley) [#5117](https://github.com/nodejs/node/pull/5117)
* [[`15091ccca2`](https://github.com/nodejs/node/commit/15091ccca2)] - **src**: remove unnecessary check (Brian White) [#5233](https://github.com/nodejs/node/pull/5233)
* [[`830bb04d90`](https://github.com/nodejs/node/commit/830bb04d90)] - **src**: remove TryCatch in MakeCallback (Trevor Norris) [#4507](https://github.com/nodejs/node/pull/4507)
* [[`7f22c8c8a6`](https://github.com/nodejs/node/commit/7f22c8c8a6)] - **src**: remove unused TickInfo::in_tick() (Trevor Norris) [#4507](https://github.com/nodejs/node/pull/4507)
* [[`406eb1f516`](https://github.com/nodejs/node/commit/406eb1f516)] - **src**: remove unused of TickInfo::last_threw() (Trevor Norris) [#4507](https://github.com/nodejs/node/pull/4507)
* [[`bcec2fecbd`](https://github.com/nodejs/node/commit/bcec2fecbd)] - **src**: add AsyncCallbackScope (Trevor Norris) [#4507](https://github.com/nodejs/node/pull/4507)
* [[`2cb1594279`](https://github.com/nodejs/node/commit/2cb1594279)] - **src**: fix MakeCallback error handling (Trevor Norris) [#4507](https://github.com/nodejs/node/pull/4507)
* [[`8d6e679a90`](https://github.com/nodejs/node/commit/8d6e679a90)] - **src,test,tools**: modify for more stringent linting (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`7684b0fcdf`](https://github.com/nodejs/node/commit/7684b0fcdf)] - **stream**: fix no data on partial decode (Brian White) [#5226](https://github.com/nodejs/node/pull/5226)
* [[`f706cb0189`](https://github.com/nodejs/node/commit/f706cb0189)] - **streams**: 5% throughput gain when sending small chunks (Matteo Collina) [#4354](https://github.com/nodejs/node/pull/4354)
* [[`25513a473a`](https://github.com/nodejs/node/commit/25513a473a)] - **string_decoder**: fix performance regression (Brian White) [#5134](https://github.com/nodejs/node/pull/5134)
* [[`0e85530d8c`](https://github.com/nodejs/node/commit/0e85530d8c)] - **test**: use String.prototype.repeat() for clarity (Rich Trott) [#5311](https://github.com/nodejs/node/pull/5311)
* [[`5683efb90a`](https://github.com/nodejs/node/commit/5683efb90a)] - **test**: remove flaky mark for test-debug-no-context (Rich Trott) [#5317](https://github.com/nodejs/node/pull/5317)
* [[`c55bb79ace`](https://github.com/nodejs/node/commit/c55bb79ace)] - **test**: add test for https server close event (Braydon Fuller) [#5106](https://github.com/nodejs/node/pull/5106)
* [[`138ee983b0`](https://github.com/nodejs/node/commit/138ee983b0)] - **test**: refactor test-http-destroyed-socket-write2 (Santiago Gimeno) [#4970](https://github.com/nodejs/node/pull/4970)
* [[`df7d91f36b`](https://github.com/nodejs/node/commit/df7d91f36b)] - **test**: mitigate flaky test-debug-no-context (Rich Trott) [#5269](https://github.com/nodejs/node/pull/5269)
* [[`d9177e7c26`](https://github.com/nodejs/node/commit/d9177e7c26)] - **test**: test-process-getactivehandles is flaky (Alexis Campailla) [#5303](https://github.com/nodejs/node/pull/5303)
* [[`f5cc04732f`](https://github.com/nodejs/node/commit/f5cc04732f)] - **test**: mark test-http-regr-gh-2928 flaky (Rich Trott) [#5280](https://github.com/nodejs/node/pull/5280)
* [[`78b349d5d1`](https://github.com/nodejs/node/commit/78b349d5d1)] - **test**: disable fs watch tests for AIX (Michael Dawson) [#5187](https://github.com/nodejs/node/pull/5187)
* [[`82ee5e94df`](https://github.com/nodejs/node/commit/82ee5e94df)] - **test**: mark test-http-agent flaky (Rich Trott) [#5209](https://github.com/nodejs/node/pull/5209)
* [[`1494d6f213`](https://github.com/nodejs/node/commit/1494d6f213)] - **test**: minimal repl eval option test (Rich Trott) [#5192](https://github.com/nodejs/node/pull/5192)
* [[`e7bf951136`](https://github.com/nodejs/node/commit/e7bf951136)] - **test**: add addons test for MakeCallback (Trevor Norris) [#4507](https://github.com/nodejs/node/pull/4507)
* [[`98596a94fa`](https://github.com/nodejs/node/commit/98596a94fa)] - **(SEMVER-MINOR)** **test**: run v8 tests from node tree (Bryon Leung) [#4704](https://github.com/nodejs/node/pull/4704)
* [[`69c544f245`](https://github.com/nodejs/node/commit/69c544f245)] - **test**: fix flaky test-http-regr-gh-2928 (Rich Trott) [#5154](https://github.com/nodejs/node/pull/5154)
* [[`7c88410507`](https://github.com/nodejs/node/commit/7c88410507)] - **test**: fix child-process-fork-regr-gh-2847 again (Santiago Gimeno) [#5179](https://github.com/nodejs/node/pull/5179)
* [[`2c2cb6700d`](https://github.com/nodejs/node/commit/2c2cb6700d)] - **test**: remove unneeded common.indirectInstanceOf() (Rich Trott) [#5149](https://github.com/nodejs/node/pull/5149)
* [[`6340974f21`](https://github.com/nodejs/node/commit/6340974f21)] - **test**: don't run test-tick-processor.js on Aix (Michael Dawson) [#5093](https://github.com/nodejs/node/pull/5093)
* [[`a8f4db236c`](https://github.com/nodejs/node/commit/a8f4db236c)] - **test**: improve path tests (Brian White) [#5123](https://github.com/nodejs/node/pull/5123)
* [[`8301773c1e`](https://github.com/nodejs/node/commit/8301773c1e)] - **test**: fix child-process-fork-regr-gh-2847 (Santiago Gimeno) [#5121](https://github.com/nodejs/node/pull/5121)
* [[`f2bd86775b`](https://github.com/nodejs/node/commit/f2bd86775b)] - **test**: update arrow function style (cjihrig) [#4813](https://github.com/nodejs/node/pull/4813)
* [[`aed04b85c2`](https://github.com/nodejs/node/commit/aed04b85c2)] - **tls**: nullify `.ssl` on handle close (Fedor Indutny) [#5168](https://github.com/nodejs/node/pull/5168)
* [[`c3f8aab652`](https://github.com/nodejs/node/commit/c3f8aab652)] - **(SEMVER-MINOR)** **tls**: add getProtocol() to TLS sockets (Brian White) [#4995](https://github.com/nodejs/node/pull/4995)
* [[`7fc2e3161f`](https://github.com/nodejs/node/commit/7fc2e3161f)] - **tools**: add Node.js-specific ESLint rules (Rich Trott) [#5320](https://github.com/nodejs/node/pull/5320)
* [[`983325cb0c`](https://github.com/nodejs/node/commit/983325cb0c)] - **tools**: replace obsolete ESLint rules (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`f601d040b5`](https://github.com/nodejs/node/commit/f601d040b5)] - **tools**: update ESLint to version 2.1.0 (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`13af565759`](https://github.com/nodejs/node/commit/13af565759)] - **tools**: remove obsolete lint rules (Rich Trott) [#5214](https://github.com/nodejs/node/pull/5214)
* [[`c566f44f1b`](https://github.com/nodejs/node/commit/c566f44f1b)] - **tools**: add recommended ES6 lint rules (Rich Trott) [#5210](https://github.com/nodejs/node/pull/5210)
* [[`b611caa0ba`](https://github.com/nodejs/node/commit/b611caa0ba)] - **tools**: add recommended linting rules (Rich Trott) [#5188](https://github.com/nodejs/node/pull/5188)
* [[`b1a16d1202`](https://github.com/nodejs/node/commit/b1a16d1202)] - **tools**: remove excessive comments from .eslintrc (Rich Trott) [#5151](https://github.com/nodejs/node/pull/5151)
* [[`c4ed5ece4d`](https://github.com/nodejs/node/commit/c4ed5ece4d)] - **tools**: enable no-proto rule for linter (Jackson Tian) [#5140](https://github.com/nodejs/node/pull/5140)
* [[`86f8477b56`](https://github.com/nodejs/node/commit/86f8477b56)] - **tools**: disallow mixed spaces and tabs for indents (Rich Trott) [#5135](https://github.com/nodejs/node/pull/5135)
* [[`21fd1496a9`](https://github.com/nodejs/node/commit/21fd1496a9)] - **tools**: alphabetize eslint stylistic issues section (Rich Trott)
* [[`22c8d50a1f`](https://github.com/nodejs/node/commit/22c8d50a1f)] - **tools**: parse types into links in doc html gen (Claudio Rodriguez) [#4741](https://github.com/nodejs/node/pull/4741)
* [[`5c54d4987d`](https://github.com/nodejs/node/commit/5c54d4987d)] - **tools**: enable no-redeclare rule for linter (Rich Trott) [#5047](https://github.com/nodejs/node/pull/5047)
* [[`a3a0cf603a`](https://github.com/nodejs/node/commit/a3a0cf603a)] - **tools**: add arrow function rules to eslint (cjihrig) [#4813](https://github.com/nodejs/node/pull/4813)
* [[`bcc26f747f`](https://github.com/nodejs/node/commit/bcc26f747f)] - **tools,doc**: fix linting errors (Rich Trott) [#5161](https://github.com/nodejs/node/pull/5161)
* [[`47274704aa`](https://github.com/nodejs/node/commit/47274704aa)] - **url**: fix lint and deopt issues (Brian White) [#5300](https://github.com/nodejs/node/pull/5300)
* [[`729ad75860`](https://github.com/nodejs/node/commit/729ad75860)] - **url**: improve url.parse() performance (Brian White) [#4892](https://github.com/nodejs/node/pull/4892)
* [[`6c8378b15b`](https://github.com/nodejs/node/commit/6c8378b15b)] - **vm**: fix `produceCachedData` (Jiho Choi) [#5343](https://github.com/nodejs/node/pull/5343)
* [[`d1cacb814f`](https://github.com/nodejs/node/commit/d1cacb814f)] - **(SEMVER-MINOR)** **vm**: introduce `cachedData`/`produceCachedData` (Fedor Indutny) [#4777](https://github.com/nodejs/node/pull/4777)

## 2016-02-16, Version 4.3.1 'Argon' (LTS), @thealphanerd

### Notable changes

* **buffer**
  * make byteLength work with Buffer correctly (Jackson Tian)
    - [#4738](https://github.com/nodejs/node/pull/4738)
* **debugger**
  * guard against call from non-node context (Ben Noordhuis)
    - [#4328](https://github.com/nodejs/node/pull/4328)
    - fixes segfaults in debugger
  * do not incept debug context (Myles Borins)
    - [#4819](https://github.com/nodejs/node/pull/4819)
    - fixes crash in debugger when using util methods
* **deps**
  * update to http-parser 2.5.2 (James Snell)
    - [#5238](https://github.com/nodejs/node/pull/5238)

### Commits

* [[`748d2b4de1`](https://github.com/nodejs/node/commit/748d2b4de1)] - **buffer**: make byteLength work with Buffer correctly (Jackson Tian) [#4738](https://github.com/nodejs/node/pull/4738)
* [[`fb615bdaf4`](https://github.com/nodejs/node/commit/fb615bdaf4)] - **buffer**: remove unnecessary TODO comments (Peter Geiss) [#4719](https://github.com/nodejs/node/pull/4719)
* [[`b8213ba7e1`](https://github.com/nodejs/node/commit/b8213ba7e1)] - **cluster**: ignore queryServer msgs on disconnection (Santiago Gimeno) [#4465](https://github.com/nodejs/node/pull/4465)
* [[`f8a676ed59`](https://github.com/nodejs/node/commit/f8a676ed59)] - **cluster**: fix race condition setting suicide prop (Santiago Gimeno) [#4349](https://github.com/nodejs/node/pull/4349)
* [[`9d4a226dad`](https://github.com/nodejs/node/commit/9d4a226dad)] - **crypto**: clear error stack in ECDH::Initialize (Fedor Indutny) [#4689](https://github.com/nodejs/node/pull/4689)
* [[`583f3347d8`](https://github.com/nodejs/node/commit/583f3347d8)] - **debugger**: remove variable redeclarations (Rich Trott) [#4633](https://github.com/nodejs/node/pull/4633)
* [[`667f7a7ab3`](https://github.com/nodejs/node/commit/667f7a7ab3)] - **debugger**: guard against call from non-node context (Ben Noordhuis) [#4328](https://github.com/nodejs/node/pull/4328)
* [[`188cff3c31`](https://github.com/nodejs/node/commit/188cff3c31)] - **deps**: update to http-parser 2.5.2 (James Snell) [#5238](https://github.com/nodejs/node/pull/5238)
* [[`6e829b44e3`](https://github.com/nodejs/node/commit/6e829b44e3)] - **dgram**: prevent disabled optimization of bind() (Brian White) [#4613](https://github.com/nodejs/node/pull/4613)
* [[`c3956d05b1`](https://github.com/nodejs/node/commit/c3956d05b1)] - **doc**: update list of personal traits in CoC (Kat Marchán) [#4801](https://github.com/nodejs/node/pull/4801)
* [[`39cb69ca21`](https://github.com/nodejs/node/commit/39cb69ca21)] - **doc**: style fixes for the TOC (Roman Reiss) [#4748](https://github.com/nodejs/node/pull/4748)
* [[`cb5986da81`](https://github.com/nodejs/node/commit/cb5986da81)] - **doc**: add `servername` parameter docs (Alexander Makarenko) [#4729](https://github.com/nodejs/node/pull/4729)
* [[`91066b5f34`](https://github.com/nodejs/node/commit/91066b5f34)] - **doc**: update branch-diff arguments in release doc (Rod Vagg) [#4691](https://github.com/nodejs/node/pull/4691)
* [[`9ca24de41d`](https://github.com/nodejs/node/commit/9ca24de41d)] - **doc**: add docs for more stream options (zoubin) [#4639](https://github.com/nodejs/node/pull/4639)
* [[`437d0e336d`](https://github.com/nodejs/node/commit/437d0e336d)] - **doc**: mention that http.Server inherits from net.Server (Ryan Sobol) [#4455](https://github.com/nodejs/node/pull/4455)
* [[`393e569160`](https://github.com/nodejs/node/commit/393e569160)] - **doc**: copyedit setTimeout() documentation (Rich Trott) [#4434](https://github.com/nodejs/node/pull/4434)
* [[`e2a682ecc3`](https://github.com/nodejs/node/commit/e2a682ecc3)] - **doc**: fix formatting in process.markdown (Rich Trott) [#4433](https://github.com/nodejs/node/pull/4433)
* [[`75b0ea85bd`](https://github.com/nodejs/node/commit/75b0ea85bd)] - **doc**: add path property to Write/ReadStream in fs.markdown (Claudio Rodriguez) [#4368](https://github.com/nodejs/node/pull/4368)
* [[`48c2783421`](https://github.com/nodejs/node/commit/48c2783421)] - **doc**: add docs working group (Bryan English) [#4244](https://github.com/nodejs/node/pull/4244)
* [[`c0432e9f56`](https://github.com/nodejs/node/commit/c0432e9f56)] - **doc**: restore ICU third-party software licenses (Richard Lau) [#4762](https://github.com/nodejs/node/pull/4762)
* [[`36a4159dab`](https://github.com/nodejs/node/commit/36a4159dab)] - **doc**: rebuild LICENSE using tools/license-builder.sh (Rod Vagg) [#4194](https://github.com/nodejs/node/pull/4194)
* [[`a2998a1bce`](https://github.com/nodejs/node/commit/a2998a1bce)] - **gitignore**: never ignore debug module (Michaël Zasso) [#2286](https://github.com/nodejs/node/pull/2286)
* [[`661b2557d9`](https://github.com/nodejs/node/commit/661b2557d9)] - **http**: remove variable redeclaration (Rich Trott) [#4612](https://github.com/nodejs/node/pull/4612)
* [[`1bb2967d48`](https://github.com/nodejs/node/commit/1bb2967d48)] - **http**: fix non-string header value concatenation (Brian White) [#4460](https://github.com/nodejs/node/pull/4460)
* [[`15ed64e34c`](https://github.com/nodejs/node/commit/15ed64e34c)] - **lib**: fix style issues after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`2e92a1a6b4`](https://github.com/nodejs/node/commit/2e92a1a6b4)] - **module**: move unnecessary work for early return (Andres Suarez) [#3579](https://github.com/nodejs/node/pull/3579)
* [[`40c8e6d75d`](https://github.com/nodejs/node/commit/40c8e6d75d)] - **net**: remove hot path comment from connect (Evan Lucas) [#4648](https://github.com/nodejs/node/pull/4648)
* [[`8ed0c1c22c`](https://github.com/nodejs/node/commit/8ed0c1c22c)] - **net**: fix dns lookup for android (Josh Dague) [#4580](https://github.com/nodejs/node/pull/4580)
* [[`15fa555204`](https://github.com/nodejs/node/commit/15fa555204)] - **net, doc**: fix line wrapping lint in net.js (James M Snell) [#4588](https://github.com/nodejs/node/pull/4588)
* [[`1b070e48e0`](https://github.com/nodejs/node/commit/1b070e48e0)] - **node_contextify**: do not incept debug context (Myles Borins) [#4815](https://github.com/nodejs/node/issues/4815)
* [[`4fbcb47fe9`](https://github.com/nodejs/node/commit/4fbcb47fe9)] - **readline**: Remove XXX and output debuglog (Kohei TAKATA) [#4690](https://github.com/nodejs/node/pull/4690)
* [[`26f02405d0`](https://github.com/nodejs/node/commit/26f02405d0)] - **repl**: make sure historyPath is trimmed (Evan Lucas) [#4539](https://github.com/nodejs/node/pull/4539)
* [[`5990ba2a0a`](https://github.com/nodejs/node/commit/5990ba2a0a)] - **src**: remove redeclarations of variables (Rich Trott) [#4605](https://github.com/nodejs/node/pull/4605)
* [[`c41ed59dbc`](https://github.com/nodejs/node/commit/c41ed59dbc)] - **src**: don't check failure with ERR_peek_error() (Ben Noordhuis) [#4731](https://github.com/nodejs/node/pull/4731)
* [[`d71f9992f9`](https://github.com/nodejs/node/commit/d71f9992f9)] - **stream**: remove useless if test in transform (zoubin) [#4617](https://github.com/nodejs/node/pull/4617)
* [[`f205e9920e`](https://github.com/nodejs/node/commit/f205e9920e)] - **test**: fix tls-no-rsa-key flakiness (Santiago Gimeno) [#4043](https://github.com/nodejs/node/pull/4043)
* [[`447347cd62`](https://github.com/nodejs/node/commit/447347cd62)] - **test**: fix issues for space-in-parens ESLint rule (Roman Reiss) [#4753](https://github.com/nodejs/node/pull/4753)
* [[`be8274508c`](https://github.com/nodejs/node/commit/be8274508c)] - **test**: improve test-cluster-disconnect-suicide-race (Rich Trott) [#4739](https://github.com/nodejs/node/pull/4739)
* [[`0178001163`](https://github.com/nodejs/node/commit/0178001163)] - **test**: make test-cluster-disconnect-leak reliable (Rich Trott) [#4736](https://github.com/nodejs/node/pull/4736)
* [[`d615757da2`](https://github.com/nodejs/node/commit/d615757da2)] - **test**: fix flaky test-net-socket-local-address (cjihrig) [#4650](https://github.com/nodejs/node/pull/4650)
* [[`baa0a3dff5`](https://github.com/nodejs/node/commit/baa0a3dff5)] - **test**: fix race in test-net-server-pause-on-connect (Rich Trott) [#4637](https://github.com/nodejs/node/pull/4637)
* [[`909b5167cb`](https://github.com/nodejs/node/commit/909b5167cb)] - **test**: remove 1 second delay from test (Rich Trott) [#4616](https://github.com/nodejs/node/pull/4616)
* [[`8ea76608ed`](https://github.com/nodejs/node/commit/8ea76608ed)] - **test**: move resource intensive tests to sequential (Rich Trott) [#4615](https://github.com/nodejs/node/pull/4615)
* [[`7afcdd358e`](https://github.com/nodejs/node/commit/7afcdd358e)] - **test**: require common module only once (Rich Trott) [#4611](https://github.com/nodejs/node/pull/4611)
* [[`0e02eb0bbe`](https://github.com/nodejs/node/commit/0e02eb0bbe)] - **test**: only include http module once (Rich Trott) [#4606](https://github.com/nodejs/node/pull/4606)
* [[`34d9e48bb6`](https://github.com/nodejs/node/commit/34d9e48bb6)] - **test**: fix `http-upgrade-client` flakiness (Santiago Gimeno) [#4602](https://github.com/nodejs/node/pull/4602)
* [[`556703d531`](https://github.com/nodejs/node/commit/556703d531)] - **test**: fix flaky unrefed timers test (Rich Trott) [#4599](https://github.com/nodejs/node/pull/4599)
* [[`3d5bc69796`](https://github.com/nodejs/node/commit/3d5bc69796)] - **test**: fix `http-upgrade-agent` flakiness (Santiago Gimeno) [#4520](https://github.com/nodejs/node/pull/4520)
* [[`ec24d3767b`](https://github.com/nodejs/node/commit/ec24d3767b)] - **test**: fix flaky test-cluster-shared-leak (Rich Trott) [#4510](https://github.com/nodejs/node/pull/4510)
* [[`a256790327`](https://github.com/nodejs/node/commit/a256790327)] - **test**: fix flaky cluster-net-send (Brian White) [#4444](https://github.com/nodejs/node/pull/4444)
* [[`6809c2be1a`](https://github.com/nodejs/node/commit/6809c2be1a)] - **test**: fix flaky child-process-fork-regr-gh-2847 (Brian White) [#4442](https://github.com/nodejs/node/pull/4442)
* [[`e6448aa36b`](https://github.com/nodejs/node/commit/e6448aa36b)] - **test**: use addon.md block headings as test dir names (Rod Vagg) [#4412](https://github.com/nodejs/node/pull/4412)
* [[`305d340fca`](https://github.com/nodejs/node/commit/305d340fca)] - **test**: test each block in addon.md contains js & cc (Rod Vagg) [#4411](https://github.com/nodejs/node/pull/4411)
* [[`f213406575`](https://github.com/nodejs/node/commit/f213406575)] - **test**: fix tls-multi-key race condition (Santiago Gimeno) [#3966](https://github.com/nodejs/node/pull/3966)
* [[`607f545568`](https://github.com/nodejs/node/commit/607f545568)] - **test**: fix style issues after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`aefb20a94f`](https://github.com/nodejs/node/commit/aefb20a94f)] - **tls**: copy client CAs and cert store on CertCb (Fedor Indutny) [#3537](https://github.com/nodejs/node/pull/3537)
* [[`7821b3e305`](https://github.com/nodejs/node/commit/7821b3e305)] - **tls_legacy**: do not read on OpenSSL's stack (Fedor Indutny) [#4624](https://github.com/nodejs/node/pull/4624)
* [[`b66db49f94`](https://github.com/nodejs/node/commit/b66db49f94)] - **tools**: add support for subkeys in release tools (Myles Borins) [#4807](https://github.com/nodejs/node/pull/4807)
* [[`837ebd1985`](https://github.com/nodejs/node/commit/837ebd1985)] - **tools**: enable space-in-parens ESLint rule (Roman Reiss) [#4753](https://github.com/nodejs/node/pull/4753)
* [[`066d5e7da2`](https://github.com/nodejs/node/commit/066d5e7da2)] - **tools**: fix style issue after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`b20ea69f46`](https://github.com/nodejs/node/commit/b20ea69f46)] - **tools**: update eslint config (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`2e0352d50c`](https://github.com/nodejs/node/commit/2e0352d50c)] - **tools**: update eslint to v1.10.3 (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`c96800a432`](https://github.com/nodejs/node/commit/c96800a432)] - **tools**: fix license-builder.sh for ICU (Richard Lau) [#4762](https://github.com/nodejs/node/pull/4762)
* [[`720b03dca7`](https://github.com/nodejs/node/commit/720b03dca7)] - **tools**: add license-builder.sh to construct LICENSE (Rod Vagg) [#4194](https://github.com/nodejs/node/pull/4194)

## 2016-02-09, Version 5.6.0 (Stable), @jasnell

This is an important security release. All Node.js users should consult the security release summary at nodejs.org for details on patched vulnerabilities.

### Notable changes

* **http**: fix defects in HTTP header parsing for requests and responses that can allow request smuggling (CVE-2016-2086) or response splitting (CVE-2016-2216). HTTP header parsing now aligns more closely with the HTTP spec including restricting the acceptable characters.
* **http-parser**: upgrade from 2.6.0 to 2.6.1
* **npm**: upgrade npm from 3.3.12 to 3.6.0 (Rebecca Turner) [#4958](https://github.com/nodejs/node/pull/4958)
* **openssl**: upgrade from 1.0.2e to 1.0.2f. To mitigate against the Logjam attack, TLS clients now reject Diffie-Hellman handshakes with parameters shorter than 1024-bits, up from the previous limit of 768-bits.

### Commits

* [[`3b6283c163`](https://github.com/nodejs/node/commit/3b6283c163)] - **benchmark**: add a constant declaration for `net` (Minwoo Jung) [#3950](https://github.com/nodejs/node/pull/3950)
* [[`3175f7450e`](https://github.com/nodejs/node/commit/3175f7450e)] - **buffer**: remove duplicated code in fromObject (HUANG Wei) [#4948](https://github.com/nodejs/node/pull/4948)
* [[`58d67e26a2`](https://github.com/nodejs/node/commit/58d67e26a2)] - **buffer**: validate list elements in Buffer.concat (Michaël Zasso) [#4951](https://github.com/nodejs/node/pull/4951)
* [[`bafc86f00e`](https://github.com/nodejs/node/commit/bafc86f00e)] - **buffer**: refactor redeclared variables (Rich Trott) [#4886](https://github.com/nodejs/node/pull/4886)
* [[`0fa4d90b94`](https://github.com/nodejs/node/commit/0fa4d90b94)] - **build**: Add VARIATION variable to binary target (Stefan Budeanu) [#4631](https://github.com/nodejs/node/pull/4631)
* [[`ec62789152`](https://github.com/nodejs/node/commit/ec62789152)] - **crypto**: fix memory leak in LoadPKCS12 (Fedor Indutny) [#5109](https://github.com/nodejs/node/pull/5109)
* [[`d9e934c71f`](https://github.com/nodejs/node/commit/d9e934c71f)] - **crypto**: add `pfx` certs as CA certs too (Fedor Indutny) [#5109](https://github.com/nodejs/node/pull/5109)
* [[`0d4b538175`](https://github.com/nodejs/node/commit/0d4b538175)] - **crypto**: use SSL_CTX_clear_extra_chain_certs. (Adam Langley) [#4919](https://github.com/nodejs/node/pull/4919)
* [[`abb0f6cd53`](https://github.com/nodejs/node/commit/abb0f6cd53)] - **crypto**: fix build when OCSP-stapling not provided (Adam Langley) [#4914](https://github.com/nodejs/node/pull/4914)
* [[`755619c554`](https://github.com/nodejs/node/commit/755619c554)] - **crypto**: use a const SSL_CIPHER (Adam Langley) [#4913](https://github.com/nodejs/node/pull/4913)
* [[`d5d2f86f89`](https://github.com/nodejs/node/commit/d5d2f86f89)] - **(SEMVER-MINOR)** **deps**: update http-parser to version 2.6.1 (James M Snell)
* [[`f0bd176d6d`](https://github.com/nodejs/node/commit/f0bd176d6d)] - **deps**: reapply c-ares floating patch (Ben Noordhuis) [#5090](https://github.com/nodejs/node/pull/5090)
* [[`f1a0827417`](https://github.com/nodejs/node/commit/f1a0827417)] - **deps**: sync with upstream bagder/c-ares@2bae2d5 (Fedor Indutny) [#5090](https://github.com/nodejs/node/pull/5090)
* [[`cbf36de8f1`](https://github.com/nodejs/node/commit/cbf36de8f1)] - **deps**: upgrade npm to 3.6.0 (Rebecca Turner) [#4958](https://github.com/nodejs/node/pull/4958)
* [[`dd97d07a0d`](https://github.com/nodejs/node/commit/dd97d07a0d)] - **deps**: backport 8d00c2c from v8 upstream (Gibson Fahnestock) [#5024](https://github.com/nodejs/node/pull/5024)
* [[`b75263094b`](https://github.com/nodejs/node/commit/b75263094b)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`b312b7914f`](https://github.com/nodejs/node/commit/b312b7914f)] - **deps**: upgrade openssl sources to 1.0.2f (Myles Borins) [#4961](https://github.com/nodejs/node/pull/4961)
* [[`fa0457ed04`](https://github.com/nodejs/node/commit/fa0457ed04)] - **dns**: throw a TypeError in lookupService with invalid port (Evan Lucas) [#4839](https://github.com/nodejs/node/pull/4839)
* [[`c4c8b3bf2e`](https://github.com/nodejs/node/commit/c4c8b3bf2e)] - **doc**: fix dgram doc indentation (Rich Trott) [#5118](https://github.com/nodejs/node/pull/5118)
* [[`027cd2719f`](https://github.com/nodejs/node/commit/027cd2719f)] - **doc**: clarify code of conduct reporting (Julie Pagano) [#5107](https://github.com/nodejs/node/pull/5107)
* [[`9f7aa6f868`](https://github.com/nodejs/node/commit/9f7aa6f868)] - **doc**: clarify dgram socket.send() multi-buffer support (Matteo Collina) [#5130](https://github.com/nodejs/node/pull/5130)
* [[`a96ae2cb37`](https://github.com/nodejs/node/commit/a96ae2cb37)] - **doc**: console is asynchronous unless it's a file (Ben Noordhuis) [#5133](https://github.com/nodejs/node/pull/5133)
* [[`4c54c8f309`](https://github.com/nodejs/node/commit/4c54c8f309)] - **doc**: fix typo in dgram doc (Rich Trott) [#5114](https://github.com/nodejs/node/pull/5114)
* [[`9c93ea3d51`](https://github.com/nodejs/node/commit/9c93ea3d51)] - **doc**: fix links order in Buffer doc (Alexander Makarenko) [#5076](https://github.com/nodejs/node/pull/5076)
* [[`a0ba378880`](https://github.com/nodejs/node/commit/a0ba378880)] - **doc**: minor improvement in OS docs (Alexander Makarenko) [#5006](https://github.com/nodejs/node/pull/5006)
* [[`1e2108a6b7`](https://github.com/nodejs/node/commit/1e2108a6b7)] - **doc**: fix links in Addons docs (Alexander Makarenko) [#5072](https://github.com/nodejs/node/pull/5072)
* [[`e5134b1701`](https://github.com/nodejs/node/commit/e5134b1701)] - **doc**: fix inconsistent styling (Brian White) [#4996](https://github.com/nodejs/node/pull/4996)
* [[`dde160378e`](https://github.com/nodejs/node/commit/dde160378e)] - **doc**: fix link in cluster documentation (Timothy Gu) [#5068](https://github.com/nodejs/node/pull/5068)
* [[`e5254c12f4`](https://github.com/nodejs/node/commit/e5254c12f4)] - **doc**: fix reference to  API `hash.final` (Minwoo Jung) [#5050](https://github.com/nodejs/node/pull/5050)
* [[`87fd9968a8`](https://github.com/nodejs/node/commit/87fd9968a8)] - **doc**: clarify optional arguments of Buffer methods (Michaël Zasso) [#5008](https://github.com/nodejs/node/pull/5008)
* [[`9908eced24`](https://github.com/nodejs/node/commit/9908eced24)] - **doc**: uppercase 'RSA-SHA256' in crypto.markdown (Rainer Oviir) [#5044](https://github.com/nodejs/node/pull/5044)
* [[`bf0383bbea`](https://github.com/nodejs/node/commit/bf0383bbea)] - **doc**: apply consistent styling for functions (Rich Trott) [#4974](https://github.com/nodejs/node/pull/4974)
* [[`8c7f4bab2d`](https://github.com/nodejs/node/commit/8c7f4bab2d)] - **doc**: multiple improvements in Stream docs (Alexander Makarenko) [#5009](https://github.com/nodejs/node/pull/5009)
* [[`ee013715b9`](https://github.com/nodejs/node/commit/ee013715b9)] - **doc**: improve styling consistency in VM docs (Alexander Makarenko) [#5005](https://github.com/nodejs/node/pull/5005)
* [[`9824b0d132`](https://github.com/nodejs/node/commit/9824b0d132)] - **doc**: fix anchor links from stream to http and events (piepmatz) [#5007](https://github.com/nodejs/node/pull/5007)
* [[`2c85f79569`](https://github.com/nodejs/node/commit/2c85f79569)] - **doc**: minor improvement to HTTPS doc (Alexander Makarenko) [#5002](https://github.com/nodejs/node/pull/5002)
* [[`9cf1370017`](https://github.com/nodejs/node/commit/9cf1370017)] - **doc**: improve styling consistency in Buffer docs (Alexander Makarenko) [#5001](https://github.com/nodejs/node/pull/5001)
* [[`2750cb0613`](https://github.com/nodejs/node/commit/2750cb0613)] - **doc**: consistent styling for functions in TLS docs (Alexander Makarenko) [#5000](https://github.com/nodejs/node/pull/5000)
* [[`4758bf13a5`](https://github.com/nodejs/node/commit/4758bf13a5)] - **doc**: update npm LICENSE using license-builder.sh (Rebecca Turner) [#4958](https://github.com/nodejs/node/pull/4958)
* [[`3b08b5d22c`](https://github.com/nodejs/node/commit/3b08b5d22c)] - **doc**: fix minor typo in process doc (Prayag Verma) [#5018](https://github.com/nodejs/node/pull/5018)
* [[`129977c9c7`](https://github.com/nodejs/node/commit/129977c9c7)] - **doc**: fix typo in Readme.md (Prayag Verma) [#5017](https://github.com/nodejs/node/pull/5017)
* [[`5de3dc557f`](https://github.com/nodejs/node/commit/5de3dc557f)] - **doc**: fix `notDeepEqual` API (Minwoo Jung) [#4971](https://github.com/nodejs/node/pull/4971)
* [[`d47dadcc1f`](https://github.com/nodejs/node/commit/d47dadcc1f)] - **doc**: make buffer methods styles consistent (Timothy Gu) [#4873](https://github.com/nodejs/node/pull/4873)
* [[`17888b122c`](https://github.com/nodejs/node/commit/17888b122c)] - **doc**: fix JSON generation for aliased methods (Timothy Gu) [#4871](https://github.com/nodejs/node/pull/4871)
* [[`396e4b9199`](https://github.com/nodejs/node/commit/396e4b9199)] - **doc**: add more details to process.env (Evan Lucas) [#4924](https://github.com/nodejs/node/pull/4924)
* [[`bc11bf4659`](https://github.com/nodejs/node/commit/bc11bf4659)] - **doc**: don't use "interface" as a variable name (ChALkeR) [#4900](https://github.com/nodejs/node/pull/4900)
* [[`bcf55d2f44`](https://github.com/nodejs/node/commit/bcf55d2f44)] - **doc**: spell writable consistently (Peter Lyons) [#4954](https://github.com/nodejs/node/pull/4954)
* [[`4a6d0ac436`](https://github.com/nodejs/node/commit/4a6d0ac436)] - **doc**: update eol handling in readline (Kári Tristan Helgason) [#4927](https://github.com/nodejs/node/pull/4927)
* [[`e65d3638c0`](https://github.com/nodejs/node/commit/e65d3638c0)] - **doc**: replace function expressions with arrows (Benjamin Gruenbaum) [#4832](https://github.com/nodejs/node/pull/4832)
* [[`423a58d66f`](https://github.com/nodejs/node/commit/423a58d66f)] - **doc**: show links consistently in deprecations (Sakthipriyan Vairamani) [#4907](https://github.com/nodejs/node/pull/4907)
* [[`fd87659139`](https://github.com/nodejs/node/commit/fd87659139)] - **doc**: add docs working group (Bryan English) [#4244](https://github.com/nodejs/node/pull/4244)
* [[`19ed619cff`](https://github.com/nodejs/node/commit/19ed619cff)] - **doc**: remove unnecessary bind(this) (Dmitriy Lazarev) [#4797](https://github.com/nodejs/node/pull/4797)
* [[`5129930786`](https://github.com/nodejs/node/commit/5129930786)] - **doc**: keep the names in sorted order (Sakthipriyan Vairamani) [#4876](https://github.com/nodejs/node/pull/4876)
* [[`3c46c10d54`](https://github.com/nodejs/node/commit/3c46c10d54)] - **doc**: fix nonsensical grammar in Buffer::write (Jimb Esser) [#4863](https://github.com/nodejs/node/pull/4863)
* [[`a1af6fc1a7`](https://github.com/nodejs/node/commit/a1af6fc1a7)] - **doc**: add `servername` parameter docs (Alexander Makarenko) [#4729](https://github.com/nodejs/node/pull/4729)
* [[`f4eeba8467`](https://github.com/nodejs/node/commit/f4eeba8467)] - **doc**: fix code type of markdowns (Jackson Tian) [#4858](https://github.com/nodejs/node/pull/4858)
* [[`fa1d453359`](https://github.com/nodejs/node/commit/fa1d453359)] - **doc**: check for errors in 'listen' event (Benjamin Gruenbaum) [#4834](https://github.com/nodejs/node/pull/4834)
* [[`f462320f74`](https://github.com/nodejs/node/commit/f462320f74)] - **doc**: undo move http.IncomingMessage.statusMessage (Jeff Harris) [#4822](https://github.com/nodejs/node/pull/4822)
* [[`711245e5ac`](https://github.com/nodejs/node/commit/711245e5ac)] - **doc**: style fixes for the TOC (Roman Reiss) [#4748](https://github.com/nodejs/node/pull/4748)
* [[`611c2f6fdf`](https://github.com/nodejs/node/commit/611c2f6fdf)] - **doc**: proper markdown escaping -> \_\_, \*, \_ (Robert Jefe Lindstaedt) [#4805](https://github.com/nodejs/node/pull/4805)
* [[`5a860d9cb7`](https://github.com/nodejs/node/commit/5a860d9cb7)] - **doc**: Examples work when data exceeds buffer size (Glen Arrowsmith) [#4811](https://github.com/nodejs/node/pull/4811)
* [[`71ba14de86`](https://github.com/nodejs/node/commit/71ba14de86)] - **doc**: update list of personal traits in CoC (Kat Marchán) [#4801](https://github.com/nodejs/node/pull/4801)
* [[`97eedfc57a`](https://github.com/nodejs/node/commit/97eedfc57a)] - **doc**: harmonize $ node command line notation (Robert Jefe Lindstaedt) [#4806](https://github.com/nodejs/node/pull/4806)
* [[`2dde0f08c9`](https://github.com/nodejs/node/commit/2dde0f08c9)] - **doc**: add buf.indexOf encoding param with example (Karl Skomski) [#3373](https://github.com/nodejs/node/pull/3373)
* [[`66c74548de`](https://github.com/nodejs/node/commit/66c74548de)] - **doc**: fenced all code blocks, typo fixes (Robert Jefe Lindstaedt) [#4733](https://github.com/nodejs/node/pull/4733)
* [[`54e8845b5e`](https://github.com/nodejs/node/commit/54e8845b5e)] - **fs**: refactor redeclared variables (Rich Trott) [#4959](https://github.com/nodejs/node/pull/4959)
* [[`fa940cf9bc`](https://github.com/nodejs/node/commit/fa940cf9bc)] - **fs**: remove unused branches (Benjamin Gruenbaum) [#4795](https://github.com/nodejs/node/pull/4795)
* [[`a3b84a4c93`](https://github.com/nodejs/node/commit/a3b84a4c93)] - **(SEMVER-MINOR)** **http**: strictly forbid invalid characters from headers (James M Snell)
* [[`9b03af254a`](https://github.com/nodejs/node/commit/9b03af254a)] - **http**: remove reference to onParserExecute (Tom Atkinson) [#4773](https://github.com/nodejs/node/pull/4773)
* [[`101de9de3f`](https://github.com/nodejs/node/commit/101de9de3f)] - **https**: evict cached sessions on error (Fedor Indutny) [#4982](https://github.com/nodejs/node/pull/4982)
* [[`b2c8b7f6d3`](https://github.com/nodejs/node/commit/b2c8b7f6d3)] - **internal/child_process**: call postSend on error (Fedor Indutny) [#4752](https://github.com/nodejs/node/pull/4752)
* [[`55030922e5`](https://github.com/nodejs/node/commit/55030922e5)] - **lib**: scope loop variables (Rich Trott) [#4965](https://github.com/nodejs/node/pull/4965)
* [[`725ad5b1ce`](https://github.com/nodejs/node/commit/725ad5b1ce)] - **lib**: remove string_decoder.js var redeclarations (Rich Trott) [#4978](https://github.com/nodejs/node/pull/4978)
* [[`c09eb44a59`](https://github.com/nodejs/node/commit/c09eb44a59)] - **module**: refactor redeclared variable (Rich Trott) [#4962](https://github.com/nodejs/node/pull/4962)
* [[`612ce66c78`](https://github.com/nodejs/node/commit/612ce66c78)] - **net**: refactor redeclared variables (Rich Trott) [#4963](https://github.com/nodejs/node/pull/4963)
* [[`c9b05dafe0`](https://github.com/nodejs/node/commit/c9b05dafe0)] - **net**: move isLegalPort to internal/net (Evan Lucas) [#4882](https://github.com/nodejs/node/pull/4882)
* [[`7003a4e3d8`](https://github.com/nodejs/node/commit/7003a4e3d8)] - **node_contextify**: do not incept debug context (Myles Borins) [#4815](https://github.com/nodejs/node/issues/4815)
* [[`5a77c095a6`](https://github.com/nodejs/node/commit/5a77c095a6)] - **process**: support symbol events (cjihrig) [#4798](https://github.com/nodejs/node/pull/4798)
* [[`85743c0e92`](https://github.com/nodejs/node/commit/85743c0e92)] - **querystring**: check that maxKeys is finite (Myles Borins) [#5066](https://github.com/nodejs/node/pull/5066)
* [[`5a10fe932c`](https://github.com/nodejs/node/commit/5a10fe932c)] - **querystring**: use String.prototype.split's limit (Manuel Valls) [#2288](https://github.com/nodejs/node/pull/2288)
* [[`2844cc03dc`](https://github.com/nodejs/node/commit/2844cc03dc)] - **repl**: remove variable redeclaration (Rich Trott) [#4977](https://github.com/nodejs/node/pull/4977)
* [[`ac6627a0fe`](https://github.com/nodejs/node/commit/ac6627a0fe)] - **src**: avoid compiler warning in node_revert.cc (James M Snell)
* [[`459c5844c8`](https://github.com/nodejs/node/commit/459c5844c8)] - **(SEMVER-MINOR)** **src**: add --security-revert command line flag (James M Snell)
* [[`95615196de`](https://github.com/nodejs/node/commit/95615196de)] - **src**: clean up usage of __proto__ (Jackson Tian) [#5069](https://github.com/nodejs/node/pull/5069)
* [[`e93b024214`](https://github.com/nodejs/node/commit/e93b024214)] - **src**: remove no longer relevant comments (Chris911) [#4843](https://github.com/nodejs/node/pull/4843)
* [[`a2c257a3ef`](https://github.com/nodejs/node/commit/a2c257a3ef)] - **src**: fix negative values in process.hrtime() (Ben Noordhuis) [#4757](https://github.com/nodejs/node/pull/4757)
* [[`b46f3b84d4`](https://github.com/nodejs/node/commit/b46f3b84d4)] - **src,deps**: replace LoadLibrary by LoadLibraryW (Cheng Zhao) [iojs/io.js#226](https://github.com/iojs/io.js/pull/226)
* [[`ee8d4bb075`](https://github.com/nodejs/node/commit/ee8d4bb075)] - **stream**: prevent object map change in TransformState (Evan Lucas) [#5032](https://github.com/nodejs/node/pull/5032)
* [[`c8b6de244e`](https://github.com/nodejs/node/commit/c8b6de244e)] - **stream**: refactor redeclared variables (Rich Trott) [#4816](https://github.com/nodejs/node/pull/4816)
* [[`9dcc45e9c5`](https://github.com/nodejs/node/commit/9dcc45e9c5)] - **test**: enable to work pkcs12 test in FIPS mode (Shigeki Ohtsu) [#5150](https://github.com/nodejs/node/pull/5150)
* [[`e4390664ae`](https://github.com/nodejs/node/commit/e4390664ae)] - **test**: disable gh-5100 test when in FIPS mode (Fedor Indutny) [#5144](https://github.com/nodejs/node/pull/5144)
* [[`cf3aa911ec`](https://github.com/nodejs/node/commit/cf3aa911ec)] - **test**: fix flaky test-dgram-pingpong (Rich Trott) [#5125](https://github.com/nodejs/node/pull/5125)
* [[`63884f57dd`](https://github.com/nodejs/node/commit/63884f57dd)] - **test**: mark flaky tests on Raspberry Pi (Rich Trott) [#5082](https://github.com/nodejs/node/pull/5082)
* [[`09917c99d8`](https://github.com/nodejs/node/commit/09917c99d8)] - **test**: fix `net-socket-timeout-unref` flakiness (Santiago Gimeno) [#4772](https://github.com/nodejs/node/pull/4772)
* [[`83da19aa48`](https://github.com/nodejs/node/commit/83da19aa48)] - **test**: fix redeclared test-event-emitter-* vars (Rich Trott) [#4985](https://github.com/nodejs/node/pull/4985)
* [[`87b27c913d`](https://github.com/nodejs/node/commit/87b27c913d)] - **test**: fix redeclared test-intl var (Rich Trott) [#4988](https://github.com/nodejs/node/pull/4988)
* [[`e98772d68e`](https://github.com/nodejs/node/commit/e98772d68e)] - **test**: remove redeclared var in test-domain (Rich Trott) [#4984](https://github.com/nodejs/node/pull/4984)
* [[`443d0463ca`](https://github.com/nodejs/node/commit/443d0463ca)] - **test**: add common.platformTimeout() to dgram test (Rich Trott) [#4938](https://github.com/nodejs/node/pull/4938)
* [[`90219c3398`](https://github.com/nodejs/node/commit/90219c3398)] - **test**: fix flaky cluster test on Windows 10 (Rich Trott) [#4934](https://github.com/nodejs/node/pull/4934)
* [[`3488fa81b5`](https://github.com/nodejs/node/commit/3488fa81b5)] - **test**: fix variable redeclarations (Rich Trott) [#4992](https://github.com/nodejs/node/pull/4992)
* [[`7dc0905d4d`](https://github.com/nodejs/node/commit/7dc0905d4d)] - **test**: fix redeclared test-util-* vars (Rich Trott) [#4994](https://github.com/nodejs/node/pull/4994)
* [[`53e7d605c9`](https://github.com/nodejs/node/commit/53e7d605c9)] - **test**: fix redeclared vars in sequential tests (Rich Trott) [#4999](https://github.com/nodejs/node/pull/4999)
* [[`a62ace9f7e`](https://github.com/nodejs/node/commit/a62ace9f7e)] - **test**: fix tls-no-rsa-key flakiness (Santiago Gimeno) [#4043](https://github.com/nodejs/node/pull/4043)
* [[`9b8f025816`](https://github.com/nodejs/node/commit/9b8f025816)] - **test**: fix redeclared vars in test-url (Rich Trott) [#4993](https://github.com/nodejs/node/pull/4993)
* [[`51fb8845d5`](https://github.com/nodejs/node/commit/51fb8845d5)] - **test**: fix redeclared test-path vars (Rich Trott) [#4991](https://github.com/nodejs/node/pull/4991)
* [[`b16b360ae8`](https://github.com/nodejs/node/commit/b16b360ae8)] - **test**: fix var redeclarations in test-os (Rich Trott) [#4990](https://github.com/nodejs/node/pull/4990)
* [[`d6199773e8`](https://github.com/nodejs/node/commit/d6199773e8)] - **test**: fix test-net-* variable redeclarations (Rich Trott) [#4989](https://github.com/nodejs/node/pull/4989)
* [[`9dd5b3e01b`](https://github.com/nodejs/node/commit/9dd5b3e01b)] - **test**: fix redeclared test-http-* vars (Rich Trott) [#4987](https://github.com/nodejs/node/pull/4987)
* [[`835bf13c1d`](https://github.com/nodejs/node/commit/835bf13c1d)] - **test**: fix var redeclarations in test-fs-* (Rich Trott) [#4986](https://github.com/nodejs/node/pull/4986)
* [[`71d7a4457d`](https://github.com/nodejs/node/commit/71d7a4457d)] - **test**: fix redeclared vars in test-vm-* (Rich Trott) [#4997](https://github.com/nodejs/node/pull/4997)
* [[`38459402a5`](https://github.com/nodejs/node/commit/38459402a5)] - **test**: fix inconsistent styling in test-url (Brian White) [#5014](https://github.com/nodejs/node/pull/5014)
* [[`4934798c0d`](https://github.com/nodejs/node/commit/4934798c0d)] - **test**: pummel test fixes (Rich Trott) [#4998](https://github.com/nodejs/node/pull/4998)
* [[`3970504298`](https://github.com/nodejs/node/commit/3970504298)] - **test**: remove var redeclarations in test-crypto-* (Rich Trott) [#4981](https://github.com/nodejs/node/pull/4981)
* [[`a2881e2187`](https://github.com/nodejs/node/commit/a2881e2187)] - **test**: remove test-cluster-* var redeclarations (Rich Trott) [#4980](https://github.com/nodejs/node/pull/4980)
* [[`c3d93299c2`](https://github.com/nodejs/node/commit/c3d93299c2)] - **test**: fix test-http-extra-response flakiness (Santiago Gimeno) [#4979](https://github.com/nodejs/node/pull/4979)
* [[`0384a43885`](https://github.com/nodejs/node/commit/0384a43885)] - **test**: Add assertion for TLS peer certificate fingerprint (Alan Cohen) [#4923](https://github.com/nodejs/node/pull/4923)
* [[`48a353fe41`](https://github.com/nodejs/node/commit/48a353fe41)] - **test**: scope redeclared vars in test-child-process* (Rich Trott) [#4944](https://github.com/nodejs/node/pull/4944)
* [[`89d1149467`](https://github.com/nodejs/node/commit/89d1149467)] - **test**: fix test-tls-zero-clear-in flakiness (Santiago Gimeno) [#4888](https://github.com/nodejs/node/pull/4888)
* [[`f7ed47341a`](https://github.com/nodejs/node/commit/f7ed47341a)] - **test**: remove Object.observe from tests (Vladimir Kurchatkin) [#4769](https://github.com/nodejs/node/pull/4769)
* [[`d95e53dc3b`](https://github.com/nodejs/node/commit/d95e53dc3b)] - **test**: refactor switch (Rich Trott) [#4870](https://github.com/nodejs/node/pull/4870)
* [[`7f1e3e929a`](https://github.com/nodejs/node/commit/7f1e3e929a)] - **test**: remove race condition in http flood test (Rich Trott) [#4793](https://github.com/nodejs/node/pull/4793)
* [[`6539c64e67`](https://github.com/nodejs/node/commit/6539c64e67)] - **test**: scope redeclared variable (Rich Trott) [#4854](https://github.com/nodejs/node/pull/4854)
* [[`62fb941557`](https://github.com/nodejs/node/commit/62fb941557)] - **test**: fix irregular whitespace issue (Roman Reiss) [#4864](https://github.com/nodejs/node/pull/4864)
* [[`3b225209f0`](https://github.com/nodejs/node/commit/3b225209f0)] - **test**: fs.link() test runs on same device (Drew Folta) [#4861](https://github.com/nodejs/node/pull/4861)
* [[`1860eae110`](https://github.com/nodejs/node/commit/1860eae110)] - **test**: refactor test-net-settimeout (Rich Trott) [#4799](https://github.com/nodejs/node/pull/4799)
* [[`ae9a8cd053`](https://github.com/nodejs/node/commit/ae9a8cd053)] - **test**: mark test-tick-processor flaky (Rich Trott) [#4809](https://github.com/nodejs/node/pull/4809)
* [[`57cea9e421`](https://github.com/nodejs/node/commit/57cea9e421)] - **test**: remove test-http-exit-delay (Rich Trott) [#4786](https://github.com/nodejs/node/pull/4786)
* [[`2119c76d5a`](https://github.com/nodejs/node/commit/2119c76d5a)] - **test**: refactor test-fs-watch (Rich Trott) [#4776](https://github.com/nodejs/node/pull/4776)
* [[`e487b72459`](https://github.com/nodejs/node/commit/e487b72459)] - **test**: move cluster tests to parallel (Rich Trott) [#4774](https://github.com/nodejs/node/pull/4774)
* [[`8c694a658c`](https://github.com/nodejs/node/commit/8c694a658c)] - **test**: improve test-cluster-disconnect-suicide-race (Rich Trott) [#4739](https://github.com/nodejs/node/pull/4739)
* [[`14f5bb7a99`](https://github.com/nodejs/node/commit/14f5bb7a99)] - **test,buffer**: refactor redeclarations (Rich Trott) [#4893](https://github.com/nodejs/node/pull/4893)
* [[`62479e3406`](https://github.com/nodejs/node/commit/62479e3406)] - **tls**: scope loop vars with let (Rich Trott) [#4853](https://github.com/nodejs/node/pull/4853)
* [[`d6fbd81a7a`](https://github.com/nodejs/node/commit/d6fbd81a7a)] - **tls_wrap**: reach error reporting for UV_EPROTO (Fedor Indutny) [#4885](https://github.com/nodejs/node/pull/4885)
* [[`f75d06bf10`](https://github.com/nodejs/node/commit/f75d06bf10)] - **tools**: lint for empty character classes in regex (Rich Trott) [#5115](https://github.com/nodejs/node/pull/5115)
* [[`53cbd0564f`](https://github.com/nodejs/node/commit/53cbd0564f)] - **tools**: lint for spacing around unary operators (Rich Trott) [#5063](https://github.com/nodejs/node/pull/5063)
* [[`7fa5959c59`](https://github.com/nodejs/node/commit/7fa5959c59)] - **tools**: fix redeclared vars in doc/json.js (Rich Trott) [#5047](https://github.com/nodejs/node/pull/5047)
* [[`e95fd6ae70`](https://github.com/nodejs/node/commit/e95fd6ae70)] - **tools**: apply linting to doc tools (Rich Trott) [#4973](https://github.com/nodejs/node/pull/4973)
* [[`777ed82162`](https://github.com/nodejs/node/commit/777ed82162)] - **tools**: fix detecting constructor for JSON doc (Timothy Gu) [#4966](https://github.com/nodejs/node/pull/4966)
* [[`5d55f59c85`](https://github.com/nodejs/node/commit/5d55f59c85)] - **tools**: add property types in JSON documentation (Timothy Gu) [#4884](https://github.com/nodejs/node/pull/4884)
* [[`fd5c56698e`](https://github.com/nodejs/node/commit/fd5c56698e)] - **tools**: add support for subkeys in release tools (Myles Borins) [#4807](https://github.com/nodejs/node/pull/4807)
* [[`34df6a5c0c`](https://github.com/nodejs/node/commit/34df6a5c0c)] - **tools**: enable assorted ESLint error rules (Roman Reiss) [#4864](https://github.com/nodejs/node/pull/4864)
* [[`386ad7e0b5`](https://github.com/nodejs/node/commit/386ad7e0b5)] - **tools**: fix setting path containing an ampersand (Brian White) [#4804](https://github.com/nodejs/node/pull/4804)
* [[`e415eb27e5`](https://github.com/nodejs/node/commit/e415eb27e5)] - **url**: change scoping of variables with let (Kári Tristan Helgason) [#4867](https://github.com/nodejs/node/pull/4867)

## 2016-02-09, Version 4.3.0 'Argon' (LTS), @jasnell

This is an important security release. All Node.js users should consult the security release summary at nodejs.org for details on patched vulnerabilities.

Note that this release includes a non-backward compatible change to address a security issue. This change increases the version of the LTS v4.x line to v4.3.0. There will be *no further updates* to v4.2.x.

### Notable changes

* **http**: fix defects in HTTP header parsing for requests and responses that can allow request smuggling (CVE-2016-2086) or response splitting (CVE-2016-2216). HTTP header parsing now aligns more closely with the HTTP spec including restricting the acceptable characters.
* **http-parser**: upgrade from 2.5.0 to 2.5.1
* **openssl**: upgrade from 1.0.2e to 1.0.2f. To mitigate against the Logjam attack, TLS clients now reject Diffie-Hellman handshakes with parameters shorter than 1024-bits, up from the previous limit of 768-bits.
* **src**:
  - introduce new `--security-revert={cvenum}` command line flag for selective reversion of specific CVE fixes
  - allow the fix for CVE-2016-2216 to be selectively reverted using `--security-revert=CVE-2016-2216`

### Commits

* [[`d94f864abd`](https://github.com/nodejs/node/commit/d94f864abd)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`136295e202`](https://github.com/nodejs/node/commit/136295e202)] - **deps**: upgrade openssl sources to 1.0.2f (Myles Borins) [#4961](https://github.com/nodejs/node/pull/4961)
* [[`0eae95eae3`](https://github.com/nodejs/node/commit/0eae95eae3)] - **(SEMVER-MINOR)** **deps**: update http-parser to version 2.5.1 (James M Snell)
* [[`cf2b714b02`](https://github.com/nodejs/node/commit/cf2b714b02)] - **(SEMVER-MINOR)** **http**: strictly forbid invalid characters from headers (James M Snell)
* [[`49ae2e0334`](https://github.com/nodejs/node/commit/49ae2e0334)] - **src**: avoid compiler warning in node_revert.cc (James M Snell)
* [[`da3750f981`](https://github.com/nodejs/node/commit/da3750f981)] - **(SEMVER-MAJOR)** **src**: add --security-revert command line flag (James M Snell)

## 2016-02-09, Version 0.12.10 (LTS), @jasnell

This is an important security release. All Node.js users should consult the security release summary at nodejs.org for details on patched vulnerabilities.

### Notable changes

* http: fix defects in HTTP header parsing for requests and responses that can allow request smuggling (CVE-2016-2086) or response splitting (CVE-2016-2216). HTTP header parsing now aligns more closely with the HTTP spec including restricting the acceptable characters.
* http-parser: upgrade from 2.3.0 to 2.3.1
* openssl: upgrade from 1.0.1q to 1.0.1r. To mitigate against the Logjam attack, TLS clients now reject Diffie-Hellman handshakes with parameters shorter than 1024-bits, up from the previous limit of 768-bits.
* src:
  - introduce new `--security-revert={cvenum}` command line flag for selective reversion of specific CVE fixes
  - allow the fix for CVE-2016-2216 to be selectively reverted using `--security-revert=CVE-2016-2216`
* build:
  - xz compressed tar files will be made available from nodejs.org for v0.12 builds from v0.12.10 onward
  - A headers.tar.gz file will be made available from nodejs.org for v0.12 builds from v0.12.10 onward, a future change to node-gyp will be required to make use of these

### Commits

* [4312848bff] - build: enable xz compressed tarballs where possible (Rod Vagg) https://github.com/nodejs/node/pull/4894
* [247626245c] - deps: upgrade openssl sources to 1.0.1r (Shigeki Ohtsu) https://github.com/joyent/node/pull/25368
* [744c9749fc] - deps: update http-parser to version 2.3.1 (James M Snell)
* [d1c56ec7d1] - doc: clarify v0.12.9 notable items (Rod Vagg) https://github.com/nodejs/node/pull/4154
* [e128d9a5b4] - http: strictly forbid invalid characters from headers (James M Snell)
* [bdb9f2cf89] - src: avoiding compiler warnings in node_revert.cc (James M Snell)
* [23bced1fb3] - src: add --security-revert command line flag (James M Snell)
* [f41a3c73e7] - tools: backport tools/install.py for headers (Richard Lau) https://github.com/nodejs/node/pull/4149

## 2016-02-09, Version 0.10.42 (Maintenance), @jasnell

This is an important security release. All Node.js users should consult the security release summary at nodejs.org for details on patched vulnerabilities.

### Notable changes

* http: fix defects in HTTP header parsing for requests and responses that can allow request smuggling (CVE-2016-2086) or response splitting (CVE-2016-2216). HTTP header parsing now aligns more closely with the HTTP spec including restricting the acceptable characters.
* http-parser: upgrade from 1.0 to 1.1
* openssl: upgrade from 1.0.1q to 1.0.1r. To mitigate against the Logjam attack, TLS clients now reject Diffie-Hellman handshakes with parameters shorter than 1024-bits, up from the previous limit of 768-bits.
* src:
  - introduce new `--security-revert={cvenum}` command line flag for selective reversion of specific CVE fixes
  - allow the fix for CVE-2016-2216 to be selectively reverted using `--security-revert=CVE-2016-2216`
* build:
  - xz compressed tar files will be made available from nodejs.org for v0.10 builds from v0.10.42 onward
  - A headers.tar.gz file will be made available from nodejs.org for v0.10 builds from v0.10.42 onward, a future change to node-gyp will be required to make use of these

### Commits

* [fdc332183e] - build: enable xz compressed tarballs where possible (Rod Vagg) https://github.com/nodejs/node/pull/4894
* [2d35b421b5] - deps: upgrade openssl sources to 1.0.1r (Shigeki Ohtsu) https://github.com/joyent/node/pull/25368
* [b31c0f3ea4] - deps: update http-parser to version 1.1 (James M Snell)
* [616ec1d6b0] - doc: clarify v0.10.41 openssl tls security impact (Rod Vagg) https://github.com/nodejs/node/pull/4153
* [ccb3c2377c] - http: strictly forbid invalid characters from headers (James M Snell)
* [f0af0d1f96] - src: avoid compiler warning in node_revert.cc (James M Snell)
* [df80e856c6] - src: add --security-revert command line flag (James M Snell)
* [ff58dcdd74] - tools: backport tools/install.py for headers (Richard Lau) https://github.com/nodejs/node/pull/4149

## 2016-01-21, Version 4.2.6 'Argon' (LTS), @TheAlphaNerd

### Notable changes

* Fix regression in debugger and profiler functionality

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`1408f7abb1`](https://github.com/nodejs/node/commit/1408f7abb1)] - **module,src**: do not wrap modules with -1 lineOffset (cjihrig) [#4298](https://github.com/nodejs/node/pull/4298)
* [[`1f8e1472cc`](https://github.com/nodejs/node/commit/1f8e1472cc)] - **test**: add test for debugging one line files (cjihrig) [#4298](https://github.com/nodejs/node/pull/4298)

## 2016-01-20, Version 5.5.0 (Stable), @evanlucas

### Notable Changes

- **events**: make sure console functions exist (Dave) [#4479](https://github.com/nodejs/node/pull/4479)
- **fs**: add autoClose option to fs.createWriteStream (Saquib) [#3679](https://github.com/nodejs/node/pull/3679)
- **http**: improves expect header handling (Daniel Sellers) [#4501](https://github.com/nodejs/node/pull/4501)
- **node**: allow preload modules with -i (Evan Lucas) [#4696](https://github.com/nodejs/node/pull/4696)
- **v8,src**: expose statistics about heap spaces (`v8.getHeapSpaceStatistics()`) (Ben Ripkens) [#4463](https://github.com/nodejs/node/pull/4463)
* Minor performance improvements:
  - **lib**: Use arrow functions instead of bind where possible (Minwoo Jung) [#3622](https://github.com/nodejs/node/pull/3622).
    - (Mistakenly missing from v5.4.0)
  - **module**: cache stat() results more aggressively (Ben Noordhuis) [#4575](https://github.com/nodejs/node/pull/4575)
  - **querystring**: improve parse() performance (Brian White) [#4675](https://github.com/nodejs/node/pull/4675)

### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* Unicode characters in filesystem paths are not handled consistently across platforms or Node.js APIs. See [#2088](https://github.com/nodejs/node/issues/2088), [#3401](https://github.com/nodejs/node/issues/3401) and [#3519](https://github.com/nodejs/node/issues/3519).

### Commits

* [[`8d0ca10752`](https://github.com/nodejs/node/commit/8d0ca10752)] - **buffer**: make byteLength work with Buffer correctly (Jackson Tian) [#4738](https://github.com/nodejs/node/pull/4738)
* [[`83d2b7707e`](https://github.com/nodejs/node/commit/83d2b7707e)] - **buffer**: remove unnecessary TODO comments (Peter Geiss) [#4719](https://github.com/nodejs/node/pull/4719)
* [[`8182ec094d`](https://github.com/nodejs/node/commit/8182ec094d)] - **build**: add option to select VS version (julien.waechter) [#4645](https://github.com/nodejs/node/pull/4645)
* [[`4383acd9f4`](https://github.com/nodejs/node/commit/4383acd9f4)] - **build**: fix and refactor VTune config in vcbuild.bat (Rod Vagg) [#4192](https://github.com/nodejs/node/pull/4192)
* [[`be0b0b8cb9`](https://github.com/nodejs/node/commit/be0b0b8cb9)] - **build**: minor corrections in VTune configure text (Rod Vagg) [#4192](https://github.com/nodejs/node/pull/4192)
* [[`9571be12f6`](https://github.com/nodejs/node/commit/9571be12f6)] - **cluster**: fix race condition setting suicide prop (Santiago Gimeno) [#4349](https://github.com/nodejs/node/pull/4349)
* [[`ebd9addcd1`](https://github.com/nodejs/node/commit/ebd9addcd1)] - **crypto**: clear error stack in ECDH::Initialize (Fedor Indutny) [#4689](https://github.com/nodejs/node/pull/4689)
* [[`66b9c0d8bd`](https://github.com/nodejs/node/commit/66b9c0d8bd)] - **debugger**: remove variable redeclarations (Rich Trott) [#4633](https://github.com/nodejs/node/pull/4633)
* [[`88b2889679`](https://github.com/nodejs/node/commit/88b2889679)] - **dgram**: prevent disabled optimization of bind() (Brian White) [#4613](https://github.com/nodejs/node/pull/4613)
* [[`d56e3f8b67`](https://github.com/nodejs/node/commit/d56e3f8b67)] - **doc**: restore ICU third-party software licenses (Richard Lau) [#4762](https://github.com/nodejs/node/pull/4762)
* [[`212a44df03`](https://github.com/nodejs/node/commit/212a44df03)] - **doc**: clarify protocol default in http.request() (cjihrig) [#4714](https://github.com/nodejs/node/pull/4714)
* [[`3297036345`](https://github.com/nodejs/node/commit/3297036345)] - **doc**: update branch-diff arguments in release doc (Rod Vagg) [#4691](https://github.com/nodejs/node/pull/4691)
* [[`666c089e68`](https://github.com/nodejs/node/commit/666c089e68)] - **doc**: fix named anchors in addons.markdown and http.markdown (Michael Theriot) [#4708](https://github.com/nodejs/node/pull/4708)
* [[`310530b7ec`](https://github.com/nodejs/node/commit/310530b7ec)] - **doc**: add path property to Write/ReadStream in fs.markdown (Claudio Rodriguez) [#4368](https://github.com/nodejs/node/pull/4368)
* [[`3470574cb6`](https://github.com/nodejs/node/commit/3470574cb6)] - **doc**: clarify explanation of first stream section (Vitor Cortez) [#4234](https://github.com/nodejs/node/pull/4234)
* [[`d91646b9c7`](https://github.com/nodejs/node/commit/d91646b9c7)] - **doc**: rebuild LICENSE using tools/license-builder.sh (Rod Vagg) [#4194](https://github.com/nodejs/node/pull/4194)
* [[`265e2f557b`](https://github.com/nodejs/node/commit/265e2f557b)] - **doc**: fix typo in doc/node.1 (Jérémy Lal) [#4680](https://github.com/nodejs/node/pull/4680)
* [[`4c132fe61e`](https://github.com/nodejs/node/commit/4c132fe61e)] - **doc**: make references clickable (Roman Klauke) [#4654](https://github.com/nodejs/node/pull/4654)
* [[`d139704ff7`](https://github.com/nodejs/node/commit/d139704ff7)] - **doc**: improve child_process.execFile() code example (Ryan Sobol) [#4504](https://github.com/nodejs/node/pull/4504)
* [[`eeb6fdcd0f`](https://github.com/nodejs/node/commit/eeb6fdcd0f)] - **doc**: add docs for more stream options (zoubin) [#4639](https://github.com/nodejs/node/pull/4639)
* [[`b6ab6d2de5`](https://github.com/nodejs/node/commit/b6ab6d2de5)] - **doc**: add branch-diff example to releases.md (Myles Borins) [#4636](https://github.com/nodejs/node/pull/4636)
* [[`287325c5e8`](https://github.com/nodejs/node/commit/287325c5e8)] - **docs**: update gpg key for Myles Borins (Myles Borins) [#4657](https://github.com/nodejs/node/pull/4657)
* [[`65825b79aa`](https://github.com/nodejs/node/commit/65825b79aa)] - **docs**: fix npm command in releases.md (Myles Borins) [#4656](https://github.com/nodejs/node/pull/4656)
* [[`f9a59c1d3b`](https://github.com/nodejs/node/commit/f9a59c1d3b)] - **(SEMVER-MINOR)** **events**: make sure console functions exist (Dave) [#4479](https://github.com/nodejs/node/pull/4479)
* [[`6039a7c1b5`](https://github.com/nodejs/node/commit/6039a7c1b5)] - **(SEMVER-MINOR)** **fs**: add autoClose option to fs.createWriteStream (Saquib) [#3679](https://github.com/nodejs/node/pull/3679)
* [[`ed55169834`](https://github.com/nodejs/node/commit/ed55169834)] - **gitignore**: never ignore debug module (Michaël Zasso) [#2286](https://github.com/nodejs/node/pull/2286)
* [[`d755432fa9`](https://github.com/nodejs/node/commit/d755432fa9)] - **(SEMVER-MINOR)** **http**: improves expect header handling (Daniel Sellers) [#4501](https://github.com/nodejs/node/pull/4501)
* [[`7ce0e04f44`](https://github.com/nodejs/node/commit/7ce0e04f44)] - **lib**: fix style issues after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`ae5bcf9528`](https://github.com/nodejs/node/commit/ae5bcf9528)] - **lib**: use arrow functions instead of bind (Minwoo Jung) [#3622](https://github.com/nodejs/node/pull/3622)
* [[`0ec093cd41`](https://github.com/nodejs/node/commit/0ec093cd41)] - **lib,test**: remove extra semicolons (Michaël Zasso) [#2205](https://github.com/nodejs/node/pull/2205)
* [[`d8f5bd4fe1`](https://github.com/nodejs/node/commit/d8f5bd4fe1)] - **module**: avoid ArgumentsAdaptorTrampoline frame (Ben Noordhuis) [#4575](https://github.com/nodejs/node/pull/4575)
* [[`83f8d98806`](https://github.com/nodejs/node/commit/83f8d98806)] - **module**: cache stat() results more aggressively (Ben Noordhuis) [#4575](https://github.com/nodejs/node/pull/4575)
* [[`ff64a4c395`](https://github.com/nodejs/node/commit/ff64a4c395)] - **(SEMVER-MINOR)** **node**: allow preload modules with -i (Evan Lucas) [#4696](https://github.com/nodejs/node/pull/4696)
* [[`4bc1a47761`](https://github.com/nodejs/node/commit/4bc1a47761)] - **querystring**: improve parse() performance (Brian White) [#4675](https://github.com/nodejs/node/pull/4675)
* [[`ad63d350d4`](https://github.com/nodejs/node/commit/ad63d350d4)] - **readline**: Remove XXX and output debuglog (Kohei TAKATA) [#4690](https://github.com/nodejs/node/pull/4690)
* [[`da550aa063`](https://github.com/nodejs/node/commit/da550aa063)] - **repl**: make sure historyPath is trimmed (Evan Lucas) [#4539](https://github.com/nodejs/node/pull/4539)
* [[`1a6e7d1b52`](https://github.com/nodejs/node/commit/1a6e7d1b52)] - **src**: fix negative values in process.hrtime() (Ben Noordhuis) [#4757](https://github.com/nodejs/node/pull/4757)
* [[`8bad51977a`](https://github.com/nodejs/node/commit/8bad51977a)] - **src**: return UV_EAI_NODATA on empty lookup (cjihrig) [#4715](https://github.com/nodejs/node/pull/4715)
* [[`761cf2bf6a`](https://github.com/nodejs/node/commit/761cf2bf6a)] - **src**: don't check failure with ERR_peek_error() (Ben Noordhuis) [#4731](https://github.com/nodejs/node/pull/4731)
* [[`953f4a3999`](https://github.com/nodejs/node/commit/953f4a3999)] - **stream**: prevent object map change in ReadableState (Evan Lucas) [#4761](https://github.com/nodejs/node/pull/4761)
* [[`e65f1f7954`](https://github.com/nodejs/node/commit/e65f1f7954)] - **test**: fix tls-multi-key race condition (Santiago Gimeno) [#3966](https://github.com/nodejs/node/pull/3966)
* [[`3727ae0d7d`](https://github.com/nodejs/node/commit/3727ae0d7d)] - **test**: use addon.md block headings as test dir names (Rod Vagg) [#4412](https://github.com/nodejs/node/pull/4412)
* [[`47960a07c0`](https://github.com/nodejs/node/commit/47960a07c0)] - **test**: make test-cluster-disconnect-leak reliable (Rich Trott) [#4736](https://github.com/nodejs/node/pull/4736)
* [[`9926b5a25f`](https://github.com/nodejs/node/commit/9926b5a25f)] - **test**: fix issues for space-in-parens ESLint rule (Roman Reiss) [#4753](https://github.com/nodejs/node/pull/4753)
* [[`d1aabd6264`](https://github.com/nodejs/node/commit/d1aabd6264)] - **test**: fix style issues after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`e98bcfa2cb`](https://github.com/nodejs/node/commit/e98bcfa2cb)] - **test**: remove 1 second delay from test (Rich Trott) [#4616](https://github.com/nodejs/node/pull/4616)
* [[`6cfd0b5a32`](https://github.com/nodejs/node/commit/6cfd0b5a32)] - **test**: fix flaky test-net-socket-local-address (cjihrig) [#4650](https://github.com/nodejs/node/pull/4650)
* [[`e22cc6c2eb`](https://github.com/nodejs/node/commit/e22cc6c2eb)] - **test**: fix race in test-net-server-pause-on-connect (Rich Trott) [#4637](https://github.com/nodejs/node/pull/4637)
* [[`9164c00bdb`](https://github.com/nodejs/node/commit/9164c00bdb)] - **test**: move resource intensive tests to sequential (Rich Trott) [#4615](https://github.com/nodejs/node/pull/4615)
* [[`d8ba2c0de4`](https://github.com/nodejs/node/commit/d8ba2c0de4)] - **test**: fix `http-upgrade-client` flakiness (Santiago Gimeno) [#4602](https://github.com/nodejs/node/pull/4602)
* [[`6018fa1f57`](https://github.com/nodejs/node/commit/6018fa1f57)] - **test**: fix `http-upgrade-agent` flakiness (Santiago Gimeno) [#4520](https://github.com/nodejs/node/pull/4520)
* [[`c33f6a87d0`](https://github.com/nodejs/node/commit/c33f6a87d0)] - **tools**: enable space-in-parens ESLint rule (Roman Reiss) [#4753](https://github.com/nodejs/node/pull/4753)
* [[`162e16afdb`](https://github.com/nodejs/node/commit/162e16afdb)] - **tools**: enable no-extra-semi rule in eslint (Michaël Zasso) [#2205](https://github.com/nodejs/node/pull/2205)
* [[`031b87d42d`](https://github.com/nodejs/node/commit/031b87d42d)] - **tools**: add license-builder.sh to construct LICENSE (Rod Vagg) [#4194](https://github.com/nodejs/node/pull/4194)
* [[`ec8e0ae697`](https://github.com/nodejs/node/commit/ec8e0ae697)] - **tools**: fix style issue after eslint update (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`4d5ee7a512`](https://github.com/nodejs/node/commit/4d5ee7a512)] - **tools**: update eslint config (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`2d441493a4`](https://github.com/nodejs/node/commit/2d441493a4)] - **tools**: update eslint to v1.10.3 (Michaël Zasso) [nodejs/io.js#2286](https://github.com/nodejs/io.js/pull/2286)
* [[`aba3cc834e`](https://github.com/nodejs/node/commit/aba3cc834e)] - **tools**: fix license-builder.sh for ICU (Richard Lau) [#4762](https://github.com/nodejs/node/pull/4762)
* [[`5f57005ec9`](https://github.com/nodejs/node/commit/5f57005ec9)] - **(SEMVER-MINOR)** **v8,src**: expose statistics about heap spaces (Ben Ripkens) [#4463](https://github.com/nodejs/node/pull/4463)

## 2016-01-20, Version 4.2.5 'Argon' (LTS), @TheAlphaNerd

Maintenance update.

### Notable changes

* **assert**
  * accommodate ES6 classes that extend Error (Rich Trott) [#4166](https://github.com/nodejs/node/pull/4166)
* **build**
  * add "--partly-static" build options (Super Zheng) [#4152](https://github.com/nodejs/node/pull/4152)
* **deps**
  * backport 066747e from upstream V8 (Ali Ijaz Sheikh) [#4655](https://github.com/nodejs/node/pull/4655)
  * backport 200315c from V8 upstream (Vladimir Kurchatkin) [#4128](https://github.com/nodejs/node/pull/4128)
  * upgrade libuv to 1.8.0 (Saúl Ibarra Corretgé)
* **docs**
  * various updates landed in 70 different commits!
* **repl**
  * attach location info to syntax errors (cjihrig) [#4013](https://github.com/nodejs/node/pull/4013)
  * display error message when loading directory (Prince J Wesley) [#4170](https://github.com/nodejs/node/pull/4170)
* **tests**
  * various updates landed in over 50 commits
* **tools**
  * add tap output to cpplint (Johan Bergström) [#3448](https://github.com/nodejs/node/pull/3448)
* **util**
  * allow lookup of hidden values (cjihrig) [#3988](https://github.com/nodejs/node/pull/3988)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`87181cd74c`](https://github.com/nodejs/node/commit/87181cd74c)] - **assert**: accommodate ES6 classes that extend Error (Rich Trott) [#4166](https://github.com/nodejs/node/pull/4166)
* [[`901172a783`](https://github.com/nodejs/node/commit/901172a783)] - **assert**: typed array deepequal performance fix (Claudio Rodriguez) [#4330](https://github.com/nodejs/node/pull/4330)
* [[`55336810ee`](https://github.com/nodejs/node/commit/55336810ee)] - **async_wrap**: call callback in destructor (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`a8b45e9e96`](https://github.com/nodejs/node/commit/a8b45e9e96)] - **async_wrap**: new instances get uid (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`49f16d77c4`](https://github.com/nodejs/node/commit/49f16d77c4)] - **async_wrap**: allow some hooks to be optional (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`44ee33f945`](https://github.com/nodejs/node/commit/44ee33f945)] - **buffer**: refactor create buffer (Jackson Tian) [#4340](https://github.com/nodejs/node/pull/4340)
* [[`138d004ac0`](https://github.com/nodejs/node/commit/138d004ac0)] - **buffer**: faster case for create Buffer from new Buffer(0) (Jackson Tian) [#4326](https://github.com/nodejs/node/pull/4326)
* [[`c6dc2a1609`](https://github.com/nodejs/node/commit/c6dc2a1609)] - **buffer**: Prevent Buffer constructor deopt (Bryce Baril) [#4158](https://github.com/nodejs/node/pull/4158)
* [[`a320045e68`](https://github.com/nodejs/node/commit/a320045e68)] - **buffer**: default to UTF8 in byteLength() (Tom Gallacher) [#4010](https://github.com/nodejs/node/pull/4010)
* [[`c5f71ac771`](https://github.com/nodejs/node/commit/c5f71ac771)] - **build**: add "--partly-static" build options (Super Zheng) [#4152](https://github.com/nodejs/node/pull/4152)
* [[`e6c25335ea`](https://github.com/nodejs/node/commit/e6c25335ea)] - **build**: omit -gline-tables-only for --enable-asan (Ben Noordhuis) [#3680](https://github.com/nodejs/node/pull/3680)
* [[`80b4ba286c`](https://github.com/nodejs/node/commit/80b4ba286c)] - **build**: Updates for AIX npm support - part 1 (Michael Dawson) [#3114](https://github.com/nodejs/node/pull/3114)
* [[`35e32985ca`](https://github.com/nodejs/node/commit/35e32985ca)] - **child_process**: guard against race condition (Rich Trott) [#4418](https://github.com/nodejs/node/pull/4418)
* [[`48564204f0`](https://github.com/nodejs/node/commit/48564204f0)] - **child_process**: flush consuming streams (Dave) [#4071](https://github.com/nodejs/node/pull/4071)
* [[`481d59a74c`](https://github.com/nodejs/node/commit/481d59a74c)] - **configure**: fix arm vfpv2 (Jörg Krause) [#4203](https://github.com/nodejs/node/pull/4203)
* [[`d19da6638d`](https://github.com/nodejs/node/commit/d19da6638d)] - **crypto**: load PFX chain the same way as regular one (Fedor Indutny) [#4165](https://github.com/nodejs/node/pull/4165)
* [[`b8e75de1f3`](https://github.com/nodejs/node/commit/b8e75de1f3)] - **crypto**: fix native module compilation with FIPS (Stefan Budeanu) [#4023](https://github.com/nodejs/node/pull/4023)
* [[`b7c3fb7f75`](https://github.com/nodejs/node/commit/b7c3fb7f75)] - **crypto**: disable crypto.createCipher in FIPS mode (Stefan Budeanu) [#3754](https://github.com/nodejs/node/pull/3754)
* [[`31b4091a1e`](https://github.com/nodejs/node/commit/31b4091a1e)] - **debugger**: also exit when the repl emits 'exit' (Felix Böhm) [#2369](https://github.com/nodejs/node/pull/2369)
* [[`9baa5618f5`](https://github.com/nodejs/node/commit/9baa5618f5)] - **deps**: backport 066747e from upstream V8 (Ali Ijaz Sheikh) [#4655](https://github.com/nodejs/node/pull/4655)
* [[`c3a9d8a62e`](https://github.com/nodejs/node/commit/c3a9d8a62e)] - **deps**: backport 200315c from V8 upstream (Vladimir Kurchatkin) [#4128](https://github.com/nodejs/node/pull/4128)
* [[`1ebb0c0fdf`](https://github.com/nodejs/node/commit/1ebb0c0fdf)] - **deps**: upgrade libuv to 1.8.0 (Saúl Ibarra Corretgé) [#4276](https://github.com/nodejs/node/pull/4276)
* [[`253fe3e7c8`](https://github.com/nodejs/node/commit/253fe3e7c8)] - **dns**: remove nonexistant exports.ADNAME (Roman Reiss) [#3051](https://github.com/nodejs/node/pull/3051)
* [[`8c2b65ad82`](https://github.com/nodejs/node/commit/8c2b65ad82)] - **doc**: clarify protocol default in http.request() (cjihrig) [#4714](https://github.com/nodejs/node/pull/4714)
* [[`33e72e135f`](https://github.com/nodejs/node/commit/33e72e135f)] - **doc**: update links to use https where possible (jpersson) [#4054](https://github.com/nodejs/node/pull/4054)
* [[`5f4aa79410`](https://github.com/nodejs/node/commit/5f4aa79410)] - **doc**: clarify explanation of first stream section (Vitor Cortez) [#4234](https://github.com/nodejs/node/pull/4234)
* [[`295ca5bfb2`](https://github.com/nodejs/node/commit/295ca5bfb2)] - **doc**: add branch-diff example to releases.md (Myles Borins) [#4636](https://github.com/nodejs/node/pull/4636)
* [[`18f5cd8710`](https://github.com/nodejs/node/commit/18f5cd8710)] - **doc**: update stylesheet to match frontpage (Roman Reiss) [#4621](https://github.com/nodejs/node/pull/4621)
* [[`2f40715f08`](https://github.com/nodejs/node/commit/2f40715f08)] - **doc**: adds usage of readline line-by-line parsing (Robert Jefe Lindstaedt) [#4609](https://github.com/nodejs/node/pull/4609)
* [[`5b45a464ee`](https://github.com/nodejs/node/commit/5b45a464ee)] - **doc**: document http's server.listen return value (Sequoia McDowell) [#4590](https://github.com/nodejs/node/pull/4590)
* [[`bd31740339`](https://github.com/nodejs/node/commit/bd31740339)] - **doc**: label http.IncomingMessage as a Class (Sequoia McDowell) [#4589](https://github.com/nodejs/node/pull/4589)
* [[`bcd2cbbb93`](https://github.com/nodejs/node/commit/bcd2cbbb93)] - **doc**: fix description about the latest-codename (Minwoo Jung) [#4583](https://github.com/nodejs/node/pull/4583)
* [[`0b12bcb35d`](https://github.com/nodejs/node/commit/0b12bcb35d)] - **doc**: add Evan Lucas to Release Team (Evan Lucas) [#4579](https://github.com/nodejs/node/pull/4579)
* [[`e20b1f6f10`](https://github.com/nodejs/node/commit/e20b1f6f10)] - **doc**: add Myles Borins to Release Team (Myles Borins) [#4578](https://github.com/nodejs/node/pull/4578)
* [[`54977e63eb`](https://github.com/nodejs/node/commit/54977e63eb)] - **doc**: add missing backtick for readline (Brian White) [#4549](https://github.com/nodejs/node/pull/4549)
* [[`5d6bed895c`](https://github.com/nodejs/node/commit/5d6bed895c)] - **doc**: bring releases.md up to date (cjihrig) [#4540](https://github.com/nodejs/node/pull/4540)
* [[`0cd2252e85`](https://github.com/nodejs/node/commit/0cd2252e85)] - **doc**: fix numbering in stream.markdown (Richard Sun) [#4538](https://github.com/nodejs/node/pull/4538)
* [[`8574d91f27`](https://github.com/nodejs/node/commit/8574d91f27)] - **doc**: stronger suggestion for userland assert (Wyatt Preul) [#4535](https://github.com/nodejs/node/pull/4535)
* [[`a7bcf8b84d`](https://github.com/nodejs/node/commit/a7bcf8b84d)] - **doc**: close backtick in process.title description (Dave) [#4534](https://github.com/nodejs/node/pull/4534)
* [[`0ceb3148b0`](https://github.com/nodejs/node/commit/0ceb3148b0)] - **doc**: improvements to events.markdown copy (James M Snell) [#4468](https://github.com/nodejs/node/pull/4468)
* [[`bf56d509b9`](https://github.com/nodejs/node/commit/bf56d509b9)] - **doc**: explain ClientRequest#setTimeout time unit (Ben Ripkens) [#4458](https://github.com/nodejs/node/pull/4458)
* [[`d927c51be3`](https://github.com/nodejs/node/commit/d927c51be3)] - **doc**: improvements to errors.markdown copy (James M Snell) [#4454](https://github.com/nodejs/node/pull/4454)
* [[`ceea6df581`](https://github.com/nodejs/node/commit/ceea6df581)] - **doc**: improvements to dns.markdown copy (James M Snell) [#4449](https://github.com/nodejs/node/pull/4449)
* [[`506f2f8ed1`](https://github.com/nodejs/node/commit/506f2f8ed1)] - **doc**: add anchors for _transform _flush _writev in stream.markdown (iamchenxin) [#4448](https://github.com/nodejs/node/pull/4448)
* [[`74bcad0b78`](https://github.com/nodejs/node/commit/74bcad0b78)] - **doc**: improvements to dgram.markdown copy (James M Snell) [#4437](https://github.com/nodejs/node/pull/4437)
* [[`e244d560c9`](https://github.com/nodejs/node/commit/e244d560c9)] - **doc**: improvements to debugger.markdown copy (James M Snell) [#4436](https://github.com/nodejs/node/pull/4436)
* [[`df7e1281a5`](https://github.com/nodejs/node/commit/df7e1281a5)] - **doc**: improvements to console.markdown copy (James M Snell) [#4428](https://github.com/nodejs/node/pull/4428)
* [[`abb17cc6c1`](https://github.com/nodejs/node/commit/abb17cc6c1)] - **doc**: fix spelling error in lib/url.js comment (Nik Nyby) [#4390](https://github.com/nodejs/node/pull/4390)
* [[`823269db2d`](https://github.com/nodejs/node/commit/823269db2d)] - **doc**: improve assert.markdown copy (James M Snell) [#4360](https://github.com/nodejs/node/pull/4360)
* [[`2b1804f6cb`](https://github.com/nodejs/node/commit/2b1804f6cb)] - **doc**: copyedit releases.md (Rich Trott) [#4384](https://github.com/nodejs/node/pull/4384)
* [[`2b142fd876`](https://github.com/nodejs/node/commit/2b142fd876)] - **doc**: catch the WORKING_GROUPS.md bootstrap docs up to date (James M Snell) [#4367](https://github.com/nodejs/node/pull/4367)
* [[`ed87873de3`](https://github.com/nodejs/node/commit/ed87873de3)] - **doc**: fix link in addons.markdown (Nicholas Young) [#4331](https://github.com/nodejs/node/pull/4331)
* [[`fe693b7a4f`](https://github.com/nodejs/node/commit/fe693b7a4f)] - **doc**: Typo in buffer.markdown referencing buf.write() (chrisjohn404) [#4324](https://github.com/nodejs/node/pull/4324)
* [[`764df2166e`](https://github.com/nodejs/node/commit/764df2166e)] - **doc**: document the cache parameter for fs.realpathSync (Jackson Tian) [#4285](https://github.com/nodejs/node/pull/4285)
* [[`61f91b2f29`](https://github.com/nodejs/node/commit/61f91b2f29)] - **doc**: fix, modernize examples in docs (James M Snell) [#4282](https://github.com/nodejs/node/pull/4282)
* [[`d87ad302ce`](https://github.com/nodejs/node/commit/d87ad302ce)] - **doc**: clarify error events in HTTP module documentation (Lenny Markus) [#4275](https://github.com/nodejs/node/pull/4275)
* [[`7983577e41`](https://github.com/nodejs/node/commit/7983577e41)] - **doc**: fix improper http.get sample code (Hideki Yamamura) [#4263](https://github.com/nodejs/node/pull/4263)
* [[`6c30d087e5`](https://github.com/nodejs/node/commit/6c30d087e5)] - **doc**: Fixing broken links to the v8 wiki (Tom Gallacher) [#4241](https://github.com/nodejs/node/pull/4241)
* [[`cf214e56e4`](https://github.com/nodejs/node/commit/cf214e56e4)] - **doc**: move description of 'equals' method to right place (janriemer) [#4227](https://github.com/nodejs/node/pull/4227)
* [[`fb8e8dbb92`](https://github.com/nodejs/node/commit/fb8e8dbb92)] - **doc**: copyedit console doc (Rich Trott) [#4225](https://github.com/nodejs/node/pull/4225)
* [[`4ccf04c229`](https://github.com/nodejs/node/commit/4ccf04c229)] - **doc**: add mcollina to collaborators (Matteo Collina) [#4220](https://github.com/nodejs/node/pull/4220)
* [[`59654c21d4`](https://github.com/nodejs/node/commit/59654c21d4)] - **doc**: add rmg to collaborators (Ryan Graham) [#4219](https://github.com/nodejs/node/pull/4219)
* [[`bfe1a6bd2b`](https://github.com/nodejs/node/commit/bfe1a6bd2b)] - **doc**: add calvinmetcalf to collaborators (Calvin Metcalf) [#4218](https://github.com/nodejs/node/pull/4218)
* [[`5140c404ae`](https://github.com/nodejs/node/commit/5140c404ae)] - **doc**: harmonize description of `ca` argument (Ben Noordhuis) [#4213](https://github.com/nodejs/node/pull/4213)
* [[`2e642051cf`](https://github.com/nodejs/node/commit/2e642051cf)] - **doc**: copyedit child_process doc (Rich Trott) [#4188](https://github.com/nodejs/node/pull/4188)
* [[`7920f8dbde`](https://github.com/nodejs/node/commit/7920f8dbde)] - **doc**: copyedit buffer doc (Rich Trott) [#4187](https://github.com/nodejs/node/pull/4187)
* [[`c35a409cbe`](https://github.com/nodejs/node/commit/c35a409cbe)] - **doc**: clarify assert.fail doc (Rich Trott) [#4186](https://github.com/nodejs/node/pull/4186)
* [[`6235fdf72e`](https://github.com/nodejs/node/commit/6235fdf72e)] - **doc**: copyedit addons doc (Rich Trott) [#4185](https://github.com/nodejs/node/pull/4185)
* [[`990e7ff93e`](https://github.com/nodejs/node/commit/990e7ff93e)] - **doc**: update AUTHORS list (Rod Vagg) [#4183](https://github.com/nodejs/node/pull/4183)
* [[`8d676ef55e`](https://github.com/nodejs/node/commit/8d676ef55e)] - **doc**: change references from node to Node.js (Roman Klauke) [#4177](https://github.com/nodejs/node/pull/4177)
* [[`1c34b139a2`](https://github.com/nodejs/node/commit/1c34b139a2)] - **doc**: add brief Node.js overview to README (wurde) [#4174](https://github.com/nodejs/node/pull/4174)
* [[`27b9b72ab0`](https://github.com/nodejs/node/commit/27b9b72ab0)] - **doc**: add iarna to collaborators (Rebecca Turner) [#4144](https://github.com/nodejs/node/pull/4144)
* [[`683d8dd564`](https://github.com/nodejs/node/commit/683d8dd564)] - **doc**: add JungMinu to collaborators (Minwoo Jung) [#4143](https://github.com/nodejs/node/pull/4143)
* [[`17b06dfa94`](https://github.com/nodejs/node/commit/17b06dfa94)] - **doc**: add zkat to collaborators (Kat Marchán) [#4142](https://github.com/nodejs/node/pull/4142)
* [[`39364c4c72`](https://github.com/nodejs/node/commit/39364c4c72)] - **doc**: improve child_process.markdown wording (yorkie) [#4138](https://github.com/nodejs/node/pull/4138)
* [[`abe452835f`](https://github.com/nodejs/node/commit/abe452835f)] - **doc**: url.format - true slash postfix behaviour (fansworld-claudio) [#4119](https://github.com/nodejs/node/pull/4119)
* [[`6dd375cfe2`](https://github.com/nodejs/node/commit/6dd375cfe2)] - **doc**: document backlog for server.listen() variants (Jan Schär) [#4025](https://github.com/nodejs/node/pull/4025)
* [[`b71a3b363a`](https://github.com/nodejs/node/commit/b71a3b363a)] - **doc**: fixup socket.remoteAddress (Arthur Gautier) [#4198](https://github.com/nodejs/node/pull/4198)
* [[`e2fe214857`](https://github.com/nodejs/node/commit/e2fe214857)] - **doc**: add links and backticks around names (jpersson) [#4054](https://github.com/nodejs/node/pull/4054)
* [[`bb158f8aed`](https://github.com/nodejs/node/commit/bb158f8aed)] - **doc**: s/node.js/Node.js in readme (Rod Vagg) [#3998](https://github.com/nodejs/node/pull/3998)
* [[`f55491ad47`](https://github.com/nodejs/node/commit/f55491ad47)] - **doc**: move fs.existsSync() deprecation message (Martin Forsberg) [#3942](https://github.com/nodejs/node/pull/3942)
* [[`8c5b847f5b`](https://github.com/nodejs/node/commit/8c5b847f5b)] - **doc**: Describe FIPSDIR environment variable (Stefan Budeanu) [#3752](https://github.com/nodejs/node/pull/3752)
* [[`70c95ea0e5`](https://github.com/nodejs/node/commit/70c95ea0e5)] - **doc**: add warning about Windows process groups (Roman Klauke) [#3681](https://github.com/nodejs/node/pull/3681)
* [[`46c59b7256`](https://github.com/nodejs/node/commit/46c59b7256)] - **doc**: add CTC meeting minutes 2015-10-28 (Rod Vagg) [#3661](https://github.com/nodejs/node/pull/3661)
* [[`7ffd299a1d`](https://github.com/nodejs/node/commit/7ffd299a1d)] - **doc**: add final full stop in CONTRIBUTING.md (Emily Aviva Kapor-Mater) [#3576](https://github.com/nodejs/node/pull/3576)
* [[`1f78bff7ce`](https://github.com/nodejs/node/commit/1f78bff7ce)] - **doc**: add TSC meeting minutes 2015-10-21 (Rod Vagg) [#3480](https://github.com/nodejs/node/pull/3480)
* [[`2e623ff024`](https://github.com/nodejs/node/commit/2e623ff024)] - **doc**: add TSC meeting minutes 2015-10-14 (Rod Vagg) [#3463](https://github.com/nodejs/node/pull/3463)
* [[`b9c69964bb`](https://github.com/nodejs/node/commit/b9c69964bb)] - **doc**: add TSC meeting minutes 2015-10-07 (Rod Vagg) [#3364](https://github.com/nodejs/node/pull/3364)
* [[`f31d23c724`](https://github.com/nodejs/node/commit/f31d23c724)] - **doc**: add TSC meeting minutes 2015-09-30 (Rod Vagg) [#3235](https://github.com/nodejs/node/pull/3235)
* [[`ae8e3af178`](https://github.com/nodejs/node/commit/ae8e3af178)] - **doc**: update irc channels: #node.js and #node-dev (Nelson Pecora) [#2743](https://github.com/nodejs/node/pull/2743)
* [[`830caeb1bd`](https://github.com/nodejs/node/commit/830caeb1bd)] - **doc, test**: symbols as event names (Bryan English) [#4151](https://github.com/nodejs/node/pull/4151)
* [[`82cbfcdcbe`](https://github.com/nodejs/node/commit/82cbfcdcbe)] - **docs**: update gpg key for Myles Borins (Myles Borins) [#4657](https://github.com/nodejs/node/pull/4657)
* [[`50b72aa5a3`](https://github.com/nodejs/node/commit/50b72aa5a3)] - **docs**: fix npm command in releases.md (Myles Borins) [#4656](https://github.com/nodejs/node/pull/4656)
* [[`5bf56882e1`](https://github.com/nodejs/node/commit/5bf56882e1)] - **fs,doc**: use `target` instead of `destination` (yorkie) [#3912](https://github.com/nodejs/node/pull/3912)
* [[`41fcda840c`](https://github.com/nodejs/node/commit/41fcda840c)] - **http**: use `self.keepAlive` instead of `self.options.keepAlive` (Damian Schenkelman) [#4407](https://github.com/nodejs/node/pull/4407)
* [[`3ff237333d`](https://github.com/nodejs/node/commit/3ff237333d)] - **http**: Remove an unnecessary assignment (Bo Borgerson) [#4323](https://github.com/nodejs/node/pull/4323)
* [[`39dc054572`](https://github.com/nodejs/node/commit/39dc054572)] - **http**: remove excess calls to removeSocket (Dave) [#4172](https://github.com/nodejs/node/pull/4172)
* [[`751fbd84dd`](https://github.com/nodejs/node/commit/751fbd84dd)] - **https**: use `servername` in agent key (Fedor Indutny) [#4389](https://github.com/nodejs/node/pull/4389)
* [[`7a1a0a0055`](https://github.com/nodejs/node/commit/7a1a0a0055)] - **lib**: remove unused modules (Rich Trott) [#4683](https://github.com/nodejs/node/pull/4683)
* [[`3d81ea99bb`](https://github.com/nodejs/node/commit/3d81ea99bb)] - **lib,test**: update let to const where applicable (Sakthipriyan Vairamani) [#3152](https://github.com/nodejs/node/pull/3152)
* [[`8a9869eeab`](https://github.com/nodejs/node/commit/8a9869eeab)] - **module**: fix column offsets in errors (Tristian Flanagan) [#2867](https://github.com/nodejs/node/pull/2867)
* [[`0ae90ecd3d`](https://github.com/nodejs/node/commit/0ae90ecd3d)] - **module,repl**: remove repl require() hack (Ben Noordhuis) [#4026](https://github.com/nodejs/node/pull/4026)
* [[`a7367fdc1e`](https://github.com/nodejs/node/commit/a7367fdc1e)] - **net**: small code cleanup (Jan Schär) [#3943](https://github.com/nodejs/node/pull/3943)
* [[`03e9495cc2`](https://github.com/nodejs/node/commit/03e9495cc2)] - **node**: remove unused variables in AppendExceptionLine (Yazhong Liu) [#4264](https://github.com/nodejs/node/pull/4264)
* [[`06113b8711`](https://github.com/nodejs/node/commit/06113b8711)] - **node**: s/doNTCallbackX/nextTickCallbackWithXArgs/ (Rod Vagg) [#4167](https://github.com/nodejs/node/pull/4167)
* [[`8ce6843fe4`](https://github.com/nodejs/node/commit/8ce6843fe4)] - **os**: fix crash in GetInterfaceAddresses (Martin Bark) [#4272](https://github.com/nodejs/node/pull/4272)
* [[`53dcbb6aa4`](https://github.com/nodejs/node/commit/53dcbb6aa4)] - **repl**: remove unused function (Rich Trott)
* [[`db0e906fc1`](https://github.com/nodejs/node/commit/db0e906fc1)] - **repl**: Fixed node repl history edge case. (Mudit Ameta) [#4108](https://github.com/nodejs/node/pull/4108)
* [[`9855fab05f`](https://github.com/nodejs/node/commit/9855fab05f)] - **repl**: use String#repeat instead of Array#join (Evan Lucas) [#3900](https://github.com/nodejs/node/pull/3900)
* [[`41882e4077`](https://github.com/nodejs/node/commit/41882e4077)] - **repl**: fix require('3rdparty') regression (Ben Noordhuis) [#4215](https://github.com/nodejs/node/pull/4215)
* [[`93afc39d4a`](https://github.com/nodejs/node/commit/93afc39d4a)] - **repl**: attach location info to syntax errors (cjihrig) [#4013](https://github.com/nodejs/node/pull/4013)
* [[`d4806675a6`](https://github.com/nodejs/node/commit/d4806675a6)] - **repl**: display error message when loading directory (Prince J Wesley) [#4170](https://github.com/nodejs/node/pull/4170)
* [[`3080bdc7d7`](https://github.com/nodejs/node/commit/3080bdc7d7)] - **src**: define Is* util functions with macros (cjihrig) [#4118](https://github.com/nodejs/node/pull/4118)
* [[`2b8a32a13b`](https://github.com/nodejs/node/commit/2b8a32a13b)] - **src**: refactor vcbuild configure args creation (Rod Vagg) [#3399](https://github.com/nodejs/node/pull/3399)
* [[`d47f6ba768`](https://github.com/nodejs/node/commit/d47f6ba768)] - **src**: fix deprecation message for ErrnoException (Martin von Gagern) [#4269](https://github.com/nodejs/node/pull/4269)
* [[`5ba08fbf76`](https://github.com/nodejs/node/commit/5ba08fbf76)] - **src**: fix line numbers on core errors (cjihrig) [#4254](https://github.com/nodejs/node/pull/4254)
* [[`70974e9362`](https://github.com/nodejs/node/commit/70974e9362)] - **src**: use GetCurrentProcessId() for process.pid (Ben Noordhuis) [#4163](https://github.com/nodejs/node/pull/4163)
* [[`c96eca164f`](https://github.com/nodejs/node/commit/c96eca164f)] - **src**: don't print garbage errors (cjihrig) [#4112](https://github.com/nodejs/node/pull/4112)
* [[`f61412c753`](https://github.com/nodejs/node/commit/f61412c753)] - **test**: mark test-debug-no-context is flaky (Rich Trott) [#4421](https://github.com/nodejs/node/pull/4421)
* [[`46d8c93ed2`](https://github.com/nodejs/node/commit/46d8c93ed2)] - **test**: don't use cwd for relative path (Johan Bergström) [#4477](https://github.com/nodejs/node/pull/4477)
* [[`b6124ea39c`](https://github.com/nodejs/node/commit/b6124ea39c)] - **test**: write to tmp dir rather than fixture dir (Rich Trott) [#4489](https://github.com/nodejs/node/pull/4489)
* [[`350fa664bb`](https://github.com/nodejs/node/commit/350fa664bb)] - **test**: don't assume a certain folder structure (Johan Bergström) [#3325](https://github.com/nodejs/node/pull/3325)
* [[`6b2ef0efac`](https://github.com/nodejs/node/commit/6b2ef0efac)] - **test**: make temp path customizable (Johan Bergström) [#3325](https://github.com/nodejs/node/pull/3325)
* [[`f1837703a9`](https://github.com/nodejs/node/commit/f1837703a9)] - **test**: remove unused vars from parallel tests (Rich Trott) [#4511](https://github.com/nodejs/node/pull/4511)
* [[`b4964b099a`](https://github.com/nodejs/node/commit/b4964b099a)] - **test**: remove unused variables form http tests (Rich Trott) [#4422](https://github.com/nodejs/node/pull/4422)
* [[`0d5a508dfb`](https://github.com/nodejs/node/commit/0d5a508dfb)] - **test**: extend timeout in Debug mode (Rich Trott) [#4431](https://github.com/nodejs/node/pull/4431)
* [[`6e4598d5da`](https://github.com/nodejs/node/commit/6e4598d5da)] - **test**: remove unused variables from TLS tests (Rich Trott) [#4424](https://github.com/nodejs/node/pull/4424)
* [[`7b1aa045a0`](https://github.com/nodejs/node/commit/7b1aa045a0)] - **test**: remove unused variables from HTTPS tests (Rich Trott) [#4426](https://github.com/nodejs/node/pull/4426)
* [[`da9e5c1b01`](https://github.com/nodejs/node/commit/da9e5c1b01)] - **test**: remove unused variables from net tests (Rich Trott) [#4430](https://github.com/nodejs/node/pull/4430)
* [[`13241bd24b`](https://github.com/nodejs/node/commit/13241bd24b)] - **test**: remove unused vars in ChildProcess tests (Rich Trott) [#4425](https://github.com/nodejs/node/pull/4425)
* [[`2f4538ddda`](https://github.com/nodejs/node/commit/2f4538ddda)] - **test**: remove unused vars (Rich Trott) [#4536](https://github.com/nodejs/node/pull/4536)
* [[`dffe83ccd6`](https://github.com/nodejs/node/commit/dffe83ccd6)] - **test**: remove unused modules (Rich Trott) [#4684](https://github.com/nodejs/node/pull/4684)
* [[`c4eeb88ba1`](https://github.com/nodejs/node/commit/c4eeb88ba1)] - **test**: fix flaky cluster-disconnect-race (Brian White) [#4457](https://github.com/nodejs/node/pull/4457)
* [[`7caf87bf6c`](https://github.com/nodejs/node/commit/7caf87bf6c)] - **test**: fix flaky test-http-agent-keepalive (Rich Trott) [#4524](https://github.com/nodejs/node/pull/4524)
* [[`25c41d084d`](https://github.com/nodejs/node/commit/25c41d084d)] - **test**: remove flaky designations for tests (Rich Trott) [#4519](https://github.com/nodejs/node/pull/4519)
* [[`b8f097ece2`](https://github.com/nodejs/node/commit/b8f097ece2)] - **test**: fix flaky streams test (Rich Trott) [#4516](https://github.com/nodejs/node/pull/4516)
* [[`c24fa1437c`](https://github.com/nodejs/node/commit/c24fa1437c)] - **test**: inherit JOBS from environment (Johan Bergström) [#4495](https://github.com/nodejs/node/pull/4495)
* [[`7dc90e9e7f`](https://github.com/nodejs/node/commit/7dc90e9e7f)] - **test**: remove time check (Rich Trott) [#4494](https://github.com/nodejs/node/pull/4494)
* [[`7ca3c6c388`](https://github.com/nodejs/node/commit/7ca3c6c388)] - **test**: refactor test-fs-empty-readStream (Rich Trott) [#4490](https://github.com/nodejs/node/pull/4490)
* [[`610727dea7`](https://github.com/nodejs/node/commit/610727dea7)] - **test**: clarify role of domains in test (Rich Trott) [#4474](https://github.com/nodejs/node/pull/4474)
* [[`1ae0e355b9`](https://github.com/nodejs/node/commit/1ae0e355b9)] - **test**: improve assert message (Rich Trott) [#4461](https://github.com/nodejs/node/pull/4461)
* [[`e70c88df56`](https://github.com/nodejs/node/commit/e70c88df56)] - **test**: remove unused assert module imports (Rich Trott) [#4438](https://github.com/nodejs/node/pull/4438)
* [[`c77fc71f9b`](https://github.com/nodejs/node/commit/c77fc71f9b)] - **test**: remove unused var from test-assert.js (Rich Trott) [#4405](https://github.com/nodejs/node/pull/4405)
* [[`f613b3033f`](https://github.com/nodejs/node/commit/f613b3033f)] - **test**: add test-domain-exit-dispose-again back (Julien Gilli) [#4256](https://github.com/nodejs/node/pull/4256)
* [[`f5bfacd858`](https://github.com/nodejs/node/commit/f5bfacd858)] - **test**: remove unused `util` imports (Rich Trott) [#4562](https://github.com/nodejs/node/pull/4562)
* [[`d795301025`](https://github.com/nodejs/node/commit/d795301025)] - **test**: remove unnecessary assignments (Rich Trott) [#4563](https://github.com/nodejs/node/pull/4563)
* [[`acc3d66934`](https://github.com/nodejs/node/commit/acc3d66934)] - **test**: move ArrayStream to common (cjihrig) [#4027](https://github.com/nodejs/node/pull/4027)
* [[`6c0021361c`](https://github.com/nodejs/node/commit/6c0021361c)] - **test**: refactor test-net-connect-options-ipv6 (Rich Trott) [#4395](https://github.com/nodejs/node/pull/4395)
* [[`29804e00ad`](https://github.com/nodejs/node/commit/29804e00ad)] - **test**: use platformTimeout() in more places (Brian White) [#4387](https://github.com/nodejs/node/pull/4387)
* [[`761af37d0e`](https://github.com/nodejs/node/commit/761af37d0e)] - **test**: fix race condition in test-http-client-onerror (Devin Nakamura) [#4346](https://github.com/nodejs/node/pull/4346)
* [[`980852165f`](https://github.com/nodejs/node/commit/980852165f)] - **test**: fix flaky test-net-error-twice (Brian White) [#4342](https://github.com/nodejs/node/pull/4342)
* [[`1bc44e79d3`](https://github.com/nodejs/node/commit/1bc44e79d3)] - **test**: try other ipv6 localhost alternatives (Brian White) [#4325](https://github.com/nodejs/node/pull/4325)
* [[`44dbe15640`](https://github.com/nodejs/node/commit/44dbe15640)] - **test**: fix debug-port-cluster flakiness (Ben Noordhuis) [#4310](https://github.com/nodejs/node/pull/4310)
* [[`73e781172b`](https://github.com/nodejs/node/commit/73e781172b)] - **test**: add test for tls.parseCertString (Evan Lucas) [#4283](https://github.com/nodejs/node/pull/4283)
* [[`15c295a21b`](https://github.com/nodejs/node/commit/15c295a21b)] - **test**: use regular timeout times for ARMv8 (Jeremiah Senkpiel) [#4248](https://github.com/nodejs/node/pull/4248)
* [[`fd250b8fab`](https://github.com/nodejs/node/commit/fd250b8fab)] - **test**: parallelize test-repl-persistent-history (Jeremiah Senkpiel) [#4247](https://github.com/nodejs/node/pull/4247)
* [[`9a0f156e5a`](https://github.com/nodejs/node/commit/9a0f156e5a)] - **test**: fix domain-top-level-error-handler-throw (Santiago Gimeno) [#4364](https://github.com/nodejs/node/pull/4364)
* [[`6bc1b1c259`](https://github.com/nodejs/node/commit/6bc1b1c259)] - **test**: don't assume openssl s_client supports -ssl3 (Ben Noordhuis) [#4204](https://github.com/nodejs/node/pull/4204)
* [[`d00b9fc66f`](https://github.com/nodejs/node/commit/d00b9fc66f)] - **test**: fix tls-inception flakiness (Santiago Gimeno) [#4195](https://github.com/nodejs/node/pull/4195)
* [[`c41b280a2b`](https://github.com/nodejs/node/commit/c41b280a2b)] - **test**: fix tls-inception (Santiago Gimeno) [#4195](https://github.com/nodejs/node/pull/4195)
* [[`6f4ab1d1ab`](https://github.com/nodejs/node/commit/6f4ab1d1ab)] - **test**: mark test-cluster-shared-leak flaky (Rich Trott) [#4162](https://github.com/nodejs/node/pull/4162)
* [[`90498e2a68`](https://github.com/nodejs/node/commit/90498e2a68)] - **test**: skip long path tests on non-Windows (Rafał Pocztarski) [#4116](https://github.com/nodejs/node/pull/4116)
* [[`c9100d78f3`](https://github.com/nodejs/node/commit/c9100d78f3)] - **test**: fix flaky test-net-socket-local-address (Rich Trott) [#4109](https://github.com/nodejs/node/pull/4109)
* [[`ac939d51d9`](https://github.com/nodejs/node/commit/ac939d51d9)] - **test**: improve cluster-disconnect-handles test (Brian White) [#4084](https://github.com/nodejs/node/pull/4084)
* [[`22ba1b4115`](https://github.com/nodejs/node/commit/22ba1b4115)] - **test**: eliminate multicast test FreeBSD flakiness (Rich Trott) [#4042](https://github.com/nodejs/node/pull/4042)
* [[`2ee7853bb7`](https://github.com/nodejs/node/commit/2ee7853bb7)] - **test**: fix http-many-ended-pipelines flakiness (Santiago Gimeno) [#4041](https://github.com/nodejs/node/pull/4041)
* [[`a77dcfec06`](https://github.com/nodejs/node/commit/a77dcfec06)] - **test**: use platform-based timeout for reliability (Rich Trott) [#4015](https://github.com/nodejs/node/pull/4015)
* [[`3f0ff879cf`](https://github.com/nodejs/node/commit/3f0ff879cf)] - **test**: fix time resolution constraint (Gireesh Punathil) [#3981](https://github.com/nodejs/node/pull/3981)
* [[`22b88e1c48`](https://github.com/nodejs/node/commit/22b88e1c48)] - **test**: add TAP diagnostic message for retried tests (Rich Trott) [#3960](https://github.com/nodejs/node/pull/3960)
* [[`22d2887b1c`](https://github.com/nodejs/node/commit/22d2887b1c)] - **test**: add OS X to module loading error test (Evan Lucas) [#3901](https://github.com/nodejs/node/pull/3901)
* [[`e2141cb75e`](https://github.com/nodejs/node/commit/e2141cb75e)] - **test**: skip instead of fail when mem constrained (Michael Cornacchia) [#3697](https://github.com/nodejs/node/pull/3697)
* [[`166523d0ed`](https://github.com/nodejs/node/commit/166523d0ed)] - **test**: fix race condition in unrefd interval test (Michael Cornacchia) [#3550](https://github.com/nodejs/node/pull/3550)
* [[`86b47e8dc0`](https://github.com/nodejs/node/commit/86b47e8dc0)] - **timers**: optimize callback call: bind -> arrow (Andrei Sedoi) [#4038](https://github.com/nodejs/node/pull/4038)
* [[`4d37472ea7`](https://github.com/nodejs/node/commit/4d37472ea7)] - **tls_wrap**: clear errors on return (Fedor Indutny) [#4709](https://github.com/nodejs/node/pull/4709)
* [[`5b695d0343`](https://github.com/nodejs/node/commit/5b695d0343)] - **tls_wrap**: inherit from the `AsyncWrap` first (Fedor Indutny) [#4268](https://github.com/nodejs/node/pull/4268)
* [[`0efc35e6d8`](https://github.com/nodejs/node/commit/0efc35e6d8)] - **tls_wrap**: slice buffer properly in `ClearOut` (Fedor Indutny) [#4184](https://github.com/nodejs/node/pull/4184)
* [[`628cb8657c`](https://github.com/nodejs/node/commit/628cb8657c)] - **tools**: add .editorconfig (ronkorving) [#2993](https://github.com/nodejs/node/pull/2993)
* [[`69fef19624`](https://github.com/nodejs/node/commit/69fef19624)] - **tools**: implement no-unused-vars for eslint (Rich Trott) [#4536](https://github.com/nodejs/node/pull/4536)
* [[`3ee16706f2`](https://github.com/nodejs/node/commit/3ee16706f2)] - **tools**: enforce `throw new Error()` with lint rule (Rich Trott) [#3714](https://github.com/nodejs/node/pull/3714)
* [[`32801de4ef`](https://github.com/nodejs/node/commit/32801de4ef)] - **tools**: Use `throw new Error()` consistently (Rich Trott) [#3714](https://github.com/nodejs/node/pull/3714)
* [[`f413fae0cd`](https://github.com/nodejs/node/commit/f413fae0cd)] - **tools**: add tap output to cpplint (Johan Bergström) [#3448](https://github.com/nodejs/node/pull/3448)
* [[`efa30dd2f0`](https://github.com/nodejs/node/commit/efa30dd2f0)] - **tools**: enable prefer-const eslint rule (Sakthipriyan Vairamani) [#3152](https://github.com/nodejs/node/pull/3152)
* [[`dd0c925896`](https://github.com/nodejs/node/commit/dd0c925896)] - **udp**: remove a needless instanceof Buffer check (ronkorving) [#4301](https://github.com/nodejs/node/pull/4301)
* [[`f4414102ed`](https://github.com/nodejs/node/commit/f4414102ed)] - **util**: faster arrayToHash (Jackson Tian)
* [[`b421119984`](https://github.com/nodejs/node/commit/b421119984)] - **util**: determine object types in C++ (cjihrig) [#4100](https://github.com/nodejs/node/pull/4100)
* [[`6a7c9d9293`](https://github.com/nodejs/node/commit/6a7c9d9293)] - **util**: move .decorateErrorStack to internal/util (Ben Noordhuis) [#4026](https://github.com/nodejs/node/pull/4026)
* [[`422a865d46`](https://github.com/nodejs/node/commit/422a865d46)] - **util**: add decorateErrorStack() (cjihrig) [#4013](https://github.com/nodejs/node/pull/4013)
* [[`2d5380ea25`](https://github.com/nodejs/node/commit/2d5380ea25)] - **util**: fix constructor/instanceof checks (Brian White) [#3385](https://github.com/nodejs/node/pull/3385)
* [[`1bf84b9d41`](https://github.com/nodejs/node/commit/1bf84b9d41)] - **util,src**: allow lookup of hidden values (cjihrig) [#3988](https://github.com/nodejs/node/pull/3988)


## 2016-01-12, Version 5.4.1 (Stable), @TheAlphaNerd

### Notable Changes

* Minor performance improvements:
  - **module**: move unnecessary work for early return (Andres Suarez) [#3579](https://github.com/nodejs/node/pull/3579)
* Various bug fixes
* Various doc fixes
* Various test improvements

### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* Unicode characters in filesystem paths are not handled consistently across platforms or Node.js APIs. See [#2088](https://github.com/nodejs/node/issues/2088), [#3401](https://github.com/nodejs/node/issues/3401) and [#3519](https://github.com/nodejs/node/issues/3519).

### commits

* [[`ff539c5bb5`](https://github.com/nodejs/node/commit/ff539c5bb5)] - **cluster**: ignore queryServer msgs on disconnection (Santiago Gimeno) [#4465](https://github.com/nodejs/node/pull/4465)
* [[`00148b3de1`](https://github.com/nodejs/node/commit/00148b3de1)] - **deps**: backport 066747e from upstream V8 (Ali Ijaz Sheikh) [#4625](https://github.com/nodejs/node/pull/4625)
* [[`3912b5cbda`](https://github.com/nodejs/node/commit/3912b5cbda)] - **doc**: adds usage of readline line-by-line parsing (Robert Jefe Lindstaedt) [#4609](https://github.com/nodejs/node/pull/4609)
* [[`102fb7d3a1`](https://github.com/nodejs/node/commit/102fb7d3a1)] - **doc**: remove "above" and "below" references (Richard Sun) [#4499](https://github.com/nodejs/node/pull/4499)
* [[`df87176ae0`](https://github.com/nodejs/node/commit/df87176ae0)] - **doc**: update stylesheet to match frontpage (Roman Reiss) [#4621](https://github.com/nodejs/node/pull/4621)
* [[`ede98d1f98`](https://github.com/nodejs/node/commit/ede98d1f98)] - **doc**: stronger suggestion for userland assert (Wyatt Preul) [#4535](https://github.com/nodejs/node/pull/4535)
* [[`fdfc72c977`](https://github.com/nodejs/node/commit/fdfc72c977)] - **doc**: label http.IncomingMessage as a Class (Sequoia McDowell) [#4589](https://github.com/nodejs/node/pull/4589)
* [[`b181e26975`](https://github.com/nodejs/node/commit/b181e26975)] - **doc**: document http's server.listen return value (Sequoia McDowell) [#4590](https://github.com/nodejs/node/pull/4590)
* [[`97aaeb8519`](https://github.com/nodejs/node/commit/97aaeb8519)] - **doc**: fix description about the latest-codename (Minwoo Jung) [#4583](https://github.com/nodejs/node/pull/4583)
* [[`0126615d1e`](https://github.com/nodejs/node/commit/0126615d1e)] - **doc**: add Evan Lucas to Release Team (Evan Lucas) [#4579](https://github.com/nodejs/node/pull/4579)
* [[`ec73c69412`](https://github.com/nodejs/node/commit/ec73c69412)] - **doc**: add Myles Borins to Release Team (Myles Borins) [#4578](https://github.com/nodejs/node/pull/4578)
* [[`e703c9a4e2`](https://github.com/nodejs/node/commit/e703c9a4e2)] - **doc**: bring releases.md up to date (cjihrig) [#4540](https://github.com/nodejs/node/pull/4540)
* [[`ac1108d5e7`](https://github.com/nodejs/node/commit/ac1108d5e7)] - **doc**: add missing backtick for readline (Brian White) [#4549](https://github.com/nodejs/node/pull/4549)
* [[`09bc0c6a05`](https://github.com/nodejs/node/commit/09bc0c6a05)] - **doc**: improvements to crypto.markdown copy (James M Snell) [#4435](https://github.com/nodejs/node/pull/4435)
* [[`787c5d96bd`](https://github.com/nodejs/node/commit/787c5d96bd)] - **http**: remove variable redeclaration (Rich Trott) [#4612](https://github.com/nodejs/node/pull/4612)
* [[`145b66820f`](https://github.com/nodejs/node/commit/145b66820f)] - **module**: move unnecessary work for early return (Andres Suarez) [#3579](https://github.com/nodejs/node/pull/3579)
* [[`ffb7deb443`](https://github.com/nodejs/node/commit/ffb7deb443)] - **net**: remove hot path comment from connect (Evan Lucas) [#4648](https://github.com/nodejs/node/pull/4648)
* [[`799aa74d90`](https://github.com/nodejs/node/commit/799aa74d90)] - **net**: fix dns lookup for android (Josh Dague) [#4580](https://github.com/nodejs/node/pull/4580)
* [[`9accebe087`](https://github.com/nodejs/node/commit/9accebe087)] - **net, doc**: fix line wrapping lint in net.js (James M Snell) [#4588](https://github.com/nodejs/node/pull/4588)
* [[`37a546b490`](https://github.com/nodejs/node/commit/37a546b490)] - **src**: remove redeclarations of variables (Rich Trott) [#4605](https://github.com/nodejs/node/pull/4605)
* [[`b515ccc2a1`](https://github.com/nodejs/node/commit/b515ccc2a1)] - **stream**: remove useless if test in transform (zoubin) [#4617](https://github.com/nodejs/node/pull/4617)
* [[`ea6e26d904`](https://github.com/nodejs/node/commit/ea6e26d904)] - **test**: remove duplicate fork module import (Rich Trott) [#4634](https://github.com/nodejs/node/pull/4634)
* [[`b14b2aec5e`](https://github.com/nodejs/node/commit/b14b2aec5e)] - **test**: require common module only once (Rich Trott) [#4611](https://github.com/nodejs/node/pull/4611)
* [[`f28a640505`](https://github.com/nodejs/node/commit/f28a640505)] - **test**: only include http module once (Rich Trott) [#4606](https://github.com/nodejs/node/pull/4606)
* [[`6f9a96f497`](https://github.com/nodejs/node/commit/6f9a96f497)] - **test**: fix flaky unrefed timers test (Rich Trott) [#4599](https://github.com/nodejs/node/pull/4599)
* [[`b70eec8f7b`](https://github.com/nodejs/node/commit/b70eec8f7b)] - **tls_legacy**: do not read on OpenSSL's stack (Fedor Indutny) [#4624](https://github.com/nodejs/node/pull/4624)

## 2016-01-06, Version 5.4.0 (Stable), @Fishrock123

### Notable changes

* **http**:
  - A new status code was added: 451 - "Unavailable For Legal Reasons" (Max Barinov) [#4377](https://github.com/nodejs/node/pull/4377).
  - Idle sockets that have been kept alive now handle errors (José F. Romaniello) [#4482](https://github.com/nodejs/node/pull/4482).
* This release also includes several minor performance improvements:
  - **assert**: deepEqual is now speedier when comparing TypedArrays (Claudio Rodriguez) [#4330](https://github.com/nodejs/node/pull/4330).
  - **lib**: Use arrow functions instead of bind where possible (Minwoo Jung) [node#3622](https://github.com/nodejs/node/pull/3622).
  - **node**: Improved accessor perf of `process.env` (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780).
  - **node**: Improved performance of `process.hrtime()` (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780), (Evan Lucas) [#4484](https://github.com/nodejs/node/pull/4484).
  - **node**: Improved GetActiveHandles performance (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780).
  - **util**: Use faster iteration in `util.format()` (Jackson Tian) [#3964](https://github.com/nodejs/node/pull/3964).

### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* Unicode characters in filesystem paths are not handled consistently across platforms or Node.js APIs. See [#2088](https://github.com/nodejs/node/issues/2088), [#3401](https://github.com/nodejs/node/issues/3401) and [#3519](https://github.com/nodejs/node/issues/3519).

### Commits

* [[`d265fc821a`](https://github.com/nodejs/node/commit/d265fc821a)] - **assert**: typed array deepequal performance fix (Claudio Rodriguez) [#4330](https://github.com/nodejs/node/pull/4330)
* [[`6d8053ab56`](https://github.com/nodejs/node/commit/6d8053ab56)] - **buffer**: faster case for create Buffer from new Buffer(0) (Jackson Tian) [#4326](https://github.com/nodejs/node/pull/4326)
* [[`8781c59878`](https://github.com/nodejs/node/commit/8781c59878)] - **buffer**: refactor create buffer (Jackson Tian) [#4340](https://github.com/nodejs/node/pull/4340)
* [[`252628294b`](https://github.com/nodejs/node/commit/252628294b)] - **child_process**: guard against race condition (Rich Trott) [#4418](https://github.com/nodejs/node/pull/4418)
* [[`fcf632bc6a`](https://github.com/nodejs/node/commit/fcf632bc6a)] - **crypto**: load PFX chain the same way as regular one (Fedor Indutny) [#4165](https://github.com/nodejs/node/pull/4165)
* [[`a5094a35a0`](https://github.com/nodejs/node/commit/a5094a35a0)] - **debugger**: guard against call from non-node context (Ben Noordhuis) [#4328](https://github.com/nodejs/node/pull/4328)
* [[`b4c51c5b76`](https://github.com/nodejs/node/commit/b4c51c5b76)] - **deps**: backport 200315c from V8 upstream (Vladimir Kurchatkin) [#4128](https://github.com/nodejs/node/pull/4128)
* [[`334e73942e`](https://github.com/nodejs/node/commit/334e73942e)] - **doc**: fix heading level error in Buffer doc (Shigeki Ohtsu) [#4537](https://github.com/nodejs/node/pull/4537)
* [[`5be0259181`](https://github.com/nodejs/node/commit/5be0259181)] - **doc**: close backtick in process.title description (Dave) [#4534](https://github.com/nodejs/node/pull/4534)
* [[`35aec4c14d`](https://github.com/nodejs/node/commit/35aec4c14d)] - **doc**: fix numbering in stream.markdown (Richard Sun) [#4538](https://github.com/nodejs/node/pull/4538)
* [[`982f3227a5`](https://github.com/nodejs/node/commit/982f3227a5)] - **doc**: improvements to dgram.markdown copy (James M Snell) [#4437](https://github.com/nodejs/node/pull/4437)
* [[`6cdfa38d23`](https://github.com/nodejs/node/commit/6cdfa38d23)] - **doc**: improvements to errors.markdown copy (James M Snell) [#4454](https://github.com/nodejs/node/pull/4454)
* [[`6c7bcd5007`](https://github.com/nodejs/node/commit/6c7bcd5007)] - **doc**: fix website wg mislink (jona) [#4357](https://github.com/nodejs/node/pull/4357)
* [[`eee50821dc`](https://github.com/nodejs/node/commit/eee50821dc)] - **doc**: mention that http.Server inherits from net.Server (Ryan Sobol) [#4455](https://github.com/nodejs/node/pull/4455)
* [[`c745b4d5f8`](https://github.com/nodejs/node/commit/c745b4d5f8)] - **doc**: explain ClientRequest#setTimeout time unit (Ben Ripkens) [#4458](https://github.com/nodejs/node/pull/4458)
* [[`40076bf7f8`](https://github.com/nodejs/node/commit/40076bf7f8)] - **doc**: fix spelling error in lib/url.js comment (Nik Nyby) [#4390](https://github.com/nodejs/node/pull/4390)
* [[`5a223d64e3`](https://github.com/nodejs/node/commit/5a223d64e3)] - **doc**: add anchors for _transform _flush _writev in stream.markdown (iamchenxin) [#4448](https://github.com/nodejs/node/pull/4448)
* [[`e8bbeecc4c`](https://github.com/nodejs/node/commit/e8bbeecc4c)] - **doc**: improvements to debugger.markdown copy (James M Snell) [#4436](https://github.com/nodejs/node/pull/4436)
* [[`ccd75fe3fb`](https://github.com/nodejs/node/commit/ccd75fe3fb)] - **doc**: improvements to events.markdown copy (James M Snell) [#4468](https://github.com/nodejs/node/pull/4468)
* [[`ed15962777`](https://github.com/nodejs/node/commit/ed15962777)] - **doc**: improvements to dns.markdown copy (James M Snell) [#4449](https://github.com/nodejs/node/pull/4449)
* [[`e177cc9fdf`](https://github.com/nodejs/node/commit/e177cc9fdf)] - **doc**: improvements to console.markdown copy (James M Snell) [#4428](https://github.com/nodejs/node/pull/4428)
* [[`c1bc9a1023`](https://github.com/nodejs/node/commit/c1bc9a1023)] - **doc**: improve child_process.markdown copy (James M Snell) [#4383](https://github.com/nodejs/node/pull/4383)
* [[`150f62847c`](https://github.com/nodejs/node/commit/150f62847c)] - **doc**: copyedit setTimeout() documentation (Rich Trott) [#4434](https://github.com/nodejs/node/pull/4434)
* [[`9e667354be`](https://github.com/nodejs/node/commit/9e667354be)] - **doc**: fix formatting in process.markdown (Rich Trott) [#4433](https://github.com/nodejs/node/pull/4433)
* [[`bc1c0dc3fb`](https://github.com/nodejs/node/commit/bc1c0dc3fb)] - **doc**: catch the WORKING_GROUPS.md bootstrap docs up to date (James M Snell) [#4367](https://github.com/nodejs/node/pull/4367)
* [[`c835ba3601`](https://github.com/nodejs/node/commit/c835ba3601)] - **doc**: improve assert.markdown copy (James M Snell) [#4360](https://github.com/nodejs/node/pull/4360)
* [[`e79eda74c0`](https://github.com/nodejs/node/commit/e79eda74c0)] - **doc**: copyedit releases.md (Rich Trott) [#4384](https://github.com/nodejs/node/pull/4384)
* [[`6450d8667f`](https://github.com/nodejs/node/commit/6450d8667f)] - **doc**: improve grammar in tls docs (Adri Van Houdt) [#4315](https://github.com/nodejs/node/pull/4315)
* [[`474a0f081a`](https://github.com/nodejs/node/commit/474a0f081a)] - **doc**: improvements to buffer.markdown copy (James M Snell) [#4370](https://github.com/nodejs/node/pull/4370)
* [[`57684d650e`](https://github.com/nodejs/node/commit/57684d650e)] - **doc**: improve addons.markdown copy (James M Snell) [#4320](https://github.com/nodejs/node/pull/4320)
* [[`04dd861221`](https://github.com/nodejs/node/commit/04dd861221)] - **doc**: fix, modernize examples in docs (James M Snell) [#4282](https://github.com/nodejs/node/pull/4282)
* [[`5ce6e99474`](https://github.com/nodejs/node/commit/5ce6e99474)] - **doc**: Typo in buffer.markdown referencing buf.write() (chrisjohn404) [#4324](https://github.com/nodejs/node/pull/4324)
* [[`699bf2c464`](https://github.com/nodejs/node/commit/699bf2c464)] - **doc**: fix link in addons.markdown (Nicholas Young) [#4331](https://github.com/nodejs/node/pull/4331)
* [[`e742422757`](https://github.com/nodejs/node/commit/e742422757)] - **fs**: use pushValueToArray for readdir(Sync) (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780)
* [[`1dd2d015d2`](https://github.com/nodejs/node/commit/1dd2d015d2)] - **(SEMVER-MINOR)** **http**: handle errors on idle sockets (José F. Romaniello) [#4482](https://github.com/nodejs/node/pull/4482)
* [[`083ae166bb`](https://github.com/nodejs/node/commit/083ae166bb)] - **http**: use `self.keepAlive` instead of `self.options.keepAlive` (Damian Schenkelman) [#4407](https://github.com/nodejs/node/pull/4407)
* [[`ffb4a6e0e4`](https://github.com/nodejs/node/commit/ffb4a6e0e4)] - **http**: fix non-string header value concatenation (Brian White) [#4460](https://github.com/nodejs/node/pull/4460)
* [[`c77fd6829a`](https://github.com/nodejs/node/commit/c77fd6829a)] - **(SEMVER-MINOR)** **http**: 451 status code "Unavailable For Legal Reasons" (Max Barinov) [#4377](https://github.com/nodejs/node/pull/4377)
* [[`8f7af9a489`](https://github.com/nodejs/node/commit/8f7af9a489)] - **http**: remove excess calls to removeSocket (Dave) [#4172](https://github.com/nodejs/node/pull/4172)
* [[`b841967103`](https://github.com/nodejs/node/commit/b841967103)] - **http**: Remove an unnecessary assignment (Bo Borgerson) [#4323](https://github.com/nodejs/node/pull/4323)
* [[`b8366e76dd`](https://github.com/nodejs/node/commit/b8366e76dd)] - **http_parser**: use pushValueToArray for headers (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780)
* [[`ca97e7276e`](https://github.com/nodejs/node/commit/ca97e7276e)] - **https**: use `servername` in agent key (Fedor Indutny) [#4389](https://github.com/nodejs/node/pull/4389)
* [[`b5aaccc6af`](https://github.com/nodejs/node/commit/b5aaccc6af)] - **lib**: remove unused modules (Rich Trott) [#4396](https://github.com/nodejs/node/pull/4396)
* [[`921fb540c1`](https://github.com/nodejs/node/commit/921fb540c1)] - **node**: improve performance of process.hrtime() (Evan Lucas) [#4484](https://github.com/nodejs/node/pull/4484)
* [[`ecef817a28`](https://github.com/nodejs/node/commit/ecef817a28)] - **node**: improve accessor perf of process.env (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780)
* [[`89f056bdf3`](https://github.com/nodejs/node/commit/89f056bdf3)] - **node**: improve performance of hrtime() (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780)
* [[`c8fc217dc7`](https://github.com/nodejs/node/commit/c8fc217dc7)] - **node**: improve GetActiveHandles performance (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780)
* [[`8464667071`](https://github.com/nodejs/node/commit/8464667071)] - **node**: fix erroneously named function call (Trevor Norris) [#3780](https://github.com/nodejs/node/pull/3780)
* [[`e57fd51a5e`](https://github.com/nodejs/node/commit/e57fd51a5e)] - **os**: fix crash in GetInterfaceAddresses (Martin Bark) [#4272](https://github.com/nodejs/node/pull/4272)
* [[`65c40d753f`](https://github.com/nodejs/node/commit/65c40d753f)] - **repl**: remove unused function (Rich Trott)
* [[`3d41a44dba`](https://github.com/nodejs/node/commit/3d41a44dba)] - **repl**: Fixed node repl history edge case. (Mudit Ameta) [#4108](https://github.com/nodejs/node/pull/4108)
* [[`d11930d604`](https://github.com/nodejs/node/commit/d11930d604)] - **repl**: use String#repeat instead of Array#join (Evan Lucas) [#3900](https://github.com/nodejs/node/pull/3900)
* [[`4220d25626`](https://github.com/nodejs/node/commit/4220d25626)] - **test**: fix linting for the v5.x branch (Jeremiah Senkpiel) [#4547](https://github.com/nodejs/node/pull/4547)
* [[`4b14f1c983`](https://github.com/nodejs/node/commit/4b14f1c983)] - **test**: remove unused vars (Rich Trott) [#4536](https://github.com/nodejs/node/pull/4536)
* [[`2a69ab32ec`](https://github.com/nodejs/node/commit/2a69ab32ec)] - **test**: add test-domain-exit-dispose-again back (Julien Gilli) [#4256](https://github.com/nodejs/node/pull/4256)
* [[`ae0246641c`](https://github.com/nodejs/node/commit/ae0246641c)] - **test**: remove unused vars from parallel tests (Rich Trott) [#4511](https://github.com/nodejs/node/pull/4511)
* [[`984db93e7c`](https://github.com/nodejs/node/commit/984db93e7c)] - **test**: fix flaky test-cluster-shared-leak (Rich Trott) [#4510](https://github.com/nodejs/node/pull/4510)
* [[`30b0d7583a`](https://github.com/nodejs/node/commit/30b0d7583a)] - **test**: fix flaky streams test (Rich Trott) [#4516](https://github.com/nodejs/node/pull/4516)
* [[`46fefbc1b5`](https://github.com/nodejs/node/commit/46fefbc1b5)] - **test**: fix flaky test-http-agent-keepalive (Rich Trott) [#4524](https://github.com/nodejs/node/pull/4524)
* [[`e04a8401d9`](https://github.com/nodejs/node/commit/e04a8401d9)] - **test**: remove flaky designations for tests (Rich Trott) [#4519](https://github.com/nodejs/node/pull/4519)
* [[`a703b1bf73`](https://github.com/nodejs/node/commit/a703b1bf73)] - **test**: remove time check (Rich Trott) [#4494](https://github.com/nodejs/node/pull/4494)
* [[`02b3a5be52`](https://github.com/nodejs/node/commit/02b3a5be52)] - **test**: refactor test-fs-empty-readStream (Rich Trott) [#4490](https://github.com/nodejs/node/pull/4490)
* [[`ab3e5c1417`](https://github.com/nodejs/node/commit/ab3e5c1417)] - **test**: write to tmp dir rather than fixture dir (Rich Trott) [#4489](https://github.com/nodejs/node/pull/4489)
* [[`06043fdfa3`](https://github.com/nodejs/node/commit/06043fdfa3)] - **test**: remove unused modules (Rich Trott) [#4475](https://github.com/nodejs/node/pull/4475)
* [[`f1a66bc249`](https://github.com/nodejs/node/commit/f1a66bc249)] - **test**: clarify role of domains in test (Rich Trott) [#4474](https://github.com/nodejs/node/pull/4474)
* [[`08a3490dd6`](https://github.com/nodejs/node/commit/08a3490dd6)] - **test**: inherit JOBS from environment (Johan Bergström) [#4495](https://github.com/nodejs/node/pull/4495)
* [[`3bfc18763a`](https://github.com/nodejs/node/commit/3bfc18763a)] - **test**: improve assert message (Rich Trott) [#4461](https://github.com/nodejs/node/pull/4461)
* [[`d46d850461`](https://github.com/nodejs/node/commit/d46d850461)] - **test**: shorten path for bogus socket (Rich Trott) [#4478](https://github.com/nodejs/node/pull/4478)
* [[`f68f86cd0a`](https://github.com/nodejs/node/commit/f68f86cd0a)] - **test**: fix race condition in test-http-client-onerror (Devin Nakamura) [#4346](https://github.com/nodejs/node/pull/4346)
* [[`ec0b6362cf`](https://github.com/nodejs/node/commit/ec0b6362cf)] - **test**: remove unused assert module imports (Rich Trott) [#4438](https://github.com/nodejs/node/pull/4438)
* [[`ba2445046c`](https://github.com/nodejs/node/commit/ba2445046c)] - **test**: don't use cwd for relative path (Johan Bergström) [#4477](https://github.com/nodejs/node/pull/4477)
* [[`5110e4deed`](https://github.com/nodejs/node/commit/5110e4deed)] - **test**: don't assume a certain folder structure (Johan Bergström) [#3325](https://github.com/nodejs/node/pull/3325)
* [[`55c6946400`](https://github.com/nodejs/node/commit/55c6946400)] - **test**: make temp path customizable (Johan Bergström) [#3325](https://github.com/nodejs/node/pull/3325)
* [[`b19d19efaa`](https://github.com/nodejs/node/commit/b19d19efaa)] - **test**: extend timeout in Debug mode (Rich Trott) [#4431](https://github.com/nodejs/node/pull/4431)
* [[`c6a99ddd37`](https://github.com/nodejs/node/commit/c6a99ddd37)] - **test**: remove unused variables from net tests (Rich Trott) [#4430](https://github.com/nodejs/node/pull/4430)
* [[`54004f0e26`](https://github.com/nodejs/node/commit/54004f0e26)] - **test**: remove unused vars in ChildProcess tests (Rich Trott) [#4425](https://github.com/nodejs/node/pull/4425)
* [[`e72112f90e`](https://github.com/nodejs/node/commit/e72112f90e)] - **test**: fix flaky cluster-disconnect-race (Brian White) [#4457](https://github.com/nodejs/node/pull/4457)
* [[`715afc9bbd`](https://github.com/nodejs/node/commit/715afc9bbd)] - **test**: fix flaky cluster-net-send (Brian White) [#4444](https://github.com/nodejs/node/pull/4444)
* [[`03c4bc704f`](https://github.com/nodejs/node/commit/03c4bc704f)] - **test**: fix flaky child-process-fork-regr-gh-2847 (Brian White) [#4442](https://github.com/nodejs/node/pull/4442)
* [[`684eb32072`](https://github.com/nodejs/node/commit/684eb32072)] - **test**: remove unused variables from HTTPS tests (Rich Trott) [#4426](https://github.com/nodejs/node/pull/4426)
* [[`585c01f674`](https://github.com/nodejs/node/commit/585c01f674)] - **test**: remove unused variables from TLS tests (Rich Trott) [#4424](https://github.com/nodejs/node/pull/4424)
* [[`c36ca37e2a`](https://github.com/nodejs/node/commit/c36ca37e2a)] - **test**: remove unused variables form http tests (Rich Trott) [#4422](https://github.com/nodejs/node/pull/4422)
* [[`c639d0f1fe`](https://github.com/nodejs/node/commit/c639d0f1fe)] - **test**: mark test-debug-no-context is flaky (Rich Trott) [#4421](https://github.com/nodejs/node/pull/4421)
* [[`cd79ec268d`](https://github.com/nodejs/node/commit/cd79ec268d)] - **test**: remove unnecessary assignments (Rich Trott) [#4408](https://github.com/nodejs/node/pull/4408)
* [[`0799a9abaf`](https://github.com/nodejs/node/commit/0799a9abaf)] - **test**: remove unused var from test-assert.js (Rich Trott) [#4405](https://github.com/nodejs/node/pull/4405)
* [[`3710028a85`](https://github.com/nodejs/node/commit/3710028a85)] - **test**: remove unused `util` imports (Rich Trott) [#4397](https://github.com/nodejs/node/pull/4397)
* [[`8c9d0c1f6f`](https://github.com/nodejs/node/commit/8c9d0c1f6f)] - **test**: refactor test-net-connect-options-ipv6 (Rich Trott) [#4395](https://github.com/nodejs/node/pull/4395)
* [[`874209022f`](https://github.com/nodejs/node/commit/874209022f)] - **test**: fix http-response-multiheaders (Santiago Gimeno) [#3958](https://github.com/nodejs/node/pull/3958)
* [[`71b79bcf54`](https://github.com/nodejs/node/commit/71b79bcf54)] - **test**: test each block in addon.md contains js & cc (Rod Vagg) [#4411](https://github.com/nodejs/node/pull/4411)
* [[`00b37de243`](https://github.com/nodejs/node/commit/00b37de243)] - **test**: fix domain-top-level-error-handler-throw (Santiago Gimeno) [#4364](https://github.com/nodejs/node/pull/4364)
* [[`6d14b6520f`](https://github.com/nodejs/node/commit/6d14b6520f)] - **test**: use platformTimeout() in more places (Brian White) [#4387](https://github.com/nodejs/node/pull/4387)
* [[`82f74caa56`](https://github.com/nodejs/node/commit/82f74caa56)] - **test**: fix flaky test-net-error-twice (Brian White) [#4342](https://github.com/nodejs/node/pull/4342)
* [[`96501e55be`](https://github.com/nodejs/node/commit/96501e55be)] - **test**: try other ipv6 localhost alternatives (Brian White) [#4325](https://github.com/nodejs/node/pull/4325)
* [[`69343d6d2e`](https://github.com/nodejs/node/commit/69343d6d2e)] - **tls_wrap**: clear errors on return (Fedor Indutny) [#4515](https://github.com/nodejs/node/pull/4515)
* [[`ca9812cf4d`](https://github.com/nodejs/node/commit/ca9812cf4d)] - **tools**: fix warning in doc parsing (Shigeki Ohtsu) [#4537](https://github.com/nodejs/node/pull/4537)
* [[`386030b524`](https://github.com/nodejs/node/commit/386030b524)] - **tools**: implement no-unused-vars for eslint (Rich Trott) [#4536](https://github.com/nodejs/node/pull/4536)
* [[`14a947fc70`](https://github.com/nodejs/node/commit/14a947fc70)] - **tools**: run tick processor without forking (Matt Loring) [#4224](https://github.com/nodejs/node/pull/4224)
* [[`8039ca06eb`](https://github.com/nodejs/node/commit/8039ca06eb)] - **util**: faster arrayToHash (Jackson Tian)  [#3964](https://github.com/nodejs/node/pull/3964)

## 2015-12-23, Version 4.2.4 'Argon' (LTS), @jasnell

Maintenance update.

### Notable changes

* Roughly 78% of the commits are documentation and test improvements
* **domains**:
** Fix handling of uncaught exceptions (Julien Gilli) [#3884](https://github.com/nodejs/node/pull/3884)
* **deps**:
** Upgrade to npm 2.14.12 (Kat Marchán) [#4110](https://github.com/nodejs/node/pull/4110)
** Backport 819b40a from V8 upstream (Michaël Zasso) [#3938](https://github.com/nodejs/node/pull/3938)
** Updated node LICENSE file with new npm license (Kat Marchán) [#4110](https://github.com/nodejs/node/pull/4110)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`907a13a07f`](https://github.com/nodejs/node/commit/907a13a07f)] - Add missing va_end before return (Ömer Fadıl Usta) [#3565](https://github.com/nodejs/node/pull/3565)
* [[`7ffc01756f`](https://github.com/nodejs/node/commit/7ffc01756f)] - **buffer**: fix writeInt{B,L}E for some neg values (Peter A. Bigot) [#3994](https://github.com/nodejs/node/pull/3994)
* [[`db0186e435`](https://github.com/nodejs/node/commit/db0186e435)] - **buffer**: let WriteFloatGeneric silently drop values (Minqi Pan)
* [[`5c6740865a`](https://github.com/nodejs/node/commit/5c6740865a)] - **build**: update signtool description, add url (Rod Vagg) [#4011](https://github.com/nodejs/node/pull/4011)
* [[`60dda70f89`](https://github.com/nodejs/node/commit/60dda70f89)] - **build**: fix --with-intl=system-icu for x-compile (Steven R. Loomis) [#3808](https://github.com/nodejs/node/pull/3808)
* [[`22208b067c`](https://github.com/nodejs/node/commit/22208b067c)] - **build**: fix configuring with prebuilt libraries (Markus Tzoe) [#3135](https://github.com/nodejs/node/pull/3135)
* [[`914caf9c69`](https://github.com/nodejs/node/commit/914caf9c69)] - **child_process**: add safety checks on stdio access (cjihrig) [#3799](https://github.com/nodejs/node/pull/3799)
* [[`236ad90a84`](https://github.com/nodejs/node/commit/236ad90a84)] - **child_process**: don't fork bomb ourselves from -e (Ben Noordhuis) [#3575](https://github.com/nodejs/node/pull/3575)
* [[`f28f69dac4`](https://github.com/nodejs/node/commit/f28f69dac4)] - **cluster**: remove handles when disconnecting worker (Ben Noordhuis) [#3677](https://github.com/nodejs/node/pull/3677)
* [[`f5c5e8bf91`](https://github.com/nodejs/node/commit/f5c5e8bf91)] - **cluster**: send suicide message on disconnect (cjihrig) [#3720](https://github.com/nodejs/node/pull/3720)
* [[`629d5d18d7`](https://github.com/nodejs/node/commit/629d5d18d7)] - **configure**: `v8_use_snapshot` should be `true` (Fedor Indutny) [#3962](https://github.com/nodejs/node/pull/3962)
* [[`3094464871`](https://github.com/nodejs/node/commit/3094464871)] - **configure**: use __ARM_ARCH to determine arm version (João Reis) [#4123](https://github.com/nodejs/node/pull/4123)
* [[`1e1173fc5c`](https://github.com/nodejs/node/commit/1e1173fc5c)] - **configure**: respect CC_host in host arch detection (João Reis) [#4117](https://github.com/nodejs/node/pull/4117)
* [[`2e9b886fbf`](https://github.com/nodejs/node/commit/2e9b886fbf)] - **crypto**: DSA parameter validation in FIPS mode (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`00b77d9e84`](https://github.com/nodejs/node/commit/00b77d9e84)] - **crypto**: Improve error checking and reporting (Stefan Budeanu) [#3753](https://github.com/nodejs/node/pull/3753)
* [[`3dd90ddc73`](https://github.com/nodejs/node/commit/3dd90ddc73)] - **deps**: upgrade to npm 2.14.12 (Kat Marchán) [#4110](https://github.com/nodejs/node/pull/4110)
* [[`51ae8d10b3`](https://github.com/nodejs/node/commit/51ae8d10b3)] - **deps**: Updated node LICENSE file with new npm license (Kat Marchán) [#4110](https://github.com/nodejs/node/pull/4110)
* [[`9e1edead22`](https://github.com/nodejs/node/commit/9e1edead22)] - **deps**: backport 819b40a from V8 upstream (Michaël Zasso) [#3938](https://github.com/nodejs/node/pull/3938)
* [[`a2ce3843cc`](https://github.com/nodejs/node/commit/a2ce3843cc)] - **deps**: upgrade npm to 2.14.9 (Forrest L Norvell) [#3686](https://github.com/nodejs/node/pull/3686)
* [[`b140cb29f4`](https://github.com/nodejs/node/commit/b140cb29f4)] - **dns**: prevent undefined values in results (Junliang Yan) [#3696](https://github.com/nodejs/node/pull/3696)
* [[`8aafa2ecc0`](https://github.com/nodejs/node/commit/8aafa2ecc0)] - **doc**: standardize references to node.js in docs (Scott Buchanan) [#4136](https://github.com/nodejs/node/pull/4136)
* [[`72f43a263a`](https://github.com/nodejs/node/commit/72f43a263a)] - **doc**: fix internal link to child.send() (Luigi Pinca) [#4089](https://github.com/nodejs/node/pull/4089)
* [[`dcfdbac457`](https://github.com/nodejs/node/commit/dcfdbac457)] - **doc**: reword https.Agent example text (Jan Krems) [#4075](https://github.com/nodejs/node/pull/4075)
* [[`f93d268dec`](https://github.com/nodejs/node/commit/f93d268dec)] - **doc**: add HTTP working group (James M Snell) [#3919](https://github.com/nodejs/node/pull/3919)
* [[`beee0553ca`](https://github.com/nodejs/node/commit/beee0553ca)] - **doc**: update WORKING_GROUPS.md - add missing groups (Michael Dawson) [#3450](https://github.com/nodejs/node/pull/3450)
* [[`3327415fc4`](https://github.com/nodejs/node/commit/3327415fc4)] - **doc**: fix the exception description (yorkie) [#3658](https://github.com/nodejs/node/pull/3658)
* [[`da8d012c88`](https://github.com/nodejs/node/commit/da8d012c88)] - **doc**: clarify v4.2.3 notable items (Rod Vagg) [#4155](https://github.com/nodejs/node/pull/4155)
* [[`44a2d8ca24`](https://github.com/nodejs/node/commit/44a2d8ca24)] - **doc**: fix color of linked code blocks (jpersson) [#4068](https://github.com/nodejs/node/pull/4068)
* [[`bebde48ebc`](https://github.com/nodejs/node/commit/bebde48ebc)] - **doc**: fix typo in README (Rich Trott) [#4000](https://github.com/nodejs/node/pull/4000)
* [[`b48d5ec301`](https://github.com/nodejs/node/commit/b48d5ec301)] - **doc**: message.header duplication correction (Bryan English) [#3997](https://github.com/nodejs/node/pull/3997)
* [[`6ef3625456`](https://github.com/nodejs/node/commit/6ef3625456)] - **doc**: replace sane with reasonable (Lewis Cowper) [#3980](https://github.com/nodejs/node/pull/3980)
* [[`c5be3c63f0`](https://github.com/nodejs/node/commit/c5be3c63f0)] - **doc**: fix rare case of misaligned columns (Roman Reiss) [#3948](https://github.com/nodejs/node/pull/3948)
* [[`bd82fb06ff`](https://github.com/nodejs/node/commit/bd82fb06ff)] - **doc**: fix broken references (Alexander Gromnitsky) [#3944](https://github.com/nodejs/node/pull/3944)
* [[`8eb28c3d50`](https://github.com/nodejs/node/commit/8eb28c3d50)] - **doc**: add reference for buffer.inspect() (cjihrig) [#3921](https://github.com/nodejs/node/pull/3921)
* [[`4bc71e0078`](https://github.com/nodejs/node/commit/4bc71e0078)] - **doc**: clarify module loading behavior (cjihrig) [#3920](https://github.com/nodejs/node/pull/3920)
* [[`4c382e7aaa`](https://github.com/nodejs/node/commit/4c382e7aaa)] - **doc**: numeric flags to fs.open (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`5207099dc9`](https://github.com/nodejs/node/commit/5207099dc9)] - **doc**: clarify that fs streams expect blocking fd (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`753c5071ea`](https://github.com/nodejs/node/commit/753c5071ea)] - **doc**: Adding best practises for crypto.pbkdf2 (Tom Gallacher) [#3290](https://github.com/nodejs/node/pull/3290)
* [[`8f0291beba`](https://github.com/nodejs/node/commit/8f0291beba)] - **doc**: update WORKING_GROUPS.md to include Intl (Steven R. Loomis) [#3251](https://github.com/nodejs/node/pull/3251)
* [[`c31d472487`](https://github.com/nodejs/node/commit/c31d472487)] - **doc**: sort repl alphabetically (Tristian Flanagan) [#3859](https://github.com/nodejs/node/pull/3859)
* [[`6b172d9fe8`](https://github.com/nodejs/node/commit/6b172d9fe8)] - **doc**: consistent reference-style links (Bryan English) [#3845](https://github.com/nodejs/node/pull/3845)
* [[`ffd3335e29`](https://github.com/nodejs/node/commit/ffd3335e29)] - **doc**: address use of profanity in code of conduct (James M Snell) [#3827](https://github.com/nodejs/node/pull/3827)
* [[`a36a5b63cf`](https://github.com/nodejs/node/commit/a36a5b63cf)] - **doc**: reword message.headers to indicate they are not read-only (Tristian Flanagan) [#3814](https://github.com/nodejs/node/pull/3814)
* [[`6de77cd320`](https://github.com/nodejs/node/commit/6de77cd320)] - **doc**: clarify duplicate header handling (Bryan English) [#3810](https://github.com/nodejs/node/pull/3810)
* [[`b22973af81`](https://github.com/nodejs/node/commit/b22973af81)] - **doc**: replace head of readme with updated text (Rod Vagg) [#3482](https://github.com/nodejs/node/pull/3482)
* [[`eab0d56ea9`](https://github.com/nodejs/node/commit/eab0d56ea9)] - **doc**: repl: add defineComand and displayPrompt (Bryan English) [#3765](https://github.com/nodejs/node/pull/3765)
* [[`15fb02985f`](https://github.com/nodejs/node/commit/15fb02985f)] - **doc**: document release types in readme (Rod Vagg) [#3482](https://github.com/nodejs/node/pull/3482)
* [[`29f26b882f`](https://github.com/nodejs/node/commit/29f26b882f)] - **doc**: add link to \[customizing util.inspect colors\]. (Jesse McCarthy) [#3749](https://github.com/nodejs/node/pull/3749)
* [[`90fdb4f7b3`](https://github.com/nodejs/node/commit/90fdb4f7b3)] - **doc**: sort tls alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`39fa9fa85c`](https://github.com/nodejs/node/commit/39fa9fa85c)] - **doc**: sort stream alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`e98e8afb2b`](https://github.com/nodejs/node/commit/e98e8afb2b)] - **doc**: sort net alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`6de887483d`](https://github.com/nodejs/node/commit/6de887483d)] - **doc**: sort process alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`37033dcb71`](https://github.com/nodejs/node/commit/37033dcb71)] - **doc**: sort zlib alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`9878034567`](https://github.com/nodejs/node/commit/9878034567)] - **doc**: sort util alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`48fc765eb6`](https://github.com/nodejs/node/commit/48fc765eb6)] - **doc**: sort https alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`3546eb4f40`](https://github.com/nodejs/node/commit/3546eb4f40)] - **doc**: sort http alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`dedfb1156a`](https://github.com/nodejs/node/commit/dedfb1156a)] - **doc**: sort modules alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`71722fe1a1`](https://github.com/nodejs/node/commit/71722fe1a1)] - **doc**: sort readline alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`660062bf9e`](https://github.com/nodejs/node/commit/660062bf9e)] - **doc**: sort repl alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`34b8d28725`](https://github.com/nodejs/node/commit/34b8d28725)] - **doc**: sort string_decoder alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`3f3b9ed7d7`](https://github.com/nodejs/node/commit/3f3b9ed7d7)] - **doc**: sort timers alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`af876ddc64`](https://github.com/nodejs/node/commit/af876ddc64)] - **doc**: sort tty alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`3c2068704a`](https://github.com/nodejs/node/commit/3c2068704a)] - **doc**: sort url alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`363692fd0c`](https://github.com/nodejs/node/commit/363692fd0c)] - **doc**: sort vm alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`ca41b55166`](https://github.com/nodejs/node/commit/ca41b55166)] - **doc**: sort querystring alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`f37ff22b9f`](https://github.com/nodejs/node/commit/f37ff22b9f)] - **doc**: sort punycode alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`4d569607af`](https://github.com/nodejs/node/commit/4d569607af)] - **doc**: sort path alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`daa62447d1`](https://github.com/nodejs/node/commit/daa62447d1)] - **doc**: sort os alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`0906f9a8bb`](https://github.com/nodejs/node/commit/0906f9a8bb)] - **doc**: sort globals alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`6cd06c1319`](https://github.com/nodejs/node/commit/6cd06c1319)] - **doc**: sort fs alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`5b310f8d9e`](https://github.com/nodejs/node/commit/5b310f8d9e)] - **doc**: sort events alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`782cb7d15b`](https://github.com/nodejs/node/commit/782cb7d15b)] - **doc**: sort errors alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`c39eabbec4`](https://github.com/nodejs/node/commit/c39eabbec4)] - **doc**: sort dgram alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`261e0f3a21`](https://github.com/nodejs/node/commit/261e0f3a21)] - **doc**: sort crypto alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`0e6121d04d`](https://github.com/nodejs/node/commit/0e6121d04d)] - **doc**: sort dns alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`435ffb79f7`](https://github.com/nodejs/node/commit/435ffb79f7)] - **doc**: sort console alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`28935a10d6`](https://github.com/nodejs/node/commit/28935a10d6)] - **doc**: sort cluster alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`5e79dc4406`](https://github.com/nodejs/node/commit/5e79dc4406)] - **doc**: sort child_process alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`af0bf1a72c`](https://github.com/nodejs/node/commit/af0bf1a72c)] - **doc**: sort buffer alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`f43a0330aa`](https://github.com/nodejs/node/commit/f43a0330aa)] - **doc**: sort assert alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`1bbc3b3ff8`](https://github.com/nodejs/node/commit/1bbc3b3ff8)] - **doc**: add note on tls connection meta data methods (Tyler Henkel) [#3746](https://github.com/nodejs/node/pull/3746)
* [[`3c415bbb12`](https://github.com/nodejs/node/commit/3c415bbb12)] - **doc**: add note to util.isBuffer (Evan Lucas) [#3790](https://github.com/nodejs/node/pull/3790)
* [[`7b5e4574fd`](https://github.com/nodejs/node/commit/7b5e4574fd)] - **doc**: add romankl to collaborators (Roman Klauke) [#3725](https://github.com/nodejs/node/pull/3725)
* [[`4f7c638a7a`](https://github.com/nodejs/node/commit/4f7c638a7a)] - **doc**: add saghul as a collaborator (Saúl Ibarra Corretgé)
* [[`523251270a`](https://github.com/nodejs/node/commit/523251270a)] - **doc**: add thealphanerd to collaborators (Myles Borins) [#3723](https://github.com/nodejs/node/pull/3723)
* [[`488e74f27d`](https://github.com/nodejs/node/commit/488e74f27d)] - **doc**: update lts description in the collaborator guide (James M Snell) [#3668](https://github.com/nodejs/node/pull/3668)
* [[`fe3ae3cea4`](https://github.com/nodejs/node/commit/fe3ae3cea4)] - **doc**: add LTS info to COLLABORATOR_GUIDE.md (Myles Borins) [#3442](https://github.com/nodejs/node/pull/3442)
* [[`daa10a345e`](https://github.com/nodejs/node/commit/daa10a345e)] - **doc**: typo fix in readme.md (Sam P Gallagher-Bishop) [#3649](https://github.com/nodejs/node/pull/3649)
* [[`eca5720761`](https://github.com/nodejs/node/commit/eca5720761)] - **doc**: fix wrong date and known issue in changelog.md (James M Snell) [#3650](https://github.com/nodejs/node/pull/3650)
* [[`83494f8f3e`](https://github.com/nodejs/node/commit/83494f8f3e)] - **doc**: rename iojs-* groups to nodejs-* (Steven R. Loomis) [#3634](https://github.com/nodejs/node/pull/3634)
* [[`347fb65aee`](https://github.com/nodejs/node/commit/347fb65aee)] - **doc**: fix crypto spkac function descriptions (Jason Gerfen) [#3614](https://github.com/nodejs/node/pull/3614)
* [[`11d2050d63`](https://github.com/nodejs/node/commit/11d2050d63)] - **doc**: Updated streams simplified constructor API (Tom Gallacher) [#3602](https://github.com/nodejs/node/pull/3602)
* [[`6db4392bfb`](https://github.com/nodejs/node/commit/6db4392bfb)] - **doc**: made code spans more visible in the API docs (phijohns) [#3573](https://github.com/nodejs/node/pull/3573)
* [[`8a7dd73af1`](https://github.com/nodejs/node/commit/8a7dd73af1)] - **doc**: added what buf.copy returns (Manuel B) [#3555](https://github.com/nodejs/node/pull/3555)
* [[`cf4b65c2d6`](https://github.com/nodejs/node/commit/cf4b65c2d6)] - **doc**: fix function param order in assert doc (David Woods) [#3533](https://github.com/nodejs/node/pull/3533)
* [[`a2efe4c72b`](https://github.com/nodejs/node/commit/a2efe4c72b)] - **doc**: add note about timeout delay > TIMEOUT_MAX (Guilherme Souza) [#3512](https://github.com/nodejs/node/pull/3512)
* [[`d1b5833476`](https://github.com/nodejs/node/commit/d1b5833476)] - **doc**: add caveats of algs and key size in crypto (Shigeki Ohtsu) [#3479](https://github.com/nodejs/node/pull/3479)
* [[`12cdf6fcf3`](https://github.com/nodejs/node/commit/12cdf6fcf3)] - **doc**: add method links in events.markdown (Alejandro Oviedo) [#3187](https://github.com/nodejs/node/pull/3187)
* [[`f50f19e384`](https://github.com/nodejs/node/commit/f50f19e384)] - **doc**: stdout/stderr can block when directed to file (Ben Noordhuis) [#3170](https://github.com/nodejs/node/pull/3170)
* [[`b2cc1302e0`](https://github.com/nodejs/node/commit/b2cc1302e0)] - **docs**: improve discoverability of Code of Conduct (Ashley Williams) [#3774](https://github.com/nodejs/node/pull/3774)
* [[`fa1ab497f1`](https://github.com/nodejs/node/commit/fa1ab497f1)] - **docs**: fs - change links to buffer encoding to Buffer class anchor (fansworld-claudio) [#2796](https://github.com/nodejs/node/pull/2796)
* [[`34e64e5390`](https://github.com/nodejs/node/commit/34e64e5390)] - **domains**: fix handling of uncaught exceptions (Julien Gilli) [#3884](https://github.com/nodejs/node/pull/3884)
* [[`0311836e7a`](https://github.com/nodejs/node/commit/0311836e7a)] - **meta**: remove use of profanity in source (Myles Borins) [#4122](https://github.com/nodejs/node/pull/4122)
* [[`971762ada9`](https://github.com/nodejs/node/commit/971762ada9)] - **module**: cache regular expressions (Evan Lucas) [#3869](https://github.com/nodejs/node/pull/3869)
* [[`d80fa2c77c`](https://github.com/nodejs/node/commit/d80fa2c77c)] - **module**: remove unnecessary JSON.stringify (Andres Suarez) [#3578](https://github.com/nodejs/node/pull/3578)
* [[`aa85d62f09`](https://github.com/nodejs/node/commit/aa85d62f09)] - **net**: add local address/port for better errors (Jan Schär) [#3946](https://github.com/nodejs/node/pull/3946)
* [[`803a56de52`](https://github.com/nodejs/node/commit/803a56de52)] - **querystring**: Parse multiple separator characters (Yosuke Furukawa) [#3807](https://github.com/nodejs/node/pull/3807)
* [[`ff02b295fc`](https://github.com/nodejs/node/commit/ff02b295fc)] - **repl**: don't crash if cannot open history file (Evan Lucas) [#3630](https://github.com/nodejs/node/pull/3630)
* [[`329e88e545`](https://github.com/nodejs/node/commit/329e88e545)] - **repl**: To exit, press ^C again or type .exit. (Hemanth.HM) [#3368](https://github.com/nodejs/node/pull/3368)
* [[`9b05905361`](https://github.com/nodejs/node/commit/9b05905361)] - **src**: Revert "nix stdin _readableState.reading" (Roman Reiss) [#3490](https://github.com/nodejs/node/pull/3490)
* [[`957c1f2543`](https://github.com/nodejs/node/commit/957c1f2543)] - **stream_wrap**: error if stream has StringDecoder (Fedor Indutny) [#4031](https://github.com/nodejs/node/pull/4031)
* [[`43e3b69dae`](https://github.com/nodejs/node/commit/43e3b69dae)] - **test**: refactor test-http-exit-delay (Rich Trott) [#4055](https://github.com/nodejs/node/pull/4055)
* [[`541d0d21be`](https://github.com/nodejs/node/commit/541d0d21be)] - **test**: fix cluster-disconnect-handles flakiness (Santiago Gimeno) [#4009](https://github.com/nodejs/node/pull/4009)
* [[`5f66d66e84`](https://github.com/nodejs/node/commit/5f66d66e84)] - **test**: don't check the # of chunks in test-http-1.0 (Santiago Gimeno) [#3961](https://github.com/nodejs/node/pull/3961)
* [[`355edf585b`](https://github.com/nodejs/node/commit/355edf585b)] - **test**: fix cluster-worker-isdead (Santiago Gimeno) [#3954](https://github.com/nodejs/node/pull/3954)
* [[`4e46e04002`](https://github.com/nodejs/node/commit/4e46e04002)] - **test**: add test for repl.defineCommand() (Bryan English) [#3908](https://github.com/nodejs/node/pull/3908)
* [[`4ea1a69c53`](https://github.com/nodejs/node/commit/4ea1a69c53)] - **test**: mark test flaky on FreeBSD (Rich Trott) [#4016](https://github.com/nodejs/node/pull/4016)
* [[`05b64c11f5`](https://github.com/nodejs/node/commit/05b64c11f5)] - **test**: mark cluster-net-send test flaky on windows (Rich Trott) [#4006](https://github.com/nodejs/node/pull/4006)
* [[`695015579b`](https://github.com/nodejs/node/commit/695015579b)] - **test**: remove flaky designation from ls-no-sslv3 (Rich Trott) [#3620](https://github.com/nodejs/node/pull/3620)
* [[`abbd87b273`](https://github.com/nodejs/node/commit/abbd87b273)] - **test**: mark fork regression test flaky on windows (Rich Trott) [#4005](https://github.com/nodejs/node/pull/4005)
* [[`38ba152a7a`](https://github.com/nodejs/node/commit/38ba152a7a)] - **test**: skip test if in FreeBSD jail (Rich Trott) [#3995](https://github.com/nodejs/node/pull/3995)
* [[`cc24f0ea58`](https://github.com/nodejs/node/commit/cc24f0ea58)] - **test**: fix test-domain-exit-dispose-again (Julien Gilli) [#3990](https://github.com/nodejs/node/pull/3990)
* [[`b2f1014d26`](https://github.com/nodejs/node/commit/b2f1014d26)] - **test**: remove flaky status for cluster test (Rich Trott) [#3975](https://github.com/nodejs/node/pull/3975)
* [[`e66794fd30`](https://github.com/nodejs/node/commit/e66794fd30)] - **test**: address flaky test-http-client-timeout-event (Rich Trott) [#3968](https://github.com/nodejs/node/pull/3968)
* [[`5a2727421a`](https://github.com/nodejs/node/commit/5a2727421a)] - **test**: retry on smartos if ECONNREFUSED (Rich Trott) [#3941](https://github.com/nodejs/node/pull/3941)
* [[`dbc85a275c`](https://github.com/nodejs/node/commit/dbc85a275c)] - **test**: avoid test timeouts on rpi (Stefan Budeanu) [#3902](https://github.com/nodejs/node/pull/3902)
* [[`b9d7378d20`](https://github.com/nodejs/node/commit/b9d7378d20)] - **test**: fix flaky test-child-process-spawnsync-input (Rich Trott) [#3889](https://github.com/nodejs/node/pull/3889)
* [[`cca216a034`](https://github.com/nodejs/node/commit/cca216a034)] - **test**: move test-specific function out of common (Rich Trott) [#3871](https://github.com/nodejs/node/pull/3871)
* [[`fb8df8d6c2`](https://github.com/nodejs/node/commit/fb8df8d6c2)] - **test**: module loading error fix solaris #3798 (fansworld-claudio) [#3855](https://github.com/nodejs/node/pull/3855)
* [[`9ea6bc1e0f`](https://github.com/nodejs/node/commit/9ea6bc1e0f)] - **test**: skip test if FreeBSD jail will break it (Rich Trott) [#3839](https://github.com/nodejs/node/pull/3839)
* [[`150f126618`](https://github.com/nodejs/node/commit/150f126618)] - **test**: fix flaky SmartOS test (Rich Trott) [#3830](https://github.com/nodejs/node/pull/3830)
* [[`603a6f5405`](https://github.com/nodejs/node/commit/603a6f5405)] - **test**: run pipeline flood test in parallel (Rich Trott) [#3811](https://github.com/nodejs/node/pull/3811)
* [[`4a26f74ee3`](https://github.com/nodejs/node/commit/4a26f74ee3)] - **test**: skip/replace weak crypto tests in FIPS mode (Stefan Budeanu) [#3757](https://github.com/nodejs/node/pull/3757)
* [[`3f9562b6bd`](https://github.com/nodejs/node/commit/3f9562b6bd)] - **test**: stronger crypto in test fixtures (Stefan Budeanu) [#3759](https://github.com/nodejs/node/pull/3759)
* [[`1f83eebec5`](https://github.com/nodejs/node/commit/1f83eebec5)] - **test**: increase crypto strength for FIPS standard (Stefan Budeanu) [#3758](https://github.com/nodejs/node/pull/3758)
* [[`7c5fbf7850`](https://github.com/nodejs/node/commit/7c5fbf7850)] - **test**: add hasFipsCrypto to test/common.js (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`f30214f135`](https://github.com/nodejs/node/commit/f30214f135)] - **test**: add test for invalid DSA key size (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`9a6c9faafb`](https://github.com/nodejs/node/commit/9a6c9faafb)] - **test**: numeric flags to fs.open (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`93d1d3cfcd`](https://github.com/nodejs/node/commit/93d1d3cfcd)] - **test**: refactor test-http-pipeline-flood (Rich Trott) [#3636](https://github.com/nodejs/node/pull/3636)
* [[`6c23f67504`](https://github.com/nodejs/node/commit/6c23f67504)] - **test**: fix flaky test test-http-pipeline-flood (Devin Nakamura) [#3636](https://github.com/nodejs/node/pull/3636)
* [[`4e5cae4360`](https://github.com/nodejs/node/commit/4e5cae4360)] - **test**: use really invalid hostname (Sakthipriyan Vairamani) [#3711](https://github.com/nodejs/node/pull/3711)
* [[`da189f793b`](https://github.com/nodejs/node/commit/da189f793b)] - **test**: Fix test-cluster-worker-exit.js for AIX (Imran Iqbal) [#3666](https://github.com/nodejs/node/pull/3666)
* [[`7b4194a863`](https://github.com/nodejs/node/commit/7b4194a863)] - **test**: fix test-module-loading-error for musl (Hugues Malphettes) [#3657](https://github.com/nodejs/node/pull/3657)
* [[`3dc52e99df`](https://github.com/nodejs/node/commit/3dc52e99df)] - **test**: fix test-net-persistent-keepalive for AIX (Imran Iqbal) [#3646](https://github.com/nodejs/node/pull/3646)
* [[`0e8eb66a78`](https://github.com/nodejs/node/commit/0e8eb66a78)] - **test**: fix path to module for repl test on Windows (Michael Cornacchia) [#3608](https://github.com/nodejs/node/pull/3608)
* [[`3aecbc86d2`](https://github.com/nodejs/node/commit/3aecbc86d2)] - **test**: add test-zlib-flush-drain (Myles Borins) [#3534](https://github.com/nodejs/node/pull/3534)
* [[`542d05cbe1`](https://github.com/nodejs/node/commit/542d05cbe1)] - **test**: enhance fs-watch-recursive test (Sakthipriyan Vairamani) [#2599](https://github.com/nodejs/node/pull/2599)
* [[`0eb0119d64`](https://github.com/nodejs/node/commit/0eb0119d64)] - **tls**: Use SHA1 for sessionIdContext in FIPS mode (Stefan Budeanu) [#3755](https://github.com/nodejs/node/pull/3755)
* [[`c10c08604c`](https://github.com/nodejs/node/commit/c10c08604c)] - **tls**: remove util and calls to util.format (Myles Borins) [#3456](https://github.com/nodejs/node/pull/3456)
* [[`a558a570c0`](https://github.com/nodejs/node/commit/a558a570c0)] - **util**: use regexp instead of str.replace().join() (qinjia) [#3689](https://github.com/nodejs/node/pull/3689)
* [[`47bb94a0c3`](https://github.com/nodejs/node/commit/47bb94a0c3)] - **zlib**: only apply drain listener if given callback (Craig Cavalier) [#3534](https://github.com/nodejs/node/pull/3534)
* [[`4733a60158`](https://github.com/nodejs/node/commit/4733a60158)] - **zlib**: pass kind to recursive calls to flush (Myles Borins) [#3534](https://github.com/nodejs/node/pull/3534)

## 2015-12-16, Version 5.3.0 (Stable), @cjihrig

### Notable changes

* **buffer**:
  - `Buffer.prototype.includes()` has been added to keep parity with TypedArrays. (Alexander Martin) [#3567](https://github.com/nodejs/node/pull/3567).
* **domains**:
  - Fix handling of uncaught exceptions. (Julien Gilli) [#3654](https://github.com/nodejs/node/pull/3654).
* **https**:
  - Added support for disabling session caching. (Fedor Indutny) [#4252](https://github.com/nodejs/node/pull/4252).
* **repl**:
  - Allow third party modules to be imported using `require()`. This corrects a regression from 5.2.0. (Ben Noordhuis) [#4215](https://github.com/nodejs/node/pull/4215).
* **deps**:
  - Upgrade libuv to 1.8.0. (Saúl Ibarra Corretgé) [#4276](https://github.com/nodejs/node/pull/4276).


### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* Unicode characters in filesystem paths are not handled consistently across platforms or Node.js APIs. See [#2088](https://github.com/nodejs/node/issues/2088), [#3401](https://github.com/nodejs/node/issues/3401) and [#3519](https://github.com/nodejs/node/issues/3519).

### Commits

* [[`6ca5ea3860`](https://github.com/nodejs/node/commit/6ca5ea3860)] - 2015-12-09, Version 5.2.0 (Stable) (Rod Vagg) [#4181](https://github.com/nodejs/node/pull/4181)
* [[`da5cdc2207`](https://github.com/nodejs/node/commit/da5cdc2207)] - **assert**: accommodate ES6 classes that extend Error (Rich Trott) [#4166](https://github.com/nodejs/node/pull/4166)
* [[`67e181986a`](https://github.com/nodejs/node/commit/67e181986a)] - **(SEMVER-MINOR)** **buffer**: add includes() for parity with TypedArray (Alexander Martin) [#3567](https://github.com/nodejs/node/pull/3567)
* [[`84dea1bd0c`](https://github.com/nodejs/node/commit/84dea1bd0c)] - **configure**: fix arm vfpv2 (Jörg Krause) [#4203](https://github.com/nodejs/node/pull/4203)
* [[`a7f5dfd14c`](https://github.com/nodejs/node/commit/a7f5dfd14c)] - **configure**: use \_\_ARM\_ARCH to determine arm version (João Reis) [#4123](https://github.com/nodejs/node/pull/4123)
* [[`0e3912be0b`](https://github.com/nodejs/node/commit/0e3912be0b)] - **configure**: respect CC\_host in host arch detection (João Reis) [#4117](https://github.com/nodejs/node/pull/4117)
* [[`69b94ec55c`](https://github.com/nodejs/node/commit/69b94ec55c)] - **deps**: upgrade libuv to 1.8.0 (Saúl Ibarra Corretgé) [#4276](https://github.com/nodejs/node/pull/4276)
* [[`a8854e5b59`](https://github.com/nodejs/node/commit/a8854e5b59)] - **doc**: document the cache parameter for fs.realpathSync (Jackson Tian) [#4285](https://github.com/nodejs/node/pull/4285)
* [[`9e1b7aa874`](https://github.com/nodejs/node/commit/9e1b7aa874)] - **doc**: document backlog for server.listen() variants (Jan Schär) [#4025](https://github.com/nodejs/node/pull/4025)
* [[`435d571f22`](https://github.com/nodejs/node/commit/435d571f22)] - **doc**: update AUTHORS list (Rod Vagg) [#4183](https://github.com/nodejs/node/pull/4183)
* [[`3b3061365a`](https://github.com/nodejs/node/commit/3b3061365a)] - **doc**: update irc channels: #node.js and #node-dev (Nelson Pecora) [#2743](https://github.com/nodejs/node/pull/2743)
* [[`9538fd02e5`](https://github.com/nodejs/node/commit/9538fd02e5)] - **doc**: clarify error events in HTTP module documentation (Lenny Markus) [#4275](https://github.com/nodejs/node/pull/4275)
* [[`c6efd535e4`](https://github.com/nodejs/node/commit/c6efd535e4)] - **doc**: fix improper http.get sample code (Hideki Yamamura) [#4263](https://github.com/nodejs/node/pull/4263)
* [[`498c9adb08`](https://github.com/nodejs/node/commit/498c9adb08)] - **doc**: add CTC meeting minutes 2015-10-28 (Rod Vagg) [#3661](https://github.com/nodejs/node/pull/3661)
* [[`671347cf13`](https://github.com/nodejs/node/commit/671347cf13)] - **doc**: fixup socket.remoteAddress (Arthur Gautier) [#4198](https://github.com/nodejs/node/pull/4198)
* [[`f050cab3d8`](https://github.com/nodejs/node/commit/f050cab3d8)] - **doc**: copyedit console doc (Rich Trott) [#4225](https://github.com/nodejs/node/pull/4225)
* [[`1a21a5368b`](https://github.com/nodejs/node/commit/1a21a5368b)] - **doc**: move description of 'equals' method to right place (janriemer) [#4227](https://github.com/nodejs/node/pull/4227)
* [[`9a9c5259bf`](https://github.com/nodejs/node/commit/9a9c5259bf)] - **doc**: Fixing broken links to the v8 wiki (Tom Gallacher) [#4241](https://github.com/nodejs/node/pull/4241)
* [[`37ed05b8c1`](https://github.com/nodejs/node/commit/37ed05b8c1)] - **doc**: copyedit child_process doc (Rich Trott) [#4188](https://github.com/nodejs/node/pull/4188)
* [[`e47ae5808b`](https://github.com/nodejs/node/commit/e47ae5808b)] - **doc**: copyedit buffer doc (Rich Trott) [#4187](https://github.com/nodejs/node/pull/4187)
* [[`70fb06a90b`](https://github.com/nodejs/node/commit/70fb06a90b)] - **doc**: clarify assert.fail doc (Rich Trott) [#4186](https://github.com/nodejs/node/pull/4186)
* [[`e3187cc81e`](https://github.com/nodejs/node/commit/e3187cc81e)] - **doc**: copyedit addons doc (Rich Trott) [#4185](https://github.com/nodejs/node/pull/4185)
* [[`931ab967ff`](https://github.com/nodejs/node/commit/931ab967ff)] - **doc**: add calvinmetcalf to collaborators (Calvin Metcalf) [#4218](https://github.com/nodejs/node/pull/4218)
* [[`01ce23148b`](https://github.com/nodejs/node/commit/01ce23148b)] - **doc**: add mcollina to collaborators (Matteo Collina) [#4220](https://github.com/nodejs/node/pull/4220)
* [[`bd8753aabf`](https://github.com/nodejs/node/commit/bd8753aabf)] - **doc**: add rmg to collaborators (Ryan Graham) [#4219](https://github.com/nodejs/node/pull/4219)
* [[`73a9a6fc92`](https://github.com/nodejs/node/commit/73a9a6fc92)] - **doc**: harmonize description of `ca` argument (Ben Noordhuis) [#4213](https://github.com/nodejs/node/pull/4213)
* [[`dfc8bedbc5`](https://github.com/nodejs/node/commit/dfc8bedbc5)] - **doc**: change references from node to Node.js (Roman Klauke) [#4177](https://github.com/nodejs/node/pull/4177)
* [[`7a518788e9`](https://github.com/nodejs/node/commit/7a518788e9)] - **doc, test**: symbols as event names (Bryan English) [#4151](https://github.com/nodejs/node/pull/4151)
* [[`425a3545d2`](https://github.com/nodejs/node/commit/425a3545d2)] - **(SEMVER-MINOR)** **domains**: fix handling of uncaught exceptions (Julien Gilli) [#3654](https://github.com/nodejs/node/pull/3654)
* [[`acef181fde`](https://github.com/nodejs/node/commit/acef181fde)] - **(SEMVER-MINOR)** **https**: support disabling session caching (Fedor Indutny) [#4252](https://github.com/nodejs/node/pull/4252)
* [[`2a60e2ad71`](https://github.com/nodejs/node/commit/2a60e2ad71)] - **module,src**: do not wrap modules with -1 lineOffset (cjihrig) [#4298](https://github.com/nodejs/node/pull/4298)
* [[`d3c498b1b7`](https://github.com/nodejs/node/commit/d3c498b1b7)] - **node**: remove unused variables in AppendExceptionLine (Yazhong Liu) [#4264](https://github.com/nodejs/node/pull/4264)
* [[`aad6b9f0eb`](https://github.com/nodejs/node/commit/aad6b9f0eb)] - **repl**: display error message when loading directory (Prince J Wesley) [#4170](https://github.com/nodejs/node/pull/4170)
* [[`213ede6cee`](https://github.com/nodejs/node/commit/213ede6cee)] - **repl**: fix require('3rdparty') regression (Ben Noordhuis) [#4215](https://github.com/nodejs/node/pull/4215)
* [[`f176b31e74`](https://github.com/nodejs/node/commit/f176b31e74)] - **src**: remove \_\_builtin\_bswap16 call (Ben Noordhuis) [#4290](https://github.com/nodejs/node/pull/4290)
* [[`ce2471673f`](https://github.com/nodejs/node/commit/ce2471673f)] - **src**: remove unused BITS_PER_LONG macro (Ben Noordhuis) [#4290](https://github.com/nodejs/node/pull/4290)
* [[`b799a74709`](https://github.com/nodejs/node/commit/b799a74709)] - **src**: fix line numbers on core errors (cjihrig) [#4254](https://github.com/nodejs/node/pull/4254)
* [[`c311b61430`](https://github.com/nodejs/node/commit/c311b61430)] - **src**: fix deprecation message for ErrnoException (Martin von Gagern) [#4269](https://github.com/nodejs/node/pull/4269)
* [[`2859f9ef92`](https://github.com/nodejs/node/commit/2859f9ef92)] - **test**: fix debug-port-cluster flakiness (Ben Noordhuis) [#4310](https://github.com/nodejs/node/pull/4310)
* [[`cb0b4a6bc0`](https://github.com/nodejs/node/commit/cb0b4a6bc0)] - **test**: add test for debugging one line files (cjihrig) [#4298](https://github.com/nodejs/node/pull/4298)
* [[`0b9c3a30d6`](https://github.com/nodejs/node/commit/0b9c3a30d6)] - **test**: add test for tls.parseCertString (Evan Lucas) [#4283](https://github.com/nodejs/node/pull/4283)
* [[`7598ed6cc0`](https://github.com/nodejs/node/commit/7598ed6cc0)] - **test**: parallelize test-repl-persistent-history (Jeremiah Senkpiel) [#4247](https://github.com/nodejs/node/pull/4247)
* [[`668449ad14`](https://github.com/nodejs/node/commit/668449ad14)] - **test**: use regular timeout times for ARMv8 (Jeremiah Senkpiel) [#4248](https://github.com/nodejs/node/pull/4248)
* [[`23e7703c85`](https://github.com/nodejs/node/commit/23e7703c85)] - **test**: fix http-many-ended-pipelines flakiness (Santiago Gimeno) [#4041](https://github.com/nodejs/node/pull/4041)
* [[`3b94991bda`](https://github.com/nodejs/node/commit/3b94991bda)] - **test**: fix tls-inception flakiness (Santiago Gimeno) [#4195](https://github.com/nodejs/node/pull/4195)
* [[`86a3bd09b0`](https://github.com/nodejs/node/commit/86a3bd09b0)] - **test**: fix tls-inception (Santiago Gimeno) [#4195](https://github.com/nodejs/node/pull/4195)
* [[`1e89830a11`](https://github.com/nodejs/node/commit/1e89830a11)] - **test**: don't assume openssl s\_client supports -ssl3 (Ben Noordhuis) [#4204](https://github.com/nodejs/node/pull/4204)
* [[`c5b4f6bc99`](https://github.com/nodejs/node/commit/c5b4f6bc99)] - **(SEMVER-MINOR)** **tls**: introduce `secureContext` for `tls.connect` (Fedor Indutny) [#4246](https://github.com/nodejs/node/pull/4246)
* [[`e0bb118a1d`](https://github.com/nodejs/node/commit/e0bb118a1d)] - **tls_wrap**: inherit from the `AsyncWrap` first (Fedor Indutny) [#4268](https://github.com/nodejs/node/pull/4268)
* [[`d63cceeb10`](https://github.com/nodejs/node/commit/d63cceeb10)] - **tools**: add .editorconfig (ronkorving) [#2993](https://github.com/nodejs/node/pull/2993)
* [[`4b267df93e`](https://github.com/nodejs/node/commit/4b267df93e)] - **udp**: remove a needless instanceof Buffer check (ronkorving) [#4301](https://github.com/nodejs/node/pull/4301)

## 2015-12-09, Version 5.2.0 (Stable), @rvagg

### Notable changes

* **build**:
  - Add support for Intel's VTune JIT profiling when compiled with `--enable-vtune-profiling`. For more information about VTune, see <https://software.intel.com/en-us/node/544211>. (Chunyang Dai) [#3785](https://github.com/nodejs/node/pull/3785).
  - Properly enable V8 snapshots by default. Due to a configuration error, snapshots have been kept off by default when the intention is for the feature to be enabled. (Fedor Indutny) [#3962](https://github.com/nodejs/node/pull/3962).
* **crypto**:
  - Simplify use of ECDH (Elliptic Curve Diffie-Hellman) objects (created via `crypto.createECDH(curve_name)`) with private keys that are not dynamically generated via `generateKeys()`. The public key is now computed when explicitly setting a private key. Added validity checks to reduce the possibility of computing weak or invalid shared secrets. Also, deprecated the `setPublicKey()` method for ECDH objects as its usage is unnecessary and can lead to inconsistent state. (Michael Ruddy) [#3511](https://github.com/nodejs/node/pull/3511).
  - Update root certificates from the current list stored maintained by Mozilla NSS. (Ben Noordhuis) [#3951](https://github.com/nodejs/node/pull/3951).
  - Multiple CA certificates can now be passed with the `ca` option to TLS methods as an array of strings or in a single new-line separated string. (Ben Noordhuis) [#4099](https://github.com/nodejs/node/pull/4099)
* **tools**: Include a tick processor in core, exposed via the `--prof-process` command-line argument which can be used to process V8 profiling output files generated when using the `--prof` command-line argument. (Matt Loring) [#4021](https://github.com/nodejs/node/pull/4021).


### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* Unicode characters in filesystem paths are not handled consistently across platforms or Node.js APIs. See [#2088](https://github.com/nodejs/node/issues/2088), [#3401](https://github.com/nodejs/node/issues/3401) and [#3519](https://github.com/nodejs/node/issues/3519).

### Commits

* [[`08a3f29fd4`](https://github.com/nodejs/node/commit/08a3f29fd4)] - **buffer**: fix range checking for slowToString (Matt Loring) [#4019](https://github.com/nodejs/node/pull/4019)
* [[`e3a8e8bba4`](https://github.com/nodejs/node/commit/e3a8e8bba4)] - **buffer**: Prevent Buffer constructor deopt (Bryce Baril) [#4158](https://github.com/nodejs/node/pull/4158)
* [[`0e18e68324`](https://github.com/nodejs/node/commit/0e18e68324)] - **buffer**: fix writeInt{B,L}E for some neg values (Peter A. Bigot) [#3994](https://github.com/nodejs/node/pull/3994)
* [[`ab5b529dd2`](https://github.com/nodejs/node/commit/ab5b529dd2)] - **buffer**: default to UTF8 in byteLength() (Tom Gallacher) [#4010](https://github.com/nodejs/node/pull/4010)
* [[`fcf0e8ebdf`](https://github.com/nodejs/node/commit/fcf0e8ebdf)] - **buffer**: move checkFloat from lib into src (Matt Loring) [#3763](https://github.com/nodejs/node/pull/3763)
* [[`12649f4496`](https://github.com/nodejs/node/commit/12649f4496)] - **build**: add "--partly-static" build options (Super Zheng) [#4152](https://github.com/nodejs/node/pull/4152)
* [[`a76d788119`](https://github.com/nodejs/node/commit/a76d788119)] - **build**: update signtool description, add url (Rod Vagg) [#4011](https://github.com/nodejs/node/pull/4011)
* [[`ed255abdc1`](https://github.com/nodejs/node/commit/ed255abdc1)] - **(SEMVER-MINOR)** **build,src**: add Intel Vtune profiling support (Chunyang Dai) [#3785](https://github.com/nodejs/node/pull/3785)
* [[`7793c364fc`](https://github.com/nodejs/node/commit/7793c364fc)] - **child_process**: flush consuming streams (Dave) [#4071](https://github.com/nodejs/node/pull/4071)
* [[`f29c5d6e70`](https://github.com/nodejs/node/commit/f29c5d6e70)] - **configure**: `v8_use_snapshot` should be `true` (Fedor Indutny) [#3962](https://github.com/nodejs/node/pull/3962)
* [[`da5ac55c83`](https://github.com/nodejs/node/commit/da5ac55c83)] - **(SEMVER-MINOR)** **crypto**: simplify using pre-existing keys with ECDH (Michael Ruddy) [#3511](https://github.com/nodejs/node/pull/3511)
* [[`cfc97641ee`](https://github.com/nodejs/node/commit/cfc97641ee)] - **crypto**: fix native module compilation with FIPS (Stefan Budeanu) [#4023](https://github.com/nodejs/node/pull/4023)
* [[`b81b45dabd`](https://github.com/nodejs/node/commit/b81b45dabd)] - **crypto**: update root certificates (Ben Noordhuis) [#3951](https://github.com/nodejs/node/pull/3951)
* [[`def681a07e`](https://github.com/nodejs/node/commit/def681a07e)] - **crypto**: disable crypto.createCipher in FIPS mode (Stefan Budeanu) [#3754](https://github.com/nodejs/node/pull/3754)
* [[`ce423f3624`](https://github.com/nodejs/node/commit/ce423f3624)] - **debugger**: introduce exec method for debugger (Jackson Tian)
* [[`99fd1ec28d`](https://github.com/nodejs/node/commit/99fd1ec28d)] - **deps**: backport 819b40a from V8 upstream (Michaël Zasso) [#3937](https://github.com/nodejs/node/pull/3937)
* [[`82252b2a17`](https://github.com/nodejs/node/commit/82252b2a17)] - **doc**: add brief Node.js overview to README (wurde) [#4174](https://github.com/nodejs/node/pull/4174)
* [[`634c5f1f81`](https://github.com/nodejs/node/commit/634c5f1f81)] - **doc**: url.format - true slash postfix behaviour (fansworld-claudio) [#4119](https://github.com/nodejs/node/pull/4119)
* [[`6f957a70d8`](https://github.com/nodejs/node/commit/6f957a70d8)] - **doc**: s/node.js/Node.js in readme (Rod Vagg) [#3998](https://github.com/nodejs/node/pull/3998)
* [[`0cd4a52392`](https://github.com/nodejs/node/commit/0cd4a52392)] - **doc**: improve child_process.markdown wording (yorkie) [#4138](https://github.com/nodejs/node/pull/4138)
* [[`fd5ed6888d`](https://github.com/nodejs/node/commit/fd5ed6888d)] - **doc**: add JungMinu to collaborators (Minwoo Jung) [#4143](https://github.com/nodejs/node/pull/4143)
* [[`fa0cdf75d9`](https://github.com/nodejs/node/commit/fa0cdf75d9)] - **doc**: add iarna to collaborators (Rebecca Turner) [#4144](https://github.com/nodejs/node/pull/4144)
* [[`424eb962b1`](https://github.com/nodejs/node/commit/424eb962b1)] - **doc**: add zkat to collaborators (Kat Marchán) [#4142](https://github.com/nodejs/node/pull/4142)
* [[`85b601224b`](https://github.com/nodejs/node/commit/85b601224b)] - **doc**: add HTTP working group (James M Snell) [#3919](https://github.com/nodejs/node/pull/3919)
* [[`f4164bd8df`](https://github.com/nodejs/node/commit/f4164bd8df)] - **doc**: update links to use https where possible (jpersson) [#4054](https://github.com/nodejs/node/pull/4054)
* [[`3169eed1e3`](https://github.com/nodejs/node/commit/3169eed1e3)] - **doc**: add links and backticks around names (jpersson) [#4054](https://github.com/nodejs/node/pull/4054)
* [[`f3417e2574`](https://github.com/nodejs/node/commit/f3417e2574)] - **doc**: standardize references to node.js in docs (Scott Buchanan) [#4136](https://github.com/nodejs/node/pull/4136)
* [[`95dd60c657`](https://github.com/nodejs/node/commit/95dd60c657)] - **doc**: reword https.Agent example text (Jan Krems) [#4075](https://github.com/nodejs/node/pull/4075)
* [[`c61237d3ea`](https://github.com/nodejs/node/commit/c61237d3ea)] - **doc**: fix internal link to child.send() (Luigi Pinca) [#4089](https://github.com/nodejs/node/pull/4089)
* [[`aaeced915e`](https://github.com/nodejs/node/commit/aaeced915e)] - **doc**: fix the exception description (yorkie) [#3658](https://github.com/nodejs/node/pull/3658)
* [[`a2b7596ac0`](https://github.com/nodejs/node/commit/a2b7596ac0)] - **doc**: fix color of linked code blocks (jpersson) [#4068](https://github.com/nodejs/node/pull/4068)
* [[`f3c50f5fb5`](https://github.com/nodejs/node/commit/f3c50f5fb5)] - **doc**: fix rare case of misaligned columns (Roman Reiss) [#3948](https://github.com/nodejs/node/pull/3948)
* [[`f0a2e2cdec`](https://github.com/nodejs/node/commit/f0a2e2cdec)] - **doc**: message.header duplication correction (Bryan English) [#3997](https://github.com/nodejs/node/pull/3997)
* [[`b1dfa8bebb`](https://github.com/nodejs/node/commit/b1dfa8bebb)] - **doc**: fix typo in README (Rich Trott) [#4000](https://github.com/nodejs/node/pull/4000)
* [[`4602e01221`](https://github.com/nodejs/node/commit/4602e01221)] - **doc**: replace sane with reasonable (Lewis Cowper) [#3980](https://github.com/nodejs/node/pull/3980)
* [[`4849a54386`](https://github.com/nodejs/node/commit/4849a54386)] - **doc**: Adding best practises for crypto.pbkdf2 (Tom Gallacher) [#3290](https://github.com/nodejs/node/pull/3290)
* [[`77251d99de`](https://github.com/nodejs/node/commit/77251d99de)] - **doc**: numeric flags to fs.open (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`f4ca007b42`](https://github.com/nodejs/node/commit/f4ca007b42)] - **doc**: clarify that fs streams expect blocking fd (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`26eeae8016`](https://github.com/nodejs/node/commit/26eeae8016)] - **doc**: fix broken references (Alexander Gromnitsky) [#3944](https://github.com/nodejs/node/pull/3944)
* [[`f90227b0e8`](https://github.com/nodejs/node/commit/f90227b0e8)] - **doc**: move fs.existsSync() deprecation message (Martin Forsberg) [#3942](https://github.com/nodejs/node/pull/3942)
* [[`bbcb2a2e65`](https://github.com/nodejs/node/commit/bbcb2a2e65)] - **doc**: clarify module loading behavior (cjihrig) [#3920](https://github.com/nodejs/node/pull/3920)
* [[`0997178037`](https://github.com/nodejs/node/commit/0997178037)] - **doc**: add reference for buffer.inspect() (cjihrig) [#3921](https://github.com/nodejs/node/pull/3921)
* [[`6c16c40283`](https://github.com/nodejs/node/commit/6c16c40283)] - **doc**: clarify v5.1.1 notable items (Rod Vagg) [#4156](https://github.com/nodejs/node/pull/4156)
* [[`4c8800c2de`](https://github.com/nodejs/node/commit/4c8800c2de)] - **fs,doc**: use `target` instead of `destination` (yorkie) [#3912](https://github.com/nodejs/node/pull/3912)
* [[`1f0e8dca8e`](https://github.com/nodejs/node/commit/1f0e8dca8e)] - **installer**: install the tick processor (Matt Loring) [#3032](https://github.com/nodejs/node/pull/3032)
* [[`e8e4e0718b`](https://github.com/nodejs/node/commit/e8e4e0718b)] - **meta**: remove use of profanity in source (Myles Borins) [#4122](https://github.com/nodejs/node/pull/4122)
* [[`13834caa28`](https://github.com/nodejs/node/commit/13834caa28)] - **module**: fix column offsets in errors (Tristian Flanagan) [#2867](https://github.com/nodejs/node/pull/2867)
* [[`8988e1e117`](https://github.com/nodejs/node/commit/8988e1e117)] - **module,repl**: remove repl require() hack (Ben Noordhuis) [#4026](https://github.com/nodejs/node/pull/4026)
* [[`baac81d95f`](https://github.com/nodejs/node/commit/baac81d95f)] - **net**: add local address/port for better errors (Jan Schär) [#3946](https://github.com/nodejs/node/pull/3946)
* [[`12754c5dc3`](https://github.com/nodejs/node/commit/12754c5dc3)] - **net**: small code cleanup (Jan Schär) [#3943](https://github.com/nodejs/node/pull/3943)
* [[`8a5e4345fd`](https://github.com/nodejs/node/commit/8a5e4345fd)] - **node**: s/doNTCallbackX/nextTickCallbackWithXArgs/ (Rod Vagg) [#4167](https://github.com/nodejs/node/pull/4167)
* [[`0869ef3c55`](https://github.com/nodejs/node/commit/0869ef3c55)] - **(SEMVER-MINOR)** **repl**: allow leading period in multiline input (Zirak) [#3835](https://github.com/nodejs/node/pull/3835)
* [[`aaab108dfe`](https://github.com/nodejs/node/commit/aaab108dfe)] - **repl**: attach location info to syntax errors (cjihrig) [#4013](https://github.com/nodejs/node/pull/4013)
* [[`b08126dc9d`](https://github.com/nodejs/node/commit/b08126dc9d)] - **src**: refactor vcbuild configure args creation (Rod Vagg) [#3399](https://github.com/nodejs/node/pull/3399)
* [[`da3137d0c5`](https://github.com/nodejs/node/commit/da3137d0c5)] - **src**: don't print garbage errors (cjihrig) [#4112](https://github.com/nodejs/node/pull/4112)
* [[`9e9346fa32`](https://github.com/nodejs/node/commit/9e9346fa32)] - **src**: use GetCurrentProcessId() for process.pid (Ben Noordhuis) [#4163](https://github.com/nodejs/node/pull/4163)
* [[`d969c0965c`](https://github.com/nodejs/node/commit/d969c0965c)] - **src**: define Is* util functions with macros (cjihrig) [#4118](https://github.com/nodejs/node/pull/4118)
* [[`458facdf66`](https://github.com/nodejs/node/commit/458facdf66)] - **src**: define getpid() based on OS (cjihrig) [#4146](https://github.com/nodejs/node/pull/4146)
* [[`7e18f2ec62`](https://github.com/nodejs/node/commit/7e18f2ec62)] - **(SEMVER-MINOR)** **src**: add BE support to StringBytes::Encode() (Bryon Leung) [#3410](https://github.com/nodejs/node/pull/3410)
* [[`756ab9caad`](https://github.com/nodejs/node/commit/756ab9caad)] - **stream**: be less eager with readable flag (Brian White) [#4141](https://github.com/nodejs/node/pull/4141)
* [[`8f845ba28a`](https://github.com/nodejs/node/commit/8f845ba28a)] - **stream_wrap**: error if stream has StringDecoder (Fedor Indutny) [#4031](https://github.com/nodejs/node/pull/4031)
* [[`1c1af81ea0`](https://github.com/nodejs/node/commit/1c1af81ea0)] - **streams**: update .readable/.writable to false (Brian White) [#4083](https://github.com/nodejs/node/pull/4083)
* [[`1d50819c85`](https://github.com/nodejs/node/commit/1d50819c85)] - **test**: check range fix for slowToString (Sakthipriyan Vairamani) [#4019](https://github.com/nodejs/node/pull/4019)
* [[`0c2a0dc859`](https://github.com/nodejs/node/commit/0c2a0dc859)] - **test**: skip long path tests on non-Windows (Rafał Pocztarski) [#4116](https://github.com/nodejs/node/pull/4116)
* [[`8a60aa1303`](https://github.com/nodejs/node/commit/8a60aa1303)] - **test**: don't check the # of chunks in test-http-1.0 (Santiago Gimeno) [#3961](https://github.com/nodejs/node/pull/3961)
* [[`e84aeec883`](https://github.com/nodejs/node/commit/e84aeec883)] - **test**: mark test-cluster-shared-leak flaky (Rich Trott) [#4162](https://github.com/nodejs/node/pull/4162)
* [[`b3f3b2e157`](https://github.com/nodejs/node/commit/b3f3b2e157)] - **test**: fix cluster-worker-isdead (Santiago Gimeno) [#3954](https://github.com/nodejs/node/pull/3954)
* [[`da6be4d31a`](https://github.com/nodejs/node/commit/da6be4d31a)] - **test**: fix time resolution constraint (Gireesh Punathil) [#3981](https://github.com/nodejs/node/pull/3981)
* [[`9d16729b20`](https://github.com/nodejs/node/commit/9d16729b20)] - **test**: skip instead of fail when mem constrained (Michael Cornacchia) [#3697](https://github.com/nodejs/node/pull/3697)
* [[`be41eb751b`](https://github.com/nodejs/node/commit/be41eb751b)] - **test**: refactor test-http-exit-delay (Rich Trott) [#4055](https://github.com/nodejs/node/pull/4055)
* [[`4b43bf0385`](https://github.com/nodejs/node/commit/4b43bf0385)] - **test**: fix flaky test-net-socket-local-address (Rich Trott) [#4109](https://github.com/nodejs/node/pull/4109)
* [[`cb55c67a00`](https://github.com/nodejs/node/commit/cb55c67a00)] - **test**: improve cluster-disconnect-handles test (Brian White) [#4084](https://github.com/nodejs/node/pull/4084)
* [[`2b5b127e14`](https://github.com/nodejs/node/commit/2b5b127e14)] - **test**: fix cluster-disconnect-handles flakiness (Santiago Gimeno) [#4009](https://github.com/nodejs/node/pull/4009)
* [[`430264817b`](https://github.com/nodejs/node/commit/430264817b)] - **test**: add test for repl.defineCommand() (Bryan English) [#3908](https://github.com/nodejs/node/pull/3908)
* [[`22b0971222`](https://github.com/nodejs/node/commit/22b0971222)] - **test**: eliminate multicast test FreeBSD flakiness (Rich Trott) [#4042](https://github.com/nodejs/node/pull/4042)
* [[`c50003746b`](https://github.com/nodejs/node/commit/c50003746b)] - **test**: mark test flaky on FreeBSD (Rich Trott) [#4016](https://github.com/nodejs/node/pull/4016)
* [[`69c95bbdb7`](https://github.com/nodejs/node/commit/69c95bbdb7)] - **test**: move ArrayStream to common (cjihrig) [#4027](https://github.com/nodejs/node/pull/4027)
* [[`d94a70ec51`](https://github.com/nodejs/node/commit/d94a70ec51)] - **test**: fix test-domain-exit-dispose-again (Julien Gilli) [#3990](https://github.com/nodejs/node/pull/3990)
* [[`00b839a2b8`](https://github.com/nodejs/node/commit/00b839a2b8)] - **test**: use platform-based timeout for reliability (Rich Trott) [#4015](https://github.com/nodejs/node/pull/4015)
* [[`054a216b6f`](https://github.com/nodejs/node/commit/054a216b6f)] - **test**: mark cluster-net-send test flaky on windows (Rich Trott) [#4006](https://github.com/nodejs/node/pull/4006)
* [[`d0621c5649`](https://github.com/nodejs/node/commit/d0621c5649)] - **test**: mark fork regression test flaky on windows (Rich Trott) [#4005](https://github.com/nodejs/node/pull/4005)
* [[`19ed33df80`](https://github.com/nodejs/node/commit/19ed33df80)] - **test**: skip test if in FreeBSD jail (Rich Trott) [#3995](https://github.com/nodejs/node/pull/3995)
* [[`a863e8d667`](https://github.com/nodejs/node/commit/a863e8d667)] - **test**: remove flaky status for cluster test (Rich Trott) [#3975](https://github.com/nodejs/node/pull/3975)
* [[`dd0d15fc47`](https://github.com/nodejs/node/commit/dd0d15fc47)] - **test**: add TAP diagnostic message for retried tests (Rich Trott) [#3960](https://github.com/nodejs/node/pull/3960)
* [[`1fe4d30efc`](https://github.com/nodejs/node/commit/1fe4d30efc)] - **test**: retry on smartos if ECONNREFUSED (Rich Trott) [#3941](https://github.com/nodejs/node/pull/3941)
* [[`665a35d45e`](https://github.com/nodejs/node/commit/665a35d45e)] - **test**: address flaky test-http-client-timeout-event (Rich Trott) [#3968](https://github.com/nodejs/node/pull/3968)
* [[`f9fe0aee53`](https://github.com/nodejs/node/commit/f9fe0aee53)] - **test**: numeric flags to fs.open (Carl Lei) [#3641](https://github.com/nodejs/node/pull/3641)
* [[`54aafa17af`](https://github.com/nodejs/node/commit/54aafa17af)] - **test**: http complete list of non-concat headers (Bryan English) [#3930](https://github.com/nodejs/node/pull/3930)
* [[`788541b40c`](https://github.com/nodejs/node/commit/788541b40c)] - **test**: fix race condition in unrefd interval test (Michael Cornacchia) [#3550](https://github.com/nodejs/node/pull/3550)
* [[`e129d83996`](https://github.com/nodejs/node/commit/e129d83996)] - **test**: skip/replace weak crypto tests in FIPS mode (Stefan Budeanu) [#3757](https://github.com/nodejs/node/pull/3757)
* [[`bc27379453`](https://github.com/nodejs/node/commit/bc27379453)] - **test**: avoid test timeouts on rpi (Stefan Budeanu) [#3902](https://github.com/nodejs/node/pull/3902)
* [[`272732e76b`](https://github.com/nodejs/node/commit/272732e76b)] - **test**: fix flaky test-child-process-spawnsync-input (Rich Trott) [#3889](https://github.com/nodejs/node/pull/3889)
* [[`781f8c0d1e`](https://github.com/nodejs/node/commit/781f8c0d1e)] - **test**: add OS X to module loading error test (Evan Lucas) [#3901](https://github.com/nodejs/node/pull/3901)
* [[`f99c6363de`](https://github.com/nodejs/node/commit/f99c6363de)] - **test**: module loading error fix solaris #3798 (fansworld-claudio) [#3855](https://github.com/nodejs/node/pull/3855)
* [[`1279adc756`](https://github.com/nodejs/node/commit/1279adc756)] - **timers**: optimize callback call: bind -> arrow (Andrei Sedoi) [#4038](https://github.com/nodejs/node/pull/4038)
* [[`80f7f65464`](https://github.com/nodejs/node/commit/80f7f65464)] - **(SEMVER-MINOR)** **tls**: support reading multiple cas from one input (Ben Noordhuis) [#4099](https://github.com/nodejs/node/pull/4099)
* [[`939f305d56`](https://github.com/nodejs/node/commit/939f305d56)] - **tls_wrap**: slice buffer properly in `ClearOut` (Fedor Indutny) [#4184](https://github.com/nodejs/node/pull/4184)
* [[`6d4a03d3d2`](https://github.com/nodejs/node/commit/6d4a03d3d2)] - **(SEMVER-MINOR)** **tools**: list missing whitespace/if-one-line cpplint (Ben Noordhuis) [#4099](https://github.com/nodejs/node/pull/4099)
* [[`1c1c1a0f2b`](https://github.com/nodejs/node/commit/1c1c1a0f2b)] - **(SEMVER-MINOR)** **tools**: add --prof-process flag to node binary (Matt Loring) [#4021](https://github.com/nodejs/node/pull/4021)
* [[`d7a7d3e6f7`](https://github.com/nodejs/node/commit/d7a7d3e6f7)] - **tools**: update certdata.txt (Ben Noordhuis) [#3951](https://github.com/nodejs/node/pull/3951)
* [[`1b434e0654`](https://github.com/nodejs/node/commit/1b434e0654)] - **util**: determine object types in C++ (cjihrig) [#4100](https://github.com/nodejs/node/pull/4100)
* [[`c93e2678f0`](https://github.com/nodejs/node/commit/c93e2678f0)] - **util**: fix constructor/instanceof checks (Brian White) [#3385](https://github.com/nodejs/node/pull/3385)
* [[`098a3113e1`](https://github.com/nodejs/node/commit/098a3113e1)] - **util**: move .decorateErrorStack to internal/util (Ben Noordhuis) [#4026](https://github.com/nodejs/node/pull/4026)
* [[`e68ea16c32`](https://github.com/nodejs/node/commit/e68ea16c32)] - **util**: add decorateErrorStack() (cjihrig) [#4013](https://github.com/nodejs/node/pull/4013)
* [[`c584c3e08f`](https://github.com/nodejs/node/commit/c584c3e08f)] - **util,src**: allow lookup of hidden values (cjihrig) [#3988](https://github.com/nodejs/node/pull/3988)

## 2015-12-04, Version 5.1.1 (Stable), @rvagg

### Notable changes

* **http**: Fix CVE-2015-8027, a bug whereby an HTTP socket may no longer have a parser associated with it but a pipelined request attempts to trigger a pause or resume on the non-existent parser, a potential denial-of-service vulnerability. (Fedor Indutny)
* **openssl**: Upgrade to 1.0.2e, containing fixes for:
  - CVE-2015-3193 "BN_mod_exp may produce incorrect results on x86_64", an attack may be possible against a Node.js TLS server using DHE key exchange. Details are available at <http://openssl.org/news/secadv/20151203.txt>.
  - CVE-2015-3194 "Certificate verify crash with missing PSS parameter", a potential denial-of-service vector for Node.js TLS servers using client certificate authentication; TLS clients are also impacted. Details are available at <http://openssl.org/news/secadv/20151203.txt>.
  (Shigeki Ohtsu) [#4134](https://github.com/nodejs/node/pull/4134)
* **v8**: Backport fix for CVE-2015-6764, a bug in `JSON.stringify()` that can result in out-of-bounds reads for arrays. (Ben Noordhuis)

### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* Unicode characters in filesystem paths are not handled consistently across platforms or Node.js APIs. See [#2088](https://github.com/nodejs/node/issues/2088), [#3401](https://github.com/nodejs/node/issues/3401) and [#3519](https://github.com/nodejs/node/issues/3519).

### Commits

* [[`678398f250`](https://github.com/nodejs/node/commit/678398f250)] - **deps**: backport a7e50a5 from upstream v8 (Ben Noordhuis)
* [[`76a552c938`](https://github.com/nodejs/node/commit/76a552c938)] - **deps**: backport 6df9a1d from upstream v8 (Ben Noordhuis)
* [[`533881f889`](https://github.com/nodejs/node/commit/533881f889)] - **deps**: upgrade openssl sources to 1.0.2e (Shigeki Ohtsu) [#4134](https://github.com/nodejs/node/pull/4134)
* [[`12e70fafd3`](https://github.com/nodejs/node/commit/12e70fafd3)] - **http**: fix pipeline regression (Fedor Indutny)

## 2015-12-04, Version 4.2.3 'Argon' (LTS), @rvagg

Security Update

### Notable changes

* **http**: Fix CVE-2015-8027, a bug whereby an HTTP socket may no longer have a parser associated with it but a pipelined request attempts to trigger a pause or resume on the non-existent parser, a potential denial-of-service vulnerability. (Fedor Indutny)
* **openssl**: Upgrade to 1.0.2e, containing fixes for:
  - CVE-2015-3193 "BN_mod_exp may produce incorrect results on x86_64", an attack may be possible against a Node.js TLS server using DHE key exchange. Details are available at <http://openssl.org/news/secadv/20151203.txt>.
  - CVE-2015-3194 "Certificate verify crash with missing PSS parameter", a potential denial-of-service vector for Node.js TLS servers using client certificate authentication; TLS clients are also impacted. Details are available at <http://openssl.org/news/secadv/20151203.txt>.
  (Shigeki Ohtsu) [#4134](https://github.com/nodejs/node/pull/4134)
* **v8**: Backport fix for CVE-2015-6764, a bug in `JSON.stringify()` that can result in out-of-bounds reads for arrays. (Ben Noordhuis)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`49bbd563be`](https://github.com/nodejs/node/commit/49bbd563be)] - **deps**: upgrade openssl sources to 1.0.2e (Shigeki Ohtsu) [#4134](https://github.com/nodejs/node/pull/4134)
* [[`9a063fd492`](https://github.com/nodejs/node/commit/9a063fd492)] - **deps**: backport a7e50a5 from upstream v8 (Ben Noordhuis)
* [[`07233206e9`](https://github.com/nodejs/node/commit/07233206e9)] - **deps**: backport 6df9a1d from upstream v8 (Ben Noordhuis)
* [[`1c8e6de78e`](https://github.com/nodejs/node/commit/1c8e6de78e)] - **http**: fix pipeline regression (Fedor Indutny)

## 2015-12-04, Version 0.12.9 (LTS), @rvagg

Security Update

### Notable changes

* http: Fix CVE-2015-8027, a bug whereby an HTTP socket may no longer have a parser associated with it but a pipelined request attempts to trigger a pause or resume on the non-existent parser, a potential denial-of-service vulnerability. (Fedor Indutny)
* openssl: Upgrade to 1.0.1q, fixes CVE-2015-3194 "Certificate verify crash with missing PSS parameter", a potential denial-of-service vector for Node.js TLS servers using client certificate authentication; TLS clients are also impacted. Details are available at <http://openssl.org/news/secadv/20151203.txt>. (Ben Noordhuis) https://github.com/nodejs/node/pull/4133

### Commits

* [8d24a14f2c] - deps: upgrade to openssl 1.0.1q (Ben Noordhuis) https://github.com/nodejs/node/pull/4133
* [dfc6f4a9af] - http: fix pipeline regression (Fedor Indutny)

## 2015-12-04, Version 0.10.41 (Maintenance), @rvagg

Security Update

### Notable changes

* build: Add support for Microsoft Visual Studio 2015
* npm: Upgrade to v1.4.29 from v1.4.28. A special one-off release as part of the strategy to get a version of npm into Node.js v0.10.x that works with the current registry (https://github.com/nodejs/LTS/issues/37). This version of npm prints out a banner each time it is run. The banner warns that the next standard release of Node.js v0.10.x will ship with a version of npm v2.
* openssl: Upgrade to 1.0.1q, containing fixes CVE-2015-3194 "Certificate verify crash with missing PSS parameter", a potential denial-of-service vector for Node.js TLS servers using client certificate authentication; TLS clients are also impacted. Details are available at <http://openssl.org/news/secadv/20151203.txt>. (Ben Noordhuis) https://github.com/nodejs/node/pull/4133

### Commits

* [16ca0779f5] - src/node.cc: fix build error without OpenSSL support (Jörg Krause) https://github.com/nodejs/node-v0.x-archive/pull/25862
* [c559c7911d] - build: backport tools/release.sh (Rod Vagg) https://github.com/nodejs/node/pull/3965
* [268d2b4637] - build: backport config for new CI infrastructure (Rod Vagg) https://github.com/nodejs/node/pull/3965
* [c88a0b26da] - build: update manifest to include Windows 10 (Lucien Greathouse) https://github.com/nodejs/node/pull/2838
* [8564a9f5f7] - build: gcc version detection on openSUSE Tumbleweed (Henrique Aparecido Lavezzo) https://github.com/nodejs/node-v0.x-archive/pull/25671
* [9c7bd6de56] - build: run-ci makefile rule (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [ffa1e1f31d] - build: support flaky tests in test-ci (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [100dd19e61] - build: support Jenkins via test-ci (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [ec861f6f90] - build: make release process easier for multi users (Julien Gilli) https://github.com/nodejs/node-v0.x-archive/pull/25638
* [d7ae79a452] - build,win: fix node.exe resource version (João Reis) https://github.com/nodejs/node/pull/3053
* [6ac47aa9f5] - build,win: try next MSVS version on failure (João Reis) https://github.com/nodejs/node/pull/2910
* [e669b27740] - crypto: replace rwlocks with simple mutexes (Ben Noordhuis) https://github.com/nodejs/node/pull/2723
* [ce0a48826e] - deps: upgrade to openssl 1.0.1q (Ben Noordhuis) https://github.com/nodejs/node/pull/4132
* [b68781e500] - deps: upgrade npm to 1.4.29 (Forrest L Norvell) https://github.com/nodejs/node/pull/3639
* [7cf0d9c1d9] - deps: fix openssl for MSVS 2015 (Andy Polyakov) https://github.com/nodejs/node-v0.x-archive/pull/25857
* [9ee8a14f9e] - deps: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) https://github.com/nodejs/node-v0.x-archive/pull/25857
* [a525c7244e] - deps: update gyp to 25ed9ac (João Reis) https://github.com/nodejs/node-v0.x-archive/pull/25857
* [6502160294] - dns: allow v8 to optimize lookup() (Brian White) https://github.com/nodejs/node-v0.x-archive/pull/8942
* [5d829a63ab] - doc: backport README.md (Rod Vagg) https://github.com/nodejs/node/pull/3965
* [62c8948109] - doc: fix Folders as Modules omission of index.json (Elan Shanker) https://github.com/nodejs/node-v0.x-archive/pull/8868
* [572663f303] - https: don't overwrite servername option (skenqbx) https://github.com/nodejs/node-v0.x-archive/pull/9368
* [75c84b2439] - test: add test for https agent servername option (skenqbx) https://github.com/nodejs/node-v0.x-archive/pull/9368
* [841a6dd264] - test: mark more tests as flaky (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25807
* [a7fee30da1] - test: mark test-tls-securepair-server as flaky (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25807
* [7df57703dd] - test: mark test-net-error-twice flaky on SmartOS (Julien Gilli) https://github.com/nodejs/node-v0.x-archive/pull/25760
* [e10892cccc] - test: make test-abort-fatal-error non flaky (Julien Gilli) https://github.com/nodejs/node-v0.x-archive/pull/25755
* [a2f879f197] - test: mark recently failing tests as flaky (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [e7010bdf92] - test: runner should return 0 on flaky tests (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [c283c9bbb3] - test: support writing test output to file (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [eeaed586bb] - test: runner support for flaky tests (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [3bb8174b94] - test: refactor to use common testcfg (Timothy J Fontaine) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [df59d43586] - tools: pass constant to logger instead of string (Johan Bergström) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [d103d4ed9a] - tools: fix test.py after v8 upgrade (Ben Noordhuis) https://github.com/nodejs/node-v0.x-archive/pull/25686
* [8002192b4e] - win: manifest node.exe for Windows 8.1 (Alexis Campailla) https://github.com/nodejs/node/pull/2838
* [66ec1dae8f] - win: add MSVS 2015 support (Rod Vagg) https://github.com/nodejs/node-v0.x-archive/pull/25857
* [e192f61514] - win: fix custom actions for WiX older than 3.9 (João Reis) https://github.com/nodejs/node-v0.x-archive/pull/25569
* [16bcd68dc5] - win: fix custom actions on Visual Studio != 2013 (Julien Gilli) https://github.com/nodejs/node-v0.x-archive/pull/25569
* [517986c2f4] - win: backport bringing back xp/2k3 support (Bert Belder) https://github.com/nodejs/node-v0.x-archive/pull/25569
* [10f251e8dd] - win: backport set env before generating projects (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25569

## 2015.11.25, Version 0.12.8 (LTS), @rvagg

* [d9399569bd] - build: backport tools/release.sh (Rod Vagg) https://github.com/nodejs/node/pull/3642
* [78c5b4c8bd] - build: backport config for new CI infrastructure (Rod Vagg) https://github.com/nodejs/node/pull/3642
* [83441616a5] - build: fix --without-ssl compile time error (Ben Noordhuis) https://github.com/nodejs/node/pull/3825
* [8887666b0b] - build: update manifest to include Windows 10 (Lucien Greathouse) https://github.com/nodejs/node/pull/2843
* [08afe4ec8e] - build: add MSVS 2015 support (Rod Vagg) https://github.com/nodejs/node/pull/2843
* [4f2456369c] - build: work around VS2015 issue in ICU <56 (Steven R. Loomis) https://github.com/nodejs/node-v0.x-archive/pull/25804
* [15030f26fd] - build: Intl: bump ICU4C from 54 to 55 (backport) (Steven R. Loomis) https://github.com/nodejs/node-v0.x-archive/pull/25856
* [1083fa70f0] - build: run-ci makefile rule (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25653
* [2d2494cf14] - build: support flaky tests in test-ci (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25653
* [b25d26f2ef] - build: support Jenkins via test-ci (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25653
* [7e4b47f38a] - build,win: fix node.exe resource version (João Reis) https://github.com/nodejs/node/pull/3053
* [e07c86e240] - build,win: try next MSVS version on failure (João Reis) https://github.com/nodejs/node/pull/2843
* [b5a0abcfdf] - child_process: clone spawn options argument (cjihrig) https://github.com/nodejs/node-v0.x-archive/pull/9159
* [8b81f98c41] - configure: add --without-mdb flag (cgalibern) https://github.com/nodejs/node-v0.x-archive/pull/25707
* [071c860c2b] - crypto: replace rwlocks with simple mutexes (Ben Noordhuis) https://github.com/nodejs/node/pull/2723
* [ca97fb6be3] - deps: upgrade npm to 2.14.9 (Forrest L Norvell) https://github.com/nodejs/node/pull/3684
* [583734342e] - deps: fix openssl for MSVS 2015 (Andy Polyakov) https://github.com/nodejs/node/pull/2843
* [02c262a4c6] - deps: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) https://github.com/nodejs/node/pull/2843
* [f0fba0bce8] - deps: update gyp to 25ed9ac (João Reis) https://github.com/nodejs/node/pull/2843
* [f693565813] - deps: upgrade to npm 2.13.4 (Kat Marchán) https://github.com/nodejs/node-v0.x-archive/pull/25825
* [618b142679] - deps,v8: fix compilation in VS2015 (João Reis) https://github.com/nodejs/node/pull/2843
* [49b4f0d54e] - doc: backport README.md (Rod Vagg) https://github.com/nodejs/node/pull/3642
* [2860c53562] - doc: fixed child_process.exec doc (Tyler Anton) https://github.com/nodejs/node-v0.x-archive/pull/14088
* [4a91fa11a3] - doc: Update docs for os.platform() (George Kotchlamazashvili) https://github.com/nodejs/node-v0.x-archive/pull/25777
* [b03ab02fe8] - doc: Change the link for v8 docs to v8dox.com (Chad Walker) https://github.com/nodejs/node-v0.x-archive/pull/25811
* [1fd8f37efd] - doc: buffer, adding missing backtick (Dyana Rose) https://github.com/nodejs/node-v0.x-archive/pull/25811
* [162d0db3bb] - doc: tls.markdown, adjust version from v0.10.39 to v0.10.x (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [eda2560cdc] - doc: additional refinement to readable event (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [881d9bea01] - doc: readable event clarification (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [b6378f0c75] - doc: stream.unshift does not reset reading state (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [4952e2b4d2] - doc: clarify Readable._read and Readable.push (fresheneesz) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [14000b97d4] - doc: two minor stream doc improvements (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [6b6bd21497] - doc: Clarified read method with specified size argument. (Philippe Laferriere) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [16f547600a] - doc: Document http.request protocol option (Ville Skyttä) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [618e4ecda9] - doc: add a note about readable in flowing mode (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [0b165be37b] - doc: fix line wrapping in buffer.markdown (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [70dd13f88d] - doc: add CleartextStream deprecation notice (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [418cde0765] - doc: mention that mode is ignored if file exists (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [85bcb281e4] - doc: improve http.abort description (James M Snell) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [5ccb429ee8] - doc, comments: Grammar and spelling fixes (Ville Skyttä) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [a24db43101] - docs: event emitter behavior notice (Samuel Mills (Henchman)) https://github.com/nodejs/node-v0.x-archive/pull/25467
* [8cbf7cb021] - docs: events clarify emitter.listener() behavior (Benjamin Steephenson) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [b7229debbe] - docs: Fix default options for fs.createWriteStream() (Chris Neave) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [f0453caea2] - domains: port caeb677 from v0.10 to v0.12 (Jeremy Whitlock) https://github.com/nodejs/node-v0.x-archive/pull/25835
* [261fa3620f] - src: fix intermittent SIGSEGV in resolveTxt (Evan Lucas) https://github.com/nodejs/node-v0.x-archive/pull/9300
* [1f7257b02d] - test: mark test-https-aws-ssl flaky on linux (João Reis) https://github.com/nodejs/node-v0.x-archive/pull/25893
* [cf435d55db] - test: mark test-signal-unregister as flaky (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25750
* [ceb6a8c131] - test: fix test-debug-port-from-cmdline (João Reis) https://github.com/nodejs/node-v0.x-archive/pull/25748
* [22997731e6] - test: add regression test for #25735 (Fedor Indutny) https://github.com/nodejs/node-v0.x-archive/pull/25739
* [39e05639f4] - test: mark http-pipeline-flood flaky on win32 (Julien Gilli) https://github.com/nodejs/node-v0.x-archive/pull/25707
* [78d256e7f5] - test: unmark tests that are no longer flaky (João Reis) https://github.com/nodejs/node-v0.x-archive/pull/25676
* [a9b642cf5b] - test: runner should return 0 on flaky tests (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25653
* [b48639befd] - test: support writing test output to file (Alexis Campailla) https://github.com/nodejs/node-v0.x-archive/pull/25653
* [caa16b41d6] - (SEMVER-MINOR) tls: prevent server from using dhe keys < 768 (Michael Dawson) https://github.com/nodejs/node/pull/3890
* [0363cf4a80] - tls: Closing parent socket also closes the tls sock (Devin Nakamura) https://github.com/nodejs/node-v0.x-archive/pull/25642
* [75697112e8] - tls: do not hang without `newSession` handler (Fedor Indutny) https://github.com/nodejs/node-v0.x-archive/pull/25739
* [d998a65058] - tools: pass constant to logger instead of string (Johan Bergström) https://github.com/nodejs/node-v0.x-archive/pull/25653
* [1982ed6e63] - v8: port fbff705 from v0.10 to v0.12 (Jeremy Whitlock) https://github.com/nodejs/node-v0.x-archive/pull/25835
* [44d7054252] - win: fix custom actions for WiX older than 3.9 (João Reis) https://github.com/nodejs/node/pull/2843
* [586c4d8b8e] - win: fix custom actions on Visual Studio != 2013 (Julien Gilli) https://github.com/nodejs/node/pull/2843
* [14db629497] - win,msi: correct installation path registry keys (João Reis) https://github.com/nodejs/node-v0.x-archive/pull/25640
* [8e80528453] - win,msi: change InstallScope to perMachine (João Reis) https://github.com/nodejs/node-v0.x-archive/pull/25640
* [35bbe98401] - Update addons.markdown (Max Deepfield) https://github.com/nodejs/node-v0.x-archive/pull/25885
* [9a6f1ce416] - comma (Julien Valéry) https://github.com/nodejs/node-v0.x-archive/pull/25811
* [d384bf8f84] - Update assert.markdown (daveboivin) https://github.com/nodejs/node-v0.x-archive/pull/25811
* [89b22ccbe1] - Fixed typo (Andrew Murray) https://github.com/nodejs/node-v0.x-archive/pull/25811
* [5ad05af380] - Update util.markdown (Daniel Rentz) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [cb660ab3d3] - Update child_process.markdown, spelling (Jared Fox) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [59c67fe3cd] - updated documentation for fs.createReadStream (Michele Caini) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [53b6a615a5] - Documentation update about Buffer initialization (Sarath) https://github.com/nodejs/node-v0.x-archive/pull/25591
* [b8d47a7b6f] - fix (Fedor Indutny) https://github.com/nodejs/node-v0.x-archive/pull/25739

## 2015-11-17, Version 5.1.0 (Stable), @Fishrock123

### Notable changes

* **buffer**: The `noAssert` option for many buffer functions will now silently drop invalid write values rather than crashing (Minqi Pan) [#3767](https://github.com/nodejs/node/pull/3767).
  - This makes the behavior match what the docs suggest.
* **child_process**: `child.send()` now properly returns a boolean like the docs suggest (Rich Trott) [#3577](https://github.com/nodejs/node/pull/3577).
* **doc**: All of the API docs have been re-ordered so as to read in alphabetical order (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662).
* **http_parser**: update http-parser to 2.6.0 from 2.5.0 (James M Snell) [#3569](https://github.com/nodejs/node/pull/3569).
  - Now supports the following HTTP methods: `LINK`, `UNLINK`, `BIND`, `REBIND`, `UNBIND`.
  - Also added ACL and IPv6 Zone ID support.
* **npm**: upgrade npm to 3.3.12 from v3.3.6 (Rebecca Turner) [#3685](https://github.com/nodejs/node/pull/3685).
  - See the release notes for [v3.3.7](https://github.com/npm/npm/releases/tag/v3.3.7), [v3.3.8](https://github.com/npm/npm/releases/tag/v3.3.8), [v3.3.9](https://github.com/npm/npm/releases/tag/v3.3.9), [v3.3.10](https://github.com/npm/npm/releases/tag/v3.3.10), [v3.3.11](https://github.com/npm/npm/releases/tag/v3.3.11), and [v3.3.12](https://github.com/npm/npm/releases/tag/v3.3.12) for more details.
* **repl**: The REPL no longer crashes if the [persistent history](https://nodejs.org/api/repl.html#repl_persistent_history) file cannot be opened (Evan Lucas) [#3630](https://github.com/nodejs/node/pull/3630).
* **tls**: The default `sessionIdContext` now uses SHA1 in FIPS mode rather than MD5 (Stefan Budeanu) [#3755](https://github.com/nodejs/node/pull/3755).
* **v8**: Added some more useful post-mortem data (Fedor Indutny) [#3779](https://github.com/nodejs/node/pull/3779).

### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* Unicode characters in filesystem paths are not handled consistently across platforms or Node.js APIs. See [#2088](https://github.com/nodejs/node/issues/2088), [#3401](https://github.com/nodejs/node/issues/3401) and [#3519](https://github.com/nodejs/node/issues/3519).

### Commits

* [[`b663d2bbb5`](https://github.com/nodejs/node/commit/b663d2bbb5)] - **async_wrap**: call callback in destructor (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`eccbec99ea`](https://github.com/nodejs/node/commit/eccbec99ea)] - **async_wrap**: new instances get uid (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`5d34c81a5c`](https://github.com/nodejs/node/commit/5d34c81a5c)] - **async_wrap**: allow some hooks to be optional (Trevor Norris) [#3461](https://github.com/nodejs/node/pull/3461)
* [[`7bff0138e2`](https://github.com/nodejs/node/commit/7bff0138e2)] - **buffer**: let WriteFloatGeneric silently drop values (Minqi Pan) [#3767](https://github.com/nodejs/node/pull/3767)
* [[`56673693cd`](https://github.com/nodejs/node/commit/56673693cd)] - **buffer**: neuter external `nullptr` buffers (Fedor Indutny) [#3624](https://github.com/nodejs/node/pull/3624)
* [[`2d0ca0293a`](https://github.com/nodejs/node/commit/2d0ca0293a)] - **build**: fix configuring with prebuilt libraries (Markus Tzoe) [#3135](https://github.com/nodejs/node/pull/3135)
* [[`2a69b6820f`](https://github.com/nodejs/node/commit/2a69b6820f)] - **build**: fix --with-intl=system-icu for x-compile (Steven R. Loomis) [#3808](https://github.com/nodejs/node/pull/3808)
* [[`8f5a2550a7`](https://github.com/nodejs/node/commit/8f5a2550a7)] - **build**: omit -gline-tables-only for --enable-asan (Ben Noordhuis) [#3680](https://github.com/nodejs/node/pull/3680)
* [[`84bb74547d`](https://github.com/nodejs/node/commit/84bb74547d)] - **child_process**: add safety checks on stdio access (cjihrig) [#3799](https://github.com/nodejs/node/pull/3799)
* [[`e888471a11`](https://github.com/nodejs/node/commit/e888471a11)] - **child_process**: don't fork bomb ourselves from -e (Ben Noordhuis) [#3575](https://github.com/nodejs/node/pull/3575)
* [[`47f3735e88`](https://github.com/nodejs/node/commit/47f3735e88)] - **cluster**: send suicide message on disconnect (cjihrig) [#3720](https://github.com/nodejs/node/pull/3720)
* [[`d64a56cba5`](https://github.com/nodejs/node/commit/d64a56cba5)] - **cluster**: remove handles when disconnecting worker (Ben Noordhuis) [#3677](https://github.com/nodejs/node/pull/3677)
* [[`5ed30da5a0`](https://github.com/nodejs/node/commit/5ed30da5a0)] - **console**: use 'label' argument for time and timeEnd (Roman Reiss) [#3590](https://github.com/nodejs/node/pull/3590)
* [[`7a290abea6`](https://github.com/nodejs/node/commit/7a290abea6)] - **crypto**: DSA parameter validation in FIPS mode (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`2c9fb147be`](https://github.com/nodejs/node/commit/2c9fb147be)] - **crypto**: Improve error checking and reporting (Stefan Budeanu) [#3753](https://github.com/nodejs/node/pull/3753)
* [[`66dccaf0cd`](https://github.com/nodejs/node/commit/66dccaf0cd)] - **debugger**: also exit when the repl emits 'exit' (Felix Böhm) [#2369](https://github.com/nodejs/node/pull/2369)
* [[`fd0253be4d`](https://github.com/nodejs/node/commit/fd0253be4d)] - **deps**: backport bc2e393 from v8 upstream (evan.lucas) [#3792](https://github.com/nodejs/node/pull/3792)
* [[`59077acc3d`](https://github.com/nodejs/node/commit/59077acc3d)] - **deps**: cherry-pick 68e89fb from v8's upstream (Fedor Indutny) [#3779](https://github.com/nodejs/node/pull/3779)
* [[`9ef81ff5d3`](https://github.com/nodejs/node/commit/9ef81ff5d3)] - **deps**: update V8 to 4.6.85.31 (Michaël Zasso) [#3698](https://github.com/nodejs/node/pull/3698)
* [[`b48dbf9fce`](https://github.com/nodejs/node/commit/b48dbf9fce)] - **deps**: upgrade npm to 3.3.12 (Rebecca Turner) [#3685](https://github.com/nodejs/node/pull/3685)
* [[`7caeb14e11`](https://github.com/nodejs/node/commit/7caeb14e11)] - **(SEMVER-MINOR)** **deps**: update http-parser to 2.6.0 (James M Snell) [#3569](https://github.com/nodejs/node/pull/3569)
* [[`08e0de59fa`](https://github.com/nodejs/node/commit/08e0de59fa)] - **deps**: upgrade npm to 3.3.10 (Rebecca Turner) [#3599](https://github.com/nodejs/node/pull/3599)
* [[`ac9e4ffe8e`](https://github.com/nodejs/node/commit/ac9e4ffe8e)] - **dns**: prevent undefined values in results (Junliang Yan) [#3696](https://github.com/nodejs/node/pull/3696)
* [[`ea67d870f4`](https://github.com/nodejs/node/commit/ea67d870f4)] - **doc**: document release types in readme (Rod Vagg) [#3482](https://github.com/nodejs/node/pull/3482)
* [[`60d3daa65c`](https://github.com/nodejs/node/commit/60d3daa65c)] - **doc**: replace head of readme with updated text (Rod Vagg) [#3482](https://github.com/nodejs/node/pull/3482)
* [[`df1fdba2ae`](https://github.com/nodejs/node/commit/df1fdba2ae)] - **doc**: sort repl alphabetically (Tristian Flanagan) [#3859](https://github.com/nodejs/node/pull/3859)
* [[`7ecd5422c8`](https://github.com/nodejs/node/commit/7ecd5422c8)] - **doc**: address use of profanity in code of conduct (James M Snell) [#3827](https://github.com/nodejs/node/pull/3827)
* [[`c2393d1f2a`](https://github.com/nodejs/node/commit/c2393d1f2a)] - **doc**: consistent reference-style links (Bryan English) [#3845](https://github.com/nodejs/node/pull/3845)
* [[`96f53c6b02`](https://github.com/nodejs/node/commit/96f53c6b02)] - **doc**: add link to \[customizing util.inspect colors\]. (Jesse McCarthy) [#3749](https://github.com/nodejs/node/pull/3749)
* [[`132297d3f6`](https://github.com/nodejs/node/commit/132297d3f6)] - **doc**: Updated streams simplified constructor API (Tom Gallacher) [#3602](https://github.com/nodejs/node/pull/3602)
* [[`d137f0fd28`](https://github.com/nodejs/node/commit/d137f0fd28)] - **doc**: add warning about Windows process groups (Roman Klauke) [#3681](https://github.com/nodejs/node/pull/3681)
* [[`45ff31cf94`](https://github.com/nodejs/node/commit/45ff31cf94)] - **doc**: added what buf.copy returns (Manuel B) [#3555](https://github.com/nodejs/node/pull/3555)
* [[`5d1faa28cb`](https://github.com/nodejs/node/commit/5d1faa28cb)] - **doc**: reword message.headers to indicate they are not read-only (Tristian Flanagan) [#3814](https://github.com/nodejs/node/pull/3814)
* [[`25c3807051`](https://github.com/nodejs/node/commit/25c3807051)] - **doc**: clarify duplicate header handling (Bryan English) [#3810](https://github.com/nodejs/node/pull/3810)
* [[`ae2d1ee302`](https://github.com/nodejs/node/commit/ae2d1ee302)] - **doc**: repl: add defineComand and displayPrompt (Bryan English) [#3765](https://github.com/nodejs/node/pull/3765)
* [[`09e524d013`](https://github.com/nodejs/node/commit/09e524d013)] - **doc**: sort tls alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`7e60b81c81`](https://github.com/nodejs/node/commit/7e60b81c81)] - **doc**: sort stream alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`cd931a8a13`](https://github.com/nodejs/node/commit/cd931a8a13)] - **doc**: sort net alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`cfa8198af8`](https://github.com/nodejs/node/commit/cfa8198af8)] - **doc**: sort process alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`e1a512607a`](https://github.com/nodejs/node/commit/e1a512607a)] - **doc**: sort zlib alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`0996b97240`](https://github.com/nodejs/node/commit/0996b97240)] - **doc**: sort util alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`df07072b66`](https://github.com/nodejs/node/commit/df07072b66)] - **doc**: sort https alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`6e9d01c7d8`](https://github.com/nodejs/node/commit/6e9d01c7d8)] - **doc**: sort http alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`18da02fa0f`](https://github.com/nodejs/node/commit/18da02fa0f)] - **doc**: sort modules alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`29054ffc0c`](https://github.com/nodejs/node/commit/29054ffc0c)] - **doc**: sort readline alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`389ead37ef`](https://github.com/nodejs/node/commit/389ead37ef)] - **doc**: sort repl alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`d383d624de`](https://github.com/nodejs/node/commit/d383d624de)] - **doc**: sort string_decoder alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`0d2262887c`](https://github.com/nodejs/node/commit/0d2262887c)] - **doc**: sort timers alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`14b5a95d03`](https://github.com/nodejs/node/commit/14b5a95d03)] - **doc**: sort tty alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`d4dda77e4a`](https://github.com/nodejs/node/commit/d4dda77e4a)] - **doc**: sort url alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`39b8259bd5`](https://github.com/nodejs/node/commit/39b8259bd5)] - **doc**: sort vm alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`d357b3090e`](https://github.com/nodejs/node/commit/d357b3090e)] - **doc**: sort querystring alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`1f56abaa98`](https://github.com/nodejs/node/commit/1f56abaa98)] - **doc**: sort punycode alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`bc63667456`](https://github.com/nodejs/node/commit/bc63667456)] - **doc**: sort path alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`22961e011c`](https://github.com/nodejs/node/commit/22961e011c)] - **doc**: sort os alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`4ba18489d3`](https://github.com/nodejs/node/commit/4ba18489d3)] - **doc**: sort globals alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`c3f5ea704f`](https://github.com/nodejs/node/commit/c3f5ea704f)] - **doc**: sort fs alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`ce3ac8dd1e`](https://github.com/nodejs/node/commit/ce3ac8dd1e)] - **doc**: sort events alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`63a78749b8`](https://github.com/nodejs/node/commit/63a78749b8)] - **doc**: sort errors alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`488326da8d`](https://github.com/nodejs/node/commit/488326da8d)] - **doc**: sort dgram alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`e1c357e881`](https://github.com/nodejs/node/commit/e1c357e881)] - **doc**: sort crypto alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`4118fd5794`](https://github.com/nodejs/node/commit/4118fd5794)] - **doc**: sort dns alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`3e046acc50`](https://github.com/nodejs/node/commit/3e046acc50)] - **doc**: sort console alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`05f1af7124`](https://github.com/nodejs/node/commit/05f1af7124)] - **doc**: sort cluster alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`5c30e5dada`](https://github.com/nodejs/node/commit/5c30e5dada)] - **doc**: sort child_process alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`fb6a09cd0e`](https://github.com/nodejs/node/commit/fb6a09cd0e)] - **doc**: sort buffer alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`c7c05d8f02`](https://github.com/nodejs/node/commit/c7c05d8f02)] - **doc**: sort assert alphabetically (Tristian Flanagan) [#3662](https://github.com/nodejs/node/pull/3662)
* [[`f2c2e53321`](https://github.com/nodejs/node/commit/f2c2e53321)] - **doc**: add note to util.isBuffer (Evan Lucas) [#3790](https://github.com/nodejs/node/pull/3790)
* [[`35fb9f91eb`](https://github.com/nodejs/node/commit/35fb9f91eb)] - **doc**: Describe FIPSDIR environment variable (Stefan Budeanu) [#3752](https://github.com/nodejs/node/pull/3752)
* [[`da911f158b`](https://github.com/nodejs/node/commit/da911f158b)] - **doc**: update lts description in the collaborator guide (James M Snell) [#3668](https://github.com/nodejs/node/pull/3668)
* [[`597f8751d1`](https://github.com/nodejs/node/commit/597f8751d1)] - **doc**: add note on tls connection meta data methods (Tyler Henkel) [#3746](https://github.com/nodejs/node/pull/3746)
* [[`a32d9e31dc`](https://github.com/nodejs/node/commit/a32d9e31dc)] - **doc**: add romankl to collaborators (Roman Klauke) [#3725](https://github.com/nodejs/node/pull/3725)
* [[`e5b9109d12`](https://github.com/nodejs/node/commit/e5b9109d12)] - **doc**: add thealphanerd to collaborators (Myles Borins) [#3723](https://github.com/nodejs/node/pull/3723)
* [[`a05a0b47e3`](https://github.com/nodejs/node/commit/a05a0b47e3)] - **doc**: add saghul as a collaborator (Saúl Ibarra Corretgé) [#3724](https://github.com/nodejs/node/pull/3724)
* [[`b14d9c5f16`](https://github.com/nodejs/node/commit/b14d9c5f16)] - **doc**: add method links in events.markdown (Alejandro Oviedo) [#3187](https://github.com/nodejs/node/pull/3187)
* [[`44f779b112`](https://github.com/nodejs/node/commit/44f779b112)] - **doc**: add caveats of algs and key size in crypto (Shigeki Ohtsu) [#3479](https://github.com/nodejs/node/pull/3479)
* [[`a0db5fb355`](https://github.com/nodejs/node/commit/a0db5fb355)] - **doc**: stdout/stderr can block when directed to file (Ben Noordhuis) [#3170](https://github.com/nodejs/node/pull/3170)
* [[`409f29972e`](https://github.com/nodejs/node/commit/409f29972e)] - **doc**: rename iojs-* groups to nodejs-* (Steven R. Loomis) [#3634](https://github.com/nodejs/node/pull/3634)
* [[`801866280e`](https://github.com/nodejs/node/commit/801866280e)] - **doc**: fix wrong date and known issue in changelog.md (James M Snell) [#3650](https://github.com/nodejs/node/pull/3650)
* [[`325c4c7af5`](https://github.com/nodejs/node/commit/325c4c7af5)] - **doc**: fix function param order in assert doc (David Woods) [#3533](https://github.com/nodejs/node/pull/3533)
* [[`045e04e531`](https://github.com/nodejs/node/commit/045e04e531)] - **doc**: typo fix in readme.md (Sam P Gallagher-Bishop) [#3649](https://github.com/nodejs/node/pull/3649)
* [[`7fd8f1371e`](https://github.com/nodejs/node/commit/7fd8f1371e)] - **doc**: add note about timeout delay > TIMEOUT_MAX (Guilherme Souza) [#3512](https://github.com/nodejs/node/pull/3512)
* [[`7d0b589644`](https://github.com/nodejs/node/commit/7d0b589644)] - **doc**: fix crypto spkac function descriptions (Jason Gerfen) [#3614](https://github.com/nodejs/node/pull/3614)
* [[`efa19bdcb5`](https://github.com/nodejs/node/commit/efa19bdcb5)] - **doc**: add final full stop in CONTRIBUTING.md (Emily Aviva Kapor-Mater) [#3576](https://github.com/nodejs/node/pull/3576)
* [[`90723afe32`](https://github.com/nodejs/node/commit/90723afe32)] - **doc**: made code spans more visible in the API docs (phijohns) [#3573](https://github.com/nodejs/node/pull/3573)
* [[`530bb9144f`](https://github.com/nodejs/node/commit/530bb9144f)] - **docs**: improve discoverability of Code of Conduct (Ashley Williams) [#3774](https://github.com/nodejs/node/pull/3774)
* [[`73e40f0327`](https://github.com/nodejs/node/commit/73e40f0327)] - **docs**: fs - change links to buffer encoding to Buffer class anchor (fansworld-claudio) [#2796](https://github.com/nodejs/node/pull/2796)
* [[`7a84fa6c60`](https://github.com/nodejs/node/commit/7a84fa6c60)] - **docs**: fs - remove encoding list and link to buffer (fansworld-claudio) [#2796](https://github.com/nodejs/node/pull/2796)
* [[`2aa6a6d998`](https://github.com/nodejs/node/commit/2aa6a6d998)] - **fs**: return null error on readFile() success (Zheng Chaoping) [#3740](https://github.com/nodejs/node/pull/3740)
* [[`c96400c572`](https://github.com/nodejs/node/commit/c96400c572)] - **gitignore**: don't ignore 'debug' in deps/npm (Rebecca Turner) [#3599](https://github.com/nodejs/node/pull/3599)
* [[`a7f28a098e`](https://github.com/nodejs/node/commit/a7f28a098e)] - **http**: remove unneeded cb check from setTimeout() (Ashok Suthar) [#3631](https://github.com/nodejs/node/pull/3631)
* [[`d2b5dcb2de`](https://github.com/nodejs/node/commit/d2b5dcb2de)] - **lib**: return boolean from child.send() (Rich Trott) [#3577](https://github.com/nodejs/node/pull/3577)
* [[`5c54fa0095`](https://github.com/nodejs/node/commit/5c54fa0095)] - **module**: cache regular expressions (Evan Lucas) [#3869](https://github.com/nodejs/node/pull/3869)
* [[`89285db128`](https://github.com/nodejs/node/commit/89285db128)] - **module**: remove unnecessary JSON.stringify (Andres Suarez) [#3578](https://github.com/nodejs/node/pull/3578)
* [[`fd3f0d8e6e`](https://github.com/nodejs/node/commit/fd3f0d8e6e)] - **querystring**: Parse multiple separator characters (Yosuke Furukawa) [#3807](https://github.com/nodejs/node/pull/3807)
* [[`75dbafc3f8`](https://github.com/nodejs/node/commit/75dbafc3f8)] - **repl**: To exit, press ^C again or type .exit. (Hemanth.HM) [#3368](https://github.com/nodejs/node/pull/3368)
* [[`5073da0481`](https://github.com/nodejs/node/commit/5073da0481)] - **repl**: don't crash if cannot open history file (Evan Lucas) [#3630](https://github.com/nodejs/node/pull/3630)
* [[`59cd28114d`](https://github.com/nodejs/node/commit/59cd28114d)] - **src**: Add missing va_end before return (Ömer Fadıl Usta) [#3565](https://github.com/nodejs/node/pull/3565)
* [[`02e012e984`](https://github.com/nodejs/node/commit/02e012e984)] - **src**: force line buffering for stderr (Rich Trott) [#3701](https://github.com/nodejs/node/pull/3701)
* [[`2498e29344`](https://github.com/nodejs/node/commit/2498e29344)] - **src**: Revert "nix stdin _readableState.reading" (Roman Reiss) [#3490](https://github.com/nodejs/node/pull/3490)
* [[`65cd03cda6`](https://github.com/nodejs/node/commit/65cd03cda6)] - **src**: wrap source before doing syntax check (Evan Lucas) [#3587](https://github.com/nodejs/node/pull/3587)
* [[`d72bb1e96a`](https://github.com/nodejs/node/commit/d72bb1e96a)] - ***Revert*** "**src**: fix stuck debugger process" (Ben Noordhuis) [#3585](https://github.com/nodejs/node/pull/3585)
* [[`047abbd6eb`](https://github.com/nodejs/node/commit/047abbd6eb)] - **test**: move test-specific function out of common (Rich Trott) [#3871](https://github.com/nodejs/node/pull/3871)
* [[`19a36ff355`](https://github.com/nodejs/node/commit/19a36ff355)] - **test**: fix flaky SmartOS test (Rich Trott) [#3830](https://github.com/nodejs/node/pull/3830)
* [[`4bb27baf8d`](https://github.com/nodejs/node/commit/4bb27baf8d)] - **test**: skip test if FreeBSD jail will break it (Rich Trott) [#3839](https://github.com/nodejs/node/pull/3839)
* [[`1c1e70864b`](https://github.com/nodejs/node/commit/1c1e70864b)] - **test**: fix path to module for repl test on Windows (Michael Cornacchia) [#3608](https://github.com/nodejs/node/pull/3608)
* [[`413ca53107`](https://github.com/nodejs/node/commit/413ca53107)] - **test**: increase crypto strength for FIPS standard (Stefan Budeanu) [#3758](https://github.com/nodejs/node/pull/3758)
* [[`2ec5e17d16`](https://github.com/nodejs/node/commit/2ec5e17d16)] - **test**: add test-zlib-flush-drain (Myles Borins) [#3534](https://github.com/nodejs/node/pull/3534)
* [[`de707f0876`](https://github.com/nodejs/node/commit/de707f0876)] - **test**: add hasFipsCrypto to test/common.js (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`828b786e48`](https://github.com/nodejs/node/commit/828b786e48)] - **test**: add test for invalid DSA key size (Stefan Budeanu) [#3756](https://github.com/nodejs/node/pull/3756)
* [[`252e810059`](https://github.com/nodejs/node/commit/252e810059)] - **test**: Fix test-cluster-worker-exit.js for AIX (Imran Iqbal) [#3666](https://github.com/nodejs/node/pull/3666)
* [[`91248b1094`](https://github.com/nodejs/node/commit/91248b1094)] - **test**: run pipeline flood test in parallel (Rich Trott) [#3811](https://github.com/nodejs/node/pull/3811)
* [[`583f58e5d6`](https://github.com/nodejs/node/commit/583f58e5d6)] - **test**: stronger crypto in test fixtures (Stefan Budeanu) [#3759](https://github.com/nodejs/node/pull/3759)
* [[`2e67db3104`](https://github.com/nodejs/node/commit/2e67db3104)] - **test**: refactor test-http-pipeline-flood (Rich Trott) [#3636](https://github.com/nodejs/node/pull/3636)
* [[`1ab59ab9b3`](https://github.com/nodejs/node/commit/1ab59ab9b3)] - **test**: fix flaky test test-http-pipeline-flood (Devin Nakamura) [#3636](https://github.com/nodejs/node/pull/3636)
* [[`1c8a7c6351`](https://github.com/nodejs/node/commit/1c8a7c6351)] - **test**: enhance fs-watch-recursive test (Sakthipriyan Vairamani) [#2599](https://github.com/nodejs/node/pull/2599)
* [[`81997840f2`](https://github.com/nodejs/node/commit/81997840f2)] - **test**: fix test-module-loading-error for musl (Hugues Malphettes) [#3657](https://github.com/nodejs/node/pull/3657)
* [[`9cdceac782`](https://github.com/nodejs/node/commit/9cdceac782)] - **test**: use really invalid hostname (Sakthipriyan Vairamani) [#3711](https://github.com/nodejs/node/pull/3711)
* [[`f3594e77b2`](https://github.com/nodejs/node/commit/f3594e77b2)] - **test**: fix test-net-persistent-keepalive for AIX (Imran Iqbal) [#3646](https://github.com/nodejs/node/pull/3646)
* [[`81522480f1`](https://github.com/nodejs/node/commit/81522480f1)] - **test**: more regression tests for minDHSize option (Ben Noordhuis) [#3629](https://github.com/nodejs/node/pull/3629)
* [[`935b97769e`](https://github.com/nodejs/node/commit/935b97769e)] - **test**: add regression test for 512 bits DH key (Ben Noordhuis) [#3629](https://github.com/nodejs/node/pull/3629)
* [[`e302c33bb0`](https://github.com/nodejs/node/commit/e302c33bb0)] - **test**: mark http-pipeline-flood flaky (Rich Trott) [#3616](https://github.com/nodejs/node/pull/3616)
* [[`5977963bce`](https://github.com/nodejs/node/commit/5977963bce)] - **test**: remove flaky designation from ls-no-sslv3 (Rich Trott) [#3620](https://github.com/nodejs/node/pull/3620)
* [[`1e98d90db8`](https://github.com/nodejs/node/commit/1e98d90db8)] - **test**: add regression test for --debug-brk -e 0 (Ben Noordhuis) [#3585](https://github.com/nodejs/node/pull/3585)
* [[`2f16be2b70`](https://github.com/nodejs/node/commit/2f16be2b70)] - **tls**: Use SHA1 for sessionIdContext in FIPS mode (Stefan Budeanu) [#3755](https://github.com/nodejs/node/pull/3755)
* [[`05f0549b50`](https://github.com/nodejs/node/commit/05f0549b50)] - **tls**: copy client CAs and cert store on CertCb (Fedor Indutny) [#3537](https://github.com/nodejs/node/pull/3537)
* [[`bea35424a2`](https://github.com/nodejs/node/commit/bea35424a2)] - **tools**: add tap output to cpplint (Johan Bergström) [#3448](https://github.com/nodejs/node/pull/3448)
* [[`d036b35349`](https://github.com/nodejs/node/commit/d036b35349)] - **tools**: enforce `throw new Error()` with lint rule (Rich Trott) [#3714](https://github.com/nodejs/node/pull/3714)
* [[`38bb0d864e`](https://github.com/nodejs/node/commit/38bb0d864e)] - **tools**: Use `throw new Error()` consistently (Rich Trott) [#3714](https://github.com/nodejs/node/pull/3714)
* [[`e40d28283a`](https://github.com/nodejs/node/commit/e40d28283a)] - **tools**: update npm test tooling for 3.3.10+ (Rebecca Turner) [#3599](https://github.com/nodejs/node/pull/3599)
* [[`cbd358ce33`](https://github.com/nodejs/node/commit/cbd358ce33)] - **tools**: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/iojs/io.js/pull/1325)
* [[`3137e46cb8`](https://github.com/nodejs/node/commit/3137e46cb8)] - **tools**: update gyp to b3cef02 (Imran Iqbal) [#3487](https://github.com/nodejs/node/pull/3487)
* [[`d61cb90ee3`](https://github.com/nodejs/node/commit/d61cb90ee3)] - **util**: use Object.create(null) for dictionary object (Minwoo Jung) [#3831](https://github.com/nodejs/node/pull/3831)
* [[`9a45c21e6c`](https://github.com/nodejs/node/commit/9a45c21e6c)] - **util**: use regexp instead of str.replace().join() (qinjia) [#3689](https://github.com/nodejs/node/pull/3689)
* [[`33ffc62670`](https://github.com/nodejs/node/commit/33ffc62670)] - **zlib**: only apply drain listener if given callback (Craig Cavalier) [#3534](https://github.com/nodejs/node/pull/3534)
* [[`d70deabf90`](https://github.com/nodejs/node/commit/d70deabf90)] - **zlib**: pass kind to recursive calls to flush (Myles Borins) [#3534](https://github.com/nodejs/node/pull/3534)

## 2015-11-03, Version 4.2.2 'Argon' (LTS), @jasnell

### Notable changes

This is an LTS maintenance release that addresses a number of issues:

* [[`1d0f2cbf87`](https://github.com/nodejs/node/commit/1d0f2cbf87)] - **buffer**: fix value check for writeUInt{B,L}E (Trevor Norris) [#3500](https://github.com/nodejs/node/pull/3500)
* [[`2a45b72b4a`](https://github.com/nodejs/node/commit/2a45b72b4a)] - **buffer**: don't CHECK on zero-sized realloc (Ben Noordhuis) [#3499](https://github.com/nodejs/node/pull/3499)
* [[`a6469e901a`](https://github.com/nodejs/node/commit/a6469e901a)] - **deps**: backport 010897c from V8 upstream (Ali Ijaz Sheikh) [#3520](https://github.com/nodejs/node/pull/3520)
* [[`cadee67c25`](https://github.com/nodejs/node/commit/cadee67c25)] - **deps**: backport 8d6a228 from the v8's upstream (Fedor Indutny) [#3549](https://github.com/nodejs/node/pull/3549)
* [[`46c8c94055`](https://github.com/nodejs/node/commit/46c8c94055)] - **fs**: reduced duplicate code in fs.write() (ronkorving) [#2947](https://github.com/nodejs/node/pull/2947)
* [[`0427cdf094`](https://github.com/nodejs/node/commit/0427cdf094)] - **http**: fix stalled pipeline bug (Fedor Indutny) [#3342](https://github.com/nodejs/node/pull/3342)
* [[`2109708186`](https://github.com/nodejs/node/commit/2109708186)] - **lib**: fix cluster handle leak (Rich Trott) [#3510](https://github.com/nodejs/node/pull/3510)
* [[`f49c7c6955`](https://github.com/nodejs/node/commit/f49c7c6955)] - **lib**: avoid REPL exit on completion error (Rich Trott) [#3358](https://github.com/nodejs/node/pull/3358)
* [[`8a2c4aeeaa`](https://github.com/nodejs/node/commit/8a2c4aeeaa)] - **repl**: handle comments properly (Sakthipriyan Vairamani) [#3515](https://github.com/nodejs/node/pull/3515)
* [[`a04408acce`](https://github.com/nodejs/node/commit/a04408acce)] - **repl**: limit persistent history correctly on load (Jeremiah Senkpiel) [#2356](https://github.com/nodejs/node/pull/2356)
* [[`3bafe1a59b`](https://github.com/nodejs/node/commit/3bafe1a59b)] - **src**: fix race condition in debug signal on exit (Ben Noordhuis) [#3528](https://github.com/nodejs/node/pull/3528)
* [[`fe01d0df7a`](https://github.com/nodejs/node/commit/fe01d0df7a)] - **src**: fix exception message encoding on Windows (Brian White) [#3288](https://github.com/nodejs/node/pull/3288)
* [[`4bac5d9ddf`](https://github.com/nodejs/node/commit/4bac5d9ddf)] - **stream**: avoid unnecessary concat of a single buffer. (Calvin Metcalf) [#3300](https://github.com/nodejs/node/pull/3300)
* [[`8d78d687d5`](https://github.com/nodejs/node/commit/8d78d687d5)] - **timers**: reuse timer in `setTimeout().unref()` (Fedor Indutny) [#3407](https://github.com/nodejs/node/pull/3407)
* [[`e69c869399`](https://github.com/nodejs/node/commit/e69c869399)] - **tls**: TLSSocket options default isServer false (Yuval Brik) [#2614](https://github.com/nodejs/node/pull/2614)

### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`1d0f2cbf87`](https://github.com/nodejs/node/commit/1d0f2cbf87)] - **buffer**: fix value check for writeUInt{B,L}E (Trevor Norris) [#3500](https://github.com/nodejs/node/pull/3500)
* [[`2a45b72b4a`](https://github.com/nodejs/node/commit/2a45b72b4a)] - **buffer**: don't CHECK on zero-sized realloc (Ben Noordhuis) [#3499](https://github.com/nodejs/node/pull/3499)
* [[`dc655e1dd2`](https://github.com/nodejs/node/commit/dc655e1dd2)] - **build**: rectify --link-module help text (Minqi Pan) [#3379](https://github.com/nodejs/node/pull/3379)
* [[`a6469e901a`](https://github.com/nodejs/node/commit/a6469e901a)] - **deps**: backport 010897c from V8 upstream (Ali Ijaz Sheikh) [#3520](https://github.com/nodejs/node/pull/3520)
* [[`cadee67c25`](https://github.com/nodejs/node/commit/cadee67c25)] - **deps**: backport 8d6a228 from the v8's upstream (Fedor Indutny) [#3549](https://github.com/nodejs/node/pull/3549)
* [[`1ebd35550b`](https://github.com/nodejs/node/commit/1ebd35550b)] - **doc**: fix typos in changelog (reggi) [#3291](https://github.com/nodejs/node/pull/3291)
* [[`fbd93d4c1c`](https://github.com/nodejs/node/commit/fbd93d4c1c)] - **doc**: more use-cases for promise events (Domenic Denicola) [#3438](https://github.com/nodejs/node/pull/3438)
* [[`6ceb9af407`](https://github.com/nodejs/node/commit/6ceb9af407)] - **doc**: remove old note, 'cluster' is marked stable (Balázs Galambosi) [#3314](https://github.com/nodejs/node/pull/3314)
* [[`a5f0d64ddc`](https://github.com/nodejs/node/commit/a5f0d64ddc)] - **doc**: createServer's key option can be an array (Sakthipriyan Vairamani) [#3123](https://github.com/nodejs/node/pull/3123)
* [[`317e0ec6b3`](https://github.com/nodejs/node/commit/317e0ec6b3)] - **doc**: binary encoding is not deprecated (Trevor Norris) [#3441](https://github.com/nodejs/node/pull/3441)
* [[`b422f6ee1a`](https://github.com/nodejs/node/commit/b422f6ee1a)] - **doc**: mention the behaviour if URL is invalid (Sakthipriyan Vairamani) [#2966](https://github.com/nodejs/node/pull/2966)
* [[`bc29aad22b`](https://github.com/nodejs/node/commit/bc29aad22b)] - **doc**: fix indent in tls resumption example (Roman Reiss) [#3372](https://github.com/nodejs/node/pull/3372)
* [[`313877bd8f`](https://github.com/nodejs/node/commit/313877bd8f)] - **doc**: fix typo in changelog (Timothy Gu) [#3353](https://github.com/nodejs/node/pull/3353)
* [[`4be432862a`](https://github.com/nodejs/node/commit/4be432862a)] - **doc**: show keylen in pbkdf2 as a byte length (calebboyd) [#3334](https://github.com/nodejs/node/pull/3334)
* [[`23a1140ddb`](https://github.com/nodejs/node/commit/23a1140ddb)] - **doc**: add information about Assert behavior and maintenance (Rich Trott) [#3330](https://github.com/nodejs/node/pull/3330)
* [[`e04cb1e1fc`](https://github.com/nodejs/node/commit/e04cb1e1fc)] - **doc**: clarify API buffer.concat (Martii) [#3255](https://github.com/nodejs/node/pull/3255)
* [[`eae714c370`](https://github.com/nodejs/node/commit/eae714c370)] - **doc**: clarify the use of `option.detached` (Kyle Smith) [#3250](https://github.com/nodejs/node/pull/3250)
* [[`b884899e67`](https://github.com/nodejs/node/commit/b884899e67)] - **doc**: label v4.2.1 as LTS in changelog heading (Phillip Johnsen) [#3360](https://github.com/nodejs/node/pull/3360)
* [[`9120a04981`](https://github.com/nodejs/node/commit/9120a04981)] - **docs**: add missing shell option to execSync (fansworld-claudio) [#3440](https://github.com/nodejs/node/pull/3440)
* [[`46c8c94055`](https://github.com/nodejs/node/commit/46c8c94055)] - **fs**: reduced duplicate code in fs.write() (ronkorving) [#2947](https://github.com/nodejs/node/pull/2947)
* [[`0427cdf094`](https://github.com/nodejs/node/commit/0427cdf094)] - **http**: fix stalled pipeline bug (Fedor Indutny) [#3342](https://github.com/nodejs/node/pull/3342)
* [[`2109708186`](https://github.com/nodejs/node/commit/2109708186)] - **lib**: fix cluster handle leak (Rich Trott) [#3510](https://github.com/nodejs/node/pull/3510)
* [[`f49c7c6955`](https://github.com/nodejs/node/commit/f49c7c6955)] - **lib**: avoid REPL exit on completion error (Rich Trott) [#3358](https://github.com/nodejs/node/pull/3358)
* [[`8a2c4aeeaa`](https://github.com/nodejs/node/commit/8a2c4aeeaa)] - **repl**: handle comments properly (Sakthipriyan Vairamani) [#3515](https://github.com/nodejs/node/pull/3515)
* [[`a04408acce`](https://github.com/nodejs/node/commit/a04408acce)] - **repl**: limit persistent history correctly on load (Jeremiah Senkpiel) [#2356](https://github.com/nodejs/node/pull/2356)
* [[`5d1f1c5fa8`](https://github.com/nodejs/node/commit/5d1f1c5fa8)] - **src**: wrap source before doing syntax check (Evan Lucas) [#3587](https://github.com/nodejs/node/pull/3587)
* [[`3bafe1a59b`](https://github.com/nodejs/node/commit/3bafe1a59b)] - **src**: fix race condition in debug signal on exit (Ben Noordhuis) [#3528](https://github.com/nodejs/node/pull/3528)
* [[`fe01d0df7a`](https://github.com/nodejs/node/commit/fe01d0df7a)] - **src**: fix exception message encoding on Windows (Brian White) [#3288](https://github.com/nodejs/node/pull/3288)
* [[`4bac5d9ddf`](https://github.com/nodejs/node/commit/4bac5d9ddf)] - **stream**: avoid unnecessary concat of a single buffer. (Calvin Metcalf) [#3300](https://github.com/nodejs/node/pull/3300)
* [[`117fb47a16`](https://github.com/nodejs/node/commit/117fb47a16)] - **stream**: fix signature of _write() in a comment (Fábio Santos) [#3248](https://github.com/nodejs/node/pull/3248)
* [[`c563a34427`](https://github.com/nodejs/node/commit/c563a34427)] - **test**: split independent tests into separate files (Rich Trott) [#3548](https://github.com/nodejs/node/pull/3548)
* [[`3f62952d42`](https://github.com/nodejs/node/commit/3f62952d42)] - **test**: add node::MakeCallback() test coverage (Ben Noordhuis) [#3478](https://github.com/nodejs/node/pull/3478)
* [[`6b75f10d8a`](https://github.com/nodejs/node/commit/6b75f10d8a)] - **test**: use port number from env in tls socket test (Stefan Budeanu) [#3557](https://github.com/nodejs/node/pull/3557)
* [[`39ff44e94f`](https://github.com/nodejs/node/commit/39ff44e94f)] - **test**: fix heap-profiler link error LNK1194 on win (Junliang Yan) [#3572](https://github.com/nodejs/node/pull/3572)
* [[`a2786dd408`](https://github.com/nodejs/node/commit/a2786dd408)] - **test**: fix missing unistd.h on windows (Junliang Yan) [#3532](https://github.com/nodejs/node/pull/3532)
* [[`5e6f7c9a23`](https://github.com/nodejs/node/commit/5e6f7c9a23)] - **test**: add regression test for --debug-brk -e 0 (Ben Noordhuis) [#3585](https://github.com/nodejs/node/pull/3585)
* [[`7cad182cb6`](https://github.com/nodejs/node/commit/7cad182cb6)] - **test**: port domains regression test from v0.10 (Jonas Dohse) [#3356](https://github.com/nodejs/node/pull/3356)
* [[`78d854c6ce`](https://github.com/nodejs/node/commit/78d854c6ce)] - **test**: remove util from common (Rich Trott) [#3324](https://github.com/nodejs/node/pull/3324)
* [[`c566c8b8c0`](https://github.com/nodejs/node/commit/c566c8b8c0)] - **test**: remove util properties from common (Rich Trott) [#3304](https://github.com/nodejs/node/pull/3304)
* [[`eb7c3fb2f4`](https://github.com/nodejs/node/commit/eb7c3fb2f4)] - **test**: split up buffer tests for reliability (Rich Trott) [#3323](https://github.com/nodejs/node/pull/3323)
* [[`b398a85e19`](https://github.com/nodejs/node/commit/b398a85e19)] - **test**: parallelize long-running test (Rich Trott) [#3287](https://github.com/nodejs/node/pull/3287)
* [[`b5f3b4956b`](https://github.com/nodejs/node/commit/b5f3b4956b)] - **test**: change call to deprecated util.isError() (Rich Trott) [#3084](https://github.com/nodejs/node/pull/3084)
* [[`32149cacb5`](https://github.com/nodejs/node/commit/32149cacb5)] - **test**: improve tests for util.inherits (Michaël Zasso) [#3507](https://github.com/nodejs/node/pull/3507)
* [[`5be686fab8`](https://github.com/nodejs/node/commit/5be686fab8)] - **test**: print helpful err msg on test-dns-ipv6.js (Junliang Yan) [#3501](https://github.com/nodejs/node/pull/3501)
* [[`0429131e32`](https://github.com/nodejs/node/commit/0429131e32)] - **test**: fix domain with abort-on-uncaught on PPC (Julien Gilli) [#3354](https://github.com/nodejs/node/pull/3354)
* [[`788106eee9`](https://github.com/nodejs/node/commit/788106eee9)] - **test**: cleanup, improve repl-persistent-history (Jeremiah Senkpiel) [#2356](https://github.com/nodejs/node/pull/2356)
* [[`ea58fa0bac`](https://github.com/nodejs/node/commit/ea58fa0bac)] - **test**: add Symbol test for assert.deepEqual() (Rich Trott) [#3327](https://github.com/nodejs/node/pull/3327)
* [[`d409ac473b`](https://github.com/nodejs/node/commit/d409ac473b)] - **test**: disable test-tick-processor - aix and be ppc (Michael Dawson) [#3491](https://github.com/nodejs/node/pull/3491)
* [[`c1623039dd`](https://github.com/nodejs/node/commit/c1623039dd)] - **test**: harden test-child-process-fork-regr-gh-2847 (Michael Dawson) [#3459](https://github.com/nodejs/node/pull/3459)
* [[`3bb4437abb`](https://github.com/nodejs/node/commit/3bb4437abb)] - **test**: fix test-net-keepalive for AIX (Imran Iqbal) [#3458](https://github.com/nodejs/node/pull/3458)
* [[`af55641a69`](https://github.com/nodejs/node/commit/af55641a69)] - **test**: wrap assert.fail when passed to callback (Myles Borins) [#3453](https://github.com/nodejs/node/pull/3453)
* [[`7c7ef01e65`](https://github.com/nodejs/node/commit/7c7ef01e65)] - **test**: skip test-dns-ipv6.js if ipv6 is unavailable (Junliang Yan) [#3444](https://github.com/nodejs/node/pull/3444)
* [[`a4d1510ba4`](https://github.com/nodejs/node/commit/a4d1510ba4)] - **test**: repl-persistent-history is no longer flaky (Jeremiah Senkpiel) [#3437](https://github.com/nodejs/node/pull/3437)
* [[`a5d968b8a2`](https://github.com/nodejs/node/commit/a5d968b8a2)] - **test**: fix flaky test-child-process-emfile (Rich Trott) [#3430](https://github.com/nodejs/node/pull/3430)
* [[`eac2acca76`](https://github.com/nodejs/node/commit/eac2acca76)] - **test**: remove flaky status from eval_messages test (Rich Trott) [#3420](https://github.com/nodejs/node/pull/3420)
* [[`155c778584`](https://github.com/nodejs/node/commit/155c778584)] - **test**: fix flaky test for symlinks (Rich Trott) [#3418](https://github.com/nodejs/node/pull/3418)
* [[`74eb632483`](https://github.com/nodejs/node/commit/74eb632483)] - **test**: apply correct assert.fail() arguments (Rich Trott) [#3378](https://github.com/nodejs/node/pull/3378)
* [[`0a4323dd82`](https://github.com/nodejs/node/commit/0a4323dd82)] - **test**: replace util with backtick strings (Myles Borins) [#3359](https://github.com/nodejs/node/pull/3359)
* [[`93847694ec`](https://github.com/nodejs/node/commit/93847694ec)] - **test**: add test-child-process-emfile fail message (Rich Trott) [#3335](https://github.com/nodejs/node/pull/3335)
* [[`8d78d687d5`](https://github.com/nodejs/node/commit/8d78d687d5)] - **timers**: reuse timer in `setTimeout().unref()` (Fedor Indutny) [#3407](https://github.com/nodejs/node/pull/3407)
* [[`e69c869399`](https://github.com/nodejs/node/commit/e69c869399)] - **tls**: TLSSocket options default isServer false (Yuval Brik) [#2614](https://github.com/nodejs/node/pull/2614)
* [[`0b32bbbf69`](https://github.com/nodejs/node/commit/0b32bbbf69)] - **v8**: pull fix for builtin code size on PPC (Michael Dawson) [#3474](https://github.com/nodejs/node/pull/3474)

## 2015-10-29, Version 5.0.0 (Stable), @rvagg

### Notable Changes

* **buffer**: _(Breaking)_ Removed both `'raw'` and `'raws'` encoding types from `Buffer`, these have been deprecated for a long time (Sakthipriyan Vairamani) [#2859](https://github.com/nodejs/node/pull/2859).
* **console**: _(Breaking)_ Values reported by `console.time()` now have 3 decimals of accuracy added (Michaël Zasso) [#3166](https://github.com/nodejs/node/pull/3166).
* **fs**:
  - `fs.readFile*()`, `fs.writeFile*()`, and `fs.appendFile*()` now also accept a file descriptor as their first argument (Johannes Wüller) [#3163](https://github.com/nodejs/node/pull/3163).
  - _(Breaking)_ In `fs.readFile()`, if an encoding is specified and the internal `toString()` fails the error is no longer _thrown_ but is passed to the callback (Evan Lucas) [#3485](https://github.com/nodejs/node/pull/3485).
  - _(Breaking)_ In `fs.read()` (using the `fs.read(fd, length, position, encoding, callback)` form), if the internal `toString()` fails the error is no longer _thrown_ but is passed to the callback (Evan Lucas) [#3503](https://github.com/nodejs/node/pull/3503).
* **http**:
  - Fixed a bug where pipelined http requests would stall (Fedor Indutny) [#3342](https://github.com/nodejs/node/pull/3342).
  - _(Breaking)_ When parsing HTTP, don't add duplicates of the following headers: `Retry-After`, `ETag`, `Last-Modified`, `Server`, `Age`, `Expires`. This is in addition to the following headers which already block duplicates: `Content-Type`, `Content-Length`, `User-Agent`, `Referer`, `Host`, `Authorization`, `Proxy-Authorization`, `If-Modified-Since`, `If-Unmodified-Since`, `From`, `Location`, `Max-Forwards` (James M Snell) [#3090](https://github.com/nodejs/node/pull/3090).
  - _(Breaking)_ The `callback` argument to `OutgoingMessage#setTimeout()` must be a function or a `TypeError` is thrown (James M Snell) [#3090](https://github.com/nodejs/node/pull/3090).
  - _(Breaking)_ HTTP methods and header names must now conform to the RFC 2616 "token" rule, a list of allowed characters that excludes control characters and a number of _separator_ characters. Specifically, methods and header names must now match ```/^[a-zA-Z0-9_!#$%&'*+.^`|~-]+$/``` or a `TypeError` will be thrown (James M Snell) [#2526](https://github.com/nodejs/node/pull/2526).
* **node**:
  - _(Breaking)_ Deprecated the `_linklist` module (Rich Trott) [#3078](https://github.com/nodejs/node/pull/3078).
  - _(Breaking)_ Removed `require.paths` and `require.registerExtension()`, both had been previously set to throw `Error` when accessed (Sakthipriyan Vairamani) [#2922](https://github.com/nodejs/node/pull/2922).
* **npm**: Upgraded to version 3.3.6 from 2.14.7, see https://github.com/npm/npm/releases/tag/v3.3.6 for more details. This is a major version bump for npm and it has seen a significant amount of change. Please see the original [npm v3.0.0 release notes](https://github.com/npm/npm/blob/master/CHANGELOG.md#v300-2015-06-25) for a list of major changes (Rebecca Turner) [#3310](https://github.com/nodejs/node/pull/3310).
* **src**: _(Breaking)_ Bumped `NODE_MODULE_VERSION` to `47` from `46`, this is necessary due to the V8 upgrade. Native add-ons will need to be recompiled (Rod Vagg) [#3400](https://github.com/nodejs/node/pull/3400).
* **timers**: Attempt to reuse the timer handle for `setTimeout().unref()`. This fixes a long-standing known issue where unrefed timers would perviously hold `beforeExit` open (Fedor Indutny) [#3407](https://github.com/nodejs/node/pull/3407).
* **tls**:
  - Added ALPN Support (Shigeki Ohtsu) [#2564](https://github.com/nodejs/node/pull/2564).
  - TLS options can now be passed in an object to `createSecurePair()` (Коренберг Марк) [#2441](https://github.com/nodejs/node/pull/2441).
  - _(Breaking)_ The default minimum DH key size for `tls.connect()` is now 1024 bits and a warning is shown when DH key size is less than 2048 bits. This a security consideration to prevent "logjam" attacks. A new `minDHSize` TLS option can be used to override the default. (Shigeki Ohtsu) [#1831](https://github.com/nodejs/node/pull/1831).
* **util**:
  - _(Breaking)_ `util.p()` was deprecated for years, and has now been removed (Wyatt Preul) [#3432](https://github.com/nodejs/node/pull/3432).
  - _(Breaking)_ `util.inherits()` can now work with ES6 classes. This is considered a breaking change because of potential subtle side-effects caused by a change from directly reassigning the prototype of the constructor using `ctor.prototype = Object.create(superCtor.prototype, { constructor: { ... } })` to using `Object.setPrototypeOf(ctor.prototype, superCtor.prototype)` (Michaël Zasso) [#3455](https://github.com/nodejs/node/pull/3455).
* **v8**: _(Breaking)_ Upgraded to 4.6.85.25 from 4.5.103.35  (Ali Ijaz Sheikh) [#3351](https://github.com/nodejs/node/pull/3351).
  - Implements the spread operator, see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Spread_operator for further information.
  - Implements `new.target`, see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/new.target for further information.
* **zlib**: Decompression now throws on truncated input (e.g. unexpected end of file) (Yuval Brik) [#2595](https://github.com/nodejs/node/pull/2595).

### Known issues

* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* Unicode characters in filesystem paths are not handled consistently across platforms or Node.js APIs. See [#2088](https://github.com/nodejs/node/issues/2088), [#3401](https://github.com/nodejs/node/issues/3401) and [#3519](https://github.com/nodejs/node/issues/3519).


### Commits

* [[`6a04cc0a43`](https://github.com/nodejs/node/commit/6a04cc0a43)] - **buffer**: fix value check for writeUInt{B,L}E (Trevor Norris) [#3500](https://github.com/nodejs/node/pull/3500)
* [[`1a41feb559`](https://github.com/nodejs/node/commit/1a41feb559)] - **buffer**: don't CHECK on zero-sized realloc (Ben Noordhuis) [#3499](https://github.com/nodejs/node/pull/3499)
* [[`5f6579d366`](https://github.com/nodejs/node/commit/5f6579d366)] - **(SEMVER-MAJOR)** **buffer**: remove raw & raws encoding (Sakthipriyan Vairamani) [#2859](https://github.com/nodejs/node/pull/2859)
* [[`70fca2a81e`](https://github.com/nodejs/node/commit/70fca2a81e)] - **build**: Updates for AIX npm support - part 1 (Michael Dawson) [#3114](https://github.com/nodejs/node/pull/3114)
* [[`b36b4f385a`](https://github.com/nodejs/node/commit/b36b4f385a)] - **build**: rectify --link-module help text (Minqi Pan) [#3379](https://github.com/nodejs/node/pull/3379)
* [[`a89eeca590`](https://github.com/nodejs/node/commit/a89eeca590)] - **console**: rename argument of time and timeEnd (Michaël Zasso) [#3166](https://github.com/nodejs/node/pull/3166)
* [[`870108aaa8`](https://github.com/nodejs/node/commit/870108aaa8)] - **(SEMVER-MAJOR)** **console**: sub-millisecond accuracy for console.time (Michaël Zasso) [#3166](https://github.com/nodejs/node/pull/3166)
* [[`0a43697ce8`](https://github.com/nodejs/node/commit/0a43697ce8)] - **deps**: backport 010897c from V8 upstream (Ali Ijaz Sheikh) [#3520](https://github.com/nodejs/node/pull/3520)
* [[`8c0318ce8d`](https://github.com/nodejs/node/commit/8c0318ce8d)] - **deps**: backport 8d6a228 from the v8's upstream (Fedor Indutny) [#3549](https://github.com/nodejs/node/pull/3549)
* [[`2974debc6e`](https://github.com/nodejs/node/commit/2974debc6e)] - **deps**: update V8 to 4.6.85.28 (Michaël Zasso) [#3484](https://github.com/nodejs/node/pull/3484)
* [[`f76af49b13`](https://github.com/nodejs/node/commit/f76af49b13)] - **deps**: fix upgrade to npm 3.3.6 (Rebecca Turner) [#3494](https://github.com/nodejs/node/pull/3494)
* [[`32b51c97ec`](https://github.com/nodejs/node/commit/32b51c97ec)] - **deps**: upgrade npm to 3.3.6 (Rebecca Turner) [#3310](https://github.com/nodejs/node/pull/3310)
* [[`770cd229f9`](https://github.com/nodejs/node/commit/770cd229f9)] - **deps**: upgrade V8 to 4.6.85.25 (Ali Ijaz Sheikh) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`972a0c8515`](https://github.com/nodejs/node/commit/972a0c8515)] - **deps**: backport 0d01728 from v8's upstream (Fedor Indutny) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`1fdec65203`](https://github.com/nodejs/node/commit/1fdec65203)] - **deps**: improve ArrayBuffer performance in v8 (Fedor Indutny) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`5cd1fd836a`](https://github.com/nodejs/node/commit/5cd1fd836a)] - **deps**: backport 56a0a79 from V8 upstream (Julien Gilli) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`7fb128d8df`](https://github.com/nodejs/node/commit/7fb128d8df)] - **deps**: cherry-pick backports to V8 (Michaël Zasso) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`d8011d1683`](https://github.com/nodejs/node/commit/d8011d1683)] - **(SEMVER-MAJOR)** **deps**: upgrade V8 to 4.6.85.23 (Michaël Zasso) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`a334ddc467`](https://github.com/nodejs/node/commit/a334ddc467)] - ***Revert*** "**deps**: backport 03ef3cd from V8 upstream" (Ali Ijaz Sheikh) [#3237](https://github.com/nodejs/node/pull/3237)
* [[`6fff47ffac`](https://github.com/nodejs/node/commit/6fff47ffac)] - **deps**: backport 03ef3cd from V8 upstream (Ali Ijaz Sheikh) [#3165](https://github.com/nodejs/node/pull/3165)
* [[`680dda8023`](https://github.com/nodejs/node/commit/680dda8023)] - **dns**: remove nonexistant exports.ADNAME (Roman Reiss) [#3051](https://github.com/nodejs/node/pull/3051)
* [[`239ad899a3`](https://github.com/nodejs/node/commit/239ad899a3)] - **doc**: add LTS info to COLLABORATOR_GUIDE.md (Myles Borins) [#3442](https://github.com/nodejs/node/pull/3442)
* [[`5e76587fdf`](https://github.com/nodejs/node/commit/5e76587fdf)] - **doc**: createServer's key option can be an array (Sakthipriyan Vairamani) [#3123](https://github.com/nodejs/node/pull/3123)
* [[`0317c880da`](https://github.com/nodejs/node/commit/0317c880da)] - **doc**: add TSC meeting minutes 2015-10-21 (Rod Vagg) [#3480](https://github.com/nodejs/node/pull/3480)
* [[`cd245b12e0`](https://github.com/nodejs/node/commit/cd245b12e0)] - **doc**: clarify API buffer.concat (Martii) [#3255](https://github.com/nodejs/node/pull/3255)
* [[`ff9ef893fd`](https://github.com/nodejs/node/commit/ff9ef893fd)] - **doc**: add TSC meeting minutes 2015-10-14 (Rod Vagg) [#3463](https://github.com/nodejs/node/pull/3463)
* [[`605c5a7754`](https://github.com/nodejs/node/commit/605c5a7754)] - **doc**: clarify the use of `option.detached` (Kyle Smith) [#3250](https://github.com/nodejs/node/pull/3250)
* [[`cf75a175e5`](https://github.com/nodejs/node/commit/cf75a175e5)] - **doc**: more use-cases for promise events (Domenic Denicola) [#3438](https://github.com/nodejs/node/pull/3438)
* [[`1b75d4bda3`](https://github.com/nodejs/node/commit/1b75d4bda3)] - **doc**: update WORKING_GROUPS.md - add missing groups (Michael Dawson) [#3450](https://github.com/nodejs/node/pull/3450)
* [[`c658de2f99`](https://github.com/nodejs/node/commit/c658de2f99)] - **doc**: add TSC meeting minutes 2015-09-30 (Rod Vagg) [#3235](https://github.com/nodejs/node/pull/3235)
* [[`d0b8c5d3a4`](https://github.com/nodejs/node/commit/d0b8c5d3a4)] - **doc**: add TSC meeting minutes 2015-10-07 (Rod Vagg) [#3364](https://github.com/nodejs/node/pull/3364)
* [[`b483afcb20`](https://github.com/nodejs/node/commit/b483afcb20)] - **doc**: binary encoding is not deprecated (Trevor Norris) [#3441](https://github.com/nodejs/node/pull/3441)
* [[`b607366a1c`](https://github.com/nodejs/node/commit/b607366a1c)] - **doc**: add information about Assert behavior and maintenance (Rich Trott) [#3330](https://github.com/nodejs/node/pull/3330)
* [[`086103b32e`](https://github.com/nodejs/node/commit/086103b32e)] - **doc**: show keylen in pbkdf2 as a byte length (calebboyd) [#3334](https://github.com/nodejs/node/pull/3334)
* [[`f6ebc8277b`](https://github.com/nodejs/node/commit/f6ebc8277b)] - **doc**: reword description of console.time (Michaël Zasso) [#3166](https://github.com/nodejs/node/pull/3166)
* [[`503f279527`](https://github.com/nodejs/node/commit/503f279527)] - **doc**: fix indent in tls resumption example (Roman Reiss) [#3372](https://github.com/nodejs/node/pull/3372)
* [[`dae9fae0fe`](https://github.com/nodejs/node/commit/dae9fae0fe)] - **doc**: label v4.2.1 as LTS in changelog heading (Phillip Johnsen) [#3360](https://github.com/nodejs/node/pull/3360)
* [[`4fc638804c`](https://github.com/nodejs/node/commit/4fc638804c)] - **doc**: update V8 options in man page (Michaël Zasso) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`a441aa6e1d`](https://github.com/nodejs/node/commit/a441aa6e1d)] - **doc**: update WORKING_GROUPS.md to include Intl (Steven R. Loomis) [#3251](https://github.com/nodejs/node/pull/3251)
* [[`81503e597b`](https://github.com/nodejs/node/commit/81503e597b)] - **doc**: fix typo in changelog (Timothy Gu) [#3353](https://github.com/nodejs/node/pull/3353)
* [[`3ef2e4acf3`](https://github.com/nodejs/node/commit/3ef2e4acf3)] - **doc**: fix typos in changelog (reggi) [#3291](https://github.com/nodejs/node/pull/3291)
* [[`b9279aa193`](https://github.com/nodejs/node/commit/b9279aa193)] - **doc**: remove old note, 'cluster' is marked stable (Balázs Galambosi) [#3314](https://github.com/nodejs/node/pull/3314)
* [[`cdfa271164`](https://github.com/nodejs/node/commit/cdfa271164)] - **doc**: update AUTHORS list (Rod Vagg)
* [[`47b06f6bb1`](https://github.com/nodejs/node/commit/47b06f6bb1)] - **docs**: add missing shell option to execSync (fansworld-claudio) [#3440](https://github.com/nodejs/node/pull/3440)
* [[`4c9abbd1bb`](https://github.com/nodejs/node/commit/4c9abbd1bb)] - **fs**: reduced duplicate code in fs.write() (ronkorving) [#2947](https://github.com/nodejs/node/pull/2947)
* [[`2bb147535e`](https://github.com/nodejs/node/commit/2bb147535e)] - **(SEMVER-MAJOR)** **fs**: don't throw in read if buffer too big (Evan Lucas) [#3503](https://github.com/nodejs/node/pull/3503)
* [[`7added3b39`](https://github.com/nodejs/node/commit/7added3b39)] - **(SEMVER-MAJOR)** **fs**: pass err to callback if buffer is too big (Evan Lucas) [#3485](https://github.com/nodejs/node/pull/3485)
* [[`5e0759f6fd`](https://github.com/nodejs/node/commit/5e0759f6fd)] - **(SEMVER-MINOR)** **fs**: add file descriptor support to *File() funcs (Johannes Wüller) [#3163](https://github.com/nodejs/node/pull/3163)
* [[`d1a2e5357e`](https://github.com/nodejs/node/commit/d1a2e5357e)] - **gitignore**: don't ignore debug source directory in V8 (Michaël Zasso) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`ab03635fb1`](https://github.com/nodejs/node/commit/ab03635fb1)] - **http**: fix stalled pipeline bug (Fedor Indutny) [#3342](https://github.com/nodejs/node/pull/3342)
* [[`e655a437b3`](https://github.com/nodejs/node/commit/e655a437b3)] - **(SEMVER-MAJOR)** **http**: do not allow multiple instances of certain response headers (James M Snell) [#3090](https://github.com/nodejs/node/pull/3090)
* [[`0094a8dad7`](https://github.com/nodejs/node/commit/0094a8dad7)] - **(SEMVER-MAJOR)** **http**: add callback is function check (James M Snell) [#3090](https://github.com/nodejs/node/pull/3090)
* [[`6192c9892f`](https://github.com/nodejs/node/commit/6192c9892f)] - **(SEMVER-MAJOR)** **http**: add checkIsHttpToken check for header fields (James M Snell) [#2526](https://github.com/nodejs/node/pull/2526)
* [[`c9786bb680`](https://github.com/nodejs/node/commit/c9786bb680)] - **(SEMVER-MAJOR)** http{s}: don't connect to localhost on invalid URL (Sakthipriyan Vairamani) [#2967](https://github.com/nodejs/node/pull/2967)
* [[`1929d5be73`](https://github.com/nodejs/node/commit/1929d5be73)] - **lib**: fix cluster handle leak (Rich Trott) [#3510](https://github.com/nodejs/node/pull/3510)
* [[`97d081709e`](https://github.com/nodejs/node/commit/97d081709e)] - **lib**: avoid REPL exit on completion error (Rich Trott) [#3358](https://github.com/nodejs/node/pull/3358)
* [[`f236b3a904`](https://github.com/nodejs/node/commit/f236b3a904)] - **(SEMVER-MINOR)** **lib,doc**: return boolean from child.send() (Rich Trott) [#3516](https://github.com/nodejs/node/pull/3516)
* [[`6e887cc630`](https://github.com/nodejs/node/commit/6e887cc630)] - **lib,test**: update let to const where applicable (Sakthipriyan Vairamani) [#3152](https://github.com/nodejs/node/pull/3152)
* [[`47befffc53`](https://github.com/nodejs/node/commit/47befffc53)] - **(SEMVER-MAJOR)** **lib,test**: deprecate _linklist (Rich Trott) [#3078](https://github.com/nodejs/node/pull/3078)
* [[`d5ce53458e`](https://github.com/nodejs/node/commit/d5ce53458e)] - **lttng**: update flags for gc tracing (Glen Keane) [#3388](https://github.com/nodejs/node/pull/3388)
* [[`6ad458b752`](https://github.com/nodejs/node/commit/6ad458b752)] - **(SEMVER-MAJOR)** **module**: remove unnecessary property and method (Sakthipriyan Vairamani) [#2922](https://github.com/nodejs/node/pull/2922)
* [[`ae196175f4`](https://github.com/nodejs/node/commit/ae196175f4)] - **node**: improve GetActiveRequests performance (Trevor Norris) [#3375](https://github.com/nodejs/node/pull/3375)
* [[`bd4311bc9c`](https://github.com/nodejs/node/commit/bd4311bc9c)] - **repl**: handle comments properly (Sakthipriyan Vairamani) [#3515](https://github.com/nodejs/node/pull/3515)
* [[`ce391ed849`](https://github.com/nodejs/node/commit/ce391ed849)] - **(SEMVER-MAJOR)** **repl**: event ordering: delay 'close' until 'flushHistory' (Jeremiah Senkpiel) [#3435](https://github.com/nodejs/node/pull/3435)
* [[`4c80c02ac7`](https://github.com/nodejs/node/commit/4c80c02ac7)] - **repl**: limit persistent history correctly on load (Jeremiah Senkpiel) [#2356](https://github.com/nodejs/node/pull/2356)
* [[`134a60c785`](https://github.com/nodejs/node/commit/134a60c785)] - **src**: fix race condition in debug signal on exit (Ben Noordhuis) [#3528](https://github.com/nodejs/node/pull/3528)
* [[`bf7c3dabb4`](https://github.com/nodejs/node/commit/bf7c3dabb4)] - **(SEMVER-MAJOR)** **src**: bump NODE_MODULE_VERSION To 47 (Rod Vagg) [#3400](https://github.com/nodejs/node/pull/3400)
* [[`2d3560767e`](https://github.com/nodejs/node/commit/2d3560767e)] - **src**: fix exception message encoding on Windows (Brian White) [#3288](https://github.com/nodejs/node/pull/3288)
* [[`ff877e93e1`](https://github.com/nodejs/node/commit/ff877e93e1)] - **src**: fix stuck debugger process (Liang-Chi Hsieh) [#2778](https://github.com/nodejs/node/pull/2778)
* [[`8854183fe5`](https://github.com/nodejs/node/commit/8854183fe5)] - **stream**: avoid unnecessary concat of a single buffer. (Calvin Metcalf) [#3300](https://github.com/nodejs/node/pull/3300)
* [[`85b74de9de`](https://github.com/nodejs/node/commit/85b74de9de)] - **stream**: fix signature of _write() in a comment (Fábio Santos) [#3248](https://github.com/nodejs/node/pull/3248)
* [[`b8cea49c88`](https://github.com/nodejs/node/commit/b8cea49c88)] - **test**: fix heap-profiler link error LNK1194 on win (Junliang Yan) [#3572](https://github.com/nodejs/node/pull/3572)
* [[`4a5dbeab43`](https://github.com/nodejs/node/commit/4a5dbeab43)] - **test**: fix missing unistd.h on windows (Junliang Yan) [#3532](https://github.com/nodejs/node/pull/3532)
* [[`74e2328b3a`](https://github.com/nodejs/node/commit/74e2328b3a)] - **test**: split independent tests into separate files (Rich Trott) [#3548](https://github.com/nodejs/node/pull/3548)
* [[`8c6c0f915a`](https://github.com/nodejs/node/commit/8c6c0f915a)] - **test**: use port number from env in tls socket test (Stefan Budeanu) [#3557](https://github.com/nodejs/node/pull/3557)
* [[`1a968e67a5`](https://github.com/nodejs/node/commit/1a968e67a5)] - **test**: improve tests for util.inherits (Michaël Zasso) [#3507](https://github.com/nodejs/node/pull/3507)
* [[`9d8d752456`](https://github.com/nodejs/node/commit/9d8d752456)] - **test**: print helpful err msg on test-dns-ipv6.js (Junliang Yan) [#3501](https://github.com/nodejs/node/pull/3501)
* [[`60de9f8d7b`](https://github.com/nodejs/node/commit/60de9f8d7b)] - **test**: wrap assert.fail when passed to callback (Myles Borins) [#3453](https://github.com/nodejs/node/pull/3453)
* [[`cd83f7ed7f`](https://github.com/nodejs/node/commit/cd83f7ed7f)] - **test**: add node::MakeCallback() test coverage (Ben Noordhuis) [#3478](https://github.com/nodejs/node/pull/3478)
* [[`08da5c2a06`](https://github.com/nodejs/node/commit/08da5c2a06)] - **test**: disable test-tick-processor - aix and be ppc (Michael Dawson) [#3491](https://github.com/nodejs/node/pull/3491)
* [[`7c35fbcb14`](https://github.com/nodejs/node/commit/7c35fbcb14)] - **test**: harden test-child-process-fork-regr-gh-2847 (Michael Dawson) [#3459](https://github.com/nodejs/node/pull/3459)
* [[`ad2b272417`](https://github.com/nodejs/node/commit/ad2b272417)] - **test**: fix test-net-keepalive for AIX (Imran Iqbal) [#3458](https://github.com/nodejs/node/pull/3458)
* [[`04fb14cc35`](https://github.com/nodejs/node/commit/04fb14cc35)] - **test**: fix flaky test-child-process-emfile (Rich Trott) [#3430](https://github.com/nodejs/node/pull/3430)
* [[`eef0f0cd63`](https://github.com/nodejs/node/commit/eef0f0cd63)] - **test**: remove flaky status from eval_messages test (Rich Trott) [#3420](https://github.com/nodejs/node/pull/3420)
* [[`bbbd81eab2`](https://github.com/nodejs/node/commit/bbbd81eab2)] - **test**: skip test-dns-ipv6.js if ipv6 is unavailable (Junliang Yan) [#3444](https://github.com/nodejs/node/pull/3444)
* [[`f78c8e7426`](https://github.com/nodejs/node/commit/f78c8e7426)] - **test**: fix flaky test for symlinks (Rich Trott) [#3418](https://github.com/nodejs/node/pull/3418)
* [[`28e9a4f41b`](https://github.com/nodejs/node/commit/28e9a4f41b)] - **test**: repl-persistent-history is no longer flaky (Jeremiah Senkpiel) [#3437](https://github.com/nodejs/node/pull/3437)
* [[`9e981556e5`](https://github.com/nodejs/node/commit/9e981556e5)] - **test**: cleanup, improve repl-persistent-history (Jeremiah Senkpiel) [#2356](https://github.com/nodejs/node/pull/2356)
* [[`ee2e641e0a`](https://github.com/nodejs/node/commit/ee2e641e0a)] - **test**: add Symbol test for assert.deepEqual() (Rich Trott) [#3327](https://github.com/nodejs/node/pull/3327)
* [[`e2b8393ee8`](https://github.com/nodejs/node/commit/e2b8393ee8)] - **test**: port domains regression test from v0.10 (Jonas Dohse) [#3356](https://github.com/nodejs/node/pull/3356)
* [[`676e61872f`](https://github.com/nodejs/node/commit/676e61872f)] - **test**: apply correct assert.fail() arguments (Rich Trott) [#3378](https://github.com/nodejs/node/pull/3378)
* [[`bbdbef9274`](https://github.com/nodejs/node/commit/bbdbef9274)] - **test**: fix tests after V8 upgrade (Michaël Zasso) [#3351](https://github.com/nodejs/node/pull/3351)
* [[`6c032a8333`](https://github.com/nodejs/node/commit/6c032a8333)] - **test**: replace util with backtick strings (Myles Borins) [#3359](https://github.com/nodejs/node/pull/3359)
* [[`f45c315763`](https://github.com/nodejs/node/commit/f45c315763)] - **test**: fix domain with abort-on-uncaught on PPC (Julien Gilli) [#3354](https://github.com/nodejs/node/pull/3354)
* [[`e3d9d25083`](https://github.com/nodejs/node/commit/e3d9d25083)] - **test**: add test-child-process-emfile fail message (Rich Trott) [#3335](https://github.com/nodejs/node/pull/3335)
* [[`6f14b3a7db`](https://github.com/nodejs/node/commit/6f14b3a7db)] - **test**: remove util from common (Rich Trott) [#3324](https://github.com/nodejs/node/pull/3324)
* [[`7d94611ac9`](https://github.com/nodejs/node/commit/7d94611ac9)] - **test**: split up buffer tests for reliability (Rich Trott) [#3323](https://github.com/nodejs/node/pull/3323)
* [[`3202456baa`](https://github.com/nodejs/node/commit/3202456baa)] - **test**: remove util properties from common (Rich Trott) [#3304](https://github.com/nodejs/node/pull/3304)
* [[`31c971d641`](https://github.com/nodejs/node/commit/31c971d641)] - **test**: parallelize long-running test (Rich Trott) [#3287](https://github.com/nodejs/node/pull/3287)
* [[`5bbc6df7de`](https://github.com/nodejs/node/commit/5bbc6df7de)] - **test**: change call to deprecated util.isError() (Rich Trott) [#3084](https://github.com/nodejs/node/pull/3084)
* [[`522e3d3cd3`](https://github.com/nodejs/node/commit/522e3d3cd3)] - **timers**: reuse timer in `setTimeout().unref()` (Fedor Indutny) [#3407](https://github.com/nodejs/node/pull/3407)
* [[`b64ce5960f`](https://github.com/nodejs/node/commit/b64ce5960f)] - **tls**: remove util and calls to util.format (Myles Borins) [#3456](https://github.com/nodejs/node/pull/3456)
* [[`c64af7d99e`](https://github.com/nodejs/node/commit/c64af7d99e)] - **tls**: TLSSocket options default isServer false (Yuval Brik) [#2614](https://github.com/nodejs/node/pull/2614)
* [[`2296a4fc0f`](https://github.com/nodejs/node/commit/2296a4fc0f)] - **(SEMVER-MINOR)** **tls**: add `options` argument to createSecurePair (Коренберг Марк) [#2441](https://github.com/nodejs/node/pull/2441)
* [[`0140e1b5e3`](https://github.com/nodejs/node/commit/0140e1b5e3)] - **tls**: output warning of setDHParam to console.trace (Shigeki Ohtsu) [#1831](https://github.com/nodejs/node/pull/1831)
* [[`f72e178a78`](https://github.com/nodejs/node/commit/f72e178a78)] - **(SEMVER-MAJOR)** **tls**: add minDHSize option to tls.connect() (Shigeki Ohtsu) [#1831](https://github.com/nodejs/node/pull/1831)
* [[`6d92ebac11`](https://github.com/nodejs/node/commit/6d92ebac11)] - **tls**: add TLSSocket.getEphemeralKeyInfo() (Shigeki Ohtsu) [#1831](https://github.com/nodejs/node/pull/1831)
* [[`62ad1d0113`](https://github.com/nodejs/node/commit/62ad1d0113)] - **(SEMVER-MINOR)** **tls, crypto**: add ALPN Support (Shigeki Ohtsu) [#2564](https://github.com/nodejs/node/pull/2564)
* [[`5029f41b2f`](https://github.com/nodejs/node/commit/5029f41b2f)] - **(SEMVER-MINOR)** **tls,crypto**: move NPN protcol data to hidden value (Shigeki Ohtsu) [#2564](https://github.com/nodejs/node/pull/2564)
* [[`701e38c25f`](https://github.com/nodejs/node/commit/701e38c25f)] - **tools**: enable prefer-const eslint rule (Sakthipriyan Vairamani) [#3152](https://github.com/nodejs/node/pull/3152)
* [[`6e78382605`](https://github.com/nodejs/node/commit/6e78382605)] - **tools**: ensure npm always uses the local node (Jeremiah Senkpiel) [#3489](https://github.com/nodejs/node/pull/3489)
* [[`3c3435d017`](https://github.com/nodejs/node/commit/3c3435d017)] - **tools**: update test-npm to work with npm 3 (Rebecca Turner) [#3489](https://github.com/nodejs/node/pull/3489)
* [[`b4f4c24539`](https://github.com/nodejs/node/commit/b4f4c24539)] - **tools**: use absolute paths in test-npm (Rebecca Turner) [#3309](https://github.com/nodejs/node/pull/3309)
* [[`80573153b8`](https://github.com/nodejs/node/commit/80573153b8)] - **(SEMVER-MAJOR)** **util**: make inherits work with classes (Michaël Zasso) [#3455](https://github.com/nodejs/node/pull/3455)
* [[`412252ca04`](https://github.com/nodejs/node/commit/412252ca04)] - **(SEMVER-MAJOR)** **util**: Remove p, has been deprecated for years (Wyatt Preul) [#3432](https://github.com/nodejs/node/pull/3432)
* [[`718c304a4f`](https://github.com/nodejs/node/commit/718c304a4f)] - **v8**: pull fix for builtin code size on PPC (Michael Dawson) [#3474](https://github.com/nodejs/node/pull/3474)
* [[`6936468de2`](https://github.com/nodejs/node/commit/6936468de2)] - **vm**: remove Watchdog dependency on Environment (Ido Ben-Yair) [#3274](https://github.com/nodejs/node/pull/3274)
* [[`80169b1f0a`](https://github.com/nodejs/node/commit/80169b1f0a)] - **(SEMVER-MAJOR)** **zlib**: decompression throw on truncated input (Yuval Brik) [#2595](https://github.com/nodejs/node/pull/2595)

## 2015-10-13, Version 4.2.1 'Argon' (LTS), @jasnell

### Notable changes

* Includes fixes for two regressions
  - Assertion error in WeakCallback  - see [#3329](https://github.com/nodejs/node/pull/3329)
  - Undefined timeout regression - see [#3331](https://github.com/nodejs/node/pull/3331)

### Known issues

* When a server queues a large amount of data to send to a client over a pipelined HTTP connection, the underlying socket may be destroyed. See [#3332](https://github.com/nodejs/node/issues/3332) and [#3342](https://github.com/nodejs/node/pull/3342).
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`b3cbd13340`](https://github.com/nodejs/node/commit/b3cbd13340)] - **buffer**: fix assertion error in WeakCallback (Fedor Indutny) [#3329](https://github.com/nodejs/node/pull/3329)
* [[`102cb7288c`](https://github.com/nodejs/node/commit/102cb7288c)] - **doc**: label v4.2.0 as LTS in changelog heading (Rod Vagg) [#3343](https://github.com/nodejs/node/pull/3343)
* [[`c245a199a7`](https://github.com/nodejs/node/commit/c245a199a7)] - **lib**: fix undefined timeout regression (Ryan Graham) [#3331](https://github.com/nodejs/node/pull/3331)

## 2015-10-07, Version 4.2.0 'Argon' (LTS), @jasnell

### Notable changes

The first Node.js LTS release! See https://github.com/nodejs/LTS/ for details of the LTS process.

* **icu**: Updated to version 56 with significant performance improvements (Steven R. Loomis) [#3281](https://github.com/nodejs/node/pull/3281)
* **node**:
  - Added new `-c` (or `--check`) command line argument for checking script syntax without executing the code (Dave Eddy) [#2411](https://github.com/nodejs/node/pull/2411)
  - Added `process.versions.icu` to hold the current ICU library version (Evan Lucas) [#3102](https://github.com/nodejs/node/pull/3102)
  - Added `process.release.lts` to hold the current LTS codename when the binary is from an active LTS release line (Rod Vagg) [#3212](https://github.com/nodejs/node/pull/3212)
* **npm**: Upgraded to npm 2.14.7 from 2.14.4, see [release notes](https://github.com/npm/npm/releases/tag/v2.14.7) for full details (Kat Marchán) [#3299](https://github.com/nodejs/node/pull/3299)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits


* [[`8383c4fe00`](https://github.com/nodejs/node/commit/8383c4fe00)] - **assert**: support arrow functions in .throws() (Ben Noordhuis) [#3276](https://github.com/nodejs/node/pull/3276)
* [[`3eaa593a32`](https://github.com/nodejs/node/commit/3eaa593a32)] - **async_wrap**: correctly pass parent to init callback (Trevor Norris) [#3216](https://github.com/nodejs/node/pull/3216)
* [[`54795620f6`](https://github.com/nodejs/node/commit/54795620f6)] - **buffer**: don't abort on prototype getters (Trevor Norris) [#3302](https://github.com/nodejs/node/pull/3302)
* [[`660f7591c8`](https://github.com/nodejs/node/commit/660f7591c8)] - **buffer**: FreeCallback should be tied to ArrayBuffer (Fedor Indutny) [#3198](https://github.com/nodejs/node/pull/3198)
* [[`651a5b51eb`](https://github.com/nodejs/node/commit/651a5b51eb)] - **buffer**: only check if instance is Uint8Array (Trevor Norris) [#3080](https://github.com/nodejs/node/pull/3080)
* [[`d5a1b1ad7c`](https://github.com/nodejs/node/commit/d5a1b1ad7c)] - **buffer**: clean up usage of __proto__ (Trevor Norris) [#3080](https://github.com/nodejs/node/pull/3080)
* [[`af24376e18`](https://github.com/nodejs/node/commit/af24376e18)] - **build**: Intl: deps: bump ICU to 56.1 (GA) (Steven R. Loomis) [#3281](https://github.com/nodejs/node/pull/3281)
* [[`9136359d57`](https://github.com/nodejs/node/commit/9136359d57)] - **build**: make icu download path customizable (Johan Bergström) [#3200](https://github.com/nodejs/node/pull/3200)
* [[`b3c5ad10a8`](https://github.com/nodejs/node/commit/b3c5ad10a8)] - **build**: add --with-arm-fpu option (Jérémy Lal) [#3228](https://github.com/nodejs/node/pull/3228)
* [[`f00f3268e4`](https://github.com/nodejs/node/commit/f00f3268e4)] - **build**: intl: avoid 'duplicate main()' on ICU 56 (Steven R. Loomis) [#3066](https://github.com/nodejs/node/pull/3066)
* [[`071c72a6a3`](https://github.com/nodejs/node/commit/071c72a6a3)] - **deps**: upgrade to npm 2.14.7 (Kat Marchán) [#3299](https://github.com/nodejs/node/pull/3299)
* [[`8b50e95f06`](https://github.com/nodejs/node/commit/8b50e95f06)] - **(SEMVER-MINOR)** **deps**: backport 1ee712a from V8 upstream (Julien Gilli) [#3036](https://github.com/nodejs/node/pull/3036)
* [[`747271372f`](https://github.com/nodejs/node/commit/747271372f)] - **doc**: update the assert module summary (David Boivin) [#2799](https://github.com/nodejs/node/pull/2799)
* [[`0d506556b0`](https://github.com/nodejs/node/commit/0d506556b0)] - **doc**: replace node-gyp link with nodejs/node-gyp (Roman Klauke) [#3320](https://github.com/nodejs/node/pull/3320)
* [[`40a159e4f4`](https://github.com/nodejs/node/commit/40a159e4f4)] - **doc**: Amend capitalization of word JavaScript (Dave Hodder) [#3285](https://github.com/nodejs/node/pull/3285)
* [[`6dd34761fd`](https://github.com/nodejs/node/commit/6dd34761fd)] - **doc**: add method links in dns.markdown (Alejandro Oviedo) [#3196](https://github.com/nodejs/node/pull/3196)
* [[`333e8336be`](https://github.com/nodejs/node/commit/333e8336be)] - **doc**: add method links in child_process.markdown (Alejandro Oviedo) [#3186](https://github.com/nodejs/node/pull/3186)
* [[`0cfc6d39ca`](https://github.com/nodejs/node/commit/0cfc6d39ca)] - **doc**: recommend Infinity on emitter.setMaxListeners (Jason Karns) [#2559](https://github.com/nodejs/node/pull/2559)
* [[`d4fc6d93ef`](https://github.com/nodejs/node/commit/d4fc6d93ef)] - **doc**: add help repo link to CONTRIBUTING.md (Doug Shamoo) [#3233](https://github.com/nodejs/node/pull/3233)
* [[`28aac7f19d`](https://github.com/nodejs/node/commit/28aac7f19d)] - **doc**: add TLS session resumption example (Roman Reiss) [#3147](https://github.com/nodejs/node/pull/3147)
* [[`365cf22cce`](https://github.com/nodejs/node/commit/365cf22cce)] - **doc**: update AUTHORS list (Rod Vagg) [#3211](https://github.com/nodejs/node/pull/3211)
* [[`d4399613b7`](https://github.com/nodejs/node/commit/d4399613b7)] - **doc**: standardize references to userland (Martial) [#3192](https://github.com/nodejs/node/pull/3192)
* [[`75de258376`](https://github.com/nodejs/node/commit/75de258376)] - **doc**: fix spelling in Buffer documentation (Rod Machen) [#3226](https://github.com/nodejs/node/pull/3226)
* [[`725c7276dd`](https://github.com/nodejs/node/commit/725c7276dd)] - **doc**: fix README.md link to joyent/node intl wiki (Steven R. Loomis) [#3067](https://github.com/nodejs/node/pull/3067)
* [[`4a35ba4966`](https://github.com/nodejs/node/commit/4a35ba4966)] - **(SEMVER-MINOR)** **fs**: include filename in watch errors (charlierudolph) [#2748](https://github.com/nodejs/node/pull/2748)
* [[`2ddbbfd164`](https://github.com/nodejs/node/commit/2ddbbfd164)] - **http**: cork/uncork before flushing pipelined res (Fedor Indutny) [#3172](https://github.com/nodejs/node/pull/3172)
* [[`f638402e2f`](https://github.com/nodejs/node/commit/f638402e2f)] - **http**: add comment about `outputSize` in res/server (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* [[`1850879b0e`](https://github.com/nodejs/node/commit/1850879b0e)] - **js_stream**: prevent abort if isalive doesn't exist (Trevor Norris) [#3282](https://github.com/nodejs/node/pull/3282)
* [[`63644dd1cd`](https://github.com/nodejs/node/commit/63644dd1cd)] - **lib**: remove redundant code, add tests in timers.js (Rich Trott) [#3143](https://github.com/nodejs/node/pull/3143)
* [[`74f443583c`](https://github.com/nodejs/node/commit/74f443583c)] - **module**: use UNC paths when loading native addons (Justin Chase) [#2965](https://github.com/nodejs/node/pull/2965)
* [[`01cb3fc36b`](https://github.com/nodejs/node/commit/01cb3fc36b)] - **net**: don't throw on bytesWritten access (Trevor Norris) [#3305](https://github.com/nodejs/node/pull/3305)
* [[`9d65528b01`](https://github.com/nodejs/node/commit/9d65528b01)] - **(SEMVER-MINOR)** **node**: add -c|--check CLI arg to syntax check script (Dave Eddy) [#2411](https://github.com/nodejs/node/pull/2411)
* [[`42b936e78d`](https://github.com/nodejs/node/commit/42b936e78d)] - **(SEMVER-MINOR)** **src**: add process.release.lts property (Rod Vagg) [#3212](https://github.com/nodejs/node/pull/3212)
* [[`589287b2e3`](https://github.com/nodejs/node/commit/589287b2e3)] - **src**: convert BE-utf16-string to LE before search (Karl Skomski) [#3295](https://github.com/nodejs/node/pull/3295)
* [[`2314378f06`](https://github.com/nodejs/node/commit/2314378f06)] - **src**: fix u-a-free if uv returns err in ASYNC_CALL (Karl Skomski) [#3049](https://github.com/nodejs/node/pull/3049)
* [[`d99336a391`](https://github.com/nodejs/node/commit/d99336a391)] - **(SEMVER-MINOR)** **src**: replace naive search in Buffer::IndexOf (Karl Skomski) [#2539](https://github.com/nodejs/node/pull/2539)
* [[`546e8333ba`](https://github.com/nodejs/node/commit/546e8333ba)] - **(SEMVER-MINOR)** **src**: fix --abort-on-uncaught-exception (Jeremy Whitlock) [#3036](https://github.com/nodejs/node/pull/3036)
* [[`7271cb047c`](https://github.com/nodejs/node/commit/7271cb047c)] - **(SEMVER-MINOR)** **src**: add process.versions.icu (Evan Lucas) [#3102](https://github.com/nodejs/node/pull/3102)
* [[`7b9f78acb2`](https://github.com/nodejs/node/commit/7b9f78acb2)] - **stream**: avoid pause with unpipe in buffered write (Brian White) [#2325](https://github.com/nodejs/node/pull/2325)
* [[`f0f8afd879`](https://github.com/nodejs/node/commit/f0f8afd879)] - **test**: remove common.inspect() (Rich Trott) [#3257](https://github.com/nodejs/node/pull/3257)
* [[`5ca4f6f8bd`](https://github.com/nodejs/node/commit/5ca4f6f8bd)] - **test**: test `util` rather than `common` (Rich Trott) [#3256](https://github.com/nodejs/node/pull/3256)
* [[`7a5ae34345`](https://github.com/nodejs/node/commit/7a5ae34345)] - **test**: refresh temp directory when using pipe (Rich Trott) [#3231](https://github.com/nodejs/node/pull/3231)
* [[`7c85557ef0`](https://github.com/nodejs/node/commit/7c85557ef0)] - **test**: Fix test-fs-read-stream-fd-leak race cond (Junliang Yan) [#3218](https://github.com/nodejs/node/pull/3218)
* [[`26a7ec6960`](https://github.com/nodejs/node/commit/26a7ec6960)] - **test**: fix losing original env vars issue (Junliang Yan) [#3190](https://github.com/nodejs/node/pull/3190)
* [[`e922716192`](https://github.com/nodejs/node/commit/e922716192)] - **test**: remove deprecated error logging (Rich Trott) [#3079](https://github.com/nodejs/node/pull/3079)
* [[`8f29d95a8c`](https://github.com/nodejs/node/commit/8f29d95a8c)] - **test**: report timeout in TapReporter (Karl Skomski) [#2647](https://github.com/nodejs/node/pull/2647)
* [[`2d0fe4c657`](https://github.com/nodejs/node/commit/2d0fe4c657)] - **test**: linting for buffer-free-callback test (Rich Trott) [#3230](https://github.com/nodejs/node/pull/3230)
* [[`70c9e4337e`](https://github.com/nodejs/node/commit/70c9e4337e)] - **test**: make common.js mandatory via linting rule (Rich Trott) [#3157](https://github.com/nodejs/node/pull/3157)
* [[`b7179562aa`](https://github.com/nodejs/node/commit/b7179562aa)] - **test**: load common.js in all tests (Rich Trott) [#3157](https://github.com/nodejs/node/pull/3157)
* [[`bab555a1c1`](https://github.com/nodejs/node/commit/bab555a1c1)] - **test**: speed up stringbytes-external test (Evan Lucas) [#3005](https://github.com/nodejs/node/pull/3005)
* [[`ddf258376d`](https://github.com/nodejs/node/commit/ddf258376d)] - **test**: use normalize() for unicode paths (Roman Reiss) [#3007](https://github.com/nodejs/node/pull/3007)
* [[`46876d519c`](https://github.com/nodejs/node/commit/46876d519c)] - **test**: remove arguments.callee usage (Roman Reiss) [#3167](https://github.com/nodejs/node/pull/3167)
* [[`af10df6108`](https://github.com/nodejs/node/commit/af10df6108)] - **tls**: use parent handle's close callback (Fedor Indutny) [#2991](https://github.com/nodejs/node/pull/2991)
* [[`9c2748bad1`](https://github.com/nodejs/node/commit/9c2748bad1)] - **tools**: remove leftover license boilerplate (Nathan Rajlich) [#3225](https://github.com/nodejs/node/pull/3225)
* [[`5d9f83ff2a`](https://github.com/nodejs/node/commit/5d9f83ff2a)] - **tools**: apply linting to custom rules code (Rich Trott) [#3195](https://github.com/nodejs/node/pull/3195)
* [[`18a8b2ec73`](https://github.com/nodejs/node/commit/18a8b2ec73)] - **tools**: remove unused gflags module (Ben Noordhuis) [#3220](https://github.com/nodejs/node/pull/3220)
* [[`e0fffca836`](https://github.com/nodejs/node/commit/e0fffca836)] - **util**: fix for inspecting promises (Evan Lucas) [#3221](https://github.com/nodejs/node/pull/3221)
* [[`8dfdee3733`](https://github.com/nodejs/node/commit/8dfdee3733)] - **util**: correctly inspect Map/Set Iterators (Evan Lucas) [#3119](https://github.com/nodejs/node/pull/3119)
* [[`b5c51fdba0`](https://github.com/nodejs/node/commit/b5c51fdba0)] - **util**: fix check for Array constructor (Evan Lucas) [#3119](https://github.com/nodejs/node/pull/3119)

## 2015-10-05, Version 4.1.2 (Stable), @rvagg

### Notable changes

* **http**:
  - Fix out-of-order 'finish' event bug in pipelining that can abort execution, fixes DoS vulnerability [CVE-2015-7384](https://github.com/nodejs/node/issues/3138) (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
  - Account for pending response data instead of just the data on the current request to decide whether pause the socket or not (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* **libuv**: Upgraded from v1.7.4 to v1.7.5, see [release notes](https://github.com/libuv/libuv/releases/tag/v1.7.5) for details (Saúl Ibarra Corretgé) [#3010](https://github.com/nodejs/node/pull/3010)
  - A better rwlock implementation for all Windows versions
  - Improved AIX support
* **v8**:
  - Upgraded from v4.5.103.33 to v4.5.103.35 (Ali Ijaz Sheikh) [#3117](https://github.com/nodejs/node/pull/3117)
  - Backported [f782159](https://codereview.chromium.org/1367123003) from v8's upstream to help speed up Promise introspection (Ben Noordhuis) [#3130](https://github.com/nodejs/node/pull/3130)
  - Backported [c281c15](https://codereview.chromium.org/1363683002) from v8's upstream to add JSTypedArray length in post-mortem metadata (Julien Gilli) [#3031](https://github.com/nodejs/node/pull/3031)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`39b8730e8b`](https://github.com/nodejs/node/commit/39b8730e8b)] - **async_wrap**: ensure all objects have internal field (Trevor Norris) [#3139](https://github.com/nodejs/node/pull/3139)
* [[`99e66074d7`](https://github.com/nodejs/node/commit/99e66074d7)] - **async_wrap**: update providers and add test (Trevor Norris) [#3139](https://github.com/nodejs/node/pull/3139)
* [[`7a58157d4e`](https://github.com/nodejs/node/commit/7a58157d4e)] - **benchmark**: update comment in common.js (Minwoo Jung) [#2399](https://github.com/nodejs/node/pull/2399)
* [[`9e9bfa4dc0`](https://github.com/nodejs/node/commit/9e9bfa4dc0)] - **build**: iojs -> nodejs of release-urlbase (Minqi Pan) [#3015](https://github.com/nodejs/node/pull/3015)
* [[`8335ec7191`](https://github.com/nodejs/node/commit/8335ec7191)] - **build**: fix some typos inside the configure script (Minqi Pan) [#3016](https://github.com/nodejs/node/pull/3016)
* [[`d6ac547d5d`](https://github.com/nodejs/node/commit/d6ac547d5d)] - **build,win**: fix node.exe resource version (João Reis) [#3053](https://github.com/nodejs/node/pull/3053)
* [[`798dad24f4`](https://github.com/nodejs/node/commit/798dad24f4)] - **child_process**: `null` channel handle on close (Fedor Indutny) [#3041](https://github.com/nodejs/node/pull/3041)
* [[`e5615854ea`](https://github.com/nodejs/node/commit/e5615854ea)] - **contextify**: use CHECK instead of `if` (Oguz Bastemur) [#3125](https://github.com/nodejs/node/pull/3125)
* [[`f055a66a38`](https://github.com/nodejs/node/commit/f055a66a38)] - **crypto**: enable FIPS only when configured with it (Fedor Indutny) [#3153](https://github.com/nodejs/node/pull/3153)
* [[`4c8d96bc30`](https://github.com/nodejs/node/commit/4c8d96bc30)] - **crypto**: add more keylen sanity checks in pbkdf2 (Johann) [#3029](https://github.com/nodejs/node/pull/3029)
* [[`4c5940776c`](https://github.com/nodejs/node/commit/4c5940776c)] - **deps**: upgrade libuv to 1.7.5 (Saúl Ibarra Corretgé) [#3010](https://github.com/nodejs/node/pull/3010)
* [[`5a9e795577`](https://github.com/nodejs/node/commit/5a9e795577)] - **deps**: upgrade V8 to 4.5.103.35 (Ali Ijaz Sheikh) [#3117](https://github.com/nodejs/node/pull/3117)
* [[`925b29f959`](https://github.com/nodejs/node/commit/925b29f959)] - **deps**: backport f782159 from v8's upstream (Ben Noordhuis) [#3130](https://github.com/nodejs/node/pull/3130)
* [[`039f73fa83`](https://github.com/nodejs/node/commit/039f73fa83)] - **deps**: remove and gitignore .bin directory (Ben Noordhuis) [#3004](https://github.com/nodejs/node/pull/3004)
* [[`5fbb24812d`](https://github.com/nodejs/node/commit/5fbb24812d)] - **deps**: backport c281c15 from V8's upstream (Julien Gilli) [#3031](https://github.com/nodejs/node/pull/3031)
* [[`6ee5d0f69f`](https://github.com/nodejs/node/commit/6ee5d0f69f)] - **dns**: add missing exports.BADNAME (Roman Reiss) [#3051](https://github.com/nodejs/node/pull/3051)
* [[`f92aee7170`](https://github.com/nodejs/node/commit/f92aee7170)] - **doc**: fix outdated 'try/catch' statement in sync (Minwoo Jung) [#3087](https://github.com/nodejs/node/pull/3087)
* [[`c7161f39e8`](https://github.com/nodejs/node/commit/c7161f39e8)] - **doc**: add TSC meeting minutes 2015-09-16 (Rod Vagg) [#3023](https://github.com/nodejs/node/pull/3023)
* [[`928166c4a8`](https://github.com/nodejs/node/commit/928166c4a8)] - **doc**: copyedit fs.watch() information (Rich Trott) [#3097](https://github.com/nodejs/node/pull/3097)
* [[`75d5dcea76`](https://github.com/nodejs/node/commit/75d5dcea76)] - **doc**: jenkins-iojs.nodesource.com -> ci.nodejs.org (Michał Gołębiowski) [#2886](https://github.com/nodejs/node/pull/2886)
* [[`5c3f50b21d`](https://github.com/nodejs/node/commit/5c3f50b21d)] - **doc**: rearrange execSync and execFileSync (Laurent Fortin) [#2940](https://github.com/nodejs/node/pull/2940)
* [[`4fc33ac11a`](https://github.com/nodejs/node/commit/4fc33ac11a)] - **doc**: make execFileSync in line with execFile (Laurent Fortin) [#2940](https://github.com/nodejs/node/pull/2940)
* [[`a366e84b17`](https://github.com/nodejs/node/commit/a366e84b17)] - **doc**: fix typos in cluster & errors (reggi) [#3011](https://github.com/nodejs/node/pull/3011)
* [[`52031e1bf1`](https://github.com/nodejs/node/commit/52031e1bf1)] - **doc**: switch LICENSE from closure-linter to eslint (Minqi Pan) [#3018](https://github.com/nodejs/node/pull/3018)
* [[`b28f6a53bc`](https://github.com/nodejs/node/commit/b28f6a53bc)] - **docs**: Clarify assert.doesNotThrow behavior (Fabio Oliveira) [#2807](https://github.com/nodejs/node/pull/2807)
* [[`99943e189d`](https://github.com/nodejs/node/commit/99943e189d)] - **http**: fix out-of-order 'finish' bug in pipelining (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* [[`fb7a491d1c`](https://github.com/nodejs/node/commit/fb7a491d1c)] - **http_server**: pause socket properly (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* [[`a0b35bfcf3`](https://github.com/nodejs/node/commit/a0b35bfcf3)] - **i18n**: add caller to removal list for bidi in ICU55 (Michael Dawson) [#3115](https://github.com/nodejs/node/pull/3115)
* [[`ac2bce0b0c`](https://github.com/nodejs/node/commit/ac2bce0b0c)] - **path**: improve posixSplitPath performance (Evan Lucas) [#3034](https://github.com/nodejs/node/pull/3034)
* [[`37cdeafa2f`](https://github.com/nodejs/node/commit/37cdeafa2f)] - **smalloc**: remove module (Brendan Ashworth) [#3099](https://github.com/nodejs/node/pull/3099)
* [[`5ec5d0aa8b`](https://github.com/nodejs/node/commit/5ec5d0aa8b)] - **src**: internalize binding function property names (Ben Noordhuis) [#3060](https://github.com/nodejs/node/pull/3060)
* [[`c8175fc2af`](https://github.com/nodejs/node/commit/c8175fc2af)] - **src**: internalize per-isolate string properties (Ben Noordhuis) [#3060](https://github.com/nodejs/node/pull/3060)
* [[`9a593abc47`](https://github.com/nodejs/node/commit/9a593abc47)] - **src**: include signal.h in util.h (Cheng Zhao) [#3058](https://github.com/nodejs/node/pull/3058)
* [[`fde0c6f321`](https://github.com/nodejs/node/commit/fde0c6f321)] - **src**: fix function and variable names in comments (Sakthipriyan Vairamani) [#3039](https://github.com/nodejs/node/pull/3039)
* [[`1cc7b41ba4`](https://github.com/nodejs/node/commit/1cc7b41ba4)] - **stream_wrap**: support empty `TryWrite`s (Fedor Indutny) [#3128](https://github.com/nodejs/node/pull/3128)
* [[`9faf4c6fcf`](https://github.com/nodejs/node/commit/9faf4c6fcf)] - **test**: load common.js to test for global leaks (Rich Trott) [#3095](https://github.com/nodejs/node/pull/3095)
* [[`0858c86374`](https://github.com/nodejs/node/commit/0858c86374)] - **test**: fix invalid variable name (Sakthipriyan Vairamani) [#3150](https://github.com/nodejs/node/pull/3150)
* [[`1167171004`](https://github.com/nodejs/node/commit/1167171004)] - **test**: change calls to deprecated util.print() (Rich Trott) [#3083](https://github.com/nodejs/node/pull/3083)
* [[`5ada45bf28`](https://github.com/nodejs/node/commit/5ada45bf28)] - **test**: replace deprecated util.debug() calls (Rich Trott) [#3082](https://github.com/nodejs/node/pull/3082)
* [[`d8ab4e185d`](https://github.com/nodejs/node/commit/d8ab4e185d)] - **util**: optimize promise introspection (Ben Noordhuis) [#3130](https://github.com/nodejs/node/pull/3130)

## 2015-09-22, Version 4.1.1 (Stable), @rvagg

### Notable changes

* **buffer**: Fixed a bug introduced in v4.1.0 where allocating a new zero-length buffer can result in the _next_ allocation of a TypedArray in JavaScript not being zero-filled. In certain circumstances this could result in data leakage via reuse of memory space in TypedArrays, breaking the normally safe assumption that TypedArrays should be always zero-filled. (Trevor Norris) [#2931](https://github.com/nodejs/node/pull/2931).
* **http**: Guard against response-splitting of HTTP trailing headers added via [`response.addTrailers()`](https://nodejs.org/api/http.html#http_response_addtrailers_headers) by removing new-line (`[\r\n]`) characters from values. Note that standard header values are already stripped of new-line characters. The expected security impact is low because trailing headers are rarely used. (Ben Noordhuis) [#2945](https://github.com/nodejs/node/pull/2945).
* **npm**: Upgrade to npm 2.14.4 from 2.14.3, see [release notes](https://github.com/npm/npm/releases/tag/v2.14.4) for full details (Kat Marchán) [#2958](https://github.com/nodejs/node/pull/2958)
  - Upgrades `graceful-fs` on multiple dependencies to no longer rely on monkey-patching `fs`
  - Fix `npm link` for pre-release / RC builds of Node
* **v8**: Update post-mortem metadata to allow post-mortem debugging tools to find and inspect:
  - JavaScript objects that use dictionary properties (Julien Gilli) [#2959](https://github.com/nodejs/node/pull/2959)
  - ScopeInfo and thus closures (Julien Gilli) [#2974](https://github.com/nodejs/node/pull/2974)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`d63e02e08d`](https://github.com/nodejs/node/commit/d63e02e08d)] - **buffer**: don't set zero fill for zero-length buffer (Trevor Norris) [#2931](https://github.com/nodejs/node/pull/2931)
* [[`5905b14bff`](https://github.com/nodejs/node/commit/5905b14bff)] - **build**: fix icutrim when building small-icu on BE (Stewart Addison) [#2602](https://github.com/nodejs/node/pull/2602)
* [[`f010cb5d96`](https://github.com/nodejs/node/commit/f010cb5d96)] - **configure**: detect mipsel host (Jérémy Lal) [#2971](https://github.com/nodejs/node/pull/2971)
* [[`b93ad5abbd`](https://github.com/nodejs/node/commit/b93ad5abbd)] - **deps**: backport 357e6b9 from V8's upstream (Julien Gilli) [#2974](https://github.com/nodejs/node/pull/2974)
* [[`8da3da4d41`](https://github.com/nodejs/node/commit/8da3da4d41)] - **deps**: backport ff7d70b from V8's upstream (Julien Gilli) [#2959](https://github.com/nodejs/node/pull/2959)
* [[`2600fb8ae6`](https://github.com/nodejs/node/commit/2600fb8ae6)] - **deps**: upgraded to node-gyp@3.0.3 in npm (Kat Marchán) [#2958](https://github.com/nodejs/node/pull/2958)
* [[`793aad2d7a`](https://github.com/nodejs/node/commit/793aad2d7a)] - **deps**: upgrade to npm 2.14.4 (Kat Marchán) [#2958](https://github.com/nodejs/node/pull/2958)
* [[`43e2b7f836`](https://github.com/nodejs/node/commit/43e2b7f836)] - **doc**: remove usage of events.EventEmitter (Sakthipriyan Vairamani) [#2921](https://github.com/nodejs/node/pull/2921)
* [[`9c59d2f16a`](https://github.com/nodejs/node/commit/9c59d2f16a)] - **doc**: remove extra using v8::HandleScope statement (Christopher J. Brody) [#2983](https://github.com/nodejs/node/pull/2983)
* [[`f7edbab367`](https://github.com/nodejs/node/commit/f7edbab367)] - **doc**: clarify description of assert.ifError() (Rich Trott) [#2941](https://github.com/nodejs/node/pull/2941)
* [[`b2ddf0f9a2`](https://github.com/nodejs/node/commit/b2ddf0f9a2)] - **doc**: refine process.kill() and exit explanations (Rich Trott) [#2918](https://github.com/nodejs/node/pull/2918)
* [[`f68fed2e6f`](https://github.com/nodejs/node/commit/f68fed2e6f)] - **http**: remove redundant code in _deferToConnect (Malcolm Ahoy) [#2769](https://github.com/nodejs/node/pull/2769)
* [[`f542e74c93`](https://github.com/nodejs/node/commit/f542e74c93)] - **http**: guard against response splitting in trailers (Ben Noordhuis) [#2945](https://github.com/nodejs/node/pull/2945)
* [[`bc9f629387`](https://github.com/nodejs/node/commit/bc9f629387)] - **http_parser**: do not dealloc during kOnExecute (Fedor Indutny) [#2956](https://github.com/nodejs/node/pull/2956)
* [[`1860e0cebd`](https://github.com/nodejs/node/commit/1860e0cebd)] - **lib,src**: remove usage of events.EventEmitter (Sakthipriyan Vairamani) [#2921](https://github.com/nodejs/node/pull/2921)
* [[`d4cd5ac407`](https://github.com/nodejs/node/commit/d4cd5ac407)] - **readline**: fix tab completion bug (Matt Harrison) [#2816](https://github.com/nodejs/node/pull/2816)
* [[`9760e04839`](https://github.com/nodejs/node/commit/9760e04839)] - **repl**: don't use tty control codes when $TERM is set to "dumb" (Salman Aljammaz) [#2712](https://github.com/nodejs/node/pull/2712)
* [[`cb971cc97d`](https://github.com/nodejs/node/commit/cb971cc97d)] - **repl**: backslash bug fix (Sakthipriyan Vairamani) [#2968](https://github.com/nodejs/node/pull/2968)
* [[`2034f68668`](https://github.com/nodejs/node/commit/2034f68668)] - **src**: honor --abort_on_uncaught_exception flag (Evan Lucas) [#2776](https://github.com/nodejs/node/pull/2776)
* [[`0b1ca4a9ef`](https://github.com/nodejs/node/commit/0b1ca4a9ef)] - **src**: Add ABORT macro (Evan Lucas) [#2776](https://github.com/nodejs/node/pull/2776)
* [[`4519dd00f9`](https://github.com/nodejs/node/commit/4519dd00f9)] - **test**: test sync version of mkdir & rmdir (Sakthipriyan Vairamani) [#2588](https://github.com/nodejs/node/pull/2588)
* [[`816f609c8b`](https://github.com/nodejs/node/commit/816f609c8b)] - **test**: use tmpDir instead of fixtures in readdir (Sakthipriyan Vairamani) [#2587](https://github.com/nodejs/node/pull/2587)
* [[`2084f52585`](https://github.com/nodejs/node/commit/2084f52585)] - **test**: test more http response splitting scenarios (Ben Noordhuis) [#2945](https://github.com/nodejs/node/pull/2945)
* [[`fa08d1d8a1`](https://github.com/nodejs/node/commit/fa08d1d8a1)] - **test**: add test-spawn-cmd-named-pipe (Alexis Campailla) [#2770](https://github.com/nodejs/node/pull/2770)
* [[`71b5d80682`](https://github.com/nodejs/node/commit/71b5d80682)] - **test**: make cluster tests more time tolerant (Michael Dawson) [#2891](https://github.com/nodejs/node/pull/2891)
* [[`3e09dcfc32`](https://github.com/nodejs/node/commit/3e09dcfc32)] - **test**: update cwd-enoent tests for AIX (Imran Iqbal) [#2909](https://github.com/nodejs/node/pull/2909)
* [[`6ea8ec1c59`](https://github.com/nodejs/node/commit/6ea8ec1c59)] - **tools**: single, cross-platform tick processor (Matt Loring) [#2868](https://github.com/nodejs/node/pull/2868)

## 2015-09-17, Version 4.1.0 (Stable), @Fishrock123

### Notable changes

* **buffer**:
  - Buffers are now created in JavaScript, rather than C++. This increases the speed of buffer creation (Trevor Norris) [#2866](https://github.com/nodejs/node/pull/2866).
  - `Buffer#slice()` now uses `Uint8Array#subarray()` internally, increasing `slice()` performance (Karl Skomski) [#2777](https://github.com/nodejs/node/pull/2777).
* **fs**:
  - `fs.utimes()` now properly converts numeric strings, `NaN`, and `Infinity` (Yazhong Liu) [#2387](https://github.com/nodejs/node/pull/2387).
  - `fs.WriteStream` now implements `_writev`, allowing for super-fast bulk writes (Ron Korving) [#2167](https://github.com/nodejs/node/pull/2167).
* **http**: Fixed an issue with certain `write()` sizes causing errors when using `http.request()` (Fedor Indutny) [#2824](https://github.com/nodejs/node/pull/2824).
* **npm**: Upgrade to version 2.14.3, see https://github.com/npm/npm/releases/tag/v2.14.3 for more details (Kat Marchán) [#2822](https://github.com/nodejs/node/pull/2822).
* **src**: V8 cpu profiling no longer erroneously shows idle time (Oleksandr Chekhovskyi) [#2324](https://github.com/nodejs/node/pull/2324).
* **timers**: `#ref()` and `#unref()` now return the timer they belong to (Sam Roberts) [#2905](https://github.com/nodejs/node/pull/2905).
* **v8**: Lateral upgrade to 4.5.103.33 from 4.5.103.30, contains minor fixes (Ali Ijaz Sheikh) [#2870](https://github.com/nodejs/node/pull/2870).
  - This fixes a previously known bug where some computed object shorthand properties did not work correctly ([#2507](https://github.com/nodejs/node/issues/2507)).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`b1abe812cd`](https://github.com/nodejs/node/commit/b1abe812cd)] - Working on 4.0.1 (Rod Vagg)
* [[`f9f8378853`](https://github.com/nodejs/node/commit/f9f8378853)] - 2015-09-08, Version 4.0.0 (Stable) Release (Rod Vagg)
* [[`9683e5df51`](https://github.com/nodejs/node/commit/9683e5df51)] - **bindings**: close after reading module struct (Fedor Indutny) [#2792](https://github.com/nodejs/node/pull/2792)
* [[`4b4cfa2d44`](https://github.com/nodejs/node/commit/4b4cfa2d44)] - **buffer**: always allocate typed arrays outside heap (Trevor Norris) [#2893](https://github.com/nodejs/node/pull/2893)
* [[`7df018a29b`](https://github.com/nodejs/node/commit/7df018a29b)] - **buffer**: construct Uint8Array in JS (Trevor Norris) [#2866](https://github.com/nodejs/node/pull/2866)
* [[`43397b204e`](https://github.com/nodejs/node/commit/43397b204e)] - **(SEMVER-MINOR)** **build**: Updates to enable AIX support (Michael Dawson) [#2364](https://github.com/nodejs/node/pull/2364)
* [[`e35b1fd610`](https://github.com/nodejs/node/commit/e35b1fd610)] - **build**: clean up the generated tap file (Sakthipriyan Vairamani) [#2837](https://github.com/nodejs/node/pull/2837)
* [[`96670ebe37`](https://github.com/nodejs/node/commit/96670ebe37)] - **deps**: backport 6d32be2 from v8's upstream (Michaël Zasso) [#2916](https://github.com/nodejs/node/pull/2916)
* [[`94972d5b13`](https://github.com/nodejs/node/commit/94972d5b13)] - **deps**: backport 0d01728 from v8's upstream (Fedor Indutny) [#2912](https://github.com/nodejs/node/pull/2912)
* [[`7ebd881c29`](https://github.com/nodejs/node/commit/7ebd881c29)] - **deps**: upgrade V8 to 4.5.103.33 (Ali Ijaz Sheikh) [#2870](https://github.com/nodejs/node/pull/2870)
* [[`ed47ab6e44`](https://github.com/nodejs/node/commit/ed47ab6e44)] - **deps**: upgraded to node-gyp@3.0.3 in npm (Kat Marchán) [#2822](https://github.com/nodejs/node/pull/2822)
* [[`f4641ae875`](https://github.com/nodejs/node/commit/f4641ae875)] - **deps**: upgrade to npm 2.14.3 (Kat Marchán) [#2822](https://github.com/nodejs/node/pull/2822)
* [[`8119693a3d`](https://github.com/nodejs/node/commit/8119693a3d)] - **deps**: update libuv to version 1.7.4 (Saúl Ibarra Corretgé) [#2817](https://github.com/nodejs/node/pull/2817)
* [[`6098504685`](https://github.com/nodejs/node/commit/6098504685)] - **deps**: cherry-pick 6da51b4 from v8's upstream (Fedor Indutny) [#2801](https://github.com/nodejs/node/pull/2801)
* [[`bf42cc8dba`](https://github.com/nodejs/node/commit/bf42cc8dba)] - **doc**: process exit event is not guaranteed to fire (Rich Trott) [#2861](https://github.com/nodejs/node/pull/2861)
* [[`bb0f869f67`](https://github.com/nodejs/node/commit/bb0f869f67)] - **doc**: remove incorrect reference to TCP in net docs (Sam Roberts) [#2903](https://github.com/nodejs/node/pull/2903)
* [[`302d59dce8`](https://github.com/nodejs/node/commit/302d59dce8)] - **doc**: correct buffer.slice arg syntax (Sam Roberts) [#2903](https://github.com/nodejs/node/pull/2903)
* [[`74db9637b7`](https://github.com/nodejs/node/commit/74db9637b7)] - **doc**: describe spawn option.detached (Sam Roberts) [#2903](https://github.com/nodejs/node/pull/2903)
* [[`a7bd897273`](https://github.com/nodejs/node/commit/a7bd897273)] - **doc**: rename from iojs(1) to node(1) in benchmarks (Dmitry Vasilyev) [#2884](https://github.com/nodejs/node/pull/2884)
* [[`cd643d7c37`](https://github.com/nodejs/node/commit/cd643d7c37)] - **doc**: add missing backtick in buffer.markdown (Sven Slootweg) [#2881](https://github.com/nodejs/node/pull/2881)
* [[`e8a206e802`](https://github.com/nodejs/node/commit/e8a206e802)] - **doc**: fix broken link in repl.markdown (Danny Nemer) [#2827](https://github.com/nodejs/node/pull/2827)
* [[`7ee36d61f7`](https://github.com/nodejs/node/commit/7ee36d61f7)] - **doc**: fix typos in README (Ionică Bizău) [#2852](https://github.com/nodejs/node/pull/2852)
* [[`4d1ae26196`](https://github.com/nodejs/node/commit/4d1ae26196)] - **doc**: add tunniclm as a collaborator (Mike Tunnicliffe) [#2826](https://github.com/nodejs/node/pull/2826)
* [[`2d77d03643`](https://github.com/nodejs/node/commit/2d77d03643)] - **doc**: fix two doc errors in stream and process (Jeremiah Senkpiel) [#2549](https://github.com/nodejs/node/pull/2549)
* [[`55ac24f721`](https://github.com/nodejs/node/commit/55ac24f721)] - **doc**: fixed io.js references in process.markdown (Tristian Flanagan) [#2846](https://github.com/nodejs/node/pull/2846)
* [[`cd1297fb57`](https://github.com/nodejs/node/commit/cd1297fb57)] - **doc**: use "Calls" over "Executes" for consistency (Minwoo Jung) [#2800](https://github.com/nodejs/node/pull/2800)
* [[`d664b95581`](https://github.com/nodejs/node/commit/d664b95581)] - **doc**: use US English for consistency (Anne-Gaelle Colom) [#2784](https://github.com/nodejs/node/pull/2784)
* [[`82ba1839fb`](https://github.com/nodejs/node/commit/82ba1839fb)] - **doc**: use 3rd person singular for consistency (Anne-Gaelle Colom) [#2765](https://github.com/nodejs/node/pull/2765)
* [[`432cce6e95`](https://github.com/nodejs/node/commit/432cce6e95)] - **doc**: describe process API for IPC (Sam Roberts) [#1978](https://github.com/nodejs/node/pull/1978)
* [[`1d75012b9d`](https://github.com/nodejs/node/commit/1d75012b9d)] - **doc**: fix comma splice in Assertion Testing doc (Rich Trott) [#2728](https://github.com/nodejs/node/pull/2728)
* [[`6108ea9bb4`](https://github.com/nodejs/node/commit/6108ea9bb4)] - **fs**: consider NaN/Infinity in toUnixTimestamp (Yazhong Liu) [#2387](https://github.com/nodejs/node/pull/2387)
* [[`2b6aa9415f`](https://github.com/nodejs/node/commit/2b6aa9415f)] - **(SEMVER-MINOR)** **fs**: implemented WriteStream#writev (Ron Korving) [#2167](https://github.com/nodejs/node/pull/2167)
* [[`431bf74c55`](https://github.com/nodejs/node/commit/431bf74c55)] - **http**: default Agent.getName to 'localhost' (Malcolm Ahoy) [#2825](https://github.com/nodejs/node/pull/2825)
* [[`ea15d71c16`](https://github.com/nodejs/node/commit/ea15d71c16)] - **http_server**: fix resume after socket close (Fedor Indutny) [#2824](https://github.com/nodejs/node/pull/2824)
* [[`8e5843405b`](https://github.com/nodejs/node/commit/8e5843405b)] - **src**: null env_ field from constructor (Trevor Norris) [#2913](https://github.com/nodejs/node/pull/2913)
* [[`0a5f80a11f`](https://github.com/nodejs/node/commit/0a5f80a11f)] - **src**: use subarray() in Buffer#slice() for speedup (Karl Skomski) [#2777](https://github.com/nodejs/node/pull/2777)
* [[`57707e2490`](https://github.com/nodejs/node/commit/57707e2490)] - **src**: use ZCtxt as a source for v8::Isolates (Roman Klauke) [#2547](https://github.com/nodejs/node/pull/2547)
* [[`b0df2273ab`](https://github.com/nodejs/node/commit/b0df2273ab)] - **src**: fix v8::CpuProfiler idle sampling (Oleksandr Chekhovskyi) [#2324](https://github.com/nodejs/node/pull/2324)
* [[`eaa8e60b91`](https://github.com/nodejs/node/commit/eaa8e60b91)] - **streams**: refactor LazyTransform to internal/ (Brendan Ashworth) [#2566](https://github.com/nodejs/node/pull/2566)
* [[`648c003e14`](https://github.com/nodejs/node/commit/648c003e14)] - **test**: use tmp directory in chdir test (Sakthipriyan Vairamani) [#2589](https://github.com/nodejs/node/pull/2589)
* [[`079a2173d4`](https://github.com/nodejs/node/commit/079a2173d4)] - **test**: fix Buffer OOM error message (Trevor Norris) [#2915](https://github.com/nodejs/node/pull/2915)
* [[`52019a1b21`](https://github.com/nodejs/node/commit/52019a1b21)] - **test**: fix default value for additional param (Sakthipriyan Vairamani) [#2553](https://github.com/nodejs/node/pull/2553)
* [[`5df5d0423a`](https://github.com/nodejs/node/commit/5df5d0423a)] - **test**: remove disabled test (Rich Trott) [#2841](https://github.com/nodejs/node/pull/2841)
* [[`9e5f0995bd`](https://github.com/nodejs/node/commit/9e5f0995bd)] - **test**: split up internet dns tests (Rich Trott) [#2802](https://github.com/nodejs/node/pull/2802)
* [[`41f2dde51a`](https://github.com/nodejs/node/commit/41f2dde51a)] - **test**: increase dgram timeout for armv6 (Rich Trott) [#2808](https://github.com/nodejs/node/pull/2808)
* [[`6e2fe1c21a`](https://github.com/nodejs/node/commit/6e2fe1c21a)] - **test**: remove valid hostname check in test-dns.js (Rich Trott) [#2785](https://github.com/nodejs/node/pull/2785)
* [[`779e14f1a7`](https://github.com/nodejs/node/commit/779e14f1a7)] - **test**: expect error for test_lookup_ipv6_hint on FreeBSD (Rich Trott) [#2724](https://github.com/nodejs/node/pull/2724)
* [[`f931b9dd95`](https://github.com/nodejs/node/commit/f931b9dd95)] - **(SEMVER-MINOR)** **timer**: ref/unref return self (Sam Roberts) [#2905](https://github.com/nodejs/node/pull/2905)
* [[`59d03738cc`](https://github.com/nodejs/node/commit/59d03738cc)] - **tools**: enable arrow functions in .eslintrc (Sakthipriyan Vairamani) [#2840](https://github.com/nodejs/node/pull/2840)
* [[`69e7b875a2`](https://github.com/nodejs/node/commit/69e7b875a2)] - **tools**: open `test.tap` file in write-binary mode (Sakthipriyan Vairamani) [#2837](https://github.com/nodejs/node/pull/2837)
* [[`ff6d30d784`](https://github.com/nodejs/node/commit/ff6d30d784)] - **tools**: add missing tick processor polyfill (Matt Loring) [#2694](https://github.com/nodejs/node/pull/2694)
* [[`519caba021`](https://github.com/nodejs/node/commit/519caba021)] - **tools**: fix flakiness in test-tick-processor (Matt Loring) [#2694](https://github.com/nodejs/node/pull/2694)
* [[`ac004b8555`](https://github.com/nodejs/node/commit/ac004b8555)] - **tools**: remove hyphen in TAP result (Sakthipriyan Vairamani) [#2718](https://github.com/nodejs/node/pull/2718)
* [[`ba47511976`](https://github.com/nodejs/node/commit/ba47511976)] - **tsc**: adjust TSC membership for IBM+StrongLoop (James M Snell) [#2858](https://github.com/nodejs/node/pull/2858)
* [[`e035266805`](https://github.com/nodejs/node/commit/e035266805)] - **win,msi**: fix documentation shortcut url (Brian White) [#2781](https://github.com/nodejs/node/pull/2781)

## 2015-09-15, io.js Version 3.3.1 @rvagg

### Notable changes

* **buffer**: Fixed a minor errors that was causing crashes (Michaël Zasso) [#2635](https://github.com/nodejs/node/pull/2635),
* **child_process**: Fix error that was causing crashes (Evan Lucas) [#2727](https://github.com/nodejs/node/pull/2727)
* **crypto**: Replace use of rwlocks, unsafe on Windows XP / 2003 (Ben Noordhuis) [#2723](https://github.com/nodejs/node/pull/2723)
* **libuv**: Upgrade from 1.7.3 to 1.7.4 (Saúl Ibarra Corretgé) [#2817](https://github.com/nodejs/node/pull/2817)
* **node**: Fix faulty `process.release.libUrl` on Windows (Rod Vagg) [#2699](https://github.com/nodejs/node/pull/2699)
* **node-gyp**: Float v3.0.3 which has improved support for Node.js and io.js v0.10 to v4+ (Rod Vagg) [#2700](https://github.com/nodejs/node/pull/2700)
* **npm**: Upgrade to version 2.14.3 from 2.13.3, includes a security update, see https://github.com/npm/npm/releases/tag/v2.14.2 for more details, (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696).
* **timers**: Improved timer performance from porting the 0.12 implementation, plus minor fixes (Jeremiah Senkpiel) [#2540](https://github.com/nodejs/node/pull/2540), (Julien Gilli) [nodejs/node-v0.x-archive#8751](https://github.com/nodejs/node-v0.x-archive/pull/8751) [nodejs/node-v0.x-archive#8905](https://github.com/nodejs/node-v0.x-archive/pull/8905)

### Known issues

See https://github.com/nodejs/io.js/labels/confirmed-bug for complete and current list of known issues.

* Some uses of computed object shorthand properties are not handled correctly by the current version of V8. e.g. `[{ [prop]: val }]` evaluates to `[{}]`. [#2507](https://github.com/nodejs/node/issues/2507)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/io.js/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/io.js/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/io.js/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/io.js/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/io.js/issues/1435).

### Commits

* [[`b73ff52fe6`](https://github.com/nodejs/node/commit/b73ff52fe6)] - **bindings**: close after reading module struct (Fedor Indutny) [#2792](https://github.com/nodejs/node/pull/2792)
* [[`aa1140e59a`](https://github.com/nodejs/node/commit/aa1140e59a)] - **buffer**: SlowBuffer only accept valid numeric values (Michaël Zasso) [#2635](https://github.com/nodejs/node/pull/2635)
* [[`574475d56e`](https://github.com/nodejs/node/commit/574475d56e)] - **build**: clean up the generated tap file (Sakthipriyan Vairamani) [#2837](https://github.com/nodejs/node/pull/2837)
* [[`aa0001271e`](https://github.com/nodejs/node/commit/aa0001271e)] - **build**: remote commands on staging in single session (Rod Vagg) [#2717](https://github.com/nodejs/node/pull/2717)
* [[`1428661095`](https://github.com/nodejs/node/commit/1428661095)] - **build**: fix v8_enable_handle_zapping override (Karl Skomski) [#2731](https://github.com/nodejs/node/pull/2731)
* [[`5a51edd718`](https://github.com/nodejs/node/commit/5a51edd718)] - **build**: add --enable-asan with builtin leakcheck (Karl Skomski) [#2376](https://github.com/nodejs/node/pull/2376)
* [[`618caa5de0`](https://github.com/nodejs/node/commit/618caa5de0)] - **child_process**: use stdio.fd even if it is 0 (Evan Lucas) [#2727](https://github.com/nodejs/node/pull/2727)
* [[`7be4e49cb6`](https://github.com/nodejs/node/commit/7be4e49cb6)] - **child_process**: check execFile and fork args (James M Snell) [#2667](https://github.com/nodejs/node/pull/2667)
* [[`7f5d6e72c6`](https://github.com/nodejs/node/commit/7f5d6e72c6)] - **cluster**: allow shared reused dgram sockets (Fedor Indutny) [#2548](https://github.com/nodejs/node/pull/2548)
* [[`e68c7ec498`](https://github.com/nodejs/node/commit/e68c7ec498)] - **contextify**: ignore getters during initialization (Fedor Indutny) [nodejs/io.js#2091](https://github.com/nodejs/io.js/pull/2091)
* [[`610fa964aa`](https://github.com/nodejs/node/commit/610fa964aa)] - **cpplint**: make it possible to run outside git repo (Ben Noordhuis) [#2710](https://github.com/nodejs/node/pull/2710)
* [[`4237373dd7`](https://github.com/nodejs/node/commit/4237373dd7)] - **crypto**: replace rwlocks with simple mutexes (Ben Noordhuis) [#2723](https://github.com/nodejs/node/pull/2723)
* [[`777eb00306`](https://github.com/nodejs/node/commit/777eb00306)] - **deps**: upgraded to node-gyp@3.0.3 in npm (Kat Marchán) [#2822](https://github.com/nodejs/node/pull/2822)
* [[`b729ad384b`](https://github.com/nodejs/node/commit/b729ad384b)] - **deps**: upgrade to npm 2.14.3 (Kat Marchán) [#2822](https://github.com/nodejs/node/pull/2822)
* [[`b09fde761c`](https://github.com/nodejs/node/commit/b09fde761c)] - **deps**: update libuv to version 1.7.4 (Saúl Ibarra Corretgé) [#2817](https://github.com/nodejs/node/pull/2817)
* [[`4cf225daad`](https://github.com/nodejs/node/commit/4cf225daad)] - **deps**: float node-gyp v3.0.0 (Rod Vagg) [#2700](https://github.com/nodejs/node/pull/2700)
* [[`118f48c0f3`](https://github.com/nodejs/node/commit/118f48c0f3)] - **deps**: create .npmrc during npm tests (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696)
* [[`b3fee8e6a6`](https://github.com/nodejs/node/commit/b3fee8e6a6)] - **deps**: upgrade to npm 2.14.2 (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696)
* [[`4593539b92`](https://github.com/nodejs/node/commit/4593539b92)] - **deps**: backport 75e43a6 from v8 upstream (saper) [#2636](https://github.com/nodejs/node/pull/2636)
* [[`2d1438cfe0`](https://github.com/nodejs/node/commit/2d1438cfe0)] - **doc**: fix broken link in repl.markdown (Danny Nemer) [#2827](https://github.com/nodejs/node/pull/2827)
* [[`9dd9c85a48`](https://github.com/nodejs/node/commit/9dd9c85a48)] - **doc**: fix typos in README (Ionică Bizău) [#2852](https://github.com/nodejs/node/pull/2852)
* [[`476125d403`](https://github.com/nodejs/node/commit/476125d403)] - **doc**: add tunniclm as a collaborator (Mike Tunnicliffe) [#2826](https://github.com/nodejs/node/pull/2826)
* [[`0603a92d48`](https://github.com/nodejs/node/commit/0603a92d48)] - **doc**: fix two doc errors in stream and process (Jeremiah Senkpiel) [#2549](https://github.com/nodejs/node/pull/2549)
* [[`da2902ddfd`](https://github.com/nodejs/node/commit/da2902ddfd)] - **doc**: use "Calls" over "Executes" for consistency (Minwoo Jung) [#2800](https://github.com/nodejs/node/pull/2800)
* [[`5e93bc4fba`](https://github.com/nodejs/node/commit/5e93bc4fba)] - **doc**: use US English for consistency (Anne-Gaelle Colom) [#2784](https://github.com/nodejs/node/pull/2784)
* [[`3ee7fbcefd`](https://github.com/nodejs/node/commit/3ee7fbcefd)] - **doc**: use 3rd person singular for consistency (Anne-Gaelle Colom) [#2765](https://github.com/nodejs/node/pull/2765)
* [[`4fdccb9eb7`](https://github.com/nodejs/node/commit/4fdccb9eb7)] - **doc**: fix comma splice in Assertion Testing doc (Rich Trott) [#2728](https://github.com/nodejs/node/pull/2728)
* [[`28c2d310d6`](https://github.com/nodejs/node/commit/28c2d310d6)] - **doc**: update AUTHORS list (Rod Vagg)
* [[`324c073fb9`](https://github.com/nodejs/node/commit/324c073fb9)] - **doc**: add TSC meeting minutes 2015-09-02 (Rod Vagg) [#2674](https://github.com/nodejs/node/pull/2674)
* [[`8929445686`](https://github.com/nodejs/node/commit/8929445686)] - **doc**: update url doc to account for escaping (Jeremiah Senkpiel) [#2605](https://github.com/nodejs/node/pull/2605)
* [[`512dad6883`](https://github.com/nodejs/node/commit/512dad6883)] - **doc**: reorder collaborators by their usernames (Johan Bergström) [#2322](https://github.com/nodejs/node/pull/2322)
* [[`8372ea2ca5`](https://github.com/nodejs/node/commit/8372ea2ca5)] - **doc,test**: enable recursive file watching in Windows (Sakthipriyan Vairamani) [#2649](https://github.com/nodejs/node/pull/2649)
* [[`daf6c533cc`](https://github.com/nodejs/node/commit/daf6c533cc)] - **events,lib**: don't require EE#listenerCount() (Jeremiah Senkpiel) [#2661](https://github.com/nodejs/node/pull/2661)
* [[`d8371a801e`](https://github.com/nodejs/node/commit/d8371a801e)] - **http_server**: fix resume after socket close (Fedor Indutny) [#2824](https://github.com/nodejs/node/pull/2824)
* [[`7f7d4fdddd`](https://github.com/nodejs/node/commit/7f7d4fdddd)] - **node-gyp**: float 3.0.1, minor fix for download url (Rod Vagg) [#2737](https://github.com/nodejs/node/pull/2737)
* [[`91cee73294`](https://github.com/nodejs/node/commit/91cee73294)] - **src**: use ZCtxt as a source for v8::Isolates (Roman Klauke) [#2547](https://github.com/nodejs/node/pull/2547)
* [[`ac98e13b95`](https://github.com/nodejs/node/commit/ac98e13b95)] - **src**: s/ia32/x86 for process.release.libUrl for win (Rod Vagg) [#2699](https://github.com/nodejs/node/pull/2699)
* [[`ca6c3223e1`](https://github.com/nodejs/node/commit/ca6c3223e1)] - **src**: use standard conform snprintf on windows (Karl Skomski) [#2404](https://github.com/nodejs/node/pull/2404)
* [[`b028978a53`](https://github.com/nodejs/node/commit/b028978a53)] - **src**: fix buffer overflow for long exception lines (Karl Skomski) [#2404](https://github.com/nodejs/node/pull/2404)
* [[`e73eafd7e7`](https://github.com/nodejs/node/commit/e73eafd7e7)] - **src**: fix memory leak in ExternString (Karl Skomski) [#2402](https://github.com/nodejs/node/pull/2402)
* [[`d370306de1`](https://github.com/nodejs/node/commit/d370306de1)] - **src**: only set v8 flags if argc > 1 (Evan Lucas) [#2646](https://github.com/nodejs/node/pull/2646)
* [[`ed087836af`](https://github.com/nodejs/node/commit/ed087836af)] - **streams**: refactor LazyTransform to internal/ (Brendan Ashworth) [#2566](https://github.com/nodejs/node/pull/2566)
* [[`993c22fe0e`](https://github.com/nodejs/node/commit/993c22fe0e)] - **test**: remove disabled test (Rich Trott) [#2841](https://github.com/nodejs/node/pull/2841)
* [[`1474f29d1f`](https://github.com/nodejs/node/commit/1474f29d1f)] - **test**: split up internet dns tests (Rich Trott) [#2802](https://github.com/nodejs/node/pull/2802)
* [[`601a97622b`](https://github.com/nodejs/node/commit/601a97622b)] - **test**: increase dgram timeout for armv6 (Rich Trott) [#2808](https://github.com/nodejs/node/pull/2808)
* [[`1dad19ba81`](https://github.com/nodejs/node/commit/1dad19ba81)] - **test**: remove valid hostname check in test-dns.js (Rich Trott) [#2785](https://github.com/nodejs/node/pull/2785)
* [[`f3d5891a3f`](https://github.com/nodejs/node/commit/f3d5891a3f)] - **test**: expect error for test_lookup_ipv6_hint on FreeBSD (Rich Trott) [#2724](https://github.com/nodejs/node/pull/2724)
* [[`2ffb21baf1`](https://github.com/nodejs/node/commit/2ffb21baf1)] - **test**: fix use of `common` before required (Rod Vagg) [#2685](https://github.com/nodejs/node/pull/2685)
* [[`b2c5479a14`](https://github.com/nodejs/node/commit/b2c5479a14)] - **test**: refactor to eliminate flaky test (Rich Trott) [#2609](https://github.com/nodejs/node/pull/2609)
* [[`fcfd15f8f9`](https://github.com/nodejs/node/commit/fcfd15f8f9)] - **test**: mark eval_messages as flaky (Alexis Campailla) [#2648](https://github.com/nodejs/node/pull/2648)
* [[`1865cad7ae`](https://github.com/nodejs/node/commit/1865cad7ae)] - **test**: mark test-vm-syntax-error-stderr as flaky (João Reis) [#2662](https://github.com/nodejs/node/pull/2662)
* [[`b0014ecd27`](https://github.com/nodejs/node/commit/b0014ecd27)] - **test**: mark test-repl-persistent-history as flaky (João Reis) [#2659](https://github.com/nodejs/node/pull/2659)
* [[`74ff9bc86c`](https://github.com/nodejs/node/commit/74ff9bc86c)] - **timers**: minor _unrefActive fixes and improvements (Jeremiah Senkpiel) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`5d14a6eca7`](https://github.com/nodejs/node/commit/5d14a6eca7)] - **timers**: don't mutate unref list while iterating it (Julien Gilli) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`6e744c58f2`](https://github.com/nodejs/node/commit/6e744c58f2)] - **timers**: Avoid linear scan in _unrefActive. (Julien Gilli) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`07fbf835ad`](https://github.com/nodejs/node/commit/07fbf835ad)] - **tools**: open `test.tap` file in write-binary mode (Sakthipriyan Vairamani) [#2837](https://github.com/nodejs/node/pull/2837)
* [[`6d9198f7f1`](https://github.com/nodejs/node/commit/6d9198f7f1)] - **tools**: add missing tick processor polyfill (Matt Loring) [#2694](https://github.com/nodejs/node/pull/2694)
* [[`7b16597527`](https://github.com/nodejs/node/commit/7b16597527)] - **tools**: fix flakiness in test-tick-processor (Matt Loring) [#2694](https://github.com/nodejs/node/pull/2694)
* [[`ef83029356`](https://github.com/nodejs/node/commit/ef83029356)] - **tools**: remove hyphen in TAP result (Sakthipriyan Vairamani) [#2718](https://github.com/nodejs/node/pull/2718)
* [[`ac45ef9157`](https://github.com/nodejs/node/commit/ac45ef9157)] - **win,msi**: fix documentation shortcut url (Brian White) [#2781](https://github.com/nodejs/node/pull/2781)

## 2015-09-08, Version 4.0.0 (Stable), @rvagg

### Notable changes

This list of changes is relative to the last io.js v3.x branch release, v3.3.0. Please see the list of notable changes in the v3.x, v2.x and v1.x releases for a more complete list of changes from 0.12.x. Note, that some changes in the v3.x series as well as major breaking changes in this release constitute changes required for full convergence of the Node.js and io.js projects.

* **child_process**: `ChildProcess.prototype.send()` and `process.send()` operate asynchronously across all platforms so an optional callback parameter has been introduced that will be invoked once the message has been sent, i.e. `.send(message[, sendHandle][, callback])` (Ben Noordhuis) [#2620](https://github.com/nodejs/node/pull/2620).
* **node**: Rename "io.js" code to "Node.js" (cjihrig) [#2367](https://github.com/nodejs/node/pull/2367).
* **node-gyp**: This release bundles an updated version of node-gyp that works with all versions of Node.js and io.js including nightly and release candidate builds. From io.js v3 and Node.js v4 onward, it will only download a headers tarball when building addons rather than the entire source. (Rod Vagg) [#2700](https://github.com/nodejs/node/pull/2700)
* **npm**: Upgrade to version 2.14.2 from 2.13.3, includes a security update, see https://github.com/npm/npm/releases/tag/v2.14.2 for more details, (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696).
* **timers**: Improved timer performance from porting the 0.12 implementation, plus minor fixes (Jeremiah Senkpiel) [#2540](https://github.com/nodejs/node/pull/2540), (Julien Gilli) [nodejs/node-v0.x-archive#8751](https://github.com/nodejs/node-v0.x-archive/pull/8751) [nodejs/node-v0.x-archive#8905](https://github.com/nodejs/node-v0.x-archive/pull/8905)
* **util**: The `util.is*()` functions have been deprecated, beginning with deprecation warnings in the documentation for this release, users are encouraged to seek more robust alternatives in the npm registry, (Sakthipriyan Vairamani) [#2447](https://github.com/nodejs/node/pull/2447).
* **v8**: Upgrade to version 4.5.103.30 from 4.4.63.30 (Ali Ijaz Sheikh) [#2632](https://github.com/nodejs/node/pull/2632).
 - Implement new `TypedArray` prototype methods: `copyWithin()`, `every()`, `fill()`, `filter()`, `find()`, `findIndex()`, `forEach()`, `indexOf()`, `join()`, `lastIndexOf()`, `map()`, `reduce()`, `reduceRight()`, `reverse()`, `slice()`, `some()`, `sort()`. See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray for further information.
 - Implement new `TypedArray.from()` and `TypedArray.of()` functions. See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray for further information.
 - Implement arrow functions, see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/Arrow_functions for further information.
 - Full ChangeLog available at https://github.com/v8/v8-git-mirror/blob/4.5.103/ChangeLog

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some uses of computed object shorthand properties are not handled correctly by the current version of V8. e.g. `[{ [prop]: val }]` evaluates to `[{}]`. [#2507](https://github.com/nodejs/node/issues/2507)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`4f50d3fb90`](https://github.com/nodejs/node/commit/4f50d3fb90)] - **(SEMVER-MAJOR)** This commit sets the value of process.release.name to "node". (cjihrig) [#2367](https://github.com/nodejs/node/pull/2367)
* [[`d3178d8b1b`](https://github.com/nodejs/node/commit/d3178d8b1b)] - **buffer**: SlowBuffer only accept valid numeric values (Michaël Zasso) [#2635](https://github.com/nodejs/node/pull/2635)
* [[`0cb0f4a6e4`](https://github.com/nodejs/node/commit/0cb0f4a6e4)] - **build**: fix v8_enable_handle_zapping override (Karl Skomski) [#2731](https://github.com/nodejs/node/pull/2731)
* [[`a7596d7efc`](https://github.com/nodejs/node/commit/a7596d7efc)] - **build**: remote commands on staging in single session (Rod Vagg) [#2717](https://github.com/nodejs/node/pull/2717)
* [[`be427e9efa`](https://github.com/nodejs/node/commit/be427e9efa)] - **build**: make .msi install to "nodejs", not "node" (Rod Vagg) [#2701](https://github.com/nodejs/node/pull/2701)
* [[`5652ce0dbc`](https://github.com/nodejs/node/commit/5652ce0dbc)] - **build**: fix .pkg creation tooling (Rod Vagg) [#2687](https://github.com/nodejs/node/pull/2687)
* [[`101db80111`](https://github.com/nodejs/node/commit/101db80111)] - **build**: add --enable-asan with builtin leakcheck (Karl Skomski) [#2376](https://github.com/nodejs/node/pull/2376)
* [[`2c3939c9c0`](https://github.com/nodejs/node/commit/2c3939c9c0)] - **child_process**: use stdio.fd even if it is 0 (Evan Lucas) [#2727](https://github.com/nodejs/node/pull/2727)
* [[`609db5a1dd`](https://github.com/nodejs/node/commit/609db5a1dd)] - **child_process**: check execFile and fork args (James M Snell) [#2667](https://github.com/nodejs/node/pull/2667)
* [[`d010568c23`](https://github.com/nodejs/node/commit/d010568c23)] - **(SEMVER-MAJOR)** **child_process**: add callback parameter to .send() (Ben Noordhuis) [#2620](https://github.com/nodejs/node/pull/2620)
* [[`c60857a81a`](https://github.com/nodejs/node/commit/c60857a81a)] - **cluster**: allow shared reused dgram sockets (Fedor Indutny) [#2548](https://github.com/nodejs/node/pull/2548)
* [[`b2ecbb6191`](https://github.com/nodejs/node/commit/b2ecbb6191)] - **contextify**: ignore getters during initialization (Fedor Indutny) [#2091](https://github.com/nodejs/node/pull/2091)
* [[`3711934095`](https://github.com/nodejs/node/commit/3711934095)] - **cpplint**: make it possible to run outside git repo (Ben Noordhuis) [#2710](https://github.com/nodejs/node/pull/2710)
* [[`03f900ab25`](https://github.com/nodejs/node/commit/03f900ab25)] - **crypto**: replace rwlocks with simple mutexes (Ben Noordhuis) [#2723](https://github.com/nodejs/node/pull/2723)
* [[`847459c29b`](https://github.com/nodejs/node/commit/847459c29b)] - **(SEMVER-MAJOR)** **crypto**: show exponent in decimal and hex (Chad Johnston) [#2320](https://github.com/nodejs/node/pull/2320)
* [[`e1c976184d`](https://github.com/nodejs/node/commit/e1c976184d)] - **deps**: improve ArrayBuffer performance in v8 (Fedor Indutny) [#2732](https://github.com/nodejs/node/pull/2732)
* [[`cc0ab17a23`](https://github.com/nodejs/node/commit/cc0ab17a23)] - **deps**: float node-gyp v3.0.0 (Rod Vagg) [#2700](https://github.com/nodejs/node/pull/2700)
* [[`b2c3c6d727`](https://github.com/nodejs/node/commit/b2c3c6d727)] - **deps**: create .npmrc during npm tests (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696)
* [[`babdbfdbd5`](https://github.com/nodejs/node/commit/babdbfdbd5)] - **deps**: upgrade to npm 2.14.2 (Kat Marchán) [#2696](https://github.com/nodejs/node/pull/2696)
* [[`155783d876`](https://github.com/nodejs/node/commit/155783d876)] - **deps**: backport 75e43a6 from v8 upstream (again) (saper) [#2692](https://github.com/nodejs/node/pull/2692)
* [[`5424d6fcf0`](https://github.com/nodejs/node/commit/5424d6fcf0)] - **deps**: upgrade V8 to 4.5.103.30 (Ali Ijaz Sheikh) [#2632](https://github.com/nodejs/node/pull/2632)
* [[`c43172578e`](https://github.com/nodejs/node/commit/c43172578e)] - **(SEMVER-MAJOR)** **deps**: upgrade V8 to 4.5.103.24 (Ali Ijaz Sheikh) [#2509](https://github.com/nodejs/node/pull/2509)
* [[`714e96e8b9`](https://github.com/nodejs/node/commit/714e96e8b9)] - **deps**: backport 75e43a6 from v8 upstream (saper) [#2636](https://github.com/nodejs/node/pull/2636)
* [[`8637755cbf`](https://github.com/nodejs/node/commit/8637755cbf)] - **doc**: add TSC meeting minutes 2015-09-02 (Rod Vagg) [#2674](https://github.com/nodejs/node/pull/2674)
* [[`d3d5b93214`](https://github.com/nodejs/node/commit/d3d5b93214)] - **doc**: update environment vars in manpage and --help (Roman Reiss) [#2690](https://github.com/nodejs/node/pull/2690)
* [[`29f586ac0a`](https://github.com/nodejs/node/commit/29f586ac0a)] - **doc**: update url doc to account for escaping (Jeremiah Senkpiel) [#2605](https://github.com/nodejs/node/pull/2605)
* [[`ba50cfebef`](https://github.com/nodejs/node/commit/ba50cfebef)] - **doc**: reorder collaborators by their usernames (Johan Bergström) [#2322](https://github.com/nodejs/node/pull/2322)
* [[`8a9a3bf798`](https://github.com/nodejs/node/commit/8a9a3bf798)] - **doc**: update changelog for io.js v3.3.0 (Rod Vagg) [#2653](https://github.com/nodejs/node/pull/2653)
* [[`6cd0e2664b`](https://github.com/nodejs/node/commit/6cd0e2664b)] - **doc**: update io.js reference (Ben Noordhuis) [#2580](https://github.com/nodejs/node/pull/2580)
* [[`f9539c19e8`](https://github.com/nodejs/node/commit/f9539c19e8)] - **doc**: update changelog for io.js v3.2.0 (Rod Vagg) [#2512](https://github.com/nodejs/node/pull/2512)
* [[`cded6e7993`](https://github.com/nodejs/node/commit/cded6e7993)] - **doc**: fix CHANGELOG.md on master (Roman Reiss) [#2513](https://github.com/nodejs/node/pull/2513)
* [[`93e2830686`](https://github.com/nodejs/node/commit/93e2830686)] - **(SEMVER-MINOR)** **doc**: document deprecation of util.is* functions (Sakthipriyan Vairamani) [#2447](https://github.com/nodejs/node/pull/2447)
* [[`7038388558`](https://github.com/nodejs/node/commit/7038388558)] - **doc,test**: enable recursive file watching in Windows (Sakthipriyan Vairamani) [#2649](https://github.com/nodejs/node/pull/2649)
* [[`f3696f64a1`](https://github.com/nodejs/node/commit/f3696f64a1)] - **events,lib**: don't require EE#listenerCount() (Jeremiah Senkpiel) [#2661](https://github.com/nodejs/node/pull/2661)
* [[`45a2046f5d`](https://github.com/nodejs/node/commit/45a2046f5d)] - **(SEMVER-MAJOR)** **installer**: fix installers for node.js rename (Frederic Hemberger) [#2367](https://github.com/nodejs/node/pull/2367)
* [[`7a999a1376`](https://github.com/nodejs/node/commit/7a999a1376)] - **(SEMVER-MAJOR)** **lib**: add net.Socket#localFamily property (Ben Noordhuis) [#956](https://github.com/nodejs/node/pull/956)
* [[`de88255b0f`](https://github.com/nodejs/node/commit/de88255b0f)] - ***Revert*** "**lib,src**: add unix socket getsockname/getpeername" (Ben Noordhuis) [#2584](https://github.com/nodejs/node/pull/2584)
* [[`f337595441`](https://github.com/nodejs/node/commit/f337595441)] - **(SEMVER-MAJOR)** **lib,src**: add unix socket getsockname/getpeername (Ben Noordhuis) [#956](https://github.com/nodejs/node/pull/956)
* [[`3b602527d1`](https://github.com/nodejs/node/commit/3b602527d1)] - **(SEMVER-MAJOR)** **node**: additional cleanup for node rename (cjihrig) [#2367](https://github.com/nodejs/node/pull/2367)
* [[`a69ab27ab4`](https://github.com/nodejs/node/commit/a69ab27ab4)] - **(SEMVER-MAJOR)** **node**: rename from io.js to node (cjihrig) [#2367](https://github.com/nodejs/node/pull/2367)
* [[`9358eee9dd`](https://github.com/nodejs/node/commit/9358eee9dd)] - **node-gyp**: float 3.0.1, minor fix for download url (Rod Vagg) [#2737](https://github.com/nodejs/node/pull/2737)
* [[`d2d981252b`](https://github.com/nodejs/node/commit/d2d981252b)] - **src**: s/ia32/x86 for process.release.libUrl for win (Rod Vagg) [#2699](https://github.com/nodejs/node/pull/2699)
* [[`eba3d3dccd`](https://github.com/nodejs/node/commit/eba3d3dccd)] - **src**: use standard conform snprintf on windows (Karl Skomski) [#2404](https://github.com/nodejs/node/pull/2404)
* [[`cddbec231f`](https://github.com/nodejs/node/commit/cddbec231f)] - **src**: fix buffer overflow for long exception lines (Karl Skomski) [#2404](https://github.com/nodejs/node/pull/2404)
* [[`dd3f3417c7`](https://github.com/nodejs/node/commit/dd3f3417c7)] - **src**: re-enable fast math on arm (Michaël Zasso) [#2592](https://github.com/nodejs/node/pull/2592)
* [[`e137c1177c`](https://github.com/nodejs/node/commit/e137c1177c)] - **(SEMVER-MAJOR)** **src**: enable vector ics on arm again (Ali Ijaz Sheikh) [#2509](https://github.com/nodejs/node/pull/2509)
* [[`7ce749d722`](https://github.com/nodejs/node/commit/7ce749d722)] - **src**: replace usage of v8::Handle with v8::Local (Michaël Zasso) [#2202](https://github.com/nodejs/node/pull/2202)
* [[`b1a2d9509f`](https://github.com/nodejs/node/commit/b1a2d9509f)] - **src**: enable v8 deprecation warnings and fix them (Ben Noordhuis) [#2091](https://github.com/nodejs/node/pull/2091)
* [[`808de0da03`](https://github.com/nodejs/node/commit/808de0da03)] - **(SEMVER-MAJOR)** **src**: apply debug force load fixups from 41e63fb (Ali Ijaz Sheikh) [#2509](https://github.com/nodejs/node/pull/2509)
* [[`5201cb0ff1`](https://github.com/nodejs/node/commit/5201cb0ff1)] - **src**: fix memory leak in ExternString (Karl Skomski) [#2402](https://github.com/nodejs/node/pull/2402)
* [[`2308a27c0a`](https://github.com/nodejs/node/commit/2308a27c0a)] - **src**: only set v8 flags if argc > 1 (Evan Lucas) [#2646](https://github.com/nodejs/node/pull/2646)
* [[`384effed20`](https://github.com/nodejs/node/commit/384effed20)] - **test**: fix use of `common` before required (Rod Vagg) [#2685](https://github.com/nodejs/node/pull/2685)
* [[`f146f686b7`](https://github.com/nodejs/node/commit/f146f686b7)] - **(SEMVER-MAJOR)** **test**: fix test-repl-tab-complete.js for V8 4.5 (Ali Ijaz Sheikh) [#2509](https://github.com/nodejs/node/pull/2509)
* [[`fe4b309fd3`](https://github.com/nodejs/node/commit/fe4b309fd3)] - **test**: refactor to eliminate flaky test (Rich Trott) [#2609](https://github.com/nodejs/node/pull/2609)
* [[`619721e6b8`](https://github.com/nodejs/node/commit/619721e6b8)] - **test**: mark eval_messages as flaky (Alexis Campailla) [#2648](https://github.com/nodejs/node/pull/2648)
* [[`93ba585b66`](https://github.com/nodejs/node/commit/93ba585b66)] - **test**: mark test-vm-syntax-error-stderr as flaky (João Reis) [#2662](https://github.com/nodejs/node/pull/2662)
* [[`367140bca0`](https://github.com/nodejs/node/commit/367140bca0)] - **test**: mark test-repl-persistent-history as flaky (João Reis) [#2659](https://github.com/nodejs/node/pull/2659)
* [[`f6b093343d`](https://github.com/nodejs/node/commit/f6b093343d)] - **timers**: minor `_unrefActive` fixes and improvements (Jeremiah Senkpiel) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`403d7ee7d1`](https://github.com/nodejs/node/commit/403d7ee7d1)] - **timers**: don't mutate unref list while iterating it (Julien Gilli) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`7a8c3e08c3`](https://github.com/nodejs/node/commit/7a8c3e08c3)] - **timers**: Avoid linear scan in `_unrefActive`. (Julien Gilli) [#2540](https://github.com/nodejs/node/pull/2540)
* [[`b630ebaf43`](https://github.com/nodejs/node/commit/b630ebaf43)] - **win,msi**: Upgrade from old upgrade code (João Reis) [#2439](https://github.com/nodejs/node/pull/2439)

## 2015-09-02, Version 3.3.0, @rvagg

### Notable changes

* **build**: Add a `--link-module` option to `configure` that can be used to bundle additional JavaScript modules into a built binary (Bradley Meck) [#2497](https://github.com/nodejs/node/pull/2497)
* **docs**: Merge outstanding doc updates from joyent/node (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* **http_parser**: Significant performance improvement by having `http.Server` consume all initial data from its `net.Socket` and parsing directly without having to enter JavaScript. Any `'data'` listeners on the `net.Socket` will result in the data being "unconsumed" into JavaScript, thereby undoing any performance gains. (Fedor Indutny) [#2355](https://github.com/nodejs/node/pull/2355)
* **libuv**: Upgrade to 1.7.3 (from 1.6.1), see [ChangeLog](https://github.com/libuv/libuv/blob/v1.x/ChangeLog) for details (Saúl Ibarra Corretgé) [#2310](https://github.com/nodejs/node/pull/2310)
* **V8**: Upgrade to 4.4.63.30 (from 4.4.63.26) (Michaël Zasso) [#2482](https://github.com/nodejs/node/pull/2482)

### Known issues

See https://github.com/nodejs/io.js/labels/confirmed-bug for complete and current list of known issues.

* Some uses of computed object shorthand properties are not handled correctly by the current version of V8. e.g. `[{ [prop]: val }]` evaluates to `[{}]`. [#2507](https://github.com/nodejs/node/issues/2507)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/io.js/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/io.js/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/io.js/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/io.js/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/io.js/issues/1435).

### Commits

* [[`1a531b4e44`](https://github.com/nodejs/node/commit/1a531b4e44)] - **(SEMVER-MINOR)** Introduce --link-module to ./configure (Bradley Meck) [#2497](https://github.com/nodejs/node/pull/2497)
* [[`d2f314c190`](https://github.com/nodejs/node/commit/d2f314c190)] - **build**: fix borked chmod call for release uploads (Rod Vagg) [#2645](https://github.com/nodejs/node/pull/2645)
* [[`3172e9c541`](https://github.com/nodejs/node/commit/3172e9c541)] - **build**: set file permissions before uploading (Rod Vagg) [#2623](https://github.com/nodejs/node/pull/2623)
* [[`a860d7fae1`](https://github.com/nodejs/node/commit/a860d7fae1)] - **build**: change staging directory on new server (Rod Vagg) [#2623](https://github.com/nodejs/node/pull/2623)
* [[`50c0baa8d7`](https://github.com/nodejs/node/commit/50c0baa8d7)] - **build**: rename 'doc' directory to 'docs' for upload (Rod Vagg) [#2623](https://github.com/nodejs/node/pull/2623)
* [[`0a0577cf5f`](https://github.com/nodejs/node/commit/0a0577cf5f)] - **build**: fix bad cherry-pick for vcbuild.bat build-release (Rod Vagg) [#2625](https://github.com/nodejs/node/pull/2625)
* [[`34de90194b`](https://github.com/nodejs/node/commit/34de90194b)] - **build**: only define NODE_V8_OPTIONS if not empty (Evan Lucas) [#2532](https://github.com/nodejs/node/pull/2532)
* [[`944174b189`](https://github.com/nodejs/node/commit/944174b189)] - **build**: make ci test addons in test/addons (Ben Noordhuis) [#2428](https://github.com/nodejs/node/pull/2428)
* [[`e955f9a1b0`](https://github.com/nodejs/node/commit/e955f9a1b0)] - **crypto**: Use OPENSSL_cleanse to shred the data. (Сковорода Никита Андреевич) [#2575](https://github.com/nodejs/node/pull/2575)
* [[`395d736b9d`](https://github.com/nodejs/node/commit/395d736b9d)] - **debugger**: use strict equality comparison (Minwoo Jung) [#2558](https://github.com/nodejs/node/pull/2558)
* [[`1d0e5210a8`](https://github.com/nodejs/node/commit/1d0e5210a8)] - **deps**: upgrade libuv to 1.7.3 (Saúl Ibarra Corretgé) [#2310](https://github.com/nodejs/node/pull/2310)
* [[`34ef53364f`](https://github.com/nodejs/node/commit/34ef53364f)] - **deps**: update V8 to 4.4.63.30 (Michaël Zasso) [#2482](https://github.com/nodejs/node/pull/2482)
* [[`23579a5f4a`](https://github.com/nodejs/node/commit/23579a5f4a)] - **doc**: add TSC meeting minutes 2015-08-12 (Rod Vagg) [#2438](https://github.com/nodejs/node/pull/2438)
* [[`0cc59299a4`](https://github.com/nodejs/node/commit/0cc59299a4)] - **doc**: add TSC meeting minutes 2015-08-26 (Rod Vagg) [#2591](https://github.com/nodejs/node/pull/2591)
* [[`6efa96e33a`](https://github.com/nodejs/node/commit/6efa96e33a)] - **doc**: merge CHANGELOG.md with joyent/node ChangeLog (Minqi Pan) [#2536](https://github.com/nodejs/node/pull/2536)
* [[`f75d54607b`](https://github.com/nodejs/node/commit/f75d54607b)] - **doc**: clarify cluster behaviour with no workers (Jeremiah Senkpiel) [#2606](https://github.com/nodejs/node/pull/2606)
* [[`8936302121`](https://github.com/nodejs/node/commit/8936302121)] - **doc**: minor clarification in buffer.markdown (Сковорода Никита Андреевич) [#2574](https://github.com/nodejs/node/pull/2574)
* [[`0db0e53753`](https://github.com/nodejs/node/commit/0db0e53753)] - **doc**: add @jasnell and @sam-github to release team (Rod Vagg) [#2455](https://github.com/nodejs/node/pull/2455)
* [[`c16e100593`](https://github.com/nodejs/node/commit/c16e100593)] - **doc**: reorg release team to separate section (Rod Vagg) [#2455](https://github.com/nodejs/node/pull/2455)
* [[`e3e00143fd`](https://github.com/nodejs/node/commit/e3e00143fd)] - **doc**: fix bad merge on modules.markdown (James M Snell)
* [[`2f62455880`](https://github.com/nodejs/node/commit/2f62455880)] - **doc**: minor additional corrections and improvements (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`3bd08aac4b`](https://github.com/nodejs/node/commit/3bd08aac4b)] - **doc**: minor grammatical update in crypto.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`f707189370`](https://github.com/nodejs/node/commit/f707189370)] - **doc**: minor grammatical update (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`6c98cf0266`](https://github.com/nodejs/node/commit/6c98cf0266)] - **doc**: remove repeated statement in globals.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`48e6ccf8c2`](https://github.com/nodejs/node/commit/48e6ccf8c2)] - **doc**: remove 'dudes' from documentation (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`b5d68f8076`](https://github.com/nodejs/node/commit/b5d68f8076)] - **doc**: update tense in child_process.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`242e3fe3ba`](https://github.com/nodejs/node/commit/242e3fe3ba)] - **doc**: fixed worker.id type (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`ea9ee15c21`](https://github.com/nodejs/node/commit/ea9ee15c21)] - **doc**: port is optional for socket.bind() (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`0ff6657a50`](https://github.com/nodejs/node/commit/0ff6657a50)] - **doc**: fix minor types and grammar in fs docs (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`94d83c04f2`](https://github.com/nodejs/node/commit/94d83c04f2)] - **doc**: update parameter name in net.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`04111ce40f`](https://github.com/nodejs/node/commit/04111ce40f)] - **doc**: small typo in domain.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`c9fdd1bbbf`](https://github.com/nodejs/node/commit/c9fdd1bbbf)] - **doc**: fixed typo in net.markdown (missing comma) (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`27c07b3f8e`](https://github.com/nodejs/node/commit/27c07b3f8e)] - **doc**: update description of fs.exists in fs.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`52018e73d9`](https://github.com/nodejs/node/commit/52018e73d9)] - **doc**: clarification on the 'close' event (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`f6d3b87a25`](https://github.com/nodejs/node/commit/f6d3b87a25)] - **doc**: improve working in stream.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`b5da89431a`](https://github.com/nodejs/node/commit/b5da89431a)] - **doc**: update path.extname documentation (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`1d4ea609db`](https://github.com/nodejs/node/commit/1d4ea609db)] - **doc**: small clarifications to modules.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`c888985591`](https://github.com/nodejs/node/commit/c888985591)] - **doc**: code style cleanups in repl.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`105b493595`](https://github.com/nodejs/node/commit/105b493595)] - **doc**: correct grammar in cluster.markdown (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`51b86ccac7`](https://github.com/nodejs/node/commit/51b86ccac7)] - **doc**: Clarify the module.parent is set once (James M Snell) [#2378](https://github.com/nodejs/node/pull/2378)
* [[`d2ffecba2d`](https://github.com/nodejs/node/commit/d2ffecba2d)] - **doc**: add internal modules notice (Jeremiah Senkpiel) [#2523](https://github.com/nodejs/node/pull/2523)
* [[`b36debd5cb`](https://github.com/nodejs/node/commit/b36debd5cb)] - **env**: introduce `KickNextTick` (Fedor Indutny) [#2355](https://github.com/nodejs/node/pull/2355)
* [[`1bc446863f`](https://github.com/nodejs/node/commit/1bc446863f)] - **http_parser**: consume StreamBase instance (Fedor Indutny) [#2355](https://github.com/nodejs/node/pull/2355)
* [[`ce04b735cc`](https://github.com/nodejs/node/commit/ce04b735cc)] - **src**: only memcmp if length > 0 in Buffer::Compare (Karl Skomski) [#2544](https://github.com/nodejs/node/pull/2544)
* [[`31823e37c7`](https://github.com/nodejs/node/commit/31823e37c7)] - **src**: DRY getsockname/getpeername code (Ben Noordhuis) [#956](https://github.com/nodejs/node/pull/956)
* [[`13fd96dda3`](https://github.com/nodejs/node/commit/13fd96dda3)] - **src**: missing Exception::Error in node_http_parser (Jeremiah Senkpiel) [#2550](https://github.com/nodejs/node/pull/2550)
* [[`42e075ae02`](https://github.com/nodejs/node/commit/42e075ae02)] - **test**: improve performance of stringbytes test (Trevor Norris) [#2544](https://github.com/nodejs/node/pull/2544)
* [[`fc726399fd`](https://github.com/nodejs/node/commit/fc726399fd)] - **test**: unmark test-process-argv-0.js as flaky (Rich Trott) [#2613](https://github.com/nodejs/node/pull/2613)
* [[`7727ba1394`](https://github.com/nodejs/node/commit/7727ba1394)] - **test**: lint and refactor to avoid autocrlf issue (Roman Reiss) [#2494](https://github.com/nodejs/node/pull/2494)
* [[`c56aa829f0`](https://github.com/nodejs/node/commit/c56aa829f0)] - **test**: use tmpDir instead of fixturesDir (Sakthipriyan Vairamani) [#2583](https://github.com/nodejs/node/pull/2583)
* [[`5e65181ea4`](https://github.com/nodejs/node/commit/5e65181ea4)] - **test**: handling failure cases properly (Sakthipriyan Vairamani) [#2206](https://github.com/nodejs/node/pull/2206)
* [[`c48b95e847`](https://github.com/nodejs/node/commit/c48b95e847)] - **test**: initial list of flaky tests (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`94e88498ba`](https://github.com/nodejs/node/commit/94e88498ba)] - **test**: pass args to test-ci via env variable (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`09987c7a1c`](https://github.com/nodejs/node/commit/09987c7a1c)] - **test**: support flaky tests in test-ci (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`08b83c8b45`](https://github.com/nodejs/node/commit/08b83c8b45)] - **test**: add test configuration templates (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`8f8ab6fa57`](https://github.com/nodejs/node/commit/8f8ab6fa57)] - **test**: runner should return 0 on flaky tests (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`0cfd3be9c6`](https://github.com/nodejs/node/commit/0cfd3be9c6)] - **test**: runner support for flaky tests (Alexis Campailla) [#2424](https://github.com/nodejs/node/pull/2424)
* [[`3492d2d4c6`](https://github.com/nodejs/node/commit/3492d2d4c6)] - **test**: make test-process-argv-0 robust (Rich Trott) [#2541](https://github.com/nodejs/node/pull/2541)
* [[`a96cc31710`](https://github.com/nodejs/node/commit/a96cc31710)] - **test**: speed up test-child-process-spawnsync.js (Rich Trott) [#2542](https://github.com/nodejs/node/pull/2542)
* [[`856baf4c67`](https://github.com/nodejs/node/commit/856baf4c67)] - **test**: make spawnSync() test robust (Rich Trott) [#2535](https://github.com/nodejs/node/pull/2535)
* [[`3aa6bbb648`](https://github.com/nodejs/node/commit/3aa6bbb648)] - **tools**: update release.sh to work with new website (Rod Vagg) [#2623](https://github.com/nodejs/node/pull/2623)
* [[`f2f0fe45ff`](https://github.com/nodejs/node/commit/f2f0fe45ff)] - **tools**: make add-on scraper print filenames (Ben Noordhuis) [#2428](https://github.com/nodejs/node/pull/2428)
* [[`bb24c4a418`](https://github.com/nodejs/node/commit/bb24c4a418)] - **win,msi**: correct installation path registry keys (João Reis) [#2565](https://github.com/nodejs/node/pull/2565)
* [[`752977b888`](https://github.com/nodejs/node/commit/752977b888)] - **win,msi**: change InstallScope to perMachine (João Reis) [#2565](https://github.com/nodejs/node/pull/2565)

## 2015-08-25, Version 3.2.0, @rvagg

### Notable changes

* **events**: Added `EventEmitter#listenerCount(event)` as a replacement for `EventEmitter.listenerCount(emitter, event)`, which has now been marked as deprecated in the docs. (Sakthipriyan Vairamani) [#2349](https://github.com/nodejs/node/pull/2349)
* **module**: Fixed an error with preloaded modules when the current working directory doesn't exist. (Bradley Meck) [#2353](https://github.com/nodejs/node/pull/2353)
* **node**: Startup time is now about 5% faster when not passing V8 flags. (Evan Lucas) [#2483](https://github.com/nodejs/node/pull/2483)
* **repl**: Tab-completion now works better with arrays. (James M Snell) [#2409](https://github.com/nodejs/node/pull/2409)
* **string_bytes**: Fixed an unaligned write in the handling of UCS2 encoding. (Fedor Indutny) [#2480](https://github.com/nodejs/node/pull/2480)
* **tls**: Added a new `--tls-cipher-list` flag that can be used to override the built-in default cipher list. (James M Snell) [#2412](https://github.com/nodejs/node/pull/2412) _Note: it is suggested you use the built-in cipher list as it has been carefully selected to reflect current security best practices and risk mitigation._

### Known issues

See https://github.com/nodejs/io.js/labels/confirmed-bug for complete and current list of known issues.

* Some uses of computed object shorthand properties are not handled correctly by the current version of V8. e.g. `[{ [prop]: val }]` evaluates to `[{}]`. [#2507](https://github.com/nodejs/node/issues/2507)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/io.js/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/io.js/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/io.js/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/io.js/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/io.js/issues/1435).

### Commits

* [[`1cd794f129`](https://github.com/nodejs/node/commit/1cd794f129)] - **buffer**: reapply 07c0667 (Fedor Indutny) [#2487](https://github.com/nodejs/node/pull/2487)
* [[`156781dedd`](https://github.com/nodejs/node/commit/156781dedd)] - **build**: use required platform in android-configure (Evan Lucas) [#2501](https://github.com/nodejs/node/pull/2501)
* [[`77075ec906`](https://github.com/nodejs/node/commit/77075ec906)] - **crypto**: fix mem {de}allocation in ExportChallenge (Karl Skomski) [#2359](https://github.com/nodejs/node/pull/2359)
* [[`cb30414d9e`](https://github.com/nodejs/node/commit/cb30414d9e)] - **doc**: sync CHANGELOG.md from master (Roman Reiss) [#2524](https://github.com/nodejs/node/pull/2524)
* [[`9330f5ef45`](https://github.com/nodejs/node/commit/9330f5ef45)] - **doc**: make the deprecations consistent (Sakthipriyan Vairamani) [#2450](https://github.com/nodejs/node/pull/2450)
* [[`09437e0146`](https://github.com/nodejs/node/commit/09437e0146)] - **doc**: fix comments in tls_wrap.cc and _http_client.js (Minwoo Jung) [#2489](https://github.com/nodejs/node/pull/2489)
* [[`c9867fed29`](https://github.com/nodejs/node/commit/c9867fed29)] - **doc**: document response.finished in http.markdown (hackerjs) [#2414](https://github.com/nodejs/node/pull/2414)
* [[`7f23a83c42`](https://github.com/nodejs/node/commit/7f23a83c42)] - **doc**: update AUTHORS list (Rod Vagg) [#2505](https://github.com/nodejs/node/pull/2505)
* [[`cd0c362f67`](https://github.com/nodejs/node/commit/cd0c362f67)] - **doc**: update AUTHORS list (Rod Vagg) [#2318](https://github.com/nodejs/node/pull/2318)
* [[`2c7b9257ea`](https://github.com/nodejs/node/commit/2c7b9257ea)] - **doc**: add TSC meeting minutes 2015-07-29 (Rod Vagg) [#2437](https://github.com/nodejs/node/pull/2437)
* [[`aaefde793e`](https://github.com/nodejs/node/commit/aaefde793e)] - **doc**: add TSC meeting minutes 2015-08-19 (Rod Vagg) [#2460](https://github.com/nodejs/node/pull/2460)
* [[`51ef9106f5`](https://github.com/nodejs/node/commit/51ef9106f5)] - **doc**: add TSC meeting minutes 2015-06-03 (Rod Vagg) [#2453](https://github.com/nodejs/node/pull/2453)
* [[`7130b4cf1d`](https://github.com/nodejs/node/commit/7130b4cf1d)] - **doc**: fix links to original converged repo (Rod Vagg) [#2454](https://github.com/nodejs/node/pull/2454)
* [[`14f2aee1df`](https://github.com/nodejs/node/commit/14f2aee1df)] - **doc**: fix links to original gh issues for TSC meetings (Rod Vagg) [#2454](https://github.com/nodejs/node/pull/2454)
* [[`87a9ef0a40`](https://github.com/nodejs/node/commit/87a9ef0a40)] - **doc**: add audio recording links to TSC meeting minutes (Rod Vagg) [#2454](https://github.com/nodejs/node/pull/2454)
* [[`f5cf24afbc`](https://github.com/nodejs/node/commit/f5cf24afbc)] - **doc**: add TSC meeting minutes 2015-07-22 (Rod Vagg) [#2436](https://github.com/nodejs/node/pull/2436)
* [[`3f821b96eb`](https://github.com/nodejs/node/commit/3f821b96eb)] - **doc**: fix spelling mistake in node.js comment (Jacob Edelman) [#2391](https://github.com/nodejs/node/pull/2391)
* [[`3e6a6fcdd6`](https://github.com/nodejs/node/commit/3e6a6fcdd6)] - **(SEMVER-MINOR)** **events**: deprecate static listenerCount function (Sakthipriyan Vairamani) [#2349](https://github.com/nodejs/node/pull/2349)
* [[`023386c852`](https://github.com/nodejs/node/commit/023386c852)] - **fs**: replace bad_args macro with concrete error msg (Roman Klauke) [#2495](https://github.com/nodejs/node/pull/2495)
* [[`d1c27b2e29`](https://github.com/nodejs/node/commit/d1c27b2e29)] - **module**: fix module preloading when cwd is ENOENT (Bradley Meck) [#2353](https://github.com/nodejs/node/pull/2353)
* [[`5d7486941b`](https://github.com/nodejs/node/commit/5d7486941b)] - **repl**: filter integer keys from repl tab complete list (James M Snell) [#2409](https://github.com/nodejs/node/pull/2409)
* [[`7f02443a9a`](https://github.com/nodejs/node/commit/7f02443a9a)] - **repl**: dont throw ENOENT on NODE_REPL_HISTORY_FILE (Todd Kennedy) [#2451](https://github.com/nodejs/node/pull/2451)
* [[`56a2ae9cef`](https://github.com/nodejs/node/commit/56a2ae9cef)] - **src**: improve startup time (Evan Lucas) [#2483](https://github.com/nodejs/node/pull/2483)
* [[`14653c7429`](https://github.com/nodejs/node/commit/14653c7429)] - **stream**: rename poorly named function (Ben Noordhuis) [#2479](https://github.com/nodejs/node/pull/2479)
* [[`1c6e014bfa`](https://github.com/nodejs/node/commit/1c6e014bfa)] - **stream**: micro-optimize high water mark calculation (Ben Noordhuis) [#2479](https://github.com/nodejs/node/pull/2479)
* [[`f1f4b4c46d`](https://github.com/nodejs/node/commit/f1f4b4c46d)] - **stream**: fix off-by-factor-16 error in comment (Ben Noordhuis) [#2479](https://github.com/nodejs/node/pull/2479)
* [[`2d3f09bd76`](https://github.com/nodejs/node/commit/2d3f09bd76)] - **stream_base**: various improvements (Fedor Indutny) [#2351](https://github.com/nodejs/node/pull/2351)
* [[`c1ce423b35`](https://github.com/nodejs/node/commit/c1ce423b35)] - **string_bytes**: fix unaligned write in UCS2 (Fedor Indutny) [#2480](https://github.com/nodejs/node/pull/2480)
* [[`e4d0e86165`](https://github.com/nodejs/node/commit/e4d0e86165)] - **test**: refactor test-https-simple.js (Rich Trott) [#2433](https://github.com/nodejs/node/pull/2433)
* [[`0ea5c8d737`](https://github.com/nodejs/node/commit/0ea5c8d737)] - **test**: remove test-timers-first-fire (João Reis) [#2458](https://github.com/nodejs/node/pull/2458)
* [[`536c3d0537`](https://github.com/nodejs/node/commit/536c3d0537)] - **test**: use reserved IP in test-net-connect-timeout (Rich Trott) [#2257](https://github.com/nodejs/node/pull/2257)
* [[`5df06fd8df`](https://github.com/nodejs/node/commit/5df06fd8df)] - **test**: add spaces after keywords (Brendan Ashworth)
* [[`e714b5620e`](https://github.com/nodejs/node/commit/e714b5620e)] - **test**: remove unreachable code (Michaël Zasso) [#2289](https://github.com/nodejs/node/pull/2289)
* [[`3579f3a2a4`](https://github.com/nodejs/node/commit/3579f3a2a4)] - **test**: disallow unreachable code (Michaël Zasso) [#2289](https://github.com/nodejs/node/pull/2289)
* [[`3545e236fc`](https://github.com/nodejs/node/commit/3545e236fc)] - **test**: reduce timeouts in test-net-keepalive (Brendan Ashworth) [#2429](https://github.com/nodejs/node/pull/2429)
* [[`b60e690023`](https://github.com/nodejs/node/commit/b60e690023)] - **test**: improve test-net-server-pause-on-connect (Brendan Ashworth) [#2429](https://github.com/nodejs/node/pull/2429)
* [[`11d1b8fcaf`](https://github.com/nodejs/node/commit/11d1b8fcaf)] - **test**: improve test-net-pingpong (Brendan Ashworth) [#2429](https://github.com/nodejs/node/pull/2429)
* [[`5fef5c6562`](https://github.com/nodejs/node/commit/5fef5c6562)] - **(SEMVER-MINOR)** **tls**: add --tls-cipher-list command line switch (James M Snell) [#2412](https://github.com/nodejs/node/pull/2412)
* [[`d9b70f9cbf`](https://github.com/nodejs/node/commit/d9b70f9cbf)] - **tls**: handle empty cert in checkServerIndentity (Mike Atkins) [#2343](https://github.com/nodejs/node/pull/2343)
* [[`4f8e34c202`](https://github.com/nodejs/node/commit/4f8e34c202)] - **tools**: add license boilerplate to check-imports.sh (James M Snell) [#2386](https://github.com/nodejs/node/pull/2386)
* [[`b76b9197f9`](https://github.com/nodejs/node/commit/b76b9197f9)] - **tools**: enable space-after-keywords in eslint (Brendan Ashworth)
* [[`64a8f30a70`](https://github.com/nodejs/node/commit/64a8f30a70)] - **tools**: fix anchors in generated documents (Sakthipriyan Vairamani) [#2491](https://github.com/nodejs/node/pull/2491)
* [[`22e344ea10`](https://github.com/nodejs/node/commit/22e344ea10)] - **win**: fix custom actions for WiX older than 3.9 (João Reis) [#2365](https://github.com/nodejs/node/pull/2365)
* [[`b5bd3ebfc8`](https://github.com/nodejs/node/commit/b5bd3ebfc8)] - **win**: fix custom actions on Visual Studio != 2013 (Julien Gilli) [#2365](https://github.com/nodejs/node/pull/2365)

## 2015-08-18, Version 3.1.0, @Fishrock123

### Notable changes

* **buffer**: Fixed a couple large memory leaks (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352).
* **crypto**:
  - Fixed a couple of minor memory leaks (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375).
  - Signing now checks for OpenSSL errors (Minqi Pan) [#2342](https://github.com/nodejs/node/pull/2342). **Note that this may expose previously hidden errors in user code.**
* **intl**: Intl support using small-icu is now enabled by default in builds (Steven R. Loomis) [#2264](https://github.com/nodejs/node/pull/2264).
  - [`String#normalize()`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/normalize) can now be used for unicode normalization.
  - The [`Intl`](https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Global_Objects/Intl) object and various `String` and `Number` methods are present, but only support the English locale.
  - For support of all locales, node must be built with [full-icu](https://github.com/nodejs/node#build-with-full-icu-support-all-locales-supported-by-icu).
* **tls**: Fixed tls throughput being much lower after an incorrect merge (Fedor Indutny) [#2381](https://github.com/nodejs/node/pull/2381).
* **tools**: The v8 tick processor now comes bundled with node (Matt Loring) [#2090](https://github.com/nodejs/node/pull/2090).
  - This can be used by producing performance profiling output by running node with `--perf`, then running your appropriate platform's script on the output as found in [tools/v8-prof](https://github.com/nodejs/node/tree/master/tools/v8-prof).
* **util**: `util.inspect(obj)` now prints the constructor name of the object if there is one (Christopher Monsanto) [#1935](https://github.com/nodejs/io.js/pull/1935).

### Known issues

See https://github.com/nodejs/io.js/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/io.js/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/io.js/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/io.js/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/io.js/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/io.js/issues/1435).

### Commits

* [[`3645dc62ed`](https://github.com/nodejs/node/commit/3645dc62ed)] - **build**: work around VS2015 issue in ICU <56 (Steven R. Loomis) [#2283](https://github.com/nodejs/node/pull/2283)
* [[`1f12e03266`](https://github.com/nodejs/node/commit/1f12e03266)] - **(SEMVER-MINOR)** **build**: intl: converge from joyent/node (Steven R. Loomis) [#2264](https://github.com/nodejs/node/pull/2264)
* [[`071640abdd`](https://github.com/nodejs/node/commit/071640abdd)] - **build**: Intl: bump ICU4C from 54 to 55 (Steven R. Loomis) [#2293](https://github.com/nodejs/node/pull/2293)
* [[`07a88b0c8b`](https://github.com/nodejs/node/commit/07a88b0c8b)] - **build**: update manifest to include Windows 10 (Lucien Greathouse) [#2332](https://github.com/nodejs/io.js/pull/2332)
* [[`0bb099f444`](https://github.com/nodejs/node/commit/0bb099f444)] - **build**: expand ~ in install prefix early (Ben Noordhuis) [#2307](https://github.com/nodejs/io.js/pull/2307)
* [[`7fe6dd8f5d`](https://github.com/nodejs/node/commit/7fe6dd8f5d)] - **crypto**: check for OpenSSL errors when signing (Minqi Pan) [#2342](https://github.com/nodejs/node/pull/2342)
* [[`605f6ee904`](https://github.com/nodejs/node/commit/605f6ee904)] - **crypto**: fix memory leak in PBKDF2Request (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`ba6eb8af12`](https://github.com/nodejs/node/commit/ba6eb8af12)] - **crypto**: fix memory leak in ECDH::SetPrivateKey (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`6a16368611`](https://github.com/nodejs/node/commit/6a16368611)] - **crypto**: fix memory leak in PublicKeyCipher::Cipher (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`a760a87803`](https://github.com/nodejs/node/commit/a760a87803)] - **crypto**: fix memory leak in SafeX509ExtPrint (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`f45487cd6e`](https://github.com/nodejs/node/commit/f45487cd6e)] - **crypto**: fix memory leak in SetDHParam (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`2ff183dd86`](https://github.com/nodejs/node/commit/2ff183dd86)] - **doc**: Update FIPS instructions in README.md (Michael Dawson) [#2278](https://github.com/nodejs/node/pull/2278)
* [[`6483bc2e8f`](https://github.com/nodejs/node/commit/6483bc2e8f)] - **doc**: clarify options for fs.watchFile() (Rich Trott) [#2425](https://github.com/nodejs/node/pull/2425)
* [[`e76822f454`](https://github.com/nodejs/node/commit/e76822f454)] - **doc**: multiple documentation updates cherry picked from v0.12 (James M Snell) [#2302](https://github.com/nodejs/io.js/pull/2302)
* [[`1738c9680b`](https://github.com/nodejs/node/commit/1738c9680b)] - **net**: ensure Socket reported address is current (Ryan Graham) [#2095](https://github.com/nodejs/io.js/pull/2095)
* [[`844d3f0e3e`](https://github.com/nodejs/node/commit/844d3f0e3e)] - **path**: use '===' instead of '==' for comparison (Sam Stites) [#2388](https://github.com/nodejs/node/pull/2388)
* [[`7118b8a882`](https://github.com/nodejs/node/commit/7118b8a882)] - **path**: remove dead code in favor of unit tests (Nathan Woltman) [#2282](https://github.com/nodejs/io.js/pull/2282)
* [[`34f2cfa806`](https://github.com/nodejs/node/commit/34f2cfa806)] - **src**: better error message on failed Buffer malloc (Karl Skomski) [#2422](https://github.com/nodejs/node/pull/2422)
* [[`b196c1da3c`](https://github.com/nodejs/node/commit/b196c1da3c)] - **src**: fix memory leak in DLOpen (Karl Skomski) [#2375](https://github.com/nodejs/node/pull/2375)
* [[`d1307b2995`](https://github.com/nodejs/node/commit/d1307b2995)] - **src**: don't use fopen() in require() fast path (Ben Noordhuis) [#2377](https://github.com/nodejs/node/pull/2377)
* [[`455ec570d1`](https://github.com/nodejs/node/commit/455ec570d1)] - **src**: rename Buffer::Use() to Buffer::New() (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352)
* [[`fd63e1ce2b`](https://github.com/nodejs/node/commit/fd63e1ce2b)] - **src**: introduce internal Buffer::Copy() function (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352)
* [[`5586ceca13`](https://github.com/nodejs/node/commit/5586ceca13)] - **src**: move internal functions out of node_buffer.h (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352)
* [[`bff9bcddb6`](https://github.com/nodejs/node/commit/bff9bcddb6)] - **src**: plug memory leaks (Ben Noordhuis) [#2352](https://github.com/nodejs/node/pull/2352)
* [[`ccf12df4f3`](https://github.com/nodejs/node/commit/ccf12df4f3)] - **(SEMVER-MINOR)** **src**: add total_available_size to v8 statistics (Roman Klauke) [#2348](https://github.com/nodejs/io.js/pull/2348)
* [[`194eeb841b`](https://github.com/nodejs/node/commit/194eeb841b)] - **test**: drop Isolate::GetCurrent() from addon tests (Ben Noordhuis) [#2427](https://github.com/nodejs/node/pull/2427)
* [[`46cdb2f6e2`](https://github.com/nodejs/node/commit/46cdb2f6e2)] - **test**: lint addon tests (Ben Noordhuis) [#2427](https://github.com/nodejs/node/pull/2427)
* [[`850c794882`](https://github.com/nodejs/node/commit/850c794882)] - **test**: refactor test-fs-watchfile.js (Rich Trott) [#2393](https://github.com/nodejs/node/pull/2393)
* [[`a3160c0a33`](https://github.com/nodejs/node/commit/a3160c0a33)] - **test**: correct spelling of 'childProcess' (muddletoes) [#2389](https://github.com/nodejs/node/pull/2389)
* [[`e51f90d747`](https://github.com/nodejs/node/commit/e51f90d747)] - **test**: option to run a subset of tests (João Reis) [#2260](https://github.com/nodejs/io.js/pull/2260)
* [[`cc46d3bca3`](https://github.com/nodejs/node/commit/cc46d3bca3)] - **test**: clarify dropMembership() call (Rich Trott) [#2062](https://github.com/nodejs/io.js/pull/2062)
* [[`0ee4df9c7a`](https://github.com/nodejs/node/commit/0ee4df9c7a)] - **test**: make listen-fd-cluster/server more robust (Sam Roberts) [#1944](https://github.com/nodejs/io.js/pull/1944)
* [[`cf9ba81398`](https://github.com/nodejs/node/commit/cf9ba81398)] - **test**: address timing issues in simple http tests (Gireesh Punathil) [#2294](https://github.com/nodejs/io.js/pull/2294)
* [[`cbb75c4f86`](https://github.com/nodejs/node/commit/cbb75c4f86)] - **tls**: fix throughput issues after incorrect merge (Fedor Indutny) [#2381](https://github.com/nodejs/node/pull/2381)
* [[`94b765f409`](https://github.com/nodejs/node/commit/94b765f409)] - **tls**: fix check for reused session (Fedor Indutny) [#2312](https://github.com/nodejs/io.js/pull/2312)
* [[`e83a41ad65`](https://github.com/nodejs/node/commit/e83a41ad65)] - **tls**: introduce internal `onticketkeycallback` (Fedor Indutny) [#2312](https://github.com/nodejs/io.js/pull/2312)
* [[`fb0f5d733f`](https://github.com/nodejs/node/commit/fb0f5d733f)] - **(SEMVER-MINOR)** **tools**: run the tick processor without building v8 (Matt Loring) [#2090](https://github.com/nodejs/node/pull/2090)
* [[`7606bdb897`](https://github.com/nodejs/node/commit/7606bdb897)] - **(SEMVER-MINOR)** **util**: display constructor when inspecting objects (Christopher Monsanto) [#1935](https://github.com/nodejs/io.js/pull/1935)

## 2015-08-04, Version 3.0.0, @rvagg

### Notable changes

* **buffer**:
  - Due to changes in V8, it has been necessary to reimplement `Buffer` on top of V8's `Uint8Array`. Every effort has been made to minimize the performance impact, however `Buffer` instantiation is measurably slower. Access operations may be faster in some circumstances but the exact performance profile and difference over previous versions will depend on how `Buffer` is used within applications. (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825).
  - `Buffer` can now take `ArrayBuffer`s as a constructor argument (Trevor Norris) [#2002](https://github.com/nodejs/node/pull/2002).
  - When a single buffer is passed to `Buffer.concat()`, a new, copied `Buffer` object will be returned; previous behavior was to return the original `Buffer` object (Sakthipriyan Vairamani) [#1937](https://github.com/nodejs/node/pull/1937).
* **build**: PPC support has been added to core to allow compiling on pLinux BE and LE (AIX support coming soon) (Michael Dawson) [#2124](https://github.com/nodejs/node/pull/2124).
* **dgram**: If an error occurs within `socket.send()` and a callback has been provided, the error is only passed as the first argument to the callback and not emitted on the `socket` object; previous behavior was to do both (Matteo Collina & Chris Dickinson) [#1796](https://github.com/nodejs/node/pull/1796)
* **freelist**: Deprecate the undocumented `freelist` core module (Sakthipriyan Vairamani) [#2176](https://github.com/nodejs/node/pull/2176).
* **http**:
  - Status codes now all use the official [IANA names](http://www.iana.org/assignments/http-status-codes) as per [RFC7231](https://tools.ietf.org/html/rfc7231), e.g. `http.STATUS_CODES[414]` now returns `'URI Too Long'` rather than `'Request-URI Too Large'` (jomo) [#1470](https://github.com/nodejs/node/pull/1470).
  - Calling .getName() on an HTTP agent no longer returns a trailing colon, HTTPS agents will no longer return an extra colon near the middle of the string (Brendan Ashworth) [#1617](https://github.com/nodejs/node/pull/1617).
* **node**:
  - `NODE_MODULE_VERSION` has been bumped to `45` to reflect the break in ABI (Rod Vagg) [#2096](https://github.com/nodejs/node/pull/2096).
  - Introduce a new `process.release` object that contains a `name` property set to `'io.js'` and `sourceUrl`, `headersUrl` and `libUrl` (Windows only) properties containing URLs for the relevant resources; this is intended to be used by node-gyp (Rod Vagg) [#2154](https://github.com/nodejs/node/pull/2154).
  - The version of node-gyp bundled with io.js now downloads and uses a tarball of header files from iojs.org rather than the full source for compiling native add-ons; it is hoped this is a temporary floating patch and the change will be upstreamed to node-gyp soon (Rod Vagg) [#2066](https://github.com/nodejs/node/pull/2066).
* **repl**: Persistent history is now enabled by default. The history file is located at ~/.node_repl_history, which can be overridden by the new environment variable `NODE_REPL_HISTORY`. This deprecates the previous `NODE_REPL_HISTORY_FILE` variable. Additionally, the format of the file has been changed to plain text to better handle file corruption. (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224).
* **smalloc**: The `smalloc` module has been removed as it is no longer possible to provide the API due to changes in V8 (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022).
* **tls**: Add `server.getTicketKeys()` and `server.setTicketKeys()` methods for [TLS session key](https://www.ietf.org/rfc/rfc5077.txt) rotation (Fedor Indutny) [#2227](https://github.com/nodejs/node/pull/2227).
* **v8**: Upgraded to 4.4.63.26
  - ES6: Enabled [computed property names](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Object_initializer#Computed_property_names)
  - ES6: `Array` can now be subclassed in strict mode
  - ES6: Implement [rest parameters](https://developer.mozilla.org/en/docs/Web/JavaScript/Reference/Functions/rest_parameters) in staging, use the `--harmony-rest-parameters` command line flag
  - ES6: Implement the [spread operator](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Spread_operator) in staging, use the `--harmony-spreadcalls` command line flag
  - Removed `SetIndexedPropertiesToExternalArrayData` and related APIs, forcing a shift to `Buffer` to be reimplemented based on `Uint8Array`
  - Introduction of `Maybe` and `MaybeLocal` C++ API for objects which _may_ or _may not_ have a value.
  - Added support for PPC

See also https://github.com/nodejs/node/wiki/Breaking-Changes#300-from-2x for a summary of the breaking changes (SEMVER-MAJOR).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`60a974d200`](https://github.com/nodejs/node/commit/60a974d200)] - **buffer**: fix missing null/undefined check (Trevor Norris) [#2195](https://github.com/nodejs/node/pull/2195)
* [[`e6ab2d92bc`](https://github.com/nodejs/node/commit/e6ab2d92bc)] - **buffer**: fix not return on error (Trevor Norris) [#2225](https://github.com/nodejs/node/pull/2225)
* [[`1057d1186b`](https://github.com/nodejs/node/commit/1057d1186b)] - **buffer**: rename internal/buffer_new.js to buffer.js (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022)
* [[`4643b8b667`](https://github.com/nodejs/node/commit/4643b8b667)] - **(SEMVER-MINOR)** **buffer**: allow ArrayBuffer as Buffer argument (Trevor Norris) [#2002](https://github.com/nodejs/node/pull/2002)
* [[`e5ada116cd`](https://github.com/nodejs/node/commit/e5ada116cd)] - **buffer**: minor cleanup from rebase (Trevor Norris) [#2003](https://github.com/nodejs/node/pull/2003)
* [[`b625ab4242`](https://github.com/nodejs/node/commit/b625ab4242)] - **buffer**: fix usage of kMaxLength (Trevor Norris) [#2003](https://github.com/nodejs/node/pull/2003)
* [[`eea66e2a7b`](https://github.com/nodejs/node/commit/eea66e2a7b)] - **(SEMVER-MAJOR)** **buffer**: fix case of one buffer passed to concat (Sakthipriyan Vairamani) [#1937](https://github.com/nodejs/node/pull/1937)
* [[`8664084166`](https://github.com/nodejs/node/commit/8664084166)] - **buffer**: make additional changes to native API (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`36f78f4c1c`](https://github.com/nodejs/node/commit/36f78f4c1c)] - **buffer**: switch API to return MaybeLocal<T> (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`571ec13841`](https://github.com/nodejs/node/commit/571ec13841)] - **buffer**: switch to using Maybe<T> API (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`d75f5c8d0e`](https://github.com/nodejs/node/commit/d75f5c8d0e)] - **buffer**: finish implementing FreeCallback (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`63da0dfd3a`](https://github.com/nodejs/node/commit/63da0dfd3a)] - **buffer**: implement Uint8Array backed Buffer (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`23be6ca189`](https://github.com/nodejs/node/commit/23be6ca189)] - **buffer**: allow ARGS_THIS to accept a name (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`971de5e417`](https://github.com/nodejs/node/commit/971de5e417)] - **build**: prepare Windows installer for i18n support (Frederic Hemberger) [#2247](https://github.com/nodejs/node/pull/2247)
* [[`2ba8b23661`](https://github.com/nodejs/node/commit/2ba8b23661)] - **build**: add 'x86' option back in to configure (Rod Vagg) [#2233](https://github.com/nodejs/node/pull/2233)
* [[`b4226e797a`](https://github.com/nodejs/node/commit/b4226e797a)] - **build**: first set of updates to enable PPC support (Michael Dawson) [#2124](https://github.com/nodejs/node/pull/2124)
* [[`24dd016deb`](https://github.com/nodejs/node/commit/24dd016deb)] - **build**: produce symbol map files on windows (Ali Ijaz Sheikh) [#2243](https://github.com/nodejs/node/pull/2243)
* [[`423d8944ce`](https://github.com/nodejs/node/commit/423d8944ce)] - **cluster**: do not unconditionally set --debug-port (cjihrig) [#1949](https://github.com/nodejs/node/pull/1949)
* [[`fa98b97171`](https://github.com/nodejs/node/commit/fa98b97171)] - **cluster**: add handle ref/unref stubs in rr mode (Ben Noordhuis) [#2274](https://github.com/nodejs/node/pull/2274)
* [[`944f68046c`](https://github.com/nodejs/node/commit/944f68046c)] - **crypto**: remove kMaxLength on randomBytes() (Trevor Norris) [#1825](https://github.com/nodejs/node/pull/1825)
* [[`3d3c687012`](https://github.com/nodejs/node/commit/3d3c687012)] - **deps**: update V8 to 4.4.63.26 (Michaël Zasso) [#2220](https://github.com/nodejs/node/pull/2220)
* [[`3aad4fa89a`](https://github.com/nodejs/node/commit/3aad4fa89a)] - **deps**: upgrade v8 to 4.4.63.12 (Ben Noordhuis) [#2092](https://github.com/nodejs/node/pull/2092)
* [[`70d1f32f56`](https://github.com/nodejs/node/commit/70d1f32f56)] - **(SEMVER-MAJOR)** **deps**: update v8 to 4.4.63.9 (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022)
* [[`deb7ee93a7`](https://github.com/nodejs/node/commit/deb7ee93a7)] - **deps**: backport 7b24219346 from v8 upstream (Rod Vagg) [#1805](https://github.com/nodejs/node/pull/1805)
* [[`d58e780504`](https://github.com/nodejs/node/commit/d58e780504)] - **(SEMVER-MAJOR)** **deps**: update v8 to 4.3.61.21 (Chris Dickinson) [iojs/io.js#1632](https://github.com/iojs/io.js/pull/1632)
* [[`2a63cf612b`](https://github.com/nodejs/node/commit/2a63cf612b)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`bf63266460`](https://github.com/nodejs/node/commit/bf63266460)] - **deps**: upgrade to npm 2.13.3 (Kat Marchán) [#2284](https://github.com/nodejs/node/pull/2284)
* [[`ef2c8cd4ec`](https://github.com/nodejs/node/commit/ef2c8cd4ec)] - **(SEMVER-MAJOR)** **dgram**: make send cb act as "error" event handler (Matteo Collina) [#1796](https://github.com/nodejs/node/pull/1796)
* [[`3da057fef6`](https://github.com/nodejs/node/commit/3da057fef6)] - **(SEMVER-MAJOR)** **dgram**: make send cb act as "error" event handler (Chris Dickinson) [#1796](https://github.com/nodejs/node/pull/1796)
* [[`df1994fe53`](https://github.com/nodejs/node/commit/df1994fe53)] - ***Revert*** "**dns**: remove AI_V4MAPPED hint flag on FreeBSD" (cjihrig) [iojs/io.js#1555](https://github.com/iojs/io.js/pull/1555)
* [[`1721968b22`](https://github.com/nodejs/node/commit/1721968b22)] - **doc**: document repl persistent history changes (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224)
* [[`d12df7f159`](https://github.com/nodejs/node/commit/d12df7f159)] - **doc**: update v8 flags in man page (Michaël Zasso) [iojs/io.js#1701](https://github.com/iojs/io.js/pull/1701)
* [[`d168d01b04`](https://github.com/nodejs/node/commit/d168d01b04)] - **doc**: properly inheriting from EventEmitter (Sakthipriyan Vairamani) [#2168](https://github.com/nodejs/node/pull/2168)
* [[`500f2538cc`](https://github.com/nodejs/node/commit/500f2538cc)] - **doc**: a listener, not "an" listener (Sam Roberts) [#1025](https://github.com/nodejs/node/pull/1025)
* [[`54627a919d`](https://github.com/nodejs/node/commit/54627a919d)] - **doc**: server close event does not have an argument (Sam Roberts) [#1025](https://github.com/nodejs/node/pull/1025)
* [[`ed85c95a9c`](https://github.com/nodejs/node/commit/ed85c95a9c)] - **doc,test**: documents behaviour of non-existent file (Sakthipriyan Vairamani) [#2169](https://github.com/nodejs/node/pull/2169)
* [[`2965442308`](https://github.com/nodejs/node/commit/2965442308)] - **(SEMVER-MAJOR)** **http**: fix agent.getName() and add tests (Brendan Ashworth) [#1617](https://github.com/nodejs/node/pull/1617)
* [[`2d9456e3e6`](https://github.com/nodejs/node/commit/2d9456e3e6)] - **(SEMVER-MAJOR)** **http**: use official IANA Status Codes (jomo) [#1470](https://github.com/nodejs/node/pull/1470)
* [[`11e4249227`](https://github.com/nodejs/node/commit/11e4249227)] - **(SEMVER-MAJOR)** **http_server**: `prefinish` vs `finish` (Fedor Indutny) [#1411](https://github.com/nodejs/node/pull/1411)
* [[`9bc2e26720`](https://github.com/nodejs/node/commit/9bc2e26720)] - **net**: do not set V4MAPPED on FreeBSD (Julien Gilli) [iojs/io.js#1555](https://github.com/iojs/io.js/pull/1555)
* [[`ba9ccf227e`](https://github.com/nodejs/node/commit/ba9ccf227e)] - **node**: remove redundant --use-old-buffer (Rod Vagg) [#2275](https://github.com/nodejs/node/pull/2275)
* [[`ef65321083`](https://github.com/nodejs/node/commit/ef65321083)] - **(SEMVER-MAJOR)** **node**: do not override `message`/`stack` of error (Fedor Indutny) [#2108](https://github.com/nodejs/node/pull/2108)
* [[`9f727f5e03`](https://github.com/nodejs/node/commit/9f727f5e03)] - **node-gyp**: detect RC build with x.y.z-rc.n format (Rod Vagg) [#2171](https://github.com/nodejs/node/pull/2171)
* [[`e52f963632`](https://github.com/nodejs/node/commit/e52f963632)] - **node-gyp**: download header tarball for compile (Rod Vagg) [#2066](https://github.com/nodejs/node/pull/2066)
* [[`902c9ca51d`](https://github.com/nodejs/node/commit/902c9ca51d)] - **node-gyp**: make aware of nightly, next-nightly & rc (Rod Vagg) [#2066](https://github.com/nodejs/node/pull/2066)
* [[`4cffaa3f55`](https://github.com/nodejs/node/commit/4cffaa3f55)] - **(SEMVER-MINOR)** **readline**: allow tabs in input (Rich Trott) [#1761](https://github.com/nodejs/node/pull/1761)
* [[`ed6c249104`](https://github.com/nodejs/node/commit/ed6c249104)] - **(SEMVER-MAJOR)** **repl**: persist history in plain text (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224)
* [[`f7d5e4c618`](https://github.com/nodejs/node/commit/f7d5e4c618)] - **(SEMVER-MINOR)** **repl**: default persistence to ~/.node_repl_history (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224)
* [[`ea05e760cd`](https://github.com/nodejs/node/commit/ea05e760cd)] - **repl**: don't clobber RegExp.$ properties (Sakthipriyan Vairamani) [#2137](https://github.com/nodejs/node/pull/2137)
* [[`d20093246b`](https://github.com/nodejs/node/commit/d20093246b)] - **src**: disable vector ICs on arm (Michaël Zasso) [#2220](https://github.com/nodejs/node/pull/2220)
* [[`04fd4fad46`](https://github.com/nodejs/node/commit/04fd4fad46)] - **(SEMVER-MINOR)** **src**: introduce process.release object (Rod Vagg) [#2154](https://github.com/nodejs/node/pull/2154)
* [[`9d34bd1147`](https://github.com/nodejs/node/commit/9d34bd1147)] - **src**: increment NODE_MODULE_VERSION to 45 (Rod Vagg) [#2096](https://github.com/nodejs/node/pull/2096)
* [[`ceee8d2807`](https://github.com/nodejs/node/commit/ceee8d2807)] - **test**: add tests for persistent repl history (Jeremiah Senkpiel) [#2224](https://github.com/nodejs/node/pull/2224)
* [[`8e1a8ffe24`](https://github.com/nodejs/node/commit/8e1a8ffe24)] - **test**: remove two obsolete pummel tests (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022)
* [[`ae731ec0fa`](https://github.com/nodejs/node/commit/ae731ec0fa)] - **test**: don't use arguments.callee (Ben Noordhuis) [#2022](https://github.com/nodejs/node/pull/2022)
* [[`21d31c08e7`](https://github.com/nodejs/node/commit/21d31c08e7)] - **test**: remove obsolete harmony flags (Chris Dickinson)
* [[`64cf71195c`](https://github.com/nodejs/node/commit/64cf71195c)] - **test**: change the hostname to an invalid name (Sakthipriyan Vairamani) [#2287](https://github.com/nodejs/node/pull/2287)
* [[`80a1cf7425`](https://github.com/nodejs/node/commit/80a1cf7425)] - **test**: fix messages and use return to skip tests (Sakthipriyan Vairamani) [#2290](https://github.com/nodejs/node/pull/2290)
* [[`d5ab92bcc1`](https://github.com/nodejs/node/commit/d5ab92bcc1)] - **test**: use common.isWindows consistently (Sakthipriyan Vairamani) [#2269](https://github.com/nodejs/node/pull/2269)
* [[`bc733f7065`](https://github.com/nodejs/node/commit/bc733f7065)] - **test**: fix fs.readFile('/dev/stdin') tests (Ben Noordhuis) [#2265](https://github.com/nodejs/node/pull/2265)
* [[`3cbb5870e5`](https://github.com/nodejs/node/commit/3cbb5870e5)] - **tools**: expose skip output to test runner (Johan Bergström) [#2130](https://github.com/nodejs/node/pull/2130)
* [[`3b021efe11`](https://github.com/nodejs/node/commit/3b021efe11)] - **vm**: fix symbol access (Domenic Denicola) [#1773](https://github.com/nodejs/node/pull/1773)
* [[`7b81e4ba36`](https://github.com/nodejs/node/commit/7b81e4ba36)] - **vm**: remove unnecessary access checks (Domenic Denicola) [#1773](https://github.com/nodejs/node/pull/1773)
* [[`659dadd410`](https://github.com/nodejs/node/commit/659dadd410)] - **vm**: fix property descriptors of sandbox properties (Domenic Denicola) [#1773](https://github.com/nodejs/node/pull/1773)
* [[`9bac1dbae9`](https://github.com/nodejs/node/commit/9bac1dbae9)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-07-28, Version 2.5.0, @cjihrig

### Notable changes

* **https**: TLS sessions in Agent are reused (Fedor Indutny) [#2228](https://github.com/nodejs/node/pull/2228)
* **src**: base64 decoding is now 50% faster (Ben Noordhuis) [#2193](https://github.com/nodejs/node/pull/2193)
* **npm**: Upgraded to v2.13.2, release notes can be found in <https://github.com/npm/npm/releases/tag/v2.13.2> (Kat Marchán) [#2241](https://github.com/nodejs/node/pull/2241).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Using multiple REPL instances in parallel may cause some REPL history corruption or loss. [#1634](https://github.com/nodejs/node/issues/1634)
* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`bf2cd225a8`](https://github.com/nodejs/node/commit/bf2cd225a8)] - **process**: resize stderr on SIGWINCH (Jeremiah Senkpiel) [#2231](https://github.com/nodejs/node/pull/2231)
* [[`99d9d7e716`](https://github.com/nodejs/node/commit/99d9d7e716)] - **benchmark**: add remaining path benchmarks & optimize (Nathan Woltman) [#2103](https://github.com/nodejs/node/pull/2103)
* [[`66fc8ca22b`](https://github.com/nodejs/node/commit/66fc8ca22b)] - **(SEMVER-MINOR)** **cluster**: emit 'message' event on cluster master (Sam Roberts) [#861](https://github.com/nodejs/node/pull/861)
* [[`eb35968de7`](https://github.com/nodejs/node/commit/eb35968de7)] - **crypto**: fix legacy SNICallback (Fedor Indutny) [#1720](https://github.com/nodejs/node/pull/1720)
* [[`fef190cea6`](https://github.com/nodejs/node/commit/fef190cea6)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`b73a7465c5`](https://github.com/nodejs/node/commit/b73a7465c5)] - **deps**: upgrade to npm 2.13.2 (Kat Marchán) [#2241](https://github.com/nodejs/node/pull/2241)
* [[`0a7bf81d2f`](https://github.com/nodejs/node/commit/0a7bf81d2f)] - **deps**: update V8 to 4.2.77.21 (Ali Ijaz Sheikh) [#2238](https://github.com/nodejs/node/issues/2238)
* [[`73cdcdd581`](https://github.com/nodejs/node/commit/73cdcdd581)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`04893a736d`](https://github.com/nodejs/node/commit/04893a736d)] - **deps**: upgrade to npm 2.13.1 (Kat Marchán) [#2210](https://github.com/nodejs/node/pull/2210)
* [[`a3c1b9720e`](https://github.com/nodejs/node/commit/a3c1b9720e)] - **doc**: add GPG fingerprint for cjihrig (cjihrig) [#2217](https://github.com/nodejs/node/pull/2217)
* [[`d9f857df3b`](https://github.com/nodejs/node/commit/d9f857df3b)] - **doc**: note about custom inspect functions (Sakthipriyan Vairamani) [#2142](https://github.com/nodejs/node/pull/2142)
* [[`4ef2b5fbfb`](https://github.com/nodejs/node/commit/4ef2b5fbfb)] - **doc**: Replace util.debug with console.error (Yosuke Furukawa) [#2214](https://github.com/nodejs/node/pull/2214)
* [[`b612f085ec`](https://github.com/nodejs/node/commit/b612f085ec)] - **doc**: add joaocgreis as a collaborator (João Reis) [#2208](https://github.com/nodejs/node/pull/2208)
* [[`6b85d5a4b3`](https://github.com/nodejs/node/commit/6b85d5a4b3)] - **doc**: add TSC meeting minutes 2015-07-15 (Rod Vagg) [#2191](https://github.com/nodejs/node/pull/2191)
* [[`c7d8b09162`](https://github.com/nodejs/node/commit/c7d8b09162)] - **doc**: recompile before testing core module changes (Phillip Johnsen) [#2051](https://github.com/nodejs/node/pull/2051)
* [[`9afee6785e`](https://github.com/nodejs/node/commit/9afee6785e)] - **http**: Check this.connection before using it (Sakthipriyan Vairamani) [#2172](https://github.com/nodejs/node/pull/2172)
* [[`2ca5a3db47`](https://github.com/nodejs/node/commit/2ca5a3db47)] - **https**: reuse TLS sessions in Agent (Fedor Indutny) [#2228](https://github.com/nodejs/node/pull/2228)
* [[`fef87fee1d`](https://github.com/nodejs/node/commit/fef87fee1d)] - **(SEMVER-MINOR)** **lib,test**: add freelist deprecation and test (Sakthipriyan Vairamani) [#2176](https://github.com/nodejs/node/pull/2176)
* [[`503b089dd8`](https://github.com/nodejs/node/commit/503b089dd8)] - **net**: don't throw on immediately destroyed socket (Evan Lucas) [#2251](https://github.com/nodejs/node/pull/2251)
* [[`93660c8b8e`](https://github.com/nodejs/node/commit/93660c8b8e)] - **node**: remove bad fn call and check (Trevor Norris) [#2157](https://github.com/nodejs/node/pull/2157)
* [[`afd7e37ee0`](https://github.com/nodejs/node/commit/afd7e37ee0)] - **repl**: better empty line handling (Sakthipriyan Vairamani) [#2163](https://github.com/nodejs/node/pull/2163)
* [[`81ea52aa01`](https://github.com/nodejs/node/commit/81ea52aa01)] - **repl**: improving line continuation handling (Sakthipriyan Vairamani) [#2163](https://github.com/nodejs/node/pull/2163)
* [[`30edb5aee9`](https://github.com/nodejs/node/commit/30edb5aee9)] - **repl**: preventing REPL crash with inherited properties (Sakthipriyan Vairamani) [#2163](https://github.com/nodejs/node/pull/2163)
* [[`77fa385e5d`](https://github.com/nodejs/node/commit/77fa385e5d)] - **repl**: fixing `undefined` in invalid REPL keyword error (Sakthipriyan Vairamani) [#2163](https://github.com/nodejs/node/pull/2163)
* [[`8fd3ce100e`](https://github.com/nodejs/node/commit/8fd3ce100e)] - **src**: make base64 decoding 50% faster (Ben Noordhuis) [#2193](https://github.com/nodejs/node/pull/2193)
* [[`c786d6341d`](https://github.com/nodejs/node/commit/c786d6341d)] - **test**: do not use public IPs for timeout testing (Rich Trott) [#2057](https://github.com/nodejs/node/pull/2057)
* [[`4e78cd71c0`](https://github.com/nodejs/node/commit/4e78cd71c0)] - **test**: skip IPv6 part before testing it (Sakthipriyan Vairamani) [#2226](https://github.com/nodejs/node/pull/2226)
* [[`ac70bc8240`](https://github.com/nodejs/node/commit/ac70bc8240)] - **test**: fix valgrind uninitialized memory warning (Ben Noordhuis) [#2193](https://github.com/nodejs/node/pull/2193)
* [[`ac7d3fa0d9`](https://github.com/nodejs/node/commit/ac7d3fa0d9)] - **test**: add -no_rand_screen to s_client opts on Win (Shigeki Ohtsu) [#2209](https://github.com/nodejs/node/pull/2209)
* [[`79c865a53f`](https://github.com/nodejs/node/commit/79c865a53f)] - **test**: changing process.exit to return while skipping tests (Sakthipriyan Vairamani) [#2109](https://github.com/nodejs/node/pull/2109)
* [[`69298d36cf`](https://github.com/nodejs/node/commit/69298d36cf)] - **test**: formatting skip messages for TAP parsing (Sakthipriyan Vairamani) [#2109](https://github.com/nodejs/node/pull/2109)
* [[`543dabb609`](https://github.com/nodejs/node/commit/543dabb609)] - **timers**: improve Timer.now() performance (Ben Noordhuis) [#2256](https://github.com/nodejs/node/pull/2256)
* [[`3663b124e6`](https://github.com/nodejs/node/commit/3663b124e6)] - **timers**: remove unused Timer.again() (Ben Noordhuis) [#2256](https://github.com/nodejs/node/pull/2256)
* [[`bcce5cf9bb`](https://github.com/nodejs/node/commit/bcce5cf9bb)] - **timers**: remove unused Timer.getRepeat() (Ben Noordhuis) [#2256](https://github.com/nodejs/node/pull/2256)
* [[`f2c83bd202`](https://github.com/nodejs/node/commit/f2c83bd202)] - **timers**: remove unused Timer.setRepeat() (Ben Noordhuis) [#2256](https://github.com/nodejs/node/pull/2256)
* [[`e11fc67225`](https://github.com/nodejs/node/commit/e11fc67225)] - **(SEMVER-MINOR)** **tls**: add `getTicketKeys()`/`setTicketKeys()` (Fedor Indutny) [#2227](https://github.com/nodejs/node/pull/2227)
* [[`68b06e94e3`](https://github.com/nodejs/node/commit/68b06e94e3)] - **tools**: use local or specified $NODE for test-npm (Jeremiah Senkpiel) [#1984](https://github.com/nodejs/node/pull/1984)
* [[`ab479659c7`](https://github.com/nodejs/node/commit/ab479659c7)] - **util**: delay creation of debug context (Ali Ijaz Sheikh) [#2248](https://github.com/nodejs/node/pull/2248)
* [[`6391f4d2fd`](https://github.com/nodejs/node/commit/6391f4d2fd)] - **util**: removing redundant checks in is* functions (Sakthipriyan Vairamani) [#2179](https://github.com/nodejs/node/pull/2179)
* [[`b148c0dff3`](https://github.com/nodejs/node/commit/b148c0dff3)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)
* [[`f90f1e75bb`](https://github.com/nodejs/node/commit/f90f1e75bb)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-07-17, Version 2.4.0, @Fishrock123

### Notable changes

* **src**: Added a new `--track-heap-objects` flag to track heap object allocations for heap snapshots (Bradley Meck) [#2135](https://github.com/nodejs/node/pull/2135).
* **readline**: Fixed a freeze that affected the repl if the keypress event handler threw (Alex Kocharin) [#2107](https://github.com/nodejs/node/pull/2107).
* **npm**: Upgraded to v2.13.0, release notes can be found in <https://github.com/npm/npm/releases/tag/v2.13.0> (Forrest L Norvell) [#2152](https://github.com/nodejs/node/pull/2152).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`f95f9ef6ea`](https://github.com/nodejs/node/commit/f95f9ef6ea)] - **build**: always use prefix=/ for tar-headers (Rod Vagg) [#2082](https://github.com/nodejs/node/pull/2082)
* [[`12bc397207`](https://github.com/nodejs/node/commit/12bc397207)] - **build**: run-ci makefile rule (Alexis Campailla) [#2134](https://github.com/nodejs/node/pull/2134)
* [[`84012c99e0`](https://github.com/nodejs/node/commit/84012c99e0)] - **build**: fix vcbuild merge issues (Alexis Campailla) [#2131](https://github.com/nodejs/node/pull/2131)
* [[`47e2c5c828`](https://github.com/nodejs/node/commit/47e2c5c828)] - **build**: bail early if clean is invoked (Johan Bergström) [#2127](https://github.com/nodejs/node/pull/2127)
* [[`5acad6b163`](https://github.com/nodejs/node/commit/5acad6b163)] - **child_process**: fix arguments comments (Roman Reiss) [#2161](https://github.com/nodejs/node/pull/2161)
* [[`3c4121c418`](https://github.com/nodejs/node/commit/3c4121c418)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`938cc757bb`](https://github.com/nodejs/node/commit/938cc757bb)] - **deps**: upgrade to npm 2.13.0 (Forrest L Norvell) [#2152](https://github.com/nodejs/node/pull/2152)
* [[`6f306e0ed2`](https://github.com/nodejs/node/commit/6f306e0ed2)] - **doc**: add targos as a collaborator (Michaël Zasso) [#2200](https://github.com/nodejs/node/pull/2200)
* [[`c019d9a239`](https://github.com/nodejs/node/commit/c019d9a239)] - **doc**: add thefourtheye as a collaborator (Sakthipriyan Vairamani) [#2199](https://github.com/nodejs/node/pull/2199)
* [[`4e92dbc26b`](https://github.com/nodejs/node/commit/4e92dbc26b)] - **doc**: add TSC members from the combined project (Jeremiah Senkpiel) [#2085](https://github.com/nodejs/node/pull/2085)
* [[`6c3aabf455`](https://github.com/nodejs/node/commit/6c3aabf455)] - **doc**: add TSC meeting minutes 2015-07-08 (Rod Vagg) [#2184](https://github.com/nodejs/node/pull/2184)
* [[`30a0d47d51`](https://github.com/nodejs/node/commit/30a0d47d51)] - **doc**: add TSC meeting minutes 2015-07-01 (Rod Vagg) [#2132](https://github.com/nodejs/node/pull/2132)
* [[`23efb05cc3`](https://github.com/nodejs/node/commit/23efb05cc3)] - **doc**: document fs.watchFile behaviour on ENOENT (Brendan Ashworth) [#2093](https://github.com/nodejs/node/pull/2093)
* [[`65963ec26f`](https://github.com/nodejs/node/commit/65963ec26f)] - **doc,test**: empty strings in path module (Sakthipriyan Vairamani) [#2106](https://github.com/nodejs/node/pull/2106)
* [[`0ab81e6f58`](https://github.com/nodejs/node/commit/0ab81e6f58)] - **docs**: link to more up-to-date v8 docs (Jeremiah Senkpiel) [#2196](https://github.com/nodejs/node/pull/2196)
* [[`1afc0c9e86`](https://github.com/nodejs/node/commit/1afc0c9e86)] - **fs**: fix error on bad listener type (Brendan Ashworth) [#2093](https://github.com/nodejs/node/pull/2093)
* [[`2ba84606a6`](https://github.com/nodejs/node/commit/2ba84606a6)] - **path**: assert path.join() arguments equally (Phillip Johnsen) [#2159](https://github.com/nodejs/node/pull/2159)
* [[`bd01603201`](https://github.com/nodejs/node/commit/bd01603201)] - **readline**: fix freeze if `keypress` event throws (Alex Kocharin) [#2107](https://github.com/nodejs/node/pull/2107)
* [[`59f6b5da2a`](https://github.com/nodejs/node/commit/59f6b5da2a)] - **repl**: Prevent crash when tab-completed with Proxy (Sakthipriyan Vairamani) [#2120](https://github.com/nodejs/node/pull/2120)
* [[`cf14a2427c`](https://github.com/nodejs/node/commit/cf14a2427c)] - **(SEMVER-MINOR)** **src**: add --track-heap-objects (Bradley Meck) [#2135](https://github.com/nodejs/node/pull/2135)
* [[`2b4b600660`](https://github.com/nodejs/node/commit/2b4b600660)] - **test**: fix test-debug-port-from-cmdline (João Reis) [#2186](https://github.com/nodejs/node/pull/2186)
* [[`d4ceb16da2`](https://github.com/nodejs/node/commit/d4ceb16da2)] - **test**: properly clean up temp directory (Roman Reiss) [#2164](https://github.com/nodejs/node/pull/2164)
* [[`842eb5b853`](https://github.com/nodejs/node/commit/842eb5b853)] - **test**: add test for dgram.setTTL (Evan Lucas) [#2121](https://github.com/nodejs/node/pull/2121)
* [[`cff7300a57`](https://github.com/nodejs/node/commit/cff7300a57)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-07-09, Version 2.3.4, @Fishrock123

### Notable changes

* **openssl**: Upgrade to 1.0.2d, fixes CVE-2015-1793 (Alternate Chains Certificate Forgery) (Shigeki Ohtsu) [#2141](https://github.com/nodejs/node/pull/2141).
* **npm**: Upgraded to v2.12.1, release notes can be found in <https://github.com/npm/npm/releases/tag/v2.12.0> and <https://github.com/npm/npm/releases/tag/v2.12.1> (Kat Marchán) [#2112](https://github.com/nodejs/node/pull/2112).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`0d15161c24`](https://github.com/nodejs/node/commit/0d15161c24)] - **benchmark**: Add some path benchmarks for #1778 (Nathan Woltman) [#1778](https://github.com/nodejs/node/pull/1778)
* [[`c70e68fa32`](https://github.com/nodejs/node/commit/c70e68fa32)] - **deps**: update deps/openssl/conf/arch/*/opensslconf.h (Shigeki Ohtsu) [#2141](https://github.com/nodejs/node/pull/2141)
* [[`ca93f7f2e6`](https://github.com/nodejs/node/commit/ca93f7f2e6)] - **deps**: upgrade openssl sources to 1.0.2d (Shigeki Ohtsu) [#2141](https://github.com/nodejs/node/pull/2141)
* [[`b18c841ec1`](https://github.com/nodejs/node/commit/b18c841ec1)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`863cdbdd08`](https://github.com/nodejs/node/commit/863cdbdd08)] - **deps**: upgrade to npm 2.12.1 (Kat Marchán) [#2112](https://github.com/nodejs/node/pull/2112)
* [[`84b3915764`](https://github.com/nodejs/node/commit/84b3915764)] - **doc**: document current release procedure (Rod Vagg) [#2099](https://github.com/nodejs/node/pull/2099)
* [[`46140334cd`](https://github.com/nodejs/node/commit/46140334cd)] - **doc**: update AUTHORS list (Rod Vagg) [#2100](https://github.com/nodejs/node/pull/2100)
* [[`bca53dce76`](https://github.com/nodejs/node/commit/bca53dce76)] - **path**: refactor for performance and consistency (Nathan Woltman) [#1778](https://github.com/nodejs/node/pull/1778)
* [[`6bef15afe7`](https://github.com/nodejs/node/commit/6bef15afe7)] - **src**: remove traceSyncIO property from process (Bradley Meck) [#2143](https://github.com/nodejs/node/pull/2143)
* [[`2ba1740ba1`](https://github.com/nodejs/node/commit/2ba1740ba1)] - **test**: add missing crypto checks (Johan Bergström) [#2129](https://github.com/nodejs/node/pull/2129)
* [[`180fd392ca`](https://github.com/nodejs/node/commit/180fd392ca)] - **test**: refactor test-repl-tab-complete (Sakthipriyan Vairamani) [#2122](https://github.com/nodejs/node/pull/2122)
* [[`fb05c8e27d`](https://github.com/nodejs/node/commit/fb05c8e27d)] - ***Revert*** "**test**: add test for missing `close`/`finish` event" (Fedor Indutny)
* [[`9436a860cb`](https://github.com/nodejs/node/commit/9436a860cb)] - **test**: add test for missing `close`/`finish` event (Mark Plomer) [iojs/io.js#1373](https://github.com/iojs/io.js/pull/1373)
* [[`ee3ce2ed88`](https://github.com/nodejs/node/commit/ee3ce2ed88)] - **tools**: install gdbinit from v8 to $PREFIX/share (Ali Ijaz Sheikh) [#2123](https://github.com/nodejs/node/pull/2123)
* [[`dd523c75da`](https://github.com/nodejs/node/commit/dd523c75da)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-07-09, Version 1.8.4, @Fishrock123

**Maintenance release**

### Notable changes

* **openssl**: Upgrade to 1.0.2d, fixes CVE-2015-1793 (Alternate Chains Certificate Forgery) [#2141](https://github.com/nodejs/node/pull/2141).

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`52b1230628`](https://github.com/nodejs/node/commit/52b1230628)] - **deps**: update deps/openssl/conf/arch/*/opensslconf.h (Shigeki Ohtsu) [#2141](https://github.com/nodejs/node/pull/2141)
* [[`20ff1e2ecb`](https://github.com/nodejs/node/commit/20ff1e2ecb)] - **deps**: upgrade openssl sources to 1.0.2d (Shigeki Ohtsu) [#2141](https://github.com/nodejs/node/pull/2141)

## 2015-07-09, Version 0.12.7 (Stable)

### Commits

* [[`0cf9f27703`](https://github.com/nodejs/node/commit/0cf9f27703)] - **deps**: upgrade openssl sources to 1.0.1p [#25654](https://github.com/joyent/node/pull/25654)
* [[`8917e430b8`](https://github.com/nodejs/node/commit/8917e430b8)] - **deps**: upgrade to npm 2.11.3 [#25545](https://github.com/joyent/node/pull/25545)
* [[`88a27a9621`](https://github.com/nodejs/node/commit/88a27a9621)] - **V8**: cherry-pick JitCodeEvent patch from upstream (Ben Noordhuis) [#25589](https://github.com/joyent/node/pull/25589)
* [[`18d413d299`](https://github.com/nodejs/node/commit/18d413d299)] - **win,msi**: create npm folder in AppData directory (Steven Rockarts) [#8838](https://github.com/joyent/node/pull/8838)

## 2015-07-09, Version 0.10.40 (Maintenance)

### Commits

* [[`0cf9f27703`](https://github.com/nodejs/node/commit/0cf9f27703)] - **openssl**: upgrade to 1.0.1p [#25654](https://github.com/joyent/node/pull/25654)
* [[`5a60e0d904`](https://github.com/nodejs/node/commit/5a60e0d904)] - **V8**: back-port JitCodeEvent patch from upstream (Ben Noordhuis) [#25588](https://github.com/joyent/node/pull/25588)
* [[`18d413d299`](https://github.com/nodejs/node/commit/18d413d299)] - **win,msi**: create npm folder in AppData directory (Steven Rockarts) [#8838](https://github.com/joyent/node/pull/8838)

## 2015-07-04, Version 2.3.3, @Fishrock123

### Notable changes

* **deps**: Fixed an out-of-band write in utf8 decoder. **This is an important security update** as it can be used to cause a denial of service attack.

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal. [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760).
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion. [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

## Commits

* [[`030f8045c7`](https://github.com/nodejs/node/commit/030f8045c7)] - **deps**: fix out-of-band write in utf8 decoder (Fedor Indutny)
* [[`0f09b8db28`](https://github.com/nodejs/node/commit/0f09b8db28)] - **doc**: don't recommend domains for error handling (Benjamin Gruenbaum) [#2056](https://github.com/nodejs/node/pull/2056)
* [[`9cd44bb2b6`](https://github.com/nodejs/node/commit/9cd44bb2b6)] - **util**: prepend '(node) ' to deprecation messages (Sakthipriyan Vairamani) [#1892](https://github.com/nodejs/node/pull/1892)

## 2015-07-04, Version 1.8.3, @rvagg

**Maintenance release**

## Notable changes

* **v8**: Fixed an out-of-band write in utf8 decoder. **This is an important security update** as it can be used to cause a denial of service attack.
* **openssl**: Upgrade to 1.0.2b and 1.0.2c, introduces DHE man-in-the-middle protection (Logjam) and fixes malformed ECParameters causing infinite loop (CVE-2015-1788). See the [security advisory](https://www.openssl.org/news/secadv_20150611.txt) for full details. (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950) [#1958](https://github.com/nodejs/node/pull/1958)
* **build**:
  - Added support for compiling with Microsoft Visual C++ 2015
  - Started building and distributing headers-only tarballs along with binaries

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`d8f260d33b`](https://github.com/nodejs/node/commit/d8f260d33b)] - **build**: add tar-headers target for headers-only tar (Rod Vagg) [#1975](https://github.com/nodejs/node/pull/1975)
* [[`00ba429674`](https://github.com/nodejs/node/commit/00ba429674)] - **build**: update build targets for io.js (Rod Vagg) [#1938](https://github.com/nodejs/node/pull/1938)
* [[`39e2207ff1`](https://github.com/nodejs/node/commit/39e2207ff1)] - **build**: fix cherry-pick ooops, fix comment wording (Rod Vagg) [#2036](https://github.com/nodejs/node/pull/2036)
* [[`561919a67a`](https://github.com/nodejs/node/commit/561919a67a)] - **build**: add MSVS 2015 support (Rod Vagg) [#2036](https://github.com/nodejs/node/pull/2036)
* [[`8e1134c04c`](https://github.com/nodejs/node/commit/8e1134c04c)] - **build**: remove lint from test-ci on windows (Johan Bergström) [#2004](https://github.com/nodejs/node/pull/2004)
* [[`e52e99085e`](https://github.com/nodejs/node/commit/e52e99085e)] - **build**: don't run lint from test-ci (Johan Bergström) [#1965](https://github.com/nodejs/node/pull/1965)
* [[`c5d1ec7fea`](https://github.com/nodejs/node/commit/c5d1ec7fea)] - **build**: simplify execution of built binary (Johan Bergström) [#1955](https://github.com/nodejs/node/pull/1955)
* [[`2ce147551a`](https://github.com/nodejs/node/commit/2ce147551a)] - **build,win**: set env before generating projects (Alexis Campailla) [joyent/node#20109](https://github.com/joyent/node/pull/20109)
* [[`78de5f85f2`](https://github.com/nodejs/node/commit/78de5f85f2)] - **deps**: fix out-of-band write in utf8 decoder (Ben Noordhuis)
* [[`83ee07b6be`](https://github.com/nodejs/node/commit/83ee07b6be)] - **deps**: copy all openssl header files to include dir (Shigeki Ohtsu) [#2016](https://github.com/nodejs/node/pull/2016)
* [[`a97125520d`](https://github.com/nodejs/node/commit/a97125520d)] - **deps**: update UPGRADING.md doc to openssl-1.0.2c (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`0e2d068e0b`](https://github.com/nodejs/node/commit/0e2d068e0b)] - **deps**: replace all headers in openssl (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`310b8d1120`](https://github.com/nodejs/node/commit/310b8d1120)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`a472946747`](https://github.com/nodejs/node/commit/a472946747)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [nodejs/node#1389](https://github.com/nodejs/node/pull/1389)
* [[`b2467e3ebf`](https://github.com/nodejs/node/commit/b2467e3ebf)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [nodejs/node#1389](https://github.com/nodejs/node/pull/1389)
* [[`e548abb800`](https://github.com/nodejs/node/commit/e548abb800)] - **deps**: upgrade openssl sources to 1.0.2c (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`1feaa68e85`](https://github.com/nodejs/node/commit/1feaa68e85)] - **deps**: update asm files for openssl-1.0.2b (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`151720fae7`](https://github.com/nodejs/node/commit/151720fae7)] - **deps**: replace all headers in openssl (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`139da6a02a`](https://github.com/nodejs/node/commit/139da6a02a)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`283642827a`](https://github.com/nodejs/node/commit/283642827a)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [nodejs/node#1389](https://github.com/nodejs/node/pull/1389)
* [[`d593b552de`](https://github.com/nodejs/node/commit/d593b552de)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [nodejs/node#1389](https://github.com/nodejs/node/pull/1389)
* [[`2a3367a4bd`](https://github.com/nodejs/node/commit/2a3367a4bd)] - **deps**: upgrade openssl sources to 1.0.2b (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`5c29c0c519`](https://github.com/nodejs/node/commit/5c29c0c519)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [nodejs/node#1389](https://github.com/nodejs/node/pull/1389)
* [[`2cd7f73d9f`](https://github.com/nodejs/node/commit/2cd7f73d9f)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [nodejs/node#1389](https://github.com/nodejs/node/pull/1389)
* [[`c65484a74d`](https://github.com/nodejs/node/commit/c65484a74d)] - **tls**: make server not use DHE in less than 1024bits (Shigeki Ohtsu) [#1739](https://github.com/nodejs/node/pull/1739)
* [[`77f518403f`](https://github.com/nodejs/node/commit/77f518403f)] - **win,node-gyp**: make delay-load hook C89 compliant (Sharat M R) [TooTallNate/node-gyp#616](https://github.com/TooTallNa

## 2015-07-03, Version 0.12.6 (Stable)

### Notable changes

* **deps**: Fixed an out-of-band write in utf8 decoder. **This is an important security update** as it can be used to cause a denial of service attack.

### Commits

* [[`78b0e30954`](https://github.com/nodejs/node/commit/78b0e30954)] - **deps**: fix out-of-band write in utf8 decoder (Fedor Indutny)

## 2015-07-01, Version 2.3.2, @rvagg

### Notable changes

* **build**:
  - Added support for compiling with Microsoft Visual C++ 2015
  - Started building and distributing headers-only tarballs along with binaries

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

## Commits

* [[`9180140231`](https://github.com/nodejs/node/commit/9180140231)] - **_stream_wrap**: prevent use after free in TLS (Fedor Indutny) [#1910](https://github.com/nodejs/node/pull/1910)
* [[`05a73c0f25`](https://github.com/nodejs/node/commit/05a73c0f25)] - **benchmark**: make concurrent requests configurable (Rich Trott) [#2068](https://github.com/nodejs/node/pull/2068)
* [[`f52d73352e`](https://github.com/nodejs/node/commit/f52d73352e)] - **benchmark**: fix typo in README (Rich Trott) [#2067](https://github.com/nodejs/node/pull/2067)
* [[`1cd9eeb556`](https://github.com/nodejs/node/commit/1cd9eeb556)] - **buffer**: prevent abort on bad proto (Trevor Norris) [#2012](https://github.com/nodejs/node/pull/2012)
* [[`8350f3a3a2`](https://github.com/nodejs/node/commit/8350f3a3a2)] - **buffer**: optimize Buffer#toString() (Ben Noordhuis) [#2027](https://github.com/nodejs/node/pull/2027)
* [[`628a3ab093`](https://github.com/nodejs/node/commit/628a3ab093)] - **build**: add tar-headers target for headers-only tar (Rod Vagg) [#1975](https://github.com/nodejs/node/pull/1975)
* [[`dcbb9e1da6`](https://github.com/nodejs/node/commit/dcbb9e1da6)] - **build**: update build targets for io.js (Rod Vagg) [#1938](https://github.com/nodejs/node/pull/1938)
* [[`c87c34c242`](https://github.com/nodejs/node/commit/c87c34c242)] - **build**: fix cherry-pick ooops, fix comment wording (Rod Vagg) [#2036](https://github.com/nodejs/node/pull/2036)
* [[`4208dc4fef`](https://github.com/nodejs/node/commit/4208dc4fef)] - **build**: add MSVS 2015 support (Rod Vagg) [#2036](https://github.com/nodejs/node/pull/2036)
* [[`834a365113`](https://github.com/nodejs/node/commit/834a365113)] - **build**: DTrace is enabled by default on darwin (Evan Lucas) [#2019](https://github.com/nodejs/node/pull/2019)
* [[`c0c0d73269`](https://github.com/nodejs/node/commit/c0c0d73269)] - **build,win**: set env before generating projects (Alexis Campailla) [joyent/node#20109](https://github.com/joyent/node/pull/20109)
* [[`9e890fe8b4`](https://github.com/nodejs/node/commit/9e890fe8b4)] - **crypto**: fix VerifyCallback in case of verify error (Shigeki Ohtsu) [#2064](https://github.com/nodejs/node/pull/2064)
* [[`1f371e3988`](https://github.com/nodejs/node/commit/1f371e3988)] - **deps**: copy all openssl header files to include dir (Shigeki Ohtsu) [#2016](https://github.com/nodejs/node/pull/2016)
* [[`c370bd3aea`](https://github.com/nodejs/node/commit/c370bd3aea)] - **doc**: make the abbreviation 1MM clear (Ivan Yan) [#2053](https://github.com/nodejs/node/pull/2053)
* [[`54d5437566`](https://github.com/nodejs/node/commit/54d5437566)] - **doc**: Added sample command to test iojs build (Jimmy Hsu) [#850](https://github.com/nodejs/node/pull/850)
* [[`f1f1b7e597`](https://github.com/nodejs/node/commit/f1f1b7e597)] - **doc**: add TSC meeting minutes 2015-06-17 (Rod Vagg) [#2048](https://github.com/nodejs/node/pull/2048)
* [[`dbd5dc932d`](https://github.com/nodejs/node/commit/dbd5dc932d)] - **doc**: clarify prerequisites in benchmark/README.md (Jeremiah Senkpiel) [#2034](https://github.com/nodejs/node/pull/2034)
* [[`50dbc8e143`](https://github.com/nodejs/node/commit/50dbc8e143)] - **doc**: add TSC meeting minutes 2015-05-27 (Rod Vagg) [#2037](https://github.com/nodejs/node/pull/2037)
* [[`941ad362a7`](https://github.com/nodejs/node/commit/941ad362a7)] - **doc**: archive io.js TC minutes (Rod Vagg)
* [[`644b2eaa89`](https://github.com/nodejs/node/commit/644b2eaa89)] - **doc**: rename tc-meetings to tsc-meetings (Rod Vagg)
* [[`1330ee3b27`](https://github.com/nodejs/node/commit/1330ee3b27)] - **doc**: add TC meeting 2015-05-13 minutes (Rod Vagg) [#1700](https://github.com/nodejs/node/pull/1700)
* [[`392e8fd64e`](https://github.com/nodejs/node/commit/392e8fd64e)] - **doc**: add @shigeki and @mscdex to TC (Rod Vagg) [#2008](https://github.com/nodejs/node/pull/2008)
* [[`af249fa8a1`](https://github.com/nodejs/node/commit/af249fa8a1)] - **net**: wrap connect in nextTick (Evan Lucas) [#2054](https://github.com/nodejs/node/pull/2054)
* [[`7f63449fde`](https://github.com/nodejs/node/commit/7f63449fde)] - **net**: fix debug for dnsopts (Evan Lucas) [#2059](https://github.com/nodejs/node/pull/2059)
* [[`eabed2f518`](https://github.com/nodejs/node/commit/eabed2f518)] - **repl**: remove obsolete TODO (Rich Trott) [#2081](https://github.com/nodejs/node/pull/2081)
* [[`a198c68b56`](https://github.com/nodejs/node/commit/a198c68b56)] - **repl**: make 'Unexpected token' errors recoverable (Julien Gilli) [#2052](https://github.com/nodejs/node/pull/2052)
* [[`d735b2c6ef`](https://github.com/nodejs/node/commit/d735b2c6ef)] - **repl**: fix tab completion for a non-global context (Sangmin Yoon) [#2052](https://github.com/nodejs/node/pull/2052)
* [[`8cee8f54fc`](https://github.com/nodejs/node/commit/8cee8f54fc)] - **src**: nix stdin _readableState.reading manipulation (Chris Dickinson) [#454](https://github.com/nodejs/node/pull/454)
* [[`856c11f8c8`](https://github.com/nodejs/node/commit/856c11f8c8)] - **test**: purge stale disabled tests (Rich Trott) [#2045](https://github.com/nodejs/node/pull/2045)
* [[`4d5089e181`](https://github.com/nodejs/node/commit/4d5089e181)] - **test**: do not swallow OpenSSL support error (Rich Trott) [#2042](https://github.com/nodejs/node/pull/2042)
* [[`06721fe005`](https://github.com/nodejs/node/commit/06721fe005)] - **test**: fix test-repl-tab-complete.js (cjihrig) [#2052](https://github.com/nodejs/node/pull/2052)
* [[`8e9089ac35`](https://github.com/nodejs/node/commit/8e9089ac35)] - **test**: check for error on Windows (Rich Trott) [#2035](https://github.com/nodejs/node/pull/2035)
* [[`776a65ebcd`](https://github.com/nodejs/node/commit/776a65ebcd)] - **test**: remove obsolete TODO comments (Rich Trott) [#2033](https://github.com/nodejs/node/pull/2033)
* [[`bdfeb798ad`](https://github.com/nodejs/node/commit/bdfeb798ad)] - **test**: remove obsolete TODO comments (Rich Trott) [#2032](https://github.com/nodejs/node/pull/2032)
* [[`58e914f9bc`](https://github.com/nodejs/node/commit/58e914f9bc)] - **tools**: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/iojs/io.js/pull/1325)
* [[`99cbbc0a13`](https://github.com/nodejs/node/commit/99cbbc0a13)] - **tools**: update gyp to 25ed9ac (Ben Noordhuis) [#2074](https://github.com/nodejs/node/pull/2074)
* [[`e3f9335c40`](https://github.com/nodejs/node/commit/e3f9335c40)] - **tools**: re-enable comma-spacing linter rule (Roman Reiss) [#2072](https://github.com/nodejs/node/pull/2072)
* [[`d91e10b3bd`](https://github.com/nodejs/node/commit/d91e10b3bd)] - **tools**: update eslint to 0.24.0 (Roman Reiss) [#2072](https://github.com/nodejs/node/pull/2072)
* [[`6c61ca5325`](https://github.com/nodejs/node/commit/6c61ca5325)] - **url**: fix typo in comment (Rich Trott) [#2071](https://github.com/nodejs/node/pull/2071)
* [[`1a51f0058c`](https://github.com/nodejs/node/commit/1a51f0058c)] - **v8**: cherry-pick JitCodeEvent patch from upstream (Ben Noordhuis) [#2075](https://github.com/nodejs/node/pull/2075)

## 2015-06-23, Version 2.3.1, @rvagg

### Notable changes

* **module**: The number of syscalls made during a `require()` have been significantly reduced again (see [#1801](https://github.com/nodejs/node/pull/1801) from v2.2.0 for previous work), which should lead to a performance improvement (Pierre Inglebert) [#1920](https://github.com/nodejs/node/pull/1920).
* **npm**:
  * Upgrade to [v2.11.2](https://github.com/npm/npm/releases/tag/v2.11.2) (Rebecca Turner) [#1956](https://github.com/nodejs/node/pull/1956).
  * Upgrade to [v2.11.3](https://github.com/npm/npm/releases/tag/v2.11.3) (Forrest L Norvell) [#2018](https://github.com/nodejs/node/pull/2018).
* **zlib**: A bug was discovered where the process would abort if the final part of a zlib decompression results in a buffer that would exceed the maximum length of `0x3fffffff` bytes (~1GiB). This was likely to only occur during buffered decompression (rather than streaming). This is now fixed and will instead result in a thrown `RangeError` (Michaël Zasso) [#1811](https://github.com/nodejs/node/pull/1811).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

## Commits

* [[`e56758a5e0`](https://github.com/nodejs/node/commit/e56758a5e0)] - **async-wrap**: add provider id and object info cb (Trevor Norris) [#1896](https://github.com/nodejs/node/pull/1896)
* [[`d5637e67c9`](https://github.com/nodejs/node/commit/d5637e67c9)] - **buffer**: fix cyclic dependency with util (Brendan Ashworth) [#1988](https://github.com/nodejs/node/pull/1988)
* [[`c5353d7c62`](https://github.com/nodejs/node/commit/c5353d7c62)] - **build**: remove lint from test-ci on windows (Johan Bergström) [#2004](https://github.com/nodejs/node/pull/2004)
* [[`c207e8d223`](https://github.com/nodejs/node/commit/c207e8d223)] - **build**: fix pkg-config output parsing in configure (Ben Noordhuis) [#1986](https://github.com/nodejs/node/pull/1986)
* [[`8d8a26e8f7`](https://github.com/nodejs/node/commit/8d8a26e8f7)] - **build**: don't run lint from test-ci (Johan Bergström) [#1965](https://github.com/nodejs/node/pull/1965)
* [[`1ec53c044d`](https://github.com/nodejs/node/commit/1ec53c044d)] - **build**: simplify execution of built binary (Johan Bergström) [#1955](https://github.com/nodejs/node/pull/1955)
* [[`3beb880716`](https://github.com/nodejs/node/commit/3beb880716)] - **crypto**: add cert check to CNNIC Whitelist (Shigeki Ohtsu) [#1895](https://github.com/nodejs/node/pull/1895)
* [[`48c0fb8b1a`](https://github.com/nodejs/node/commit/48c0fb8b1a)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`6a359b1ce9`](https://github.com/nodejs/node/commit/6a359b1ce9)] - **deps**: upgrade to npm 2.11.3 (Forrest L Norvell) [#2018](https://github.com/nodejs/node/pull/2018)
* [[`6aab2f3b9a`](https://github.com/nodejs/node/commit/6aab2f3b9a)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`3e12561b55`](https://github.com/nodejs/node/commit/3e12561b55)] - **deps**: upgrade to npm 2.11.2 (Rebecca Turner) [#1956](https://github.com/nodejs/node/pull/1956)
* [[`8ac50819b6`](https://github.com/nodejs/node/commit/8ac50819b6)] - **doc**: add security section to README.md (Rod Vagg) [#1948](https://github.com/nodejs/node/pull/1948)
* [[`1f93b63b11`](https://github.com/nodejs/node/commit/1f93b63b11)] - **doc**: change the info to the same as in gitconfig (Christian Tellnes) [#2000](https://github.com/nodejs/node/pull/2000)
* [[`0cf94e6856`](https://github.com/nodejs/node/commit/0cf94e6856)] - **doc**: mention CI in Collaborator Guide (Rich Trott) [#1995](https://github.com/nodejs/node/pull/1995)
* [[`7a3006efe4`](https://github.com/nodejs/node/commit/7a3006efe4)] - **doc**: add TOC links to Collaborator Guide (Rich Trott) [#1994](https://github.com/nodejs/node/pull/1994)
* [[`30638b150f`](https://github.com/nodejs/node/commit/30638b150f)] - **doc**: add TSC meeting notes 2015-06-10 (Bert Belder) [#1943](https://github.com/nodejs/node/pull/1943)
* [[`c4ec04136b`](https://github.com/nodejs/node/commit/c4ec04136b)] - **doc**: reformat authors section (Johan Bergström) [#1966](https://github.com/nodejs/node/pull/1966)
* [[`96165f9be2`](https://github.com/nodejs/node/commit/96165f9be2)] - **doc**: minor clarification in the modules API doc. (Сковорода Никита Андреевич) [#1983](https://github.com/nodejs/node/pull/1983)
* [[`5c2707c1b2`](https://github.com/nodejs/node/commit/5c2707c1b2)] - **doc**: benchmark/README.md copyedit (Rich Trott) [#1970](https://github.com/nodejs/node/pull/1970)
* [[`74fdf732d0`](https://github.com/nodejs/node/commit/74fdf732d0)] - **doc**: copyedit COLLABORATOR_GUIDE.md (Rich Trott) [#1964](https://github.com/nodejs/node/pull/1964)
* [[`5fe6e83640`](https://github.com/nodejs/node/commit/5fe6e83640)] - **doc**: copyedit GOVERNANCE.md (Rich Trott) [#1963](https://github.com/nodejs/node/pull/1963)
* [[`428526544c`](https://github.com/nodejs/node/commit/428526544c)] - **doc**: add ChALkeR as collaborator (Сковорода Никита Андреевич) [#1927](https://github.com/nodejs/node/pull/1927)
* [[`5dfe0d5d61`](https://github.com/nodejs/node/commit/5dfe0d5d61)] - **doc**: remove irrelevant SEMVER-MINOR & MAJOR (Rod Vagg)
* [[`fb8811d95e`](https://github.com/nodejs/node/commit/fb8811d95e)] - **lib,test**: fix whitespace issues (Roman Reiss) [#1971](https://github.com/nodejs/node/pull/1971)
* [[`a4f4909f3d`](https://github.com/nodejs/node/commit/a4f4909f3d)] - **module**: fix stat with long paths on Windows (Michaël Zasso) [#2013](https://github.com/nodejs/node/pull/2013)
* [[`a71ee93afe`](https://github.com/nodejs/node/commit/a71ee93afe)] - **module**: reduce syscalls during require search (Pierre Inglebert) [#1920](https://github.com/nodejs/node/pull/1920)
* [[`671e64ac73`](https://github.com/nodejs/node/commit/671e64ac73)] - **module**: allow long paths for require on Windows (Michaël Zasso)
* [[`061342a500`](https://github.com/nodejs/node/commit/061342a500)] - **net**: Defer reading until listeners could be added (James Hartig) [#1496](https://github.com/nodejs/node/pull/1496)
* [[`5d2b846d11`](https://github.com/nodejs/node/commit/5d2b846d11)] - **test**: assert tmp and fixture dirs different (Rich Trott) [#2015](https://github.com/nodejs/node/pull/2015)
* [[`b0990ef45d`](https://github.com/nodejs/node/commit/b0990ef45d)] - **test**: confirm symlink (Rich Trott) [#2014](https://github.com/nodejs/node/pull/2014)
* [[`3ba4f71fc4`](https://github.com/nodejs/node/commit/3ba4f71fc4)] - **test**: check result as early as possible (Rich Trott) [#2007](https://github.com/nodejs/node/pull/2007)
* [[`0abcf44d6b`](https://github.com/nodejs/node/commit/0abcf44d6b)] - **test**: add Buffer slice UTF-8 test (Rich Trott) [#1989](https://github.com/nodejs/node/pull/1989)
* [[`88c1831ff4`](https://github.com/nodejs/node/commit/88c1831ff4)] - **test**: tmpdir creation failures should fail tests (Rich Trott) [#1976](https://github.com/nodejs/node/pull/1976)
* [[`52a822d944`](https://github.com/nodejs/node/commit/52a822d944)] - **test**: fix test-cluster-worker-disconnect (Santiago Gimeno) [#1919](https://github.com/nodejs/node/pull/1919)
* [[`7c79490bfb`](https://github.com/nodejs/node/commit/7c79490bfb)] - **test**: only refresh tmpDir for tests that need it (Rich Trott) [#1954](https://github.com/nodejs/node/pull/1954)
* [[`88d7904c0b`](https://github.com/nodejs/node/commit/88d7904c0b)] - **test**: remove test repetition (Rich Trott) [#1874](https://github.com/nodejs/node/pull/1874)
* [[`91dfb5e094`](https://github.com/nodejs/node/commit/91dfb5e094)] - **tools**: make test-npm work without global npm (Jeremiah Senkpiel) [#1926](https://github.com/nodejs/node/pull/1926)
* [[`3777f41562`](https://github.com/nodejs/node/commit/3777f41562)] - **tools**: enable whitespace related rules in eslint (Roman Reiss) [#1971](https://github.com/nodejs/node/pull/1971)
* [[`626432d843`](https://github.com/nodejs/node/commit/626432d843)] - **util**: dont repeat isBuffer (Brendan Ashworth) [#1988](https://github.com/nodejs/node/pull/1988)
* [[`1d79f572f1`](https://github.com/nodejs/node/commit/1d79f572f1)] - **util**: move deprecate() to internal module (Brendan Ashworth) [#1988](https://github.com/nodejs/node/pull/1988)
* [[`4b4b1760b5`](https://github.com/nodejs/node/commit/4b4b1760b5)] - **v8**: cherry-pick uclibc build patch from upstream (Ben Noordhuis) [#1974](https://github.com/nodejs/node/pull/1974)
* [[`5d0cee46bb`](https://github.com/nodejs/node/commit/5d0cee46bb)] - **vm**: remove unnecessary HandleScopes (Ben Noordhuis) [#2001](https://github.com/nodejs/node/pull/2001)
* [[`0ecf9457b5`](https://github.com/nodejs/node/commit/0ecf9457b5)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)
* [[`953b3e75e8`](https://github.com/nodejs/node/commit/953b3e75e8)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)
* [[`3806d875d3`](https://github.com/nodejs/node/commit/3806d875d3)] - **zlib**: prevent uncaught exception in zlibBuffer (Michaël Zasso) [#1811](https://github.com/nodejs/node/pull/1811)

## 2015-06-22, Version 0.12.5 (Stable)

### Commits

* [[`456c22f63f`](https://github.com/nodejs/node/commit/456c22f63f)] - **openssl**: upgrade to 1.0.1o (Addressing multiple CVEs) [#25523](https://github.com/joyent/node/pull/25523)
* [[`20d8db1a42`](https://github.com/nodejs/node/commit/20d8db1a42)] - **npm**: upgrade to 2.11.2 [#25517](https://github.com/joyent/node/pull/25517)
* [[`50f961596d`](https://github.com/nodejs/node/commit/50f961596d)] - **uv**: upgrade to 1.6.1 [#25475](https://github.com/joyent/node/pull/25475)
* [[`b81a643f9a`](https://github.com/nodejs/node/commit/b81a643f9a)] - **V8**: avoid deadlock when profiling is active (Dmitri Melikyan) [#25309](https://github.com/joyent/node/pull/25309)
* [[`9d19dfbfdb`](https://github.com/nodejs/node/commit/9d19dfbfdb)] - **install**: fix source path for openssl headers (Oguz Bastemur) [#14089](https://github.com/joyent/node/pull/14089)
* [[`4028669531`](https://github.com/nodejs/node/commit/4028669531)] - **install**: make sure opensslconf.h is overwritten (Oguz Bastemur) [#14089](https://github.com/joyent/node/pull/14089)
* [[`d38e865fce`](https://github.com/nodejs/node/commit/d38e865fce)] - **timers**: fix timeout when added in timer's callback (Julien Gilli) [#17203](https://github.com/joyent/node/pull/17203)
* [[`e7c84f82c7`](https://github.com/nodejs/node/commit/e7c84f82c7)] - **windows**: broadcast WM_SETTINGCHANGE after install (Mathias Küsel) [#25100](https://github.com/joyent/node/pull/25100)

## 2015-06-18, Version 0.10.39 (Maintenance)

### Commits

* [[`456c22f63f`](https://github.com/nodejs/node/commit/456c22f63f)] - **openssl**: upgrade to 1.0.1o (Addressing multiple CVEs) [#25523](https://github.com/joyent/node/pull/25523)
* [[`9d19dfbfdb`](https://github.com/nodejs/node/commit/9d19dfbfdb)] - **install**: fix source path for openssl headers (Oguz Bastemur) [#14089](https://github.com/joyent/node/pull/14089)
* [[`4028669531`](https://github.com/nodejs/node/commit/4028669531)] - **install**: make sure opensslconf.h is overwritten (Oguz Bastemur) [#14089](https://github.com/joyent/node/pull/14089)
* [[`d38e865fce`](https://github.com/nodejs/node/commit/d38e865fce)] - **timers**: fix timeout when added in timer's callback (Julien Gilli) [#17203](https://github.com/joyent/node/pull/17203)
* [[`e7c84f82c7`](https://github.com/nodejs/node/commit/e7c84f82c7)] - **windows**: broadcast WM_SETTINGCHANGE after install (Mathias Küsel) [#25100](https://github.com/joyent/node/pull/25100)

## 2015-06-13, Version 2.3.0, @rvagg

### Notable changes

* **libuv**: Upgraded to 1.6.0 and 1.6.1, see [full ChangeLog](https://github.com/libuv/libuv/blob/60e515d9e6f3d86c0eedad583805201f32ea3aed/ChangeLog#L1-L36) for details. (Saúl Ibarra Corretgé) [#1905](https://github.com/nodejs/node/pull/1905) [#1889](https://github.com/nodejs/node/pull/1889). Highlights include:
  - Fix TTY becoming blocked on OS X
  - Fix UDP send callbacks to not to be synchronous
  - Add `uv_os_homedir()` (exposed as `os.homedir()`, see below)
* **npm**: See full [release notes](https://github.com/npm/npm/releases/tag/v2.11.1) for details. (Kat Marchán) [#1899](https://github.com/nodejs/node/pull/1899). Highlight:
  - Use GIT_SSH_COMMAND (available as of Git 2.3)
* **openssl**:
  - Upgrade to 1.0.2b and 1.0.2c, introduces DHE man-in-the-middle protection (Logjam) and fixes malformed ECParameters causing infinite loop (CVE-2015-1788). See the [security advisory](https://www.openssl.org/news/secadv_20150611.txt) for full details. (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950) [#1958](https://github.com/nodejs/node/pull/1958)
  - Support [FIPS](https://en.wikipedia.org/wiki/Federal_Information_Processing_Standards) mode of OpenSSL, see [README](https://github.com/nodejs/node#building-iojs-with-fips-compliant-openssl) for instructions. (Fedor Indutny) [#1890](https://github.com/nodejs/node/pull/1890)
* **os**: Add `os.homedir()` method. (Colin Ihrig) [#1791](https://github.com/nodejs/node/pull/1791)
* **smalloc**: Deprecate whole module. (Vladimir Kurchatkin) [#1822](https://github.com/nodejs/node/pull/1822)
* Add new collaborators:
  - Alex Kocharin ([@rlidwka](https://github.com/rlidwka))
  - Christopher Monsanto ([@monsanto](https://github.com/monsanto))
  - Ali Ijaz Sheikh ([@ofrobots](https://github.com/ofrobots))
  - Oleg Elifantiev ([@Olegas](https://github.com/Olegas))
  - Domenic Denicola ([@domenic](https://github.com/domenic))
  - Rich Trott ([@Trott](https://github.com/Trott))

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

## Commits

* [[`9c0a1b8cfc`](https://github.com/nodejs/node/commit/9c0a1b8cfc)] - **cluster**: wait on servers closing before disconnect (Oleg Elifantiev) [#1400](https://github.com/nodejs/node/pull/1400)
* [[`0f68377f69`](https://github.com/nodejs/node/commit/0f68377f69)] - **crypto**: support FIPS mode of OpenSSL (Fedor Indutny) [#1890](https://github.com/nodejs/node/pull/1890)
* [[`38d1afc24d`](https://github.com/nodejs/node/commit/38d1afc24d)] - **(SEMVER-MINOR)** **crypto**: add getCurves() to get supported ECs (Brian White) [#1914](https://github.com/nodejs/node/pull/1914)
* [[`a4dbf45b59`](https://github.com/nodejs/node/commit/a4dbf45b59)] - **crypto**: update root certificates (Ben Noordhuis) [#1833](https://github.com/nodejs/node/pull/1833)
* [[`81029c639a`](https://github.com/nodejs/node/commit/81029c639a)] - **debugger**: improve ESRCH error message (Jackson Tian) [#1863](https://github.com/nodejs/node/pull/1863)
* [[`2a7fd0ad32`](https://github.com/nodejs/node/commit/2a7fd0ad32)] - **deps**: update UPGRADING.md doc to openssl-1.0.2c (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`6b3df929e0`](https://github.com/nodejs/node/commit/6b3df929e0)] - **deps**: replace all headers in openssl (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`664a659696`](https://github.com/nodejs/node/commit/664a659696)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`42a8de2ac6`](https://github.com/nodejs/node/commit/42a8de2ac6)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`c66c3d9fa3`](https://github.com/nodejs/node/commit/c66c3d9fa3)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`86737cf0a0`](https://github.com/nodejs/node/commit/86737cf0a0)] - **deps**: upgrade openssl sources to 1.0.2c (Shigeki Ohtsu) [#1958](https://github.com/nodejs/node/pull/1958)
* [[`94804969b7`](https://github.com/nodejs/node/commit/94804969b7)] - **deps**: update asm files for openssl-1.0.2b (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`38444915e0`](https://github.com/nodejs/node/commit/38444915e0)] - **deps**: replace all headers in openssl (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`f62b613252`](https://github.com/nodejs/node/commit/f62b613252)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`f624d0122c`](https://github.com/nodejs/node/commit/f624d0122c)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`dcd67cc8d7`](https://github.com/nodejs/node/commit/dcd67cc8d7)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`c21b24decf`](https://github.com/nodejs/node/commit/c21b24decf)] - **deps**: upgrade openssl sources to 1.0.2b (Shigeki Ohtsu) [#1950](https://github.com/nodejs/node/pull/1950)
* [[`2dc819b09a`](https://github.com/nodejs/node/commit/2dc819b09a)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`f41b7f12b5`](https://github.com/nodejs/node/commit/f41b7f12b5)] - **deps**: upgrade to npm 2.11.1 (Kat Marchán) [#1899](https://github.com/nodejs/node/pull/1899)
* [[`a5bd466440`](https://github.com/nodejs/node/commit/a5bd466440)] - **deps**: update libuv to version 1.6.1 (Saúl Ibarra Corretgé) [#1905](https://github.com/nodejs/node/pull/1905)
* [[`aa33db3238`](https://github.com/nodejs/node/commit/aa33db3238)] - **deps**: update libuv to version 1.6.0 (Saúl Ibarra Corretgé) [#1889](https://github.com/nodejs/node/pull/1889)
* [[`0ee497f0b4`](https://github.com/nodejs/node/commit/0ee497f0b4)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`b5cd2f0986`](https://github.com/nodejs/node/commit/b5cd2f0986)] - **dgram**: partially revert 18d457b (Saúl Ibarra Corretgé) [#1889](https://github.com/nodejs/node/pull/1889)
* [[`a3cc43d0a4`](https://github.com/nodejs/node/commit/a3cc43d0a4)] - **doc**: add Trott as collaborator (Rich Trott) [#1962](https://github.com/nodejs/node/pull/1962)
* [[`cf5020fc02`](https://github.com/nodejs/node/commit/cf5020fc02)] - **doc**: add domenic as collaborator (Domenic Denicola) [#1942](https://github.com/nodejs/node/pull/1942)
* [[`11ed5f31ab`](https://github.com/nodejs/node/commit/11ed5f31ab)] - **doc**: add Olegas as collaborator (Oleg Elifantiev) [#1930](https://github.com/nodejs/node/pull/1930)
* [[`f500e1833b`](https://github.com/nodejs/node/commit/f500e1833b)] - **doc**: add ofrobots as collaborator (Ali Ijaz Sheikh)
* [[`717724611a`](https://github.com/nodejs/node/commit/717724611a)] - **doc**: add monsanto as collaborator (Christopher Monsanto) [#1932](https://github.com/nodejs/node/pull/1932)
* [[`7192b6688c`](https://github.com/nodejs/node/commit/7192b6688c)] - **doc**: add rlidwka as collaborator (Alex Kocharin) [#1929](https://github.com/nodejs/node/pull/1929)
* [[`9f3a03f0d4`](https://github.com/nodejs/node/commit/9f3a03f0d4)] - **doc**: add references to crypto.getCurves() (Roman Reiss) [#1918](https://github.com/nodejs/node/pull/1918)
* [[`ff39ecb914`](https://github.com/nodejs/node/commit/ff39ecb914)] - **doc**: remove comma splice (Rich Trott) [#1900](https://github.com/nodejs/node/pull/1900)
* [[`deb8b87dc9`](https://github.com/nodejs/node/commit/deb8b87dc9)] - **doc**: add note about available ECC curves (Ryan Petschek) [#1913](https://github.com/nodejs/node/pull/1913)
* [[`89a5b9040e`](https://github.com/nodejs/node/commit/89a5b9040e)] - **doc**: fix http.IncomingMessage.socket documentation (Сковорода Никита Андреевич) [#1867](https://github.com/nodejs/node/pull/1867)
* [[`d29034b34b`](https://github.com/nodejs/node/commit/d29034b34b)] - **doc**: adjust changelog to clarify `client` revert (Rod Vagg) [#1859](https://github.com/nodejs/node/pull/1859)
* [[`a79dece8ad`](https://github.com/nodejs/node/commit/a79dece8ad)] - **docs**: add return value for sync fs functions (Tyler Anton) [#1770](https://github.com/nodejs/node/pull/1770)
* [[`1cb72c14c4`](https://github.com/nodejs/node/commit/1cb72c14c4)] - **docs**: delete unused/duplicate css files (Robert Kowalski) [#1770](https://github.com/nodejs/node/pull/1770)
* [[`53a4eb3198`](https://github.com/nodejs/node/commit/53a4eb3198)] - **fs**: make SyncWriteStream non-enumerable (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`a011c3243f`](https://github.com/nodejs/node/commit/a011c3243f)] - **fs**: minor refactoring (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`8841132f30`](https://github.com/nodejs/node/commit/8841132f30)] - **fs**: remove inStatWatchers and use Map for lookup (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`67a11b9bcc`](https://github.com/nodejs/node/commit/67a11b9bcc)] - **fs**: removing unnecessary nullCheckCallNT (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`09f2a67bd8`](https://github.com/nodejs/node/commit/09f2a67bd8)] - **fs**: improve error message descriptions (Sakthipriyan Vairamani) [#1870](https://github.com/nodejs/node/pull/1870)
* [[`2dcef83b5f`](https://github.com/nodejs/node/commit/2dcef83b5f)] - **fs**: use `kMaxLength` from binding (Vladimir Kurchatkin) [#1903](https://github.com/nodejs/node/pull/1903)
* [[`353e26e3c7`](https://github.com/nodejs/node/commit/353e26e3c7)] - **(SEMVER-MINOR)** **fs**: Add string encoding option for Stream method (Yosuke Furukawa) [#1845](https://github.com/nodejs/node/pull/1845)
* [[`8357c5084b`](https://github.com/nodejs/node/commit/8357c5084b)] - **fs**: set encoding on fs.createWriteStream (Yosuke Furukawa) [#1844](https://github.com/nodejs/node/pull/1844)
* [[`02c345020a`](https://github.com/nodejs/node/commit/02c345020a)] - **gitignore**: don't ignore the debug npm module (Kat Marchán) [#1908](https://github.com/nodejs/node/pull/1908)
* [[`b5b8ff117c`](https://github.com/nodejs/node/commit/b5b8ff117c)] - **lib**: don't use global Buffer (Roman Reiss) [#1794](https://github.com/nodejs/node/pull/1794)
* [[`a251657058`](https://github.com/nodejs/node/commit/a251657058)] - **node**: mark promises as handled as soon as possible (Vladimir Kurchatkin) [#1952](https://github.com/nodejs/node/pull/1952)
* [[`2eb170874a`](https://github.com/nodejs/node/commit/2eb170874a)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`a130132c8f`](https://github.com/nodejs/node/commit/a130132c8f)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`6e78e5feaa`](https://github.com/nodejs/node/commit/6e78e5feaa)] - **(SEMVER-MINOR)** **os**: add homedir() (cjihrig) [#1791](https://github.com/nodejs/node/pull/1791)
* [[`d9e250295b`](https://github.com/nodejs/node/commit/d9e250295b)] - ***Revert*** "**readline**: allow tabs in input" (Jeremiah Senkpiel) [#1961](https://github.com/nodejs/node/pull/1961)
* [[`4b3d493c4b`](https://github.com/nodejs/node/commit/4b3d493c4b)] - **readline**: allow tabs in input (Rich Trott) [#1761](https://github.com/nodejs/node/pull/1761)
* [[`6d95f4ff92`](https://github.com/nodejs/node/commit/6d95f4ff92)] - **(SEMVER-MINOR)** **smalloc**: deprecate whole module (Vladimir Kurchatkin) [#1822](https://github.com/nodejs/node/pull/1822)
* [[`8c71a9241d`](https://github.com/nodejs/node/commit/8c71a9241d)] - **src**: hide InitializeICUDirectory symbol (Ben Noordhuis) [#1815](https://github.com/nodejs/node/pull/1815)
* [[`5b6f575c1f`](https://github.com/nodejs/node/commit/5b6f575c1f)] - ***Revert*** "**src**: add getopt option parser" (Evan Lucas) [#1862](https://github.com/nodejs/node/pull/1862)
* [[`c0e7bf2d8c`](https://github.com/nodejs/node/commit/c0e7bf2d8c)] - **src**: add getopt option parser (Evan Lucas) [#1804](https://github.com/nodejs/node/pull/1804)
* [[`8ea6844d26`](https://github.com/nodejs/node/commit/8ea6844d26)] - **test**: add test for failed save in REPL (Rich Trott) [#1818](https://github.com/nodejs/node/pull/1818)
* [[`03ce84dfa1`](https://github.com/nodejs/node/commit/03ce84dfa1)] - **test**: fix cluster-worker-wait-server-close races (Sam Roberts) [#1953](https://github.com/nodejs/node/pull/1953)
* [[`a6b8ee19b8`](https://github.com/nodejs/node/commit/a6b8ee19b8)] - **test**: create temp dir in common.js (Rich Trott) [#1877](https://github.com/nodejs/node/pull/1877)
* [[`ff8202c6f4`](https://github.com/nodejs/node/commit/ff8202c6f4)] - **test**: fix undeclared variable access (Roman Reiss) [#1794](https://github.com/nodejs/node/pull/1794)
* [[`d9ddd7d345`](https://github.com/nodejs/node/commit/d9ddd7d345)] - **test**: remove TODO comment (Rich Trott) [#1820](https://github.com/nodejs/node/pull/1820)
* [[`6537fd4b55`](https://github.com/nodejs/node/commit/6537fd4b55)] - **test**: remove TODO (Rich Trott) [#1875](https://github.com/nodejs/node/pull/1875)
* [[`a804026c9b`](https://github.com/nodejs/node/commit/a804026c9b)] - **test**: fix broken FreeBSD test (Santiago Gimeno) [#1881](https://github.com/nodejs/node/pull/1881)
* [[`43a82f8a71`](https://github.com/nodejs/node/commit/43a82f8a71)] - **test**: fix test-sync-io-option (Evan Lucas) [#1840](https://github.com/nodejs/node/pull/1840)
* [[`4ed25f664d`](https://github.com/nodejs/node/commit/4ed25f664d)] - **test**: add -no_rand_screen for tls-server-verify (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`4cf323d23d`](https://github.com/nodejs/node/commit/4cf323d23d)] - **test**: kill child in tls-server-verify for speed up (Shigeki Ohtsu) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`e6ccdcc1fe`](https://github.com/nodejs/node/commit/e6ccdcc1fe)] - **test**: improve console output of tls-server-verify (João Reis) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`975e5956f0`](https://github.com/nodejs/node/commit/975e5956f0)] - **test**: run tls-server-verify servers in parallel (João Reis) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`b18604ba2c`](https://github.com/nodejs/node/commit/b18604ba2c)] - **test**: running tls-server-verify clients in parallel (João Reis) [#1836](https://github.com/nodejs/node/pull/1836)
* [[`f78c722df5`](https://github.com/nodejs/node/commit/f78c722df5)] - **test**: remove hardwired references to 'iojs' (Rod Vagg) [#1882](https://github.com/nodejs/node/pull/1882)
* [[`bd99e8de8e`](https://github.com/nodejs/node/commit/bd99e8de8e)] - **test**: more test coverage for maxConnections (Rich Trott) [#1855](https://github.com/nodejs/node/pull/1855)
* [[`b9267189a5`](https://github.com/nodejs/node/commit/b9267189a5)] - **test**: fix test-child-process-stdout-flush-exit (Santiago Gimeno) [#1868](https://github.com/nodejs/node/pull/1868)
* [[`d20f018dcf`](https://github.com/nodejs/node/commit/d20f018dcf)] - **test**: loosen condition to detect infinite loop (Yosuke Furukawa) [#1857](https://github.com/nodejs/node/pull/1857)
* [[`e0e96acc6f`](https://github.com/nodejs/node/commit/e0e96acc6f)] - **test**: remove smalloc add-on test (Ben Noordhuis) [#1835](https://github.com/nodejs/node/pull/1835)
* [[`8704c58fc4`](https://github.com/nodejs/node/commit/8704c58fc4)] - **test**: remove unneeded comment task (Rich Trott) [#1858](https://github.com/nodejs/node/pull/1858)
* [[`8732977536`](https://github.com/nodejs/node/commit/8732977536)] - **tls**: fix references to undefined `cb` (Fedor Indutny) [#1951](https://github.com/nodejs/node/pull/1951)
* [[`75930bb38c`](https://github.com/nodejs/node/commit/75930bb38c)] - **tls**: prevent use-after-free (Fedor Indutny) [#1702](https://github.com/nodejs/node/pull/1702)
* [[`5795e835a1`](https://github.com/nodejs/node/commit/5795e835a1)] - **tls**: emit errors on close whilst async action (Fedor Indutny) [#1702](https://github.com/nodejs/node/pull/1702)
* [[`59d9734e21`](https://github.com/nodejs/node/commit/59d9734e21)] - **tls_wrap**: invoke queued callbacks in DestroySSL (Fedor Indutny) [#1702](https://github.com/nodejs/node/pull/1702)
* [[`6e4d30286d`](https://github.com/nodejs/node/commit/6e4d30286d)] - **tools**: enable/add additional eslint rules (Roman Reiss) [#1794](https://github.com/nodejs/node/pull/1794)
* [[`098354a9f8`](https://github.com/nodejs/node/commit/098354a9f8)] - **tools**: update certdata.txt (Ben Noordhuis) [#1833](https://github.com/nodejs/node/pull/1833)
* [[`a2d921d6a0`](https://github.com/nodejs/node/commit/a2d921d6a0)] - **tools**: customize mk-ca-bundle.pl (Ben Noordhuis) [#1833](https://github.com/nodejs/node/pull/1833)
* [[`5be9efca40`](https://github.com/nodejs/node/commit/5be9efca40)] - **tools**: update mk-ca-bundle.pl to HEAD of upstream (Ben Noordhuis) [#1833](https://github.com/nodejs/node/pull/1833)
* [[`1baba0580d`](https://github.com/nodejs/node/commit/1baba0580d)] - **tools**: Fix copying contents of deps/npm (thefourtheye) [#1853](https://github.com/nodejs/node/pull/1853)
* [[`628845b816`](https://github.com/nodejs/node/commit/628845b816)] - **(SEMVER-MINOR)** **util**: introduce `printDeprecationMessage` function (Vladimir Kurchatkin) [#1822](https://github.com/nodejs/node/pull/1822)
* [[`91d0a8b19c`](https://github.com/nodejs/node/commit/91d0a8b19c)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [iojs/io.js#1433](https://github.com/iojs/io.js/pull/1433)

## 2015-06-01, Version 2.2.1, @rvagg

### Notable changes

* **http**: Reverts the move of the `client` property of `IncomingMessage` to its prototype. Although undocumented, this property was used and assumed to be an "own property" in the wild, most notably by [request](https://github.com/request/request) which is used by npm. (Michaël Zasso) [#1852](https://github.com/nodejs/node/pull/1852).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`c5a1009903`](https://github.com/nodejs/node/commit/c5a1009903)] - **build**: avoid passing empty strings to build flags (Johan Bergström) [#1789](https://github.com/nodejs/node/pull/1789)
* [[`5d83401086`](https://github.com/nodejs/node/commit/5d83401086)] - **doc**: put SEMVER-MINOR on pre-load module fix 2.2.0 (Rod Vagg)
* [[`4d6b768e5d`](https://github.com/nodejs/node/commit/4d6b768e5d)] - **http**: revert deprecation of client property (Michaël Zasso) [#1852](https://github.com/nodejs/node/pull/1852)

## 2015-05-31, Version 2.2.0, @rvagg

### Notable changes

* **node**: Speed-up `require()` by replacing usage of `fs.statSync()` and `fs.readFileSync()` with internal variants that are faster for this use-case and do not create as many objects for the garbage collector to clean up. The primary two benefits are: significant increase in application start-up time on typical applications and better start-up time for the debugger by eliminating almost all of the thousands of exception events. (Ben Noordhuis) [#1801](https://github.com/nodejs/node/pull/1801).
* **node**: Resolution of pre-load modules (`-r` or `--require`) now follows the standard `require()` rules rather than just resolving paths, so you can now pre-load modules in node_modules. (Ali Ijaz Sheikh) [#1812](https://github.com/nodejs/node/pull/1812).
* **npm**: Upgraded npm to v2.11.0. New hooks for `preversion`, `version`, and `postversion` lifecycle events, some SPDX-related license changes and license file inclusions. See the [release notes](https://github.com/npm/npm/releases/tag/v2.11.0) for full details.

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`a77c330c32`](https://github.com/nodejs/node/commit/a77c330c32)] - **(SEMVER-MINOR)** **child_process**: expose ChildProcess constructor (Evan Lucas) [#1760](https://github.com/nodejs/node/pull/1760)
* [[`3a1bc067d4`](https://github.com/nodejs/node/commit/3a1bc067d4)] - ***Revert*** "**core**: set PROVIDER type as Persistent class id" (Ben Noordhuis) [#1827](https://github.com/nodejs/node/pull/1827)
* [[`f9fd554500`](https://github.com/nodejs/node/commit/f9fd554500)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`c1afa53648`](https://github.com/nodejs/node/commit/c1afa53648)] - **deps**: upgrade npm to 2.11.0 (Forrest L Norvell) [iojs/io.js#1829](https://github.com/iojs/io.js/pull/1829)
* [[`ff794498e7`](https://github.com/nodejs/node/commit/ff794498e7)] - **doc**: `fs.*File()` also accept encoding strings (Rich Trott) [#1806](https://github.com/nodejs/node/pull/1806)
* [[`98649fd31a`](https://github.com/nodejs/node/commit/98649fd31a)] - **doc**: add documentation for AtExit hook (Steve Sharp) [#1014](https://github.com/nodejs/node/pull/1014)
* [[`eb1856dfd1`](https://github.com/nodejs/node/commit/eb1856dfd1)] - **doc**: clarify stability of fs.watch and relatives (Rich Trott) [#1775](https://github.com/nodejs/node/pull/1775)
* [[`a74c2c9458`](https://github.com/nodejs/node/commit/a74c2c9458)] - **doc**: state url decoding behavior (Josh Gummersall) [#1731](https://github.com/nodejs/node/pull/1731)
* [[`ba76a9d872`](https://github.com/nodejs/node/commit/ba76a9d872)] - **doc**: remove bad semver-major entry from CHANGELOG (Rod Vagg) [#1782](https://github.com/nodejs/node/pull/1782)
* [[`a6a3f8c78d`](https://github.com/nodejs/node/commit/a6a3f8c78d)] - **doc**: fix changelog s/2.0.3/2.1.0 (Rod Vagg)
* [[`2c686fd3ce`](https://github.com/nodejs/node/commit/2c686fd3ce)] - **http**: flush stored header (Vladimir Kurchatkin) [#1695](https://github.com/nodejs/node/pull/1695)
* [[`1eec5f091a`](https://github.com/nodejs/node/commit/1eec5f091a)] - **http**: simplify code and remove unused properties (Brian White) [#1572](https://github.com/nodejs/node/pull/1572)
* [[`1bbf8d0720`](https://github.com/nodejs/node/commit/1bbf8d0720)] - **lib**: speed up require(), phase 2 (Ben Noordhuis) [#1801](https://github.com/nodejs/node/pull/1801)
* [[`b14fd1a720`](https://github.com/nodejs/node/commit/b14fd1a720)] - **lib**: speed up require(), phase 1 (Ben Noordhuis) [#1801](https://github.com/nodejs/node/pull/1801)
* [[`5abd4ac079`](https://github.com/nodejs/node/commit/5abd4ac079)] - **lib**: simplify nextTick() usage (Brian White) [#1612](https://github.com/nodejs/node/pull/1612)
* [[`5759722cfa`](https://github.com/nodejs/node/commit/5759722cfa)] - **(SEMVER-MINOR)** **src**: fix module search path for preload modules (Ali Ijaz Sheikh) [#1812](https://github.com/nodejs/node/pull/1812)
* [[`a65762cab6`](https://github.com/nodejs/node/commit/a65762cab6)] - **src**: remove old code (Brendan Ashworth) [#1819](https://github.com/nodejs/node/pull/1819)
* [[`93a44d5228`](https://github.com/nodejs/node/commit/93a44d5228)] - **src**: fix deferred events not working with -e (Ben Noordhuis) [#1793](https://github.com/nodejs/node/pull/1793)
* [[`8059393934`](https://github.com/nodejs/node/commit/8059393934)] - **test**: check error type from net.Server.listen() (Rich Trott) [#1821](https://github.com/nodejs/node/pull/1821)
* [[`4e90c82cdb`](https://github.com/nodejs/node/commit/4e90c82cdb)] - **test**: add heap profiler add-on regression test (Ben Noordhuis) [#1828](https://github.com/nodejs/node/pull/1828)
* [[`6dfca71af0`](https://github.com/nodejs/node/commit/6dfca71af0)] - **test**: don't lint autogenerated test/addons/doc-*/ (Ben Noordhuis) [#1793](https://github.com/nodejs/node/pull/1793)
* [[`c2b8b30836`](https://github.com/nodejs/node/commit/c2b8b30836)] - **test**: remove stray copyright notices (Ben Noordhuis) [#1793](https://github.com/nodejs/node/pull/1793)
* [[`280fb01daf`](https://github.com/nodejs/node/commit/280fb01daf)] - **test**: fix deprecation warning in addons test (Ben Noordhuis) [#1793](https://github.com/nodejs/node/pull/1793)
* [[`8606793999`](https://github.com/nodejs/node/commit/8606793999)] - **tools**: pass constant to logger instead of string (Johan Bergström) [#1842](https://github.com/nodejs/node/pull/1842)
* [[`fbd2b59716`](https://github.com/nodejs/node/commit/fbd2b59716)] - **tools**: add objectLiteralShorthandProperties to .eslintrc (Evan Lucas) [#1760](https://github.com/nodejs/node/pull/1760)
* [[`53e98cc1b4`](https://github.com/nodejs/node/commit/53e98cc1b4)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [#1763](https://github.com/nodejs/node/pull/1763)

## 2015-05-24, Version 2.1.0, @rvagg

### Notable changes

* **crypto**: Diffie-Hellman key exchange (DHE) parameters (`'dhparams'`) must now be 1024 bits or longer or an error will be thrown. A warning will also be printed to the console if you supply less than 2048 bits. See https://weakdh.org/ for further context on this security concern. (Shigeki Ohtsu) [#1739](https://github.com/nodejs/node/pull/1739).
* **node**: A new `--trace-sync-io` command line flag will print a warning and a stack trace whenever a synchronous API is used. This can be used to track down synchronous calls that may be slowing down an application. (Trevor Norris) [#1707](https://github.com/nodejs/node/pull/1707).
* **node**: To allow for chaining of methods, the `setTimeout()`, `setKeepAlive()`, `setNoDelay()`, `ref()` and `unref()` methods used in `'net'`, `'dgram'`, `'http'`, `'https'` and `'tls'` now return the current instance instead of `undefined` (Roman Reiss & Evan Lucas) [#1699](https://github.com/nodejs/node/pull/1699) [#1768](https://github.com/nodejs/node/pull/1768) [#1779](https://github.com/nodejs/node/pull/1779).
* **npm**: Upgraded to v2.10.1, release notes can be found in <https://github.com/npm/npm/releases/tag/v2.10.1> and <https://github.com/npm/npm/releases/tag/v2.10.0>.
* **util**: A significant speed-up (in the order of 35%) for the common-case of a single string argument to `util.format()`, used by `console.log()` (Сковорода Никита Андреевич) [#1749](https://github.com/nodejs/node/pull/1749).

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`9da168b71f`](https://github.com/nodejs/node/commit/9da168b71f)] - **buffer**: optimize Buffer.byteLength (Brendan Ashworth) [#1713](https://github.com/nodejs/node/pull/1713)
* [[`2b1c01c2cc`](https://github.com/nodejs/node/commit/2b1c01c2cc)] - **build**: refactor pkg-config for shared libraries (Johan Bergström) [#1603](https://github.com/nodejs/node/pull/1603)
* [[`3c44100558`](https://github.com/nodejs/node/commit/3c44100558)] - **core**: set PROVIDER type as Persistent class id (Trevor Norris) [#1730](https://github.com/nodejs/node/pull/1730)
* [[`c1de6d249e`](https://github.com/nodejs/node/commit/c1de6d249e)] - **(SEMVER-MINOR)** **core**: implement runtime flag to trace sync io (Trevor Norris) [#1707](https://github.com/nodejs/node/pull/1707)
* [[`9e7099fa4e`](https://github.com/nodejs/node/commit/9e7099fa4e)] - **deps**: make node-gyp work with io.js (cjihrig) [iojs/io.js#990](https://github.com/iojs/io.js/pull/990)
* [[`c54d057598`](https://github.com/nodejs/node/commit/c54d057598)] - **deps**: upgrade to npm 2.10.1 (Rebecca Turner) [#1763](https://github.com/nodejs/node/pull/1763)
* [[`367ffd167d`](https://github.com/nodejs/node/commit/367ffd167d)] - **doc**: update AUTHORS list (Rod Vagg) [#1776](https://github.com/nodejs/node/pull/1776)
* [[`2bb2f06b3e`](https://github.com/nodejs/node/commit/2bb2f06b3e)] - **doc**: fix typo in CONTRIBUTING.md (Rich Trott) [#1755](https://github.com/nodejs/node/pull/1755)
* [[`515afc6367`](https://github.com/nodejs/node/commit/515afc6367)] - **doc**: path is ignored in url.format (Maurice Butler) [#1753](https://github.com/nodejs/node/pull/1753)
* [[`f0a8bc3f84`](https://github.com/nodejs/node/commit/f0a8bc3f84)] - **doc**: fix spelling in CHANGELOG (Felipe Batista)
* [[`86dd244d9b`](https://github.com/nodejs/node/commit/86dd244d9b)] - **doc**: add notes to child_process.fork() and .exec() (Rich Trott) [#1718](https://github.com/nodejs/node/pull/1718)
* [[`066274794c`](https://github.com/nodejs/node/commit/066274794c)] - **doc**: update links from iojs/io.js to nodejs/io.js (Frederic Hemberger) [#1715](https://github.com/nodejs/node/pull/1715)
* [[`cb381fe3e0`](https://github.com/nodejs/node/commit/cb381fe3e0)] - **(SEMVER-MINOR)** **net**: return this from setNoDelay and setKeepAlive (Roman Reiss) [#1779](https://github.com/nodejs/node/pull/1779)
* [[`85d9983009`](https://github.com/nodejs/node/commit/85d9983009)] - **net**: persist net.Socket options before connect (Evan Lucas) [#1518](https://github.com/nodejs/node/pull/1518)
* [[`39dde3222e`](https://github.com/nodejs/node/commit/39dde3222e)] - **(SEMVER-MINOR)** **net,dgram**: return this from ref and unref methods (Roman Reiss) [#1768](https://github.com/nodejs/node/pull/1768)
* [[`5773438913`](https://github.com/nodejs/node/commit/5773438913)] - **test**: fix jslint error (Michaël Zasso) [#1743](https://github.com/nodejs/node/pull/1743)
* [[`867631986f`](https://github.com/nodejs/node/commit/867631986f)] - **test**: fix test-sync-io-option (Santiago Gimeno) [#1734](https://github.com/nodejs/node/pull/1734)
* [[`f29762f4dd`](https://github.com/nodejs/node/commit/f29762f4dd)] - **test**: enable linting for tests (Roman Reiss) [#1721](https://github.com/nodejs/node/pull/1721)
* [[`2a71f02988`](https://github.com/nodejs/node/commit/2a71f02988)] - **tls**: emit errors happening before handshake finish (Malte-Thorben Bruns) [#1769](https://github.com/nodejs/node/pull/1769)
* [[`80342f649d`](https://github.com/nodejs/node/commit/80342f649d)] - **tls**: use `.destroy(err)` instead of destroy+emit (Fedor Indutny) [#1711](https://github.com/nodejs/node/pull/1711)
* [[`9b35be5810`](https://github.com/nodejs/node/commit/9b35be5810)] - **tls**: make server not use DHE in less than 1024bits (Shigeki Ohtsu) [#1739](https://github.com/nodejs/node/pull/1739)
* [[`214d02040e`](https://github.com/nodejs/node/commit/214d02040e)] - **util**: speed up common case of formatting string (Сковорода Никита Андреевич) [#1749](https://github.com/nodejs/node/pull/1749)
* [[`d144e96fbf`](https://github.com/nodejs/node/commit/d144e96fbf)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [#1763](https://github.com/nodejs/node/pull/1763)
* [[`0d6d3dda95`](https://github.com/nodejs/node/commit/0d6d3dda95)] - **win,node-gyp**: make delay-load hook C89 compliant (Sharat M R) [TooTallNate/node-gyp#616](https://github.com/TooTallNate/node-gyp/pull/616)

## 2015-05-22, Version 0.12.4 (Stable)

### Commits

* [[`202c18bbc3`](https://github.com/nodejs/node/commit/202c18bbc3)] - **npm**: upgrade to 2.10.1 [#25364](https://github.com/joyent/node/pull/25364)
* [[`6157697bd5`](https://github.com/nodejs/node/commit/6157697bd5)] - **V8**: revert v8 Array.prototype.values() removal (cjihrig) [#25328](https://github.com/joyent/node/pull/25328)
* [[`3122052890`](https://github.com/nodejs/node/commit/3122052890)] - **win**: bring back xp/2k3 support (Bert Belder) [#25367](https://github.com/joyent/node/pull/25367)

## 2015-05-17, Version 1.8.2, @rvagg

**Maintenance release**

## Notable changes

* **crypto**: significantly reduced memory usage for TLS (Fedor Indutny & Сковорода Никита Андреевич) [#1529](https://github.com/nodejs/node/pull/1529)
* **npm**: Upgrade npm to 2.9.0. See the [v2.8.4](https://github.com/npm/npm/releases/tag/v2.8.4) and [v2.9.0](https://github.com/npm/npm/releases/tag/v2.9.0) release notes for details. Summary:
  - Add support for default author field to make `npm init -y` work without user-input (@othiym23) [npm/npm/d8eee6cf9d](https://github.com/npm/npm/commit/d8eee6cf9d2ff7aca68dfaed2de76824a3e0d9
  - Include local modules in `npm outdated` and `npm update` (@ArnaudRinquin) [npm/npm#7426](https://github.com/npm/npm/issues/7426)
  - The prefix used before the version number on `npm version` is now configurable via `tag-version-prefix` (@kkragenbrink) [npm/npm#8014](https://github.com/npm/npm/issues/8014)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`5404cbc745`](https://github.com/nodejs/node/commit/5404cbc745)] - **buffer**: fix copy() segfault with zero arguments (Trevor Norris) [nodejs/node#1520](https://github.com/nodejs/node/pull/1520)
* [[`65dd10e9c0`](https://github.com/nodejs/node/commit/65dd10e9c0)] - **build**: remove -J from test-ci (Rod Vagg) [nodejs/node#1544](https://github.com/nodejs/node/pull/1544)
* [[`74060bb60e`](https://github.com/nodejs/node/commit/74060bb60e)] - **crypto**: track external memory for SSL structures (Fedor Indutny) [nodejs/node#1529](https://github.com/nodejs/node/pull/1529)
* [[`f10f379240`](https://github.com/nodejs/node/commit/f10f379240)] - **deps**: make node-gyp work with io.js (cjihrig) [nodejs/node#990](https://github.com/nodejs/node/pull/990)
* [[`ba0e744c2c`](https://github.com/nodejs/node/commit/ba0e744c2c)] - **deps**: upgrade npm to 2.9.0 (Forrest L Norvell) [nodejs/node#1583](https://github.com/nodejs/node/pull/1583)
* [[`b3a7da1091`](https://github.com/nodejs/node/commit/b3a7da1091)] - **deps**: update http_parser to 2.5.0 (Fedor Indutny) [nodejs/node#1517](https://github.com/nodejs/node/pull/1517)
* [[`4030545af6`](https://github.com/nodejs/node/commit/4030545af6)] - **fs**: validate fd on fs.write (Julian Duque) [#1553](https://github.com/nodejs/node/pull/1553)
* [[`898d423820`](https://github.com/nodejs/node/commit/898d423820)] - **string_decoder**: don't cache Buffer.isEncoding (Brian White) [nodejs/node#1548](https://github.com/nodejs/node/pull/1548)
* [[`32a6dbcf23`](https://github.com/nodejs/node/commit/32a6dbcf23)] - **test**: extend timeouts for ARMv6 (Rod Vagg) [nodejs/node#1554](https://github.com/nodejs/node/pull/1554)
* [[`5896fe5cd3`](https://github.com/nodejs/node/commit/5896fe5cd3)] - **test**: adjust Makefile/test-ci, add to vcbuild.bat (Rod Vagg) [nodejs/node#1530](https://github.com/nodejs/node/pull/1530)
* [[`b72e4bc596`](https://github.com/nodejs/node/commit/b72e4bc596)] - **tls**: destroy singleUse context immediately (Fedor Indutny) [nodejs/node#1529](https://github.com/nodejs/node/pull/1529)
* [[`1cfc455dc5`](https://github.com/nodejs/node/commit/1cfc455dc5)] - **tls**: zero SSL_CTX freelist for a singleUse socket (Fedor Indutny) [nodejs/node#1529](https://github.com/nodejs/node/pull/1529)
* [[`7ada680519`](https://github.com/nodejs/node/commit/7ada680519)] - **tls**: destroy SSL once it is out of use (Fedor Indutny) [nodejs/node#1529](https://github.com/nodejs/node/pull/1529)
* [[`71274b0263`](https://github.com/nodejs/node/commit/71274b0263)] - **tls_wrap**: use localhost if options.host is empty (Guilherme Souza) [nodejs/node#1493](https://github.com/nodejs/node/pull/1493)
* [[`0eb74a8b6c`](https://github.com/nodejs/node/commit/0eb74a8b6c)] - **win,node-gyp**: optionally allow node.exe/iojs.exe to be renamed (Bert Belder) [nodejs/node#1266](https://github.com/nodejs/node/pull/1266)

## 2015-05-15, Version 2.0.2, @Fishrock123

### Notable changes

* **win,node-gyp**: the delay-load hook for windows addons has now been correctly enabled by default, it had wrongly defaulted to off in the release version of 2.0.0 (Bert Belder) [#1433](https://github.com/nodejs/node/pull/1433)
* **os**: `tmpdir()`'s trailing slash stripping has been refined to fix an issue when the temp directory is at '/'. Also considers which slash is used by the operating system. (cjihrig) [#1673](https://github.com/nodejs/node/pull/1673)
* **tls**: default ciphers have been updated to use gcm and aes128 (Mike MacCana) [#1660](https://github.com/nodejs/node/pull/1660)
* **build**: v8 snapshots have been re-enabled by default as suggested by the v8 team, since prior security issues have been resolved. This should give some perf improvements to both startup and vm context creation. (Trevor Norris) [#1663](https://github.com/nodejs/node/pull/1663)
* **src**: fixed preload modules not working when other flags were used before `--require` (Yosuke Furukawa) [#1694](https://github.com/nodejs/node/pull/1694)
* **dgram**: fixed `send()`'s callback not being asynchronous (Yosuke Furukawa) [#1313](https://github.com/nodejs/node/pull/1313)
* **readline**: emitKeys now keeps buffering data until it has enough to parse. This fixes an issue with parsing split escapes. (Alex Kocharin) [#1601](https://github.com/nodejs/node/pull/1601)
* **cluster**: works now properly emit 'disconnect' to `cluser.worker` (Oleg Elifantiev) [#1386](https://github.com/nodejs/node/pull/1386)
* **events**: uncaught errors now provide some context (Evan Lucas) [#1654](https://github.com/nodejs/node/pull/1654)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).

### Commits

* [[`8a0e5295b4`](https://github.com/nodejs/node/commit/8a0e5295b4)] - **build**: use backslashes for paths on windows (Johan Bergström) [#1698](https://github.com/nodejs/node/pull/1698)
* [[`20c9a52227`](https://github.com/nodejs/node/commit/20c9a52227)] - **build**: move --with-intl to intl optgroup (Johan Bergström) [#1680](https://github.com/nodejs/node/pull/1680)
* [[`36cdc7c8ac`](https://github.com/nodejs/node/commit/36cdc7c8ac)] - **build**: re-enable V8 snapshots (Trevor Norris) [#1663](https://github.com/nodejs/node/pull/1663)
* [[`5883a59b21`](https://github.com/nodejs/node/commit/5883a59b21)] - **cluster**: disconnect event not emitted correctly (Oleg Elifantiev) [#1386](https://github.com/nodejs/node/pull/1386)
* [[`0f850f7ae7`](https://github.com/nodejs/node/commit/0f850f7ae7)] - **deps**: provide TXT chunk info in c-ares (Fedor Indutny)
* [[`7e1c0e75ed`](https://github.com/nodejs/node/commit/7e1c0e75ed)] - **deps**: sync with upstream bagder/c-ares@bba4dc5 (Ben Noordhuis) [#1678](https://github.com/nodejs/node/pull/1678)
* [[`18d457bd34`](https://github.com/nodejs/node/commit/18d457bd34)] - **dgram**: call send callback asynchronously (Yosuke Furukawa) [#1313](https://github.com/nodejs/node/pull/1313)
* [[`8b9a1537ad`](https://github.com/nodejs/node/commit/8b9a1537ad)] - **events**: provide better error message for unhandled error (Evan Lucas) [#1654](https://github.com/nodejs/node/pull/1654)
* [[`19ffb5cf1c`](https://github.com/nodejs/node/commit/19ffb5cf1c)] - **lib**: fix eslint styles (Yosuke Furukawa) [#1539](https://github.com/nodejs/node/pull/1539)
* [[`76937051f8`](https://github.com/nodejs/node/commit/76937051f8)] - **os**: refine tmpdir() trailing slash stripping (cjihrig) [#1673](https://github.com/nodejs/node/pull/1673)
* [[`aed6bce906`](https://github.com/nodejs/node/commit/aed6bce906)] - **readline**: turn emitKeys into a streaming parser (Alex Kocharin) [#1601](https://github.com/nodejs/node/pull/1601)
* [[`0a461e5360`](https://github.com/nodejs/node/commit/0a461e5360)] - **src**: fix preload when used with prior flags (Yosuke Furukawa) [#1694](https://github.com/nodejs/node/pull/1694)
* [[`931a0d4634`](https://github.com/nodejs/node/commit/931a0d4634)] - **src**: add type check to v8.setFlagsFromString() (Roman Klauke) [#1652](https://github.com/nodejs/node/pull/1652)
* [[`08d08668c9`](https://github.com/nodejs/node/commit/08d08668c9)] - **src,deps**: replace LoadLibrary by LoadLibraryW (Cheng Zhao) [#226](https://github.com/nodejs/node/pull/226)
* [[`4e2f999a62`](https://github.com/nodejs/node/commit/4e2f999a62)] - **test**: fix infinite loop detection (Yosuke Furukawa) [#1681](https://github.com/nodejs/node/pull/1681)
* [[`5755fc099f`](https://github.com/nodejs/node/commit/5755fc099f)] - **tls**: update default ciphers to use gcm and aes128 (Mike MacCana) [#1660](https://github.com/nodejs/node/pull/1660)
* [[`966acb9916`](https://github.com/nodejs/node/commit/966acb9916)] - **tools**: remove closure_linter to eslint on windows (Yosuke Furukawa) [#1685](https://github.com/nodejs/node/pull/1685)
* [[`c58264e58b`](https://github.com/nodejs/node/commit/c58264e58b)] - **tools**: make eslint work on subdirectories (Roman Reiss) [#1686](https://github.com/nodejs/node/pull/1686)
* [[`0b21ab13b7`](https://github.com/nodejs/node/commit/0b21ab13b7)] - **tools**: refactor `make test-npm` into test-npm.sh (Jeremiah Senkpiel) [#1662](https://github.com/nodejs/node/pull/1662)
* [[`f07b3b600b`](https://github.com/nodejs/node/commit/f07b3b600b)] - **tools**: set eslint comma-spacing to 'warn' (Roman Reiss) [#1672](https://github.com/nodejs/node/pull/1672)
* [[`f9dd34d301`](https://github.com/nodejs/node/commit/f9dd34d301)] - **tools**: replace closure-linter with eslint (Yosuke Furukawa) [#1539](https://github.com/nodejs/node/pull/1539)
* [[`64d3210c98`](https://github.com/nodejs/node/commit/64d3210c98)] - **win,node-gyp**: enable delay-load hook by default (Bert Belder) [#1667](https://github.com/nodejs/node/issues/1667)

## 2015-05-13, Version 0.12.3 (Stable)

### Commits

* [[`32166a90cf`](https://github.com/nodejs/node/commit/32166a90cf)] - **V8**: update to 3.28.71.19 [#18206](https://github.com/joyent/node/pull/18206)
* [[`84f1ab6114`](https://github.com/nodejs/node/commit/84f1ab6114)] - **uv**: upgrade to 1.5.0 [#25141](https://github.com/joyent/node/pull/25141)
* [[`03cfbd65fb`](https://github.com/nodejs/node/commit/03cfbd65fb)] - **npm**: upgrade to 2.9.1 [#25289](https://github.com/joyent/node/pull/25289)
* [[`80cdae855f`](https://github.com/nodejs/node/commit/80cdae855f)] - **V8**: don't busy loop in v8 cpu profiler thread (Mike Tunnicliffe) [#25268](https://github.com/joyent/node/pull/25268)
* [[`2a5f4bd7ce`](https://github.com/nodejs/node/commit/2a5f4bd7ce)] - **V8**: fix issue with let bindings in for loops (adamk) [#23948](https://github.com/joyent/node/pull/23948)
* [[`f0ef597e09`](https://github.com/nodejs/node/commit/f0ef597e09)] - **debugger**: don't spawn child process in remote mode (Jackson Tian) [#14172](https://github.com/joyent/node/pull/14172)
* [[`0e392f3b68`](https://github.com/nodejs/node/commit/0e392f3b68)] - **net**: do not set V4MAPPED on FreeBSD (Julien Gilli) [#18204](https://github.com/joyent/node/pull/18204)
* [[`101e103e3b`](https://github.com/nodejs/node/commit/101e103e3b)] - **repl**: make 'Unexpected token' errors recoverable (Julien Gilli) [#8875](https://github.com/joyent/node/pull/8875)
* [[`d5b32246fb`](https://github.com/nodejs/node/commit/d5b32246fb)] - **src**: backport ignore ENOTCONN on shutdown race (Ben Noordhuis) [#14480](https://github.com/joyent/node/pull/14480)
* [[`f99eaefe75`](https://github.com/nodejs/node/commit/f99eaefe75)] - **src**: fix backport of SIGINT crash fix on FreeBSD (Julien Gilli) [#14819](https://github.com/joyent/node/pull/14819)

## 2015-05-07, Version 2.0.1, @rvagg

### Notable changes

* **async_wrap**: (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
  - it is now possible to filter by providers
  - bit flags have been removed and replaced with method calls on the binding object
  - _note that this is an unstable API so feature additions and breaking changes won't change io.js semver_
* **libuv**: resolves numerous io.js issues:
  - [#862](https://github.com/nodejs/node/issues/862) prevent spawning child processes with invalid stdio file descriptors
  - [#1397](https://github.com/nodejs/node/issues/1397) fix EPERM error with fs.access(W_OK) on Windows
  - [#1621](https://github.com/nodejs/node/issues/1621) build errors associated with the bundled libuv
  - [#1512](https://github.com/nodejs/node/issues/1512) should properly fix Windows termination errors
* **addons**: the `NODE_DEPRECATED` macro was causing problems when compiling addons with older compilers, this should now be resolved (Ben Noordhuis) [#1626](https://github.com/nodejs/node/pull/1626)
* **V8**: upgrade V8 from 4.2.77.18 to 4.2.77.20 with minor fixes, including a bug preventing builds on FreeBSD

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`7dde95a8bd`](https://github.com/nodejs/node/commit/7dde95a8bd)] - **async-wrap**: remove before/after calls in init (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
* [[`bd42ba056a`](https://github.com/nodejs/node/commit/bd42ba056a)] - **async-wrap**: set flags using functions (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
* [[`4b2c786449`](https://github.com/nodejs/node/commit/4b2c786449)] - **async-wrap**: pass PROVIDER as first arg to init (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
* [[`84bf609fd2`](https://github.com/nodejs/node/commit/84bf609fd2)] - **async-wrap**: don't call init callback unnecessarily (Trevor Norris) [#1614](https://github.com/nodejs/node/pull/1614)
* [[`04cc03b029`](https://github.com/nodejs/node/commit/04cc03b029)] - **deps**: update libuv to 1.5.0 (Saúl Ibarra Corretgé) [#1646](https://github.com/nodejs/node/pull/1646)
* [[`b16d9c28e8`](https://github.com/nodejs/node/commit/b16d9c28e8)] - **deps**: upgrade v8 to 4.2.77.20 (Ben Noordhuis) [#1639](https://github.com/nodejs/node/pull/1639)
* [[`9ec3109272`](https://github.com/nodejs/node/commit/9ec3109272)] - **doc**: add TC meeting 2015-04-29 minutes (Rod Vagg) [#1585](https://github.com/nodejs/node/pull/1585)
* [[`2c7206254c`](https://github.com/nodejs/node/commit/2c7206254c)] - **doc**: fix typo in readme.md (AQNOUCH Mohammed) [#1643](https://github.com/nodejs/node/pull/1643)
* [[`71dc7152ee`](https://github.com/nodejs/node/commit/71dc7152ee)] - **doc**: fix PR link in CHANGELOG (Brian White) [#1624](https://github.com/nodejs/node/pull/1624)
* [[`b97b96d05a`](https://github.com/nodejs/node/commit/b97b96d05a)] - **install**: fix NameError (thefourtheye) [#1628](https://github.com/nodejs/node/pull/1628)
* [[`6ccbe75384`](https://github.com/nodejs/node/commit/6ccbe75384)] - **js_stream**: fix buffer index in DoWrite (Shigeki Ohtsu) [#1635](https://github.com/nodejs/node/pull/1635)
* [[`c43855c49c`](https://github.com/nodejs/node/commit/c43855c49c)] - **src**: export the ParseEncoding function on Windows (Ivan Kozik) [#1596](https://github.com/nodejs/node/pull/1596)
* [[`8315b22390`](https://github.com/nodejs/node/commit/8315b22390)] - **src**: fix pedantic cpplint whitespace warnings (Ben Noordhuis) [#1640](https://github.com/nodejs/node/pull/1640)
* [[`b712af79a7`](https://github.com/nodejs/node/commit/b712af79a7)] - **src**: fix NODE_DEPRECATED macro with old compilers (Ben Noordhuis) [#1626](https://github.com/nodejs/node/pull/1626)
* [[`2ed10f1349`](https://github.com/nodejs/node/commit/2ed10f1349)] - **src**: fix minor inefficiency in Buffer::New() call (Ben Noordhuis) [#1577](https://github.com/nodejs/node/pull/1577)
* [[`f696c9efab`](https://github.com/nodejs/node/commit/f696c9efab)] - **src**: fix deprecated use of Buffer::New() (Ben Noordhuis) [#1577](https://github.com/nodejs/node/pull/1577)
* [[`0c8f13df8f`](https://github.com/nodejs/node/commit/0c8f13df8f)] - **tools**: remove unused GuessWordSize function (thefourtheye) [#1638](https://github.com/nodejs/node/pull/1638)

## 2015-05-04, Version 2.0.0, @rvagg

### Breaking changes

Full details at https://github.com/nodejs/node/wiki/Breaking-Changes#200-from-1x

* V8 upgrade to 4.2, minor changes to C++ API
* `os.tmpdir()` is now cross-platform consistent and no longer returns a path with a trailing slash on any platform
* While not a *breaking change* the 'smalloc' module has been deprecated in anticipation of it becoming unsupportable with a future upgrade to V8 4.4. See [#1451](https://github.com/nodejs/node/issues/1451)  for further information.

_Note: a new version of the 'url' module was reverted prior to release as it was decided the potential for breakage across the npm ecosystem was too great and that more compatibility work needed to be done before releasing it. See [#1602](https://github.com/nodejs/node/pull/1602) for further information._

### Notable changes

* **crypto**: significantly reduced memory usage for TLS (Fedor Indutny & Сковорода Никита Андреевич) [#1529](https://github.com/nodejs/node/pull/1529)
* **net**: `socket.connect()` now accepts a `'lookup'` option for a custom DNS resolution mechanism, defaults to `dns.lookup()` (Evan Lucas) [#1505](https://github.com/nodejs/node/pull/1505)
* **npm**: Upgrade npm to 2.9.0. See the [v2.8.4](https://github.com/npm/npm/releases/tag/v2.8.4) and [v2.9.0](https://github.com/npm/npm/releases/tag/v2.9.0) release notes for details. Notable items:
  - Add support for default author field to make `npm init -y` work without user-input (@othiym23) [npm/npm/d8eee6cf9d](https://github.com/npm/npm/commit/d8eee6cf9d2ff7aca68dfaed2de76824a3e0d9af)
  - Include local modules in `npm outdated` and `npm update` (@ArnaudRinquin) [npm/npm#7426](https://github.com/npm/npm/issues/7426)
  - The prefix used before the version number on `npm version` is now configurable via `tag-version-prefix` (@kkragenbrink) [npm/npm#8014](https://github.com/npm/npm/issues/8014)
* **os**: `os.tmpdir()` is now cross-platform consistent and will no longer returns a path with a trailing slash on any platform (Christian Tellnes) [#747](https://github.com/nodejs/node/pull/747)
* **process**:
  - `process.nextTick()` performance has been improved by between 2-42% across the benchmark suite, notable because this is heavily used across core (Brian White) [#1571](https://github.com/nodejs/node/pull/1571)
  - New `process.geteuid()`, `process.seteuid(id)`, `process.getegid()` and `process.setegid(id)` methods allow you to get and set effective UID and GID of the process (Evan Lucas) [#1536](https://github.com/nodejs/node/pull/1536)
* **repl**:
  - REPL history can be persisted across sessions if the `NODE_REPL_HISTORY_FILE` environment variable is set to a user accessible file, `NODE_REPL_HISTORY_SIZE` can set the maximum history size and defaults to `1000` (Chris Dickinson) [#1513](https://github.com/nodejs/node/pull/1513)
  - The REPL can be placed in to one of three modes using the `NODE_REPL_MODE` environment variable: `sloppy`, `strict` or `magic` (default); the new `magic` mode will automatically run "strict mode only" statements in strict mode (Chris Dickinson) [#1513](https://github.com/nodejs/node/pull/1513)
* **smalloc**: the 'smalloc' module has been deprecated due to changes coming in V8 4.4 that will render it unusable
* **util**: add Promise, Map and Set inspection support (Christopher Monsanto) [#1471](https://github.com/nodejs/node/pull/1471)
* **V8**: upgrade to 4.2.77.18, see the [ChangeLog](https://chromium.googlesource.com/v8/v8/+/refs/heads/4.2.77/ChangeLog) for full details. Notable items:
  - Classes have moved out of staging; the `class` keyword is now usable in strict mode without flags
  - Object literal enhancements have moved out of staging; shorthand method and property syntax is now usable (`{ method() { }, property }`)
  - Rest parameters (`function(...args) {}`) are implemented in staging behind the `--harmony-rest-parameters` flag
  - Computed property names (`{['foo'+'bar']:'bam'}`) are implemented in staging behind the `--harmony-computed-property-names` flag
  - Unicode escapes (`'\u{xxxx}'`) are implemented in staging behind the `--harmony_unicode` flag and the `--harmony_unicode_regexps` flag for use in regular expressions
* **Windows**:
  - Random process termination on Windows fixed (Fedor Indutny)  [#1512](https://github.com/nodejs/node/issues/1512) / [#1563](https://github.com/nodejs/node/pull/1563)
  - The delay-load hook introduced to fix issues with process naming (iojs.exe / node.exe) has been made opt-out for native add-ons. Native add-ons should include `'win_delay_load_hook': 'false'` in their binding.gyp to disable this feature if they experience problems . (Bert Belder) [#1433](https://github.com/nodejs/node/pull/1433)
* **Governance**:
  - Rod Vagg (@rvagg) was added to the Technical Committee (TC)
  - Jeremiah Senkpiel (@Fishrock123) was added to the Technical Committee (TC)

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`5404cbc745`](https://github.com/nodejs/node/commit/5404cbc745)] - **buffer**: fix copy() segfault with zero arguments (Trevor Norris) [#1520](https://github.com/nodejs/node/pull/1520)
* [[`3d3083b91f`](https://github.com/nodejs/node/commit/3d3083b91f)] - **buffer**: little improve for Buffer.concat method (Jackson Tian) [#1437](https://github.com/nodejs/node/pull/1437)
* [[`e67542ae17`](https://github.com/nodejs/node/commit/e67542ae17)] - **build**: disable -Og when building with clang (Ben Noordhuis) [#1609](https://github.com/nodejs/node/pull/1609)
* [[`78f4b038f8`](https://github.com/nodejs/node/commit/78f4b038f8)] - **build**: turn on debug-safe optimizations with -Og (Ben Noordhuis) [#1569](https://github.com/nodejs/node/pull/1569)
* [[`a5dcff827a`](https://github.com/nodejs/node/commit/a5dcff827a)] - **build**: Use option groups in configure output (Johan Bergström) [#1533](https://github.com/nodejs/node/pull/1533)
* [[`2a3c8c187e`](https://github.com/nodejs/node/commit/2a3c8c187e)] - **build**: remove -J from test-ci (Rod Vagg) [#1544](https://github.com/nodejs/node/pull/1544)
* [[`e6874dd0f9`](https://github.com/nodejs/node/commit/e6874dd0f9)] - **crypto**: track external memory for SSL structures (Fedor Indutny) [#1529](https://github.com/nodejs/node/pull/1529)
* [[`935c9d3fa7`](https://github.com/nodejs/node/commit/935c9d3fa7)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`56e4255382`](https://github.com/nodejs/node/commit/56e4255382)] - **deps**: upgrade npm to 2.9.0 (Forrest L Norvell) [#1573](https://github.com/nodejs/node/pull/1573)
* [[`509b59ea7c`](https://github.com/nodejs/node/commit/509b59ea7c)] - **deps**: enable v8 postmortem debugging again (Ben Noordhuis) [#1232](https://github.com/nodejs/node/pull/1232)
* [[`01652c7709`](https://github.com/nodejs/node/commit/01652c7709)] - **deps**: upgrade v8 to 4.2.77.18 (Chris Dickinson) [#1506](https://github.com/nodejs/node/pull/1506)
* [[`01e6632d70`](https://github.com/nodejs/node/commit/01e6632d70)] - **deps**: upgrade v8 to 4.2.77.15 (Ben Noordhuis) [#1399](https://github.com/nodejs/node/pull/1399)
* [[`db4ded5903`](https://github.com/nodejs/node/commit/db4ded5903)] - **deps**: enable v8 postmortem debugging again (Ben Noordhuis) [#1232](https://github.com/nodejs/node/pull/1232)
* [[`36cd5fb9d2`](https://github.com/nodejs/node/commit/36cd5fb9d2)] - **(SEMVER-MAJOR)** **deps**: upgrade v8 to 4.2.77.13 (Ben Noordhuis) [#1232](https://github.com/nodejs/node/pull/1232)
* [[`b3a7da1091`](https://github.com/nodejs/node/commit/b3a7da1091)] - **deps**: update http_parser to 2.5.0 (Fedor Indutny) [#1517](https://github.com/nodejs/node/pull/1517)
* [[`ac1fb39ce8`](https://github.com/nodejs/node/commit/ac1fb39ce8)] - **doc**: add rvagg to the TC (Rod Vagg) [#1613](https://github.com/nodejs/node/pull/1613)
* [[`dacc1fa35c`](https://github.com/nodejs/node/commit/dacc1fa35c)] - **doc**: update AUTHORS list (Rod Vagg) [#1586](https://github.com/nodejs/node/pull/1586)
* [[`2a3a1909ab`](https://github.com/nodejs/node/commit/2a3a1909ab)] - **doc**: add require() lines to child.stdio example (Nick Raienko) [#1504](https://github.com/nodejs/node/pull/1504)
* [[`02388dbf40`](https://github.com/nodejs/node/commit/02388dbf40)] - **doc**: fix some cross-references (Alexander Gromnitsky) [#1584](https://github.com/nodejs/node/pull/1584)
* [[`57c4cc26e2`](https://github.com/nodejs/node/commit/57c4cc26e2)] - **doc**: add TC meeting 2015-04-22 minutes (Rod Vagg) [#1556](https://github.com/nodejs/node/pull/1556)
* [[`b4ad5d7050`](https://github.com/nodejs/node/commit/b4ad5d7050)] - **doc**: improve http.request and https.request opts (Roman Reiss) [#1551](https://github.com/nodejs/node/pull/1551)
* [[`7dc8eec0a6`](https://github.com/nodejs/node/commit/7dc8eec0a6)] - **doc**: deprecate smalloc module (Ben Noordhuis) [#1566](https://github.com/nodejs/node/pull/1566)
* [[`1bcdf46ca7`](https://github.com/nodejs/node/commit/1bcdf46ca7)] - **doc**: add TC meeting 2015-04-15 minutes (Rod Vagg) [#1498](https://github.com/nodejs/node/pull/1498)
* [[`391cae3595`](https://github.com/nodejs/node/commit/391cae3595)] - **doc**: Add Known issues to v1.7.0/1.7.1 CHANGELOG (Yosuke Furukawa) [#1473](https://github.com/nodejs/node/pull/1473)
* [[`e55fdc47a7`](https://github.com/nodejs/node/commit/e55fdc47a7)] - **doc**: fix util.deprecate example (Nick Raienko) [#1535](https://github.com/nodejs/node/pull/1535)
* [[`5178f93bc0`](https://github.com/nodejs/node/commit/5178f93bc0)] - **doc**: Add Addon API (NAN) to working group list (Julian Duque) [#1523](https://github.com/nodejs/node/pull/1523)
* [[`f3cc50f811`](https://github.com/nodejs/node/commit/f3cc50f811)] - **doc**: add TC meeting 2015-04-08 minutes (Rod Vagg) [#1497](https://github.com/nodejs/node/pull/1497)
* [[`bb254b533b`](https://github.com/nodejs/node/commit/bb254b533b)] - **doc**: update branch to master (Roman Reiss) [#1511](https://github.com/nodejs/node/pull/1511)
* [[`22aafa5597`](https://github.com/nodejs/node/commit/22aafa5597)] - **doc**: add Fishrock123 to the TC (Jeremiah Senkpiel) [#1507](https://github.com/nodejs/node/pull/1507)
* [[`b16a328ede`](https://github.com/nodejs/node/commit/b16a328ede)] - **doc**: add spaces to child.kill example (Nick Raienko) [#1503](https://github.com/nodejs/node/pull/1503)
* [[`26327757f8`](https://github.com/nodejs/node/commit/26327757f8)] - **doc**: update AUTHORS list (Rod Vagg) [#1476](https://github.com/nodejs/node/pull/1476)
* [[`f9c681cf62`](https://github.com/nodejs/node/commit/f9c681cf62)] - **fs**: validate fd on fs.write (Julian Duque) [#1553](https://github.com/nodejs/node/pull/1553)
* [[`801b47acc5`](https://github.com/nodejs/node/commit/801b47acc5)] - **gitignore**: ignore xcode workspaces and projects (Roman Klauke) [#1562](https://github.com/nodejs/node/pull/1562)
* [[`d5ce47e433`](https://github.com/nodejs/node/commit/d5ce47e433)] - **(SEMVER-MINOR)** **lib**: deprecate the smalloc module (Ben Noordhuis) [#1564](https://github.com/nodejs/node/pull/1564)
* [[`7384ca83f9`](https://github.com/nodejs/node/commit/7384ca83f9)] - **module**: remove '' from Module.globalPaths (Chris Yip) [#1488](https://github.com/nodejs/node/pull/1488)
* [[`b4f5898395`](https://github.com/nodejs/node/commit/b4f5898395)] - **net**: ensure Write/ShutdownWrap references handle (Fedor Indutny) [#1590](https://github.com/nodejs/node/pull/1590)
* [[`4abe2fa1cf`](https://github.com/nodejs/node/commit/4abe2fa1cf)] - **(SEMVER-MINOR)** **net**: add lookup option to Socket.prototype.connect (Evan Lucas) [#1505](https://github.com/nodejs/node/pull/1505)
* [[`1bef717476`](https://github.com/nodejs/node/commit/1bef717476)] - **(SEMVER-MINOR)** **net**: cleanup connect logic (Evan Lucas) [#1505](https://github.com/nodejs/node/pull/1505)
* [[`c7782c0af8`](https://github.com/nodejs/node/commit/c7782c0af8)] - **node**: improve nextTick performance (Brian White) [#1571](https://github.com/nodejs/node/pull/1571)
* [[`b57cc51d8d`](https://github.com/nodejs/node/commit/b57cc51d8d)] - **(SEMVER-MAJOR)** **os**: remove trailing slash from os.tmpdir() (Christian Tellnes) [#747](https://github.com/nodejs/node/pull/747)
* [[`ca219b00d1`](https://github.com/nodejs/node/commit/ca219b00d1)] - **repl**: fix for a+ fd clearing the file on read (Chris Dickinson) [#1605](https://github.com/nodejs/node/pull/1605)
* [[`051d482b15`](https://github.com/nodejs/node/commit/051d482b15)] - **repl**: fix \_debugger by properly proxying repl (Chris Dickinson) [#1605](https://github.com/nodejs/node/pull/1605)
* [[`2e2fce0502`](https://github.com/nodejs/node/commit/2e2fce0502)] - **repl**: fix persistent history and env variable name (Roman Reiss) [#1593](https://github.com/nodejs/node/pull/1593)
* [[`ea5195ccaf`](https://github.com/nodejs/node/commit/ea5195ccaf)] - **repl**: do not save history for non-terminal repl (Fedor Indutny) [#1575](https://github.com/nodejs/node/pull/1575)
* [[`0450ce7db2`](https://github.com/nodejs/node/commit/0450ce7db2)] - **repl**: add mode detection, cli persistent history (Chris Dickinson) [#1513](https://github.com/nodejs/node/pull/1513)
* [[`af9fe3bbc7`](https://github.com/nodejs/node/commit/af9fe3bbc7)] - **(SEMVER-MAJOR)** **src**: bump NODE_MODULE_VERSION due to V8 API (Rod Vagg) [#1532](https://github.com/nodejs/node/pull/1532)
* [[`279f6116aa`](https://github.com/nodejs/node/commit/279f6116aa)] - **src**: fix -Wmissing-field-initializers warning (Ben Noordhuis) [#1606](https://github.com/nodejs/node/pull/1606)
* [[`73062521a4`](https://github.com/nodejs/node/commit/73062521a4)] - **src**: deprecate smalloc public functions (Ben Noordhuis) [#1565](https://github.com/nodejs/node/pull/1565)
* [[`ccb199af17`](https://github.com/nodejs/node/commit/ccb199af17)] - **src**: fix deprecation warnings (Ben Noordhuis) [#1565](https://github.com/nodejs/node/pull/1565)
* [[`609fa0de03`](https://github.com/nodejs/node/commit/609fa0de03)] - **src**: fix NODE_DEPRECATED macro (Ben Noordhuis) [#1565](https://github.com/nodejs/node/pull/1565)
* [[`3c92ca2b5c`](https://github.com/nodejs/node/commit/3c92ca2b5c)] - **(SEMVER-MINOR)** **src**: add ability to get/set effective uid/gid (Evan Lucas) [#1536](https://github.com/nodejs/node/pull/1536)
* [[`30b7349176`](https://github.com/nodejs/node/commit/30b7349176)] - **stream_base**: dispatch reqs in the stream impl (Fedor Indutny) [#1563](https://github.com/nodejs/node/pull/1563)
* [[`0fa6c4a6fc`](https://github.com/nodejs/node/commit/0fa6c4a6fc)] - **string_decoder**: don't cache Buffer.isEncoding (Brian White) [#1548](https://github.com/nodejs/node/pull/1548)
* [[`f9b226c1c1`](https://github.com/nodejs/node/commit/f9b226c1c1)] - **test**: extend timeouts for ARMv6 (Rod Vagg) [#1554](https://github.com/nodejs/node/pull/1554)
* [[`bfae8236b1`](https://github.com/nodejs/node/commit/bfae8236b1)] - **test**: fix test-net-dns-custom-lookup test assertion (Evan Lucas) [#1531](https://github.com/nodejs/node/pull/1531)
* [[`547213913b`](https://github.com/nodejs/node/commit/547213913b)] - **test**: adjust Makefile/test-ci, add to vcbuild.bat (Rod Vagg) [#1530](https://github.com/nodejs/node/pull/1530)
* [[`550c2638c0`](https://github.com/nodejs/node/commit/550c2638c0)] - **tls**: use `SSL_set_cert_cb` for async SNI/OCSP (Fedor Indutny) [#1464](https://github.com/nodejs/node/pull/1464)
* [[`1787416376`](https://github.com/nodejs/node/commit/1787416376)] - **tls**: destroy singleUse context immediately (Fedor Indutny) [#1529](https://github.com/nodejs/node/pull/1529)
* [[`2684c902c4`](https://github.com/nodejs/node/commit/2684c902c4)] - **tls**: zero SSL_CTX freelist for a singleUse socket (Fedor Indutny) [#1529](https://github.com/nodejs/node/pull/1529)
* [[`2d241b3b82`](https://github.com/nodejs/node/commit/2d241b3b82)] - **tls**: destroy SSL once it is out of use (Fedor Indutny) [#1529](https://github.com/nodejs/node/pull/1529)
* [[`f7620fb96d`](https://github.com/nodejs/node/commit/f7620fb96d)] - **tls_wrap**: Unlink TLSWrap and SecureContext objects (Сковорода Никита Андреевич) [#1580](https://github.com/nodejs/node/pull/1580)
* [[`a7d74633f2`](https://github.com/nodejs/node/commit/a7d74633f2)] - **tls_wrap**: use localhost if options.host is empty (Guilherme Souza) [#1493](https://github.com/nodejs/node/pull/1493)
* [[`702997c1f0`](https://github.com/nodejs/node/commit/702997c1f0)] - ***Revert*** "**url**: significantly improve the performance of the url module" (Rod Vagg) [#1602](https://github.com/nodejs/node/pull/1602)
* [[`0daed24883`](https://github.com/nodejs/node/commit/0daed24883)] - ***Revert*** "**url**: delete href cache on all setter code paths" (Rod Vagg) [#1602](https://github.com/nodejs/node/pull/1602)
* [[`0f39ef4ca1`](https://github.com/nodejs/node/commit/0f39ef4ca1)] - ***Revert*** "**url**: fix treatment of some values as non-empty" (Rod Vagg) [#1602](https://github.com/nodejs/node/pull/1602)
* [[`66877216bd`](https://github.com/nodejs/node/commit/66877216bd)] - **url**: fix treatment of some values as non-empty (Petka Antonov) [#1589](https://github.com/nodejs/node/pull/1589)
* [[`dbdd81a91b`](https://github.com/nodejs/node/commit/dbdd81a91b)] - **url**: delete href cache on all setter code paths (Petka Antonov) [#1589](https://github.com/nodejs/node/pull/1589)
* [[`3fd7fc429c`](https://github.com/nodejs/node/commit/3fd7fc429c)] - **url**: significantly improve the performance of the url module (Petka Antonov) [#1561](https://github.com/nodejs/node/pull/1561)
* [[`bf7ac08dd0`](https://github.com/nodejs/node/commit/bf7ac08dd0)] - **util**: add Map and Set inspection support (Christopher Monsanto) [#1471](https://github.com/nodejs/node/pull/1471)
* [[`30e83d2e84`](https://github.com/nodejs/node/commit/30e83d2e84)] - **win,node-gyp**: optionally allow node.exe/iojs.exe to be renamed (Bert Belder) [#1266](https://github.com/nodejs/node/pull/1266)
* [[`3bda6cbfa4`](https://github.com/nodejs/node/commit/3bda6cbfa4)] - **(SEMVER-MAJOR)** **win,node-gyp**: enable delay-load hook by default (Bert Belder) [#1433](https://github.com/nodejs/node/pull/1433)

## 2015-04-20, Version 1.8.1, @chrisdickinson

### Notable changes

* **NOTICE**: Skipped v1.8.0 due to problems with release tooling.
  See [#1436](https://github.com/nodejs/node/issues/1436) for details.
* **build**: Support for building io.js as a static library (Marat Abdullin) [#1341](https://github.com/nodejs/node/pull/1341)
* **deps**: Upgrade openssl to 1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
  * Users should see performance improvements when using the crypto API.
  See [here](https://github.com/nodejs/node/wiki/Crypto-Performance-Notes-for-OpenSSL-1.0.2a-on-iojs-v1.8.0)
  for details.
* **npm**: Upgrade npm to 2.8.3. See the [release notes](https://github.com/npm/npm/releases/tag/v2.8.3) for details. Includes improved git support. Summary:
  * [`387f889`](https://github.com/npm/npm/commit/387f889c0e8fb617d9cc9a42ed0a3ec49424ab5d)
  [#7961](https://github.com/npm/npm/issues/7961) Ensure that hosted git SSH
  URLs always have a valid protocol when stored in `resolved` fields in
  `npm-shrinkwrap.json`. ([@othiym23](https://github.com/othiym23))
  * [`394c2f5`](https://github.com/npm/npm/commit/394c2f5a1227232c0baf42fbba1402aafe0d6ffb)
  Switch the order in which hosted Git providers are checked to `git:`,
  `git+https:`, then `git+ssh:` (from `git:`, `git+ssh:`, then `git+https:`) in
  an effort to go from most to least likely to succeed, to make for less
  confusing error message. ([@othiym23](https://github.com/othiym23))
  * [`431c3bf`](https://github.com/npm/npm/commit/431c3bf6cdec50f9f0c735f478cb2f3f337d3313)
  [#7699](https://github.com/npm/npm/issues/7699) `npm-registry-client@6.3.2`:
  Don't send body with HTTP GET requests when logging in.
  ([@smikes](https://github.com/smikes))
  * [`15efe12`](https://github.com/npm/npm/commit/15efe124753257728a0ddc64074fa5a4b9c2eb30)
  [#7872](https://github.com/npm/npm/issues/7872) Use the new version of
  `hosted-git-info` to pass along credentials embedded in git URLs. Test it.
  Test it a lot. ([@othiym23](https://github.com/othiym23))
  * [`b027319`](https://github.com/npm/npm/commit/b0273190c71eba14395ddfdd1d9f7ba625297523)
  [#7920](https://github.com/npm/npm/issues/7920) Scoped packages with
  `peerDependencies` were installing the `peerDependencies` into the wrong
  directory. ([@ewie](https://github.com/ewie))
  * [`6b0f588`](https://github.com/npm/npm/commit/6b0f58877f37df9904490ffbaaad33862bd36dce)
  [#7867](https://github.com/npm/npm/issues/7867) Use git shorthand and git
  URLs as presented by user. Support new `hosted-git-info` shortcut syntax.
  Save shorthand in `package.json`. Try cloning via `git:`, `git+ssh:`, and
  `git+https:`, in that order, when supported by the underlying hosting
  provider. ([@othiym23](https://github.com/othiym23))
* **src**: Allow multiple arguments to be passed to process.nextTick (Trevor Norris) [#1077](https://github.com/nodejs/node/pull/1077)
* **module**: The interaction of `require('.')` with `NODE_PATH` has been restored and deprecated. This functionality
will be removed at a later point. (Roman Reiss) [#1363](https://github.com/nodejs/node/pull/1363)

### Known issues

* Some problems with unreferenced timers running during `beforeExit` are still to be resolved. See [#1264](https://github.com/nodejs/node/issues/1264).
* Surrogate pair in REPL can freeze terminal [#690](https://github.com/nodejs/node/issues/690)
* `process.send()` is not synchronous as the docs suggest, a regression introduced in 1.0.2, see [#760](https://github.com/nodejs/node/issues/760) and fix in [#774](https://github.com/nodejs/node/issues/774)
* Calling `dns.setServers()` while a DNS query is in progress can cause the process to crash on a failed assertion [#894](https://github.com/nodejs/node/issues/894)
* `url.resolve` may transfer the auth portion of the url when resolving between two full hosts, see [#1435](https://github.com/nodejs/node/issues/1435).
* readline: split escapes are processed incorrectly, see [#1403](https://github.com/nodejs/node/issues/1403)

### Commits

* [[`53ed89d927`](https://github.com/nodejs/node/commit/53ed89d927)] - ***Revert*** "**build**: use %PYTHON% instead of python" (Rod Vagg) [#1475](https://github.com/nodejs/node/pull/1475)
* [[`2b744b0ab7`](https://github.com/nodejs/node/commit/2b744b0ab7)] - **src**: revert NODE_MODULE_VERSION to 43 (Chris Dickinson) [#1460](https://github.com/nodejs/node/pull/1460)
* [[`431673ebd1`](https://github.com/nodejs/node/commit/431673ebd1)] - **buffer**: fast-case for empty string in byteLength (Jackson Tian) [#1441](https://github.com/nodejs/node/pull/1441)
* [[`1b22bad35f`](https://github.com/nodejs/node/commit/1b22bad35f)] - **build**: fix logic for shared library flags (Jeremiah Senkpiel) [#1454](https://github.com/nodejs/node/pull/1454)
* [[`91943a99d5`](https://github.com/nodejs/node/commit/91943a99d5)] - **build**: use %PYTHON% instead of python (Rod Vagg) [#1444](https://github.com/nodejs/node/pull/1444)
* [[`c7769d417b`](https://github.com/nodejs/node/commit/c7769d417b)] - **build**: Expose xz compression level (Johan Bergström) [#1428](https://github.com/nodejs/node/pull/1428)
* [[`a530b2baf1`](https://github.com/nodejs/node/commit/a530b2baf1)] - **build**: fix error message in configure (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`92dfb794f9`](https://github.com/nodejs/node/commit/92dfb794f9)] - **build**: enable ssl support on arm64 (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`7de0dcde83`](https://github.com/nodejs/node/commit/7de0dcde83)] - **deps**: make node-gyp work with io.js (cjihrig) [#990](https://github.com/nodejs/node/pull/990)
* [[`4870213f9e`](https://github.com/nodejs/node/commit/4870213f9e)] - **deps**: upgrade npm to 2.8.3 (Forrest L Norvell)
* [[`49bb7ded2c`](https://github.com/nodejs/node/commit/49bb7ded2c)] - **deps**: fix git case sensitivity issue in npm (Chris Dickinson) [#1456](https://github.com/nodejs/node/pull/1456)
* [[`4830b4bce8`](https://github.com/nodejs/node/commit/4830b4bce8)] - **deps**: add docs to upgrade openssl (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`11bec72c87`](https://github.com/nodejs/node/commit/11bec72c87)] - **deps**: update asm files for openssl-1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`53924d8ebe`](https://github.com/nodejs/node/commit/53924d8ebe)] - **deps**: update asm Makefile for openssl-1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`418e839456`](https://github.com/nodejs/node/commit/418e839456)] - **deps**: update openssl.gyp/gypi for openssl-1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`02f12ab666`](https://github.com/nodejs/node/commit/02f12ab666)] - **deps**: update opensslconf.h for 1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`eb7a23595f`](https://github.com/nodejs/node/commit/eb7a23595f)] - **deps**: add x32 and arm64 support for opensslconf.h (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`033a663127`](https://github.com/nodejs/node/commit/033a663127)] - **deps**: replace all headers in openssl (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`ae8831f240`](https://github.com/nodejs/node/commit/ae8831f240)] - **deps**: backport openssl patch of alt cert chains 1 (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`71316c46d9`](https://github.com/nodejs/node/commit/71316c46d9)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`d293a4f096`](https://github.com/nodejs/node/commit/d293a4f096)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`e4872d7405`](https://github.com/nodejs/node/commit/e4872d7405)] - **deps**: upgrade openssl to 1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`a1c9ef3142`](https://github.com/nodejs/node/commit/a1c9ef3142)] - **deps, build**: add support older assembler (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`76f219c128`](https://github.com/nodejs/node/commit/76f219c128)] - **doc**: Document forced pushing with git (Johan Bergström) [#1420](https://github.com/nodejs/node/pull/1420)
* [[`12e51d56c1`](https://github.com/nodejs/node/commit/12e51d56c1)] - **doc**: add Addon API WG (Rod Vagg) [#1226](https://github.com/nodejs/node/pull/1226)
* [[`7956a13dad`](https://github.com/nodejs/node/commit/7956a13dad)] - **http**: logically respect maxSockets (fengmk2) [#1242](https://github.com/nodejs/node/pull/1242)
* [[`5b844e140b`](https://github.com/nodejs/node/commit/5b844e140b)] - **module**: fix style (Roman Reiss) [#1453](https://github.com/nodejs/node/pull/1453)
* [[`3ad82c335d`](https://github.com/nodejs/node/commit/3ad82c335d)] - **(SEMVER-MINOR)** **module**: handle NODE_PATH in require('.') (Roman Reiss) [#1363](https://github.com/nodejs/node/pull/1363)
* [[`cd60ff0328`](https://github.com/nodejs/node/commit/cd60ff0328)] - **net**: add fd into listen2 debug info (Jackson Tian) [#1442](https://github.com/nodejs/node/pull/1442)
* [[`10e31ba56c`](https://github.com/nodejs/node/commit/10e31ba56c)] - **(SEMVER-MINOR)** **node**: allow multiple arguments passed to nextTick (Trevor Norris) [#1077](https://github.com/nodejs/node/pull/1077)
* [[`116c54692a`](https://github.com/nodejs/node/commit/116c54692a)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`62f5f4cec9`](https://github.com/nodejs/node/commit/62f5f4cec9)] - **src**: remove duplicate byteLength from Buffer (Jackson Tian) [#1438](https://github.com/nodejs/node/pull/1438)
* [[`51d0808c90`](https://github.com/nodejs/node/commit/51d0808c90)] - **stream**: remove duplicated expression (Yazhong Liu) [#1444](https://github.com/nodejs/node/pull/1444)
* [[`deb9d23d7b`](https://github.com/nodejs/node/commit/deb9d23d7b)] - **test**: fix error message check for openssl-1.0.2a (Shigeki Ohtsu) [#1389](https://github.com/nodejs/node/pull/1389)
* [[`ca8c9ec2c8`](https://github.com/nodejs/node/commit/ca8c9ec2c8)] - **win,node-gyp**: optionally allow node.exe/iojs.exe to be renamed (Bert Belder) [#1266](https://github.com/nodejs/node/pull/1266)

## Archive

The changelog archive can be found in [docs/CHANGELOG.ARCHIVE.md](https://github.com/nodejs/node/blob/master/doc/CHANGELOG.ARCHIVE.md)
It includes the changelog for the following releases:

## 2015-04-14, Version 1.7.1, @rvagg
## 2015-04-14, Version 1.7.0, @rvagg
## 2015-04-06, Version 1.6.4, @Fishrock123
## 2015-03-31, Version 1.6.3, @rvagg
## 2015-03-31, Version 0.12.2 (Stable)
## 2015-03-23, Version 1.6.2, @rvagg
## 2015-03-23, Version 0.12.1 (Stable)
## 2015-03-23, Version 0.10.38 (Maintenance)
## 2015-03-20, Version 1.6.1, @rvagg
## 2015-03-19, Version 1.6.0, @chrisdickinson
## 2015-03-11, Version 0.10.37 (Maintenance)
## 2015-03-09, Version 1.5.1, @rvagg
## 2015-03-06, Version 1.5.0, @rvagg
## 2015-03-02, Version 1.4.3, @rvagg
## 2015-02-28, Version 1.4.2, @rvagg
## 2015-02-26, Version 1.4.1, @rvagg
## 2015-02-20, Version 1.3.0, @rvagg
## 2015-02-10, Version 1.2.0, @rvagg
## 2015-02-06, Version 0.12.0 (Stable)
## 2015-02-03, Version 1.1.0, @chrisdickinson
## 2015-01-26, Version 0.10.36 (Stable)
## 2015-01-24, Version 1.0.4, @rvagg
## 2015-01-20, Version 1.0.3, @rvagg
## 2015-01-16, Version 1.0.2, @rvagg
## 2015-01-14, Version 1.0.1, @rvagg
## 2014.09.24, Version 0.11.14 (Unstable)
## 2014.05.01, Version 0.11.13 (Unstable)
## 2014.03.11, Version 0.11.12 (Unstable)
## 2014.01.29, Version 0.11.11 (Unstable)
## 2013.12.31, Version 0.11.10 (Unstable)
## 2013.11.20, Version 0.11.9 (Unstable)
## 2013.10.30, Version 0.11.8 (Unstable)
## 2013.08.21, Version 0.11.7 (Unstable)
## 2013.08.21, Version 0.11.6 (Unstable)
## 2013.08.06, Version 0.11.5 (Unstable)
## 2013.07.12, Version 0.11.4 (Unstable)
## 2013.06.26, Version 0.11.3 (Unstable)
## 2013.05.13, Version 0.11.2 (Unstable)
## 2013.04.19, Version 0.11.1 (Unstable)
## 2013.03.28, Version 0.11.0 (Unstable)
## 2014.12.22, Version 0.10.35 (Stable)
## 2014.12.17, Version 0.10.34 (Stable)
## 2014.10.20, Version 0.10.33 (Stable)
## 2014.09.16, Version 0.10.32 (Stable)
## 2014.08.19, Version 0.10.31 (Stable)
## 2014.07.31, Version 0.10.30 (Stable)
## 2014.06.05, Version 0.10.29 (Stable)
## 2014.05.01, Version 0.10.28 (Stable)
## 2014.05.01, Version 0.10.27 (Stable)
## 2014.02.18, Version 0.10.26 (Stable)
## 2014.01.23, Version 0.10.25 (Stable)
## 2013.12.18, Version 0.10.24 (Stable)
## 2013.12.12, Version 0.10.23 (Stable)
## 2013.11.12, Version 0.10.22 (Stable)
## 2013.10.18, Version 0.10.21 (Stable)
## 2013.09.30, Version 0.10.20 (Stable)
## 2013.09.24, Version 0.10.19 (Stable)
## 2013.09.04, Version 0.10.18 (Stable)
## 2013.08.21, Version 0.10.17 (Stable)
## 2013.08.16, Version 0.10.16 (Stable)
## 2013.07.25, Version 0.10.15 (Stable)
## 2013.07.25, Version 0.10.14 (Stable)
## 2013.07.09, Version 0.10.13 (Stable)
## 2013.06.18, Version 0.10.12 (Stable)
## 2013.06.13, Version 0.10.11 (Stable)
## 2013.06.04, Version 0.10.10 (Stable)
## 2013.05.30, Version 0.10.9 (Stable)
## 2013.05.24, Version 0.10.8 (Stable)
## 2013.05.17, Version 0.10.7 (Stable)
## 2013.05.14, Version 0.10.6 (Stable)
## 2013.04.23, Version 0.10.5 (Stable)
## 2013.04.11, Version 0.10.4 (Stable)
## 2013.04.03, Version 0.10.3 (Stable)
## 2013.03.28, Version 0.10.2 (Stable)
## 2013.03.21, Version 0.10.1 (Stable)
## 2013.03.11, Version 0.10.0 (Stable)
## 2013.03.06, Version 0.9.12 (Unstable)
## 2013.03.01, Version 0.9.11 (Unstable)
## 2013.02.19, Version 0.9.10 (Unstable)
## 2013.02.07, Version 0.9.9 (Unstable)
## 2013.01.24, Version 0.9.8 (Unstable)
## 2013.01.18, Version 0.9.7 (Unstable)
## 2013.01.11, Version 0.9.6 (Unstable)
## 2012.12.30, Version 0.9.5 (Unstable)
## 2012.12.21, Version 0.9.4 (Unstable)
## 2012.10.24, Version 0.9.3 (Unstable)
## 2012.09.17, Version 0.9.2 (Unstable)
## 2012.08.28, Version 0.9.1 (Unstable)
## 2012.07.20, Version 0.9.0 (Unstable)
## 2013.06.13, Version 0.8.25 (maintenance)
## 2013.06.04, Version 0.8.24 (maintenance)
## 2013.04.09, Version 0.8.23 (maintenance)
## 2013.03.07, Version 0.8.22 (Stable)
## 2013.02.25, Version 0.8.21 (Stable)
## 2013.02.15, Version 0.8.20 (Stable)
## 2013.02.06, Version 0.8.19 (Stable)
## 2013.01.18, Version 0.8.18 (Stable)
## 2013.01.09, Version 0.8.17 (Stable)
## 2012.12.13, Version 0.8.16 (Stable)
## 2012.11.26, Version 0.8.15 (Stable)
## 2012.10.25, Version 0.8.14 (Stable)
## 2012.10.25, Version 0.8.13 (Stable)
## 2012.10.12, Version 0.8.12 (Stable)
## 2012.09.27, Version 0.8.11 (Stable)
## 2012.09.25, Version 0.8.10 (Stable)
## 2012.09.11, Version 0.8.9 (Stable)
## 2012.08.22, Version 0.8.8 (Stable)
## 2012.08.15, Version 0.8.7 (Stable)
## 2012.08.07, Version 0.8.6 (Stable)
## 2012.08.02, Version 0.8.5 (Stable)
## 2012.07.25, Version 0.8.4 (Stable)
## 2012.07.19, Version 0.8.3 (Stable)
## 2012.07.09, Version 0.8.2 (Stable)
## 2012.06.29, Version 0.8.1 (stable)
## 2012.06.25, Version 0.8.0 (stable)
## 2012.06.19, Version 0.7.12 (unstable)
## 2012.06.15, Version 0.7.11 (unstable)
## 2012.06.11, Version 0.7.10 (unstable)
## 2012.05.28, Version 0.7.9 (unstable)
## 2012.04.18, Version 0.7.8 (unstable)
## 2012.03.30, Version 0.7.7 (unstable)
## 2012.03.13, Version 0.7.6 (unstable)
## 2012.02.23, Version 0.7.5 (unstable)
## 2012.02.14, Version 0.7.4 (unstable)
## 2012.02.07, Version 0.7.3 (unstable)
## 2012.02.01, Version 0.7.2 (unstable)
## 2012.01.23, Version 0.7.1 (unstable)
## 2012.01.16, Version 0.7.0 (unstable)
## 2012.07.10 Version 0.6.20 (maintenance)
## 2012.06.06 Version 0.6.19 (stable)
## 2012.05.15 Version 0.6.18 (stable)
## 2012.05.04 Version 0.6.17 (stable)
## 2012.04.30 Version 0.6.16 (stable)
## 2012.04.09 Version 0.6.15 (stable)
## 2012.03.22 Version 0.6.14 (stable)
## 2012.03.15 Version 0.6.13 (stable)
## 2012.03.02 Version 0.6.12 (stable)
## 2012.02.17 Version 0.6.11 (stable)
## 2012.02.02, Version 0.6.10 (stable)
## 2012.01.27, Version 0.6.9 (stable)
## 2012.01.19, Version 0.6.8 (stable)
## 2012.01.06, Version 0.6.7 (stable)
## 2011.12.14, Version 0.6.6 (stable)
## 2011.12.04, Version 0.6.5 (stable)
## 2011.12.02, Version 0.6.4 (stable)
## 2011.11.25, Version 0.6.3 (stable)
## 2011.11.18, Version 0.6.2 (stable)
## 2011.11.11, Version 0.6.1 (stable)
## 2011.11.04, Version 0.6.0 (stable)
## 2011.10.21, Version 0.5.10 (unstable)
## 2011.10.10, Version 0.5.9 (unstable)
## 2011.09.30, Version 0.5.8 (unstable)
## 2011.09.16, Version 0.5.7 (unstable)
## 2011.09.08, Version 0.5.6 (unstable)
## 2011.08.26, Version 0.5.5 (unstable)
## 2011.08.12, Version 0.5.4 (unstable)
## 2011.08.01, Version 0.5.3 (unstable)
## 2011.07.22, Version 0.5.2 (unstable)
## 2011.07.14, Version 0.5.1 (unstable)
## 2011.07.05, Version 0.5.0 (unstable)
## 2011.09.15, Version 0.4.12 (stable)
## 2011.08.17, Version 0.4.11 (stable)
## 2011.07.19, Version 0.4.10 (stable)
## 2011.06.29, Version 0.4.9 (stable)
## 2011.05.20, Version 0.4.8 (stable)
## 2011.04.22, Version 0.4.7 (stable)
## 2011.04.13, Version 0.4.6 (stable)
## 2011.04.01, Version 0.4.5 (stable)
## 2011.03.26, Version 0.4.4 (stable)
## 2011.03.18, Version 0.4.3 (stable)
## 2011.03.02, Version 0.4.2 (stable)
## 2011.02.19, Version 0.4.1 (stable)
## 2011.02.10, Version 0.4.0 (stable)
## 2011.02.04, Version 0.3.8 (unstable)
## 2011.01.27, Version 0.3.7 (unstable)
## 2011.01.21, Version 0.3.6 (unstable)
## 2011.01.16, Version 0.3.5 (unstable)
## 2011.01.08, Version 0.3.4 (unstable)
## 2011.01.02, Version 0.3.3 (unstable)
## 2010.12.16, Version 0.3.2 (unstable)
## 2010.11.16, Version 0.3.1 (unstable)
## 2010.10.23, Version 0.3.0 (unstable)
## 2010.08.20, Version 0.2.0
## 2010.08.13, Version 0.1.104
## 2010.08.04, Version 0.1.103
## 2010.07.25, Version 0.1.102
## 2010.07.16, Version 0.1.101
## 2010.07.03, Version 0.1.100
## 2010.06.21, Version 0.1.99
## 2010.06.11, Version 0.1.98
## 2010.05.29, Version 0.1.97
## 2010.05.21, Version 0.1.96
## 2010.05.13, Version 0.1.95
## 2010.05.06, Version 0.1.94
## 2010.04.29, Version 0.1.93
## 2010.04.23, Version 0.1.92
## 2010.04.15, Version 0.1.91
## 2010.04.09, Version 0.1.90
## 2010.03.19, Version 0.1.33
## 2010.03.12, Version 0.1.32
## 2010.03.05, Version 0.1.31
## 2010.02.22, Version 0.1.30
## 2010.02.17, Version 0.1.29
## 2010.02.09, Version 0.1.28
## 2010.02.03, Version 0.1.27
## 2010.01.20, Version 0.1.26
## 2010.01.09, Version 0.1.25
## 2009.12.31, Version 0.1.24
## 2009.12.22, Version 0.1.23
## 2009.12.19, Version 0.1.22
## 2009.12.06, Version 0.1.21
## 2009.11.28, Version 0.1.20
## 2009.11.28, Version 0.1.19
## 2009.11.17, Version 0.1.18
## 2009.11.07, Version 0.1.17
## 2009.11.03, Version 0.1.16
## 2009.10.28, Version 0.1.15
## 2009.10.09, Version 0.1.14
## 2009.09.30, Version 0.1.13
## 2009.09.24, Version 0.1.12
## 2009.09.18, Version 0.1.11
## 2009.09.11, Version 0.1.10
## 2009.09.05, Version 0.1.9
## 2009.09.04, Version 0.1.8
## 2009.08.27, Version 0.1.7
## 2009.08.22, Version 0.1.6
## 2009.08.21, Version 0.1.5
## 2009.08.13, Version 0.1.4
## 2009.08.06, Version 0.1.3
## 2009.08.01, Version 0.1.2
## 2009.07.27, Version 0.1.1
## 2009.06.30, Version 0.1.0
## 2009.06.24, Version 0.0.6
## 2009.06.18, Version 0.0.5
## 2009.06.13, Version 0.0.4
## 2009.06.11, Version 0.0.3
