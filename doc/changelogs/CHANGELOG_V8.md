# Node.js 8 ChangeLog

<table>
<tr>
<th>Current</th>
</tr>
<tr>
<td>
<a href="#8.4.0">8.4.0</a><br/>
<a href="#8.3.0">8.3.0</a><br/>
<a href="#8.2.1">8.2.1</a><br/>
<a href="#8.2.0">8.2.0</a><br/>
<a href="#8.1.4">8.1.4</a><br/>
<a href="#8.1.3">8.1.3</a><br/>
<a href="#8.1.2">8.1.2</a><br/>
<a href="#8.1.1">8.1.1</a><br/>
<a href="#8.1.0">8.1.0</a><br/>
<a href="#8.0.0">8.0.0</a><br/>
</td>
</tr>
</table>

* Other Versions
  * [7.x](CHANGELOG_V7.md)
  * [6.x](CHANGELOG_V6.md)
  * [5.x](CHANGELOG_V5.md)
  * [4.x](CHANGELOG_V4.md)
  * [0.12.x](CHANGELOG_V012.md)
  * [0.10.x](CHANGELOG_V010.md)
  * [io.js](CHANGELOG_IOJS.md)
  * [Archive](CHANGELOG_ARCHIVE.md)

<a id="8.4.0"></a>
## 2017-08-15, Version 8.4.0 (Current), @addaleax

### Notable changes

* **HTTP2**
  * Experimental support for the built-in `http2` has been added via the
    `--expose-http2` flag.
    [#14239](https://github.com/nodejs/node/pull/14239)

* **Inspector**
  * `require()` is available in the inspector console now.
    [#8837](https://github.com/nodejs/node/pull/8837)
  * Multiple contexts, as created by the `vm` module, are supported now.
    [#14465](https://github.com/nodejs/node/pull/14465)

* **N-API**
  * New APIs for creating number values have been introduced.
    [#14573](https://github.com/nodejs/node/pull/14573)

* **Stream**
  * For `Duplex` streams, the high water mark option can now be set
    independently for the readable and the writable side.
    [#14636](https://github.com/nodejs/node/pull/14636)

* **Util**
  * `util.format` now supports the `%o` and `%O` specifiers for printing
    objects.
    [#14558](https://github.com/nodejs/node/pull/14558)

### Commits

* [[`a6539ece2c`](https://github.com/nodejs/node/commit/a6539ece2c)] - **assert**: optimize code path for deepEqual Maps (Ruben Bridgewater) [#14501](https://github.com/nodejs/node/pull/14501)
* [[`2716b626b0`](https://github.com/nodejs/node/commit/2716b626b0)] - **async_hooks**: CHECK that resource is not empty (Anna Henningsen) [#14694](https://github.com/nodejs/node/pull/14694)
* [[`b3c1c6ff7f`](https://github.com/nodejs/node/commit/b3c1c6ff7f)] - **benchmark**: fix and extend assert benchmarks (Ruben Bridgewater) [#14147](https://github.com/nodejs/node/pull/14147)
* [[`139b08863e`](https://github.com/nodejs/node/commit/139b08863e)] - **benchmark**: Correct constructor for freelist (Gareth Ellis) [#14627](https://github.com/nodejs/node/pull/14627)
* [[`574cc379b9`](https://github.com/nodejs/node/commit/574cc379b9)] - **benchmark**: remove unused parameters (nishijayaraj) [#14640](https://github.com/nodejs/node/pull/14640)
* [[`fef2aa7e27`](https://github.com/nodejs/node/commit/fef2aa7e27)] - **(SEMVER-MINOR)** **deps**: add nghttp2 dependency (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`2d806f4f71`](https://github.com/nodejs/node/commit/2d806f4f71)] - **deps**: cherry-pick f19b889 from V8 upstream (Alexey Kozyatinskiy) [#14465](https://github.com/nodejs/node/pull/14465)
* [[`dd521d0a28`](https://github.com/nodejs/node/commit/dd521d0a28)] - **deps,tools**: add missing nghttp2 license (Anna Henningsen) [#14806](https://github.com/nodejs/node/pull/14806)
* [[`621c03acfe`](https://github.com/nodejs/node/commit/621c03acfe)] - **doc**: delint (Refael Ackermann) [#14707](https://github.com/nodejs/node/pull/14707)
* [[`230cb55574`](https://github.com/nodejs/node/commit/230cb55574)] - **doc**: fix header level typo (Refael Ackermann) [#14707](https://github.com/nodejs/node/pull/14707)
* [[`af85b6e058`](https://github.com/nodejs/node/commit/af85b6e058)] - **doc**: fix http2 sample code for http2.md (Keita Akutsu) [#14667](https://github.com/nodejs/node/pull/14667)
* [[`1e7ddb200f`](https://github.com/nodejs/node/commit/1e7ddb200f)] - **doc**: explain browser support of http/2 without SSL (Gil Tayar) [#14670](https://github.com/nodejs/node/pull/14670)
* [[`be716d00cc`](https://github.com/nodejs/node/commit/be716d00cc)] - **(SEMVER-MINOR)** **doc**: include http2.md in all.md (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`9e51802f53`](https://github.com/nodejs/node/commit/9e51802f53)] - **doc**: add missing `changes:` metadata for util (Anna Henningsen) [#14810](https://github.com/nodejs/node/pull/14810)
* [[`4811fea553`](https://github.com/nodejs/node/commit/4811fea553)] - **doc**: add missing `changes:` metadata for streams (Anna Henningsen) [#14810](https://github.com/nodejs/node/pull/14810)
* [[`20fb69063a`](https://github.com/nodejs/node/commit/20fb69063a)] - **doc**: fix docs style in util.md (Daijiro Wachi) [#14711](https://github.com/nodejs/node/pull/14711)
* [[`0de63e6888`](https://github.com/nodejs/node/commit/0de63e6888)] - **doc**: fix docs style in intl.md (Daijiro Wachi) [#14711](https://github.com/nodejs/node/pull/14711)
* [[`ee2ae0f30b`](https://github.com/nodejs/node/commit/ee2ae0f30b)] - **doc**: expanded description of buffer.slice (Vishal Bisht) [#14720](https://github.com/nodejs/node/pull/14720)
* [[`9888bb1238`](https://github.com/nodejs/node/commit/9888bb1238)] - **doc**: improve fs.read() doc text (Rich Trott) [#14631](https://github.com/nodejs/node/pull/14631)
* [[`d604173a66`](https://github.com/nodejs/node/commit/d604173a66)] - **doc**: clarify the position argument for fs.read (dcharbonnier) [#14631](https://github.com/nodejs/node/pull/14631)
* [[`d3b072276b`](https://github.com/nodejs/node/commit/d3b072276b)] - **doc**: add docs for AssertionError (Mandeep Singh) [#14261](https://github.com/nodejs/node/pull/14261)
* [[`4e15a6b76a`](https://github.com/nodejs/node/commit/4e15a6b76a)] - **doc**: fix order of AtExit callbacks in addons.md (Daniel Bevenius) [#14048](https://github.com/nodejs/node/pull/14048)
* [[`e07dfffad0`](https://github.com/nodejs/node/commit/e07dfffad0)] - **doc**: remove undef NDEBUG from addons.md (Daniel Bevenius) [#14048](https://github.com/nodejs/node/pull/14048)
* [[`c5ee34e39b`](https://github.com/nodejs/node/commit/c5ee34e39b)] - **encoding**: rudimentary TextDecoder support w/o ICU (Timothy Gu) [#14489](https://github.com/nodejs/node/pull/14489)
* [[`e0001dc601`](https://github.com/nodejs/node/commit/e0001dc601)] - **(SEMVER-MINOR)** **http**: move utcDate to internal/http.js (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`1d40850338`](https://github.com/nodejs/node/commit/1d40850338)] - **http2**: fix \[kInspect\]() output for Http2Stream (Evan Lucas) [#14753](https://github.com/nodejs/node/pull/14753)
* [[`c5740f9111`](https://github.com/nodejs/node/commit/c5740f9111)] - **http2**: name padding buffer fields (Anna Henningsen) [#14744](https://github.com/nodejs/node/pull/14744)
* [[`8a0d101adf`](https://github.com/nodejs/node/commit/8a0d101adf)] - **http2**: use per-environment buffers (Anna Henningsen) [#14744](https://github.com/nodejs/node/pull/14744)
* [[`92c37fe5fd`](https://github.com/nodejs/node/commit/92c37fe5fd)] - **http2**: improve perf of passing headers to C++ (Anna Henningsen) [#14723](https://github.com/nodejs/node/pull/14723)
* [[`47bf705f75`](https://github.com/nodejs/node/commit/47bf705f75)] - **http2**: rename some nghttp2 stream flags (Kelvin Jin) [#14637](https://github.com/nodejs/node/pull/14637)
* [[`723d1af5e7`](https://github.com/nodejs/node/commit/723d1af5e7)] - **(SEMVER-MINOR)** **http2**: fix flakiness in timeout (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`6a30448bac`](https://github.com/nodejs/node/commit/6a30448bac)] - **(SEMVER-MINOR)** **http2**: fix linting after rebase (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`efd929e402`](https://github.com/nodejs/node/commit/efd929e402)] - **(SEMVER-MINOR)** **http2**: fix compilation error after V8 update (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`f46c50b3e2`](https://github.com/nodejs/node/commit/f46c50b3e2)] - **(SEMVER-MINOR)** **http2**: add some doc detail for invalid header chars (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`b43caf92c0`](https://github.com/nodejs/node/commit/b43caf92c0)] - **(SEMVER-MINOR)** **http2**: fix documentation errors (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`33b03b2ab2`](https://github.com/nodejs/node/commit/33b03b2ab2)] - **(SEMVER-MINOR)** **http2**: minor cleanup (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`174ab6fda0`](https://github.com/nodejs/node/commit/174ab6fda0)] - **(SEMVER-MINOR)** **http2**: use static allocated arrays (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`9a4be4adc4`](https://github.com/nodejs/node/commit/9a4be4adc4)] - **(SEMVER-MINOR)** **http2**: get trailers working with the compat api (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`3e5b07a8fb`](https://github.com/nodejs/node/commit/3e5b07a8fb)] - **(SEMVER-MINOR)** **http2**: refactor trailers API (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`26e1f8e01c`](https://github.com/nodejs/node/commit/26e1f8e01c)] - **(SEMVER-MINOR)** **http2**: address initial pr feedback (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`7824fa0b40`](https://github.com/nodejs/node/commit/7824fa0b40)] - **(SEMVER-MINOR)** **http2**: make writeHead behave like HTTP/1. (Matteo Collina) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`b778838337`](https://github.com/nodejs/node/commit/b778838337)] - **(SEMVER-MINOR)** **http2**: doc and fixes to the Compatibility API (Matteo Collina) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`8f3bbd9b68`](https://github.com/nodejs/node/commit/8f3bbd9b68)] - **(SEMVER-MINOR)** **http2**: add range support for respondWith{File|FD} (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`61696f1215`](https://github.com/nodejs/node/commit/61696f1215)] - **(SEMVER-MINOR)** **http2**: fix socketOnTimeout and a segfault (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`2620769e7f`](https://github.com/nodejs/node/commit/2620769e7f)] - **(SEMVER-MINOR)** **http2**: refinement and test for socketError (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`cd0f4c6652`](https://github.com/nodejs/node/commit/cd0f4c6652)] - **(SEMVER-MINOR)** **http2**: fix abort when client.destroy inside end event (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`e8cc193bcc`](https://github.com/nodejs/node/commit/e8cc193bcc)] - **(SEMVER-MINOR)** **http2**: fix documentation nits (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`a49146e446`](https://github.com/nodejs/node/commit/a49146e446)] - **(SEMVER-MINOR)** **http2**: remove redundant return in test (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`3eb61b00de`](https://github.com/nodejs/node/commit/3eb61b00de)] - **(SEMVER-MINOR)** **http2**: add tests and benchmarks (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`9623ee0f99`](https://github.com/nodejs/node/commit/9623ee0f99)] - **(SEMVER-MINOR)** **http2**: introducing HTTP/2 (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`029567a460`](https://github.com/nodejs/node/commit/029567a460)] - **inspector**: support extra contexts (Eugene Ostroukhov) [#14465](https://github.com/nodejs/node/pull/14465)
* [[`d89f9f82b0`](https://github.com/nodejs/node/commit/d89f9f82b0)] - **(SEMVER-MINOR)** **inspector**: allow require in Runtime.evaluate (Jan Krems) [#8837](https://github.com/nodejs/node/pull/8837)
* [[`ac1b81ad75`](https://github.com/nodejs/node/commit/ac1b81ad75)] - **lib**: move deprecationWarned var (Daniel Bevenius) [#14769](https://github.com/nodejs/node/pull/14769)
* [[`8433b1ad37`](https://github.com/nodejs/node/commit/8433b1ad37)] - **lib**: use Timer.now() in readline module (Rich Trott) [#14681](https://github.com/nodejs/node/pull/14681)
* [[`917ace283f`](https://github.com/nodejs/node/commit/917ace283f)] - **(SEMVER-MINOR)** **n-api**: add napi_get_node_version (Anna Henningsen) [#14696](https://github.com/nodejs/node/pull/14696)
* [[`5e2cce59ef`](https://github.com/nodejs/node/commit/5e2cce59ef)] - **(SEMVER-MINOR)** **n-api**: optimize number API performance (Jason Ginchereau) [#14573](https://github.com/nodejs/node/pull/14573)
* [[`c94f346b93`](https://github.com/nodejs/node/commit/c94f346b93)] - **net**: use rest parameters instead of arguments (Tobias Nießen) [#13472](https://github.com/nodejs/node/pull/13472)
* [[`1c00875747`](https://github.com/nodejs/node/commit/1c00875747)] - **repl**: include folder extensions in autocomplete (Teddy Katz) [#14727](https://github.com/nodejs/node/pull/14727)
* [[`59d1d56da6`](https://github.com/nodejs/node/commit/59d1d56da6)] - **src**: remove unused http2_socket_buffer from env (Anna Henningsen) [#14740](https://github.com/nodejs/node/pull/14740)
* [[`268a1ff3f1`](https://github.com/nodejs/node/commit/268a1ff3f1)] - **src**: mention that node options are space-separated (Gabriel Schulhof) [#14709](https://github.com/nodejs/node/pull/14709)
* [[`9237ef868e`](https://github.com/nodejs/node/commit/9237ef868e)] - **src**: avoid creating local data variable (Daniel Bevenius) [#14732](https://github.com/nodejs/node/pull/14732)
* [[`f83827d64b`](https://github.com/nodejs/node/commit/f83827d64b)] - **src**: use local isolate instead of args.GetIsolate (Daniel Bevenius) [#14768](https://github.com/nodejs/node/pull/14768)
* [[`d7d22ead2b`](https://github.com/nodejs/node/commit/d7d22ead2b)] - **src**: add comments for cares library init refcount (Anna Henningsen) [#14743](https://github.com/nodejs/node/pull/14743)
* [[`b87fae927d`](https://github.com/nodejs/node/commit/b87fae927d)] - **src**: remove duplicate loop (Anna Henningsen) [#14750](https://github.com/nodejs/node/pull/14750)
* [[`033773c17b`](https://github.com/nodejs/node/commit/033773c17b)] - **src**: add overlooked handle to cleanup (Anna Henningsen) [#14749](https://github.com/nodejs/node/pull/14749)
* [[`dd6444d401`](https://github.com/nodejs/node/commit/dd6444d401)] - **src,http2**: DRY header/trailer handling code up (Anna Henningsen) [#14688](https://github.com/nodejs/node/pull/14688)
* [[`ef8ac7b5ac`](https://github.com/nodejs/node/commit/ef8ac7b5ac)] - **(SEMVER-MINOR)** **stream**: support readable/writableHWM for Duplex (Guy Margalit) [#14636](https://github.com/nodejs/node/pull/14636)
* [[`6d9f94f93f`](https://github.com/nodejs/node/commit/6d9f94f93f)] - **test**: cover all HTTP methods that parser supports (Oky Antoro) [#14773](https://github.com/nodejs/node/pull/14773)
* [[`e4854fccfc`](https://github.com/nodejs/node/commit/e4854fccfc)] - **test**: use regular expressions in throw assertions (Vincent Xue) [#14318](https://github.com/nodejs/node/pull/14318)
* [[`66788fc4d0`](https://github.com/nodejs/node/commit/66788fc4d0)] - **test**: increase http2 coverage (Michael Albert) [#14701](https://github.com/nodejs/node/pull/14701)
* [[`dbb9c370d4`](https://github.com/nodejs/node/commit/dbb9c370d4)] - **test**: add crypto check to http2 tests (Daniel Bevenius) [#14657](https://github.com/nodejs/node/pull/14657)
* [[`97f622b99e`](https://github.com/nodejs/node/commit/97f622b99e)] - **(SEMVER-MINOR)** **test**: fix flaky test-http2-client-unescaped-path on osx (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`9d752d5282`](https://github.com/nodejs/node/commit/9d752d5282)] - **(SEMVER-MINOR)** **test**: fix flakiness in test-http2-client-upload (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`82c63a55ea`](https://github.com/nodejs/node/commit/82c63a55ea)] - **test**: add test-benchmark-arrays (Rich Trott) [#14728](https://github.com/nodejs/node/pull/14728)
* [[`0eab77c86f`](https://github.com/nodejs/node/commit/0eab77c86f)] - **test**: allow inspector to reopen with same port (Gibson Fahnestock) [#14320](https://github.com/nodejs/node/pull/14320)
* [[`9bbbf12827`](https://github.com/nodejs/node/commit/9bbbf12827)] - **test**: remove redundant `using` in cctest (XadillaX) [#14739](https://github.com/nodejs/node/pull/14739)
* [[`7eb9f6f6e4`](https://github.com/nodejs/node/commit/7eb9f6f6e4)] - **test**: make totalLen snake case (Daniel Bevenius) [#14765](https://github.com/nodejs/node/pull/14765)
* [[`977e22857a`](https://github.com/nodejs/node/commit/977e22857a)] - **test**: make test-tls-connect checks more strict (Rich Trott) [#14695](https://github.com/nodejs/node/pull/14695)
* [[`a781bb4508`](https://github.com/nodejs/node/commit/a781bb4508)] - ***Revert*** "**test**: disable MultipleEnvironmentsPerIsolate" (Anna Henningsen) [#14749](https://github.com/nodejs/node/pull/14749)
* [[`8ff2a5c338`](https://github.com/nodejs/node/commit/8ff2a5c338)] - ***Revert*** "**test**: add DISABLED_ prefix to commented out test" (Anna Henningsen) [#14749](https://github.com/nodejs/node/pull/14749)
* [[`0bc3124c80`](https://github.com/nodejs/node/commit/0bc3124c80)] - **test**: properly order freeing resources in cctest (Anna Henningsen) [#14749](https://github.com/nodejs/node/pull/14749)
* [[`3f1bb0a551`](https://github.com/nodejs/node/commit/3f1bb0a551)] - **test**: split out load-sensitive readline tests (Rich Trott) [#14681](https://github.com/nodejs/node/pull/14681)
* [[`5d99d7dff2`](https://github.com/nodejs/node/commit/5d99d7dff2)] - **test**: add block scoping to test-readline-interface (Rich Trott) [#14615](https://github.com/nodejs/node/pull/14615)
* [[`58742729da`](https://github.com/nodejs/node/commit/58742729da)] - **test**: set module loading error for aix (Prakash Palaniappan) [#14511](https://github.com/nodejs/node/pull/14511)
* [[`06ba2dae30`](https://github.com/nodejs/node/commit/06ba2dae30)] - **test**: fix conversion of microseconds in test (Nick Stanish) [#14706](https://github.com/nodejs/node/pull/14706)
* [[`30837b3b90`](https://github.com/nodejs/node/commit/30837b3b90)] - **test**: improve check in test-os (Rich Trott) [#14655](https://github.com/nodejs/node/pull/14655)
* [[`55aba6aee7`](https://github.com/nodejs/node/commit/55aba6aee7)] - **test**: replace indexOf with includes (Miguel Angel Asencio Hurtado) [#14630](https://github.com/nodejs/node/pull/14630)
* [[`935d34bd6b`](https://github.com/nodejs/node/commit/935d34bd6b)] - **test**: fix test-readline-interface (Azard) [#14677](https://github.com/nodejs/node/pull/14677)
* [[`2ee3320f2c`](https://github.com/nodejs/node/commit/2ee3320f2c)] - **test**: improve multiple timers tests (James M Snell) [#14616](https://github.com/nodejs/node/pull/14616)
* [[`71f2e76353`](https://github.com/nodejs/node/commit/71f2e76353)] - **test**: use ciphers supported by shared OpenSSL (Jérémy Lal) [#14566](https://github.com/nodejs/node/pull/14566)
* [[`f73f659186`](https://github.com/nodejs/node/commit/f73f659186)] - **test**: mitigate RegEx exceeding 80 chars (Aditya Anand) [#14607](https://github.com/nodejs/node/pull/14607)
* [[`96147c980c`](https://github.com/nodejs/node/commit/96147c980c)] - **test**: read proper inspector message size (Bartosz Sosnowski) [#14596](https://github.com/nodejs/node/pull/14596)
* [[`e84c9d7176`](https://github.com/nodejs/node/commit/e84c9d7176)] - **(SEMVER-MINOR)** **tls**: add tlsSocket.disableRenegotiation() (James M Snell) [#14239](https://github.com/nodejs/node/pull/14239)
* [[`a0e05e884e`](https://github.com/nodejs/node/commit/a0e05e884e)] - **tools**: fix tools/addon-verify.js (Daniel Bevenius) [#14048](https://github.com/nodejs/node/pull/14048)
* [[`116841056a`](https://github.com/nodejs/node/commit/116841056a)] - **util**: improve util.inspect performance (Ruben Bridgewater) [#14492](https://github.com/nodejs/node/pull/14492)
* [[`7203924fea`](https://github.com/nodejs/node/commit/7203924fea)] - **(SEMVER-MINOR)** **util**: implement %o and %O as formatting specifiers (Greg Alexander) [#14558](https://github.com/nodejs/node/pull/14558)

<a id="8.3.0"></a>
## 2017-08-09, Version 8.3.0 (Current), @addaleax

### Notable changes

#### V8 6.0

The V8 engine has been upgraded to version 6.0, which has a significantly
changed performance profile.
[#14574](https://github.com/nodejs/node/pull/14574)

More detailed information on performance differences can be found at
https://medium.com/the-node-js-collection/get-ready-a-new-v8-is-coming-node-js-performance-is-changing-46a63d6da4de

#### Other notable changes

* **DNS**
  * Independent DNS resolver instances are supported now, with support for
    cancelling the corresponding requests.
    [#14518](https://github.com/nodejs/node/pull/14518)

* **N-API**
  * Multiple N-API functions for error handling have been changed to support
    assigning error codes.
    [#13988](https://github.com/nodejs/node/pull/13988)

* **REPL**
  * Autocompletion support for `require()` has been improved.
    [#14409](https://github.com/nodejs/node/pull/14409)

* **Utilities**
  * The WHATWG Encoding Standard (`TextDecoder` and `TextEncoder`) has
    been implemented as an experimental feature.
    [#13644](https://github.com/nodejs/node/pull/13644)

* **Added new collaborators**
  * [XadillaX](https://github.com/XadillaX) – Khaidi Chu
  * [gabrielschulhof](https://github.com/gabrielschulhof) – Gabriel Schulhof

### Commits

* [[`e2356e72e7`](https://github.com/nodejs/node/commit/e2356e72e7)] - **assert**: improve deepEqual Set and Map worst case (Ruben Bridgewater) [#14258](https://github.com/nodejs/node/pull/14258)
* [[`9252b8c057`](https://github.com/nodejs/node/commit/9252b8c057)] - **assert**: refactor to reduce unecessary code paths (Ruben Bridgewater) [#13973](https://github.com/nodejs/node/pull/13973)
* [[`89586f6684`](https://github.com/nodejs/node/commit/89586f6684)] - **assert**: fix incorrect use of ERR_INVALID_ARG_TYPE (Tobias Nießen) [#14011](https://github.com/nodejs/node/pull/14011)
* [[`26785a23bb`](https://github.com/nodejs/node/commit/26785a23bb)] - **assert**: refactor the code (Ruben Bridgewater) [#13862](https://github.com/nodejs/node/pull/13862)
* [[`0cf1e22448`](https://github.com/nodejs/node/commit/0cf1e22448)] - **benchmark**: remove unused parameters (Matthew Alsup) [#14526](https://github.com/nodejs/node/pull/14526)
* [[`9b104b4ea8`](https://github.com/nodejs/node/commit/9b104b4ea8)] - **benchmark**: add assert map and set benchmarks (Ruben Bridgewater) [#14258](https://github.com/nodejs/node/pull/14258)
* [[`2c364ab291`](https://github.com/nodejs/node/commit/2c364ab291)] - **buffer**: remove a wrongly added attribute specifier (Jiajie Hu) [#14466](https://github.com/nodejs/node/pull/14466)
* [[`c0f0c38535`](https://github.com/nodejs/node/commit/c0f0c38535)] - **build**: enable C++ linting for src/*/* (jeyanthinath) [#14497](https://github.com/nodejs/node/pull/14497)
* [[`87e108059b`](https://github.com/nodejs/node/commit/87e108059b)] - **build**: fix build without icu (Jimmy Thomson) [#14533](https://github.com/nodejs/node/pull/14533)
* [[`0ebb4dff17`](https://github.com/nodejs/node/commit/0ebb4dff17)] - **build**: codesign tarball binary on macOS (Evan Lucas) [#14179](https://github.com/nodejs/node/pull/14179)
* [[`7f5bcbd2e9`](https://github.com/nodejs/node/commit/7f5bcbd2e9)] - **build,test**: run v8 tests on windows (Kunal Pathak) [#13992](https://github.com/nodejs/node/pull/13992)
* [[`5ab4471d72`](https://github.com/nodejs/node/commit/5ab4471d72)] - **build,tools**: do not force codesign prefix (Evan Lucas) [#14179](https://github.com/nodejs/node/pull/14179)
* [[`7b96944254`](https://github.com/nodejs/node/commit/7b96944254)] - **build,win**: fix python detection script (Jason Ginchereau) [#14546](https://github.com/nodejs/node/pull/14546)
* [[`1f16c43e80`](https://github.com/nodejs/node/commit/1f16c43e80)] - **child_process**: fix handle passing w large payloads (Anna Henningsen) [#14588](https://github.com/nodejs/node/pull/14588)
* [[`9c1199e88f`](https://github.com/nodejs/node/commit/9c1199e88f)] - **(SEMVER-MINOR)** **console**: add console.count() and console.clear() (James M Snell) [#12678](https://github.com/nodejs/node/pull/12678)
* [[`255b9bfa8a`](https://github.com/nodejs/node/commit/255b9bfa8a)] - **console,test**: make message test more accurate (Anna Henningsen) [#14580](https://github.com/nodejs/node/pull/14580)
* [[`51c1afafa6`](https://github.com/nodejs/node/commit/51c1afafa6)] - **crypto**: change segmentation faults to CHECKs (Tobias Nießen) [#14548](https://github.com/nodejs/node/pull/14548)
* [[`e2b306c831`](https://github.com/nodejs/node/commit/e2b306c831)] - **(SEMVER-MINOR)** **deps**: backport rehash strings after deserialization (Yang Guo) [#14004](https://github.com/nodejs/node/pull/14004)
* [[`2dbf95d5ee`](https://github.com/nodejs/node/commit/2dbf95d5ee)] - **(SEMVER-MINOR)** **deps**: backport c0f1ff2 from upstream V8 (Michaël Zasso) [#14004](https://github.com/nodejs/node/pull/14004)
* [[`efd297a5c9`](https://github.com/nodejs/node/commit/efd297a5c9)] - **(SEMVER-MINOR)** **deps**: fix addons compilation with VS2013 (Bartosz Sosnowski) [#14004](https://github.com/nodejs/node/pull/14004)
* [[`160e2f03d2`](https://github.com/nodejs/node/commit/160e2f03d2)] - **(SEMVER-MINOR)** **deps**: limit regress/regress-crbug-514081 v8 test (Michael Dawson) [#14004](https://github.com/nodejs/node/pull/14004)
* [[`44ad55d493`](https://github.com/nodejs/node/commit/44ad55d493)] - **(SEMVER-MINOR)** **deps**: update V8 to 6.0.286.52 (Myles Borins) [#14574](https://github.com/nodejs/node/pull/14574)
* [[`d9273ed5ed`](https://github.com/nodejs/node/commit/d9273ed5ed)] - **deps**: cherry-pick 18ea996 from c-ares upstream (Anna Henningsen) [#13883](https://github.com/nodejs/node/pull/13883)
* [[`32b30d519e`](https://github.com/nodejs/node/commit/32b30d519e)] - **(SEMVER-MINOR)** **dns**: name generated functions (Anna Henningsen) [#14518](https://github.com/nodejs/node/pull/14518)
* [[`0982810208`](https://github.com/nodejs/node/commit/0982810208)] - **(SEMVER-MINOR)** **dns**: add channel.cancel() (Anna Henningsen) [#14518](https://github.com/nodejs/node/pull/14518)
* [[`69e41dc5da`](https://github.com/nodejs/node/commit/69e41dc5da)] - **(SEMVER-MINOR)** **dns**: enable usage of independent cares resolvers (Anna Henningsen) [#14518](https://github.com/nodejs/node/pull/14518)
* [[`ad901ed272`](https://github.com/nodejs/node/commit/ad901ed272)] - **doc**: add gabrielschulhof to collaborators (Gabriel Schulhof) [#14692](https://github.com/nodejs/node/pull/14692)
* [[`dd586c6bd4`](https://github.com/nodejs/node/commit/dd586c6bd4)] - **doc**: erase unneeded eslint-plugin-markdown comment (Vse Mozhet Byt) [#14598](https://github.com/nodejs/node/pull/14598)
* [[`8c80e91a2e`](https://github.com/nodejs/node/commit/8c80e91a2e)] - **doc**: fix typo in writing-and-running-benchmarks.md (Yuta Hiroto) [#14600](https://github.com/nodejs/node/pull/14600)
* [[`91b7843aeb`](https://github.com/nodejs/node/commit/91b7843aeb)] - **doc**: add entry for subprocess.killed property (Rich Trott) [#14578](https://github.com/nodejs/node/pull/14578)
* [[`342f4cccb5`](https://github.com/nodejs/node/commit/342f4cccb5)] - **doc**: change `child` to `subprocess` (Rich Trott) [#14578](https://github.com/nodejs/node/pull/14578)
* [[`b6bd3cf00f`](https://github.com/nodejs/node/commit/b6bd3cf00f)] - **doc**: cross-link util.TextDecoder and intl.md (Vse Mozhet Byt) [#14486](https://github.com/nodejs/node/pull/14486)
* [[`fffd8f5335`](https://github.com/nodejs/node/commit/fffd8f5335)] - **doc**: document napi_finalize() signature (cjihrig) [#14230](https://github.com/nodejs/node/pull/14230)
* [[`92b0555965`](https://github.com/nodejs/node/commit/92b0555965)] - **doc**: various small revisions in url (Timothy Gu) [#14478](https://github.com/nodejs/node/pull/14478)
* [[`9dd9760951`](https://github.com/nodejs/node/commit/9dd9760951)] - **doc**: update url.origin IDNA behavior (Timothy Gu) [#14478](https://github.com/nodejs/node/pull/14478)
* [[`4e2493a20d`](https://github.com/nodejs/node/commit/4e2493a20d)] - **doc**: fix minor typos in net.md (Daiki Arai) [#14502](https://github.com/nodejs/node/pull/14502)
* [[`e9088f92d8`](https://github.com/nodejs/node/commit/e9088f92d8)] - **doc**: fix verify in crypto.md (Ruslan Iusupov) [#14469](https://github.com/nodejs/node/pull/14469)
* [[`8a9de1b3c5`](https://github.com/nodejs/node/commit/8a9de1b3c5)] - **doc**: fix typo in using-internal-errors.md (Anton Paras) [#14429](https://github.com/nodejs/node/pull/14429)
* [[`ab9bc81b0e`](https://github.com/nodejs/node/commit/ab9bc81b0e)] - **doc**: add docs for module.paths (atever) [#14435](https://github.com/nodejs/node/pull/14435)
* [[`bdcd496c98`](https://github.com/nodejs/node/commit/bdcd496c98)] - **doc**: update experimental status to reflect use (James M Snell) [#12723](https://github.com/nodejs/node/pull/12723)
* [[`6c6da38518`](https://github.com/nodejs/node/commit/6c6da38518)] - **doc**: fix some links (Vse Mozhet Byt) [#14400](https://github.com/nodejs/node/pull/14400)
* [[`83c8e5c517`](https://github.com/nodejs/node/commit/83c8e5c517)] - **doc**: describe labelling process for backports (Anna Henningsen) [#12431](https://github.com/nodejs/node/pull/12431)
* [[`592787ef4d`](https://github.com/nodejs/node/commit/592787ef4d)] - **doc**: error message are still major (Refael Ackermann) [#14375](https://github.com/nodejs/node/pull/14375)
* [[`f1b09c0a44`](https://github.com/nodejs/node/commit/f1b09c0a44)] - **doc**: fix typo in stream.md (Marc Hernández Cabot) [#14364](https://github.com/nodejs/node/pull/14364)
* [[`4be373bc4b`](https://github.com/nodejs/node/commit/4be373bc4b)] - **doc**: fixes default shell in child_process.md (Henry) [#14203](https://github.com/nodejs/node/pull/14203)
* [[`b12924d894`](https://github.com/nodejs/node/commit/b12924d894)] - **doc**: add XadillaX to collaborators (XadillaX) [#14388](https://github.com/nodejs/node/pull/14388)
* [[`dc0a26f254`](https://github.com/nodejs/node/commit/dc0a26f254)] - **doc**: replace dead link in v8 module (Devin Boyer) [#14372](https://github.com/nodejs/node/pull/14372)
* [[`d2121ab768`](https://github.com/nodejs/node/commit/d2121ab768)] - **doc**: fix minor typo in cluster.md (Lance Ball) [#14353](https://github.com/nodejs/node/pull/14353)
* [[`eb023ef7df`](https://github.com/nodejs/node/commit/eb023ef7df)] - **doc, lib, test**: do not re-require needlessly (Vse Mozhet Byt) [#14244](https://github.com/nodejs/node/pull/14244)
* [[`cfed48e81c`](https://github.com/nodejs/node/commit/cfed48e81c)] - **doc, url**: add changelog metadata for url.format (Timothy Gu) [#14543](https://github.com/nodejs/node/pull/14543)
* [[`78f0c2aa75`](https://github.com/nodejs/node/commit/78f0c2aa75)] - **doc,assert**: document stackStartFunction in fail (Ruben Bridgewater) [#13862](https://github.com/nodejs/node/pull/13862)
* [[`53ad91c3b1`](https://github.com/nodejs/node/commit/53ad91c3b1)] - **doc,stream**: \_transform happens one at a time (Matteo Collina) [#14321](https://github.com/nodejs/node/pull/14321)
* [[`f6a03439d8`](https://github.com/nodejs/node/commit/f6a03439d8)] - **docs**: add note about fs.rmdir() (Oleksandr Kushchak) [#14323](https://github.com/nodejs/node/pull/14323)
* [[`142ce5ce2c`](https://github.com/nodejs/node/commit/142ce5ce2c)] - **errors**: order internal errors list alphabetically (Anna Henningsen) [#14453](https://github.com/nodejs/node/pull/14453)
* [[`50447e837b`](https://github.com/nodejs/node/commit/50447e837b)] - **http**: reset stream to unconsumed in `unconsume()` (Anna Henningsen) [#14410](https://github.com/nodejs/node/pull/14410)
* [[`751e87338f`](https://github.com/nodejs/node/commit/751e87338f)] - **http**: check for handle before running asyncReset() (Trevor Norris) [#14419](https://github.com/nodejs/node/pull/14419)
* [[`deea68cbb2`](https://github.com/nodejs/node/commit/deea68cbb2)] - **inspector**: fix console with inspector disabled (Timothy Gu) [#14489](https://github.com/nodejs/node/pull/14489)
* [[`71cb1cdf69`](https://github.com/nodejs/node/commit/71cb1cdf69)] - **inspector**: implement V8Inspector timer (Eugene Ostroukhov) [#14346](https://github.com/nodejs/node/pull/14346)
* [[`4836f3b9b9`](https://github.com/nodejs/node/commit/4836f3b9b9)] - **inspector**: send messages after the Node is done (Eugene Ostroukhov) [#14463](https://github.com/nodejs/node/pull/14463)
* [[`9e5a08884a`](https://github.com/nodejs/node/commit/9e5a08884a)] - **lib**: adjust indentation for impending lint change (Rich Trott) [#14403](https://github.com/nodejs/node/pull/14403)
* [[`a7b3e06e9b`](https://github.com/nodejs/node/commit/a7b3e06e9b)] - **lib**: modify destructuring for indentation (Rich Trott) [#14417](https://github.com/nodejs/node/pull/14417)
* [[`28f0693796`](https://github.com/nodejs/node/commit/28f0693796)] - **lib**: include cached modules in module.children (Ben Noordhuis) [#14132](https://github.com/nodejs/node/pull/14132)
* [[`19a0e06317`](https://github.com/nodejs/node/commit/19a0e06317)] - **linkedlist**: correct grammar in comments (alexbostock) [#14546](https://github.com/nodejs/node/pull/14546)
* [[`60e0f2bb0d`](https://github.com/nodejs/node/commit/60e0f2bb0d)] - **(SEMVER-MINOR)** **n-api**: add support for DataView (Shivanth MP) [#14382](https://github.com/nodejs/node/pull/14382)
* [[`b849b3d223`](https://github.com/nodejs/node/commit/b849b3d223)] - **n-api**: re-use napi_env between modules (Gabriel Schulhof) [#14217](https://github.com/nodejs/node/pull/14217)
* [[`6078dea35d`](https://github.com/nodejs/node/commit/6078dea35d)] - **n-api**: directly create Local from Persistent (Kyle Farnung) [#14211](https://github.com/nodejs/node/pull/14211)
* [[`f2efdc880f`](https://github.com/nodejs/node/commit/f2efdc880f)] - **(SEMVER-MINOR)** **n-api**: add code parameter to error helpers (Michael Dawson) [#13988](https://github.com/nodejs/node/pull/13988)
* [[`fa134dd60c`](https://github.com/nodejs/node/commit/fa134dd60c)] - **n-api**: add fast paths for integer getters (Anna Henningsen) [#14393](https://github.com/nodejs/node/pull/14393)
* [[`58446912a6`](https://github.com/nodejs/node/commit/58446912a6)] - **net**: fix bytesWritten during writev (Brendan Ashworth) [#14236](https://github.com/nodejs/node/pull/14236)
* [[`b41ae9847e`](https://github.com/nodejs/node/commit/b41ae9847e)] - **path**: fix win32 volume-relative paths (Timothy Gu) [#14440](https://github.com/nodejs/node/pull/14440)
* [[`509039fcaf`](https://github.com/nodejs/node/commit/509039fcaf)] - **path**: remove unnecessary string copies (Tobias Nießen) [#14438](https://github.com/nodejs/node/pull/14438)
* [[`e813cfaead`](https://github.com/nodejs/node/commit/e813cfaead)] - **querystring**: avoid indexOf when parsing (Matteo Collina) [#14703](https://github.com/nodejs/node/pull/14703)
* [[`37e55bf559`](https://github.com/nodejs/node/commit/37e55bf559)] - **readline**: remove max limit of crlfDelay (Azard) [#13497](https://github.com/nodejs/node/pull/13497)
* [[`e54f75b831`](https://github.com/nodejs/node/commit/e54f75b831)] - **readline**: remove the caching variable (Lyall Sun) [#14275](https://github.com/nodejs/node/pull/14275)
* [[`1a5927fc27`](https://github.com/nodejs/node/commit/1a5927fc27)] - **repl**: do not consider `...` as a REPL command (Shivanth MP) [#14467](https://github.com/nodejs/node/pull/14467)
* [[`5a8862bfa3`](https://github.com/nodejs/node/commit/5a8862bfa3)] - **(SEMVER-MINOR)** **repl**: improve require() autocompletion (Alexey Orlenko) [#14409](https://github.com/nodejs/node/pull/14409)
* [[`34821f6400`](https://github.com/nodejs/node/commit/34821f6400)] - **repl**: don't terminate on null thrown (Benjamin Gruenbaum) [#14306](https://github.com/nodejs/node/pull/14306)
* [[`32ba8aea0b`](https://github.com/nodejs/node/commit/32ba8aea0b)] - **repl**: fix old history error handling (Ruben Bridgewater) [#13733](https://github.com/nodejs/node/pull/13733)
* [[`264e4345f8`](https://github.com/nodejs/node/commit/264e4345f8)] - **src**: reuse 'ondone' string in node_crypto.cc (Tobias Nießen) [#14587](https://github.com/nodejs/node/pull/14587)
* [[`6ae6469d4a`](https://github.com/nodejs/node/commit/6ae6469d4a)] - **src**: use existing strings over creating new ones (Anna Henningsen) [#14587](https://github.com/nodejs/node/pull/14587)
* [[`eb068a0526`](https://github.com/nodejs/node/commit/eb068a0526)] - **src**: remove unused Connection::ClearError() (Ben Noordhuis) [#14514](https://github.com/nodejs/node/pull/14514)
* [[`4b01d8cac3`](https://github.com/nodejs/node/commit/4b01d8cac3)] - **src**: replace assert with CHECK_LE in node_api.cc (Ben Noordhuis) [#14514](https://github.com/nodejs/node/pull/14514)
* [[`3c6b5e5fac`](https://github.com/nodejs/node/commit/3c6b5e5fac)] - **src**: properly manage timer in cares ChannelWrap (Anna Henningsen) [#14634](https://github.com/nodejs/node/pull/14634)
* [[`8c5cd1439e`](https://github.com/nodejs/node/commit/8c5cd1439e)] - **src**: avoid dereference without existence check (Timothy Gu) [#14591](https://github.com/nodejs/node/pull/14591)
* [[`8a3bc874fa`](https://github.com/nodejs/node/commit/8a3bc874fa)] - **src**: fix process.abort() interaction with V8 (Anna Henningsen) [#13985](https://github.com/nodejs/node/pull/13985)
* [[`997204a213`](https://github.com/nodejs/node/commit/997204a213)] - **(SEMVER-MINOR)** **src**: fix new V8 compiler warnings (Michaël Zasso) [#14004](https://github.com/nodejs/node/pull/14004)
* [[`fa3aa2e1f7`](https://github.com/nodejs/node/commit/fa3aa2e1f7)] - **src**: return MaybeLocal in AsyncWrap::MakeCallback (Tobias Nießen) [#14549](https://github.com/nodejs/node/pull/14549)
* [[`d90a5e0069`](https://github.com/nodejs/node/commit/d90a5e0069)] - **src**: replace deprecated ForceSet() method (Franziska Hinkelmann) [#14450](https://github.com/nodejs/node/pull/14450)
* [[`eb7faf6734`](https://github.com/nodejs/node/commit/eb7faf6734)] - **src**: replace ASSERT with CHECK (Ben Noordhuis) [#14474](https://github.com/nodejs/node/pull/14474)
* [[`106a23bd27`](https://github.com/nodejs/node/commit/106a23bd27)] - **(SEMVER-MINOR)** **src,dns**: refactor cares_wrap to avoid global state (Anna Henningsen) [#14518](https://github.com/nodejs/node/pull/14518)
* [[`3c46ef4717`](https://github.com/nodejs/node/commit/3c46ef4717)] - **test**: explain sloppy mode for test-global (Rich Trott) [#14604](https://github.com/nodejs/node/pull/14604)
* [[`28b9c7a477`](https://github.com/nodejs/node/commit/28b9c7a477)] - **test**: fix test-readline-position w/o ICU (Timothy Gu) [#14489](https://github.com/nodejs/node/pull/14489)
* [[`636ba8caef`](https://github.com/nodejs/node/commit/636ba8caef)] - **test**: support odd value for kStringMaxLength (Michaël Zasso) [#14476](https://github.com/nodejs/node/pull/14476)
* [[`5094f2c299`](https://github.com/nodejs/node/commit/5094f2c299)] - **test**: refactor test-domain-abort-on-uncaught (Rich Trott) [#14541](https://github.com/nodejs/node/pull/14541)
* [[`b1fef05446`](https://github.com/nodejs/node/commit/b1fef05446)] - **test**: improvements to various http tests (James M Snell) [#14315](https://github.com/nodejs/node/pull/14315)
* [[`ce9e3cfe10`](https://github.com/nodejs/node/commit/ce9e3cfe10)] - **test**: refactor test/sequential/test-fs-watch.js (Rich Trott) [#14534](https://github.com/nodejs/node/pull/14534)
* [[`9f50db2450`](https://github.com/nodejs/node/commit/9f50db2450)] - **test**: refactor test-vm-new-script-new-context (Rich Trott) [#14536](https://github.com/nodejs/node/pull/14536)
* [[`f40b9062fc`](https://github.com/nodejs/node/commit/f40b9062fc)] - **test**: add check on an addon that does not register (Ezequiel Garcia) [#13954](https://github.com/nodejs/node/pull/13954)
* [[`ddd97fe15c`](https://github.com/nodejs/node/commit/ddd97fe15c)] - **test**: fix error when foo in path to git clone (Matt Woicik) [#14506](https://github.com/nodejs/node/pull/14506)
* [[`8fea17484d`](https://github.com/nodejs/node/commit/8fea17484d)] - **test**: add DISABLED_ prefix to commented out test (Daniel Bevenius) [#14317](https://github.com/nodejs/node/pull/14317)
* [[`7b6a77403c`](https://github.com/nodejs/node/commit/7b6a77403c)] - **test**: remove disabled tests directory (Rich Trott) [#14505](https://github.com/nodejs/node/pull/14505)
* [[`15b9aa1359`](https://github.com/nodejs/node/commit/15b9aa1359)] - **test**: improve error logging for inspector test (Rich Trott) [#14508](https://github.com/nodejs/node/pull/14508)
* [[`451e643cf2`](https://github.com/nodejs/node/commit/451e643cf2)] - **test**: remove --no-crankshaft (Myles Borins) [#14531](https://github.com/nodejs/node/pull/14531)
* [[`7c51240b1a`](https://github.com/nodejs/node/commit/7c51240b1a)] - **test**: adjust indentation for stricter linting (Rich Trott) [#14431](https://github.com/nodejs/node/pull/14431)
* [[`c704c02290`](https://github.com/nodejs/node/commit/c704c02290)] - **test**: increase coverage for path.parse (Tobias Nießen) [#14438](https://github.com/nodejs/node/pull/14438)
* [[`23cd934d71`](https://github.com/nodejs/node/commit/23cd934d71)] - **test**: refactor test-httpparser.response.js (erdun) [#14290](https://github.com/nodejs/node/pull/14290)
* [[`91b6ba1973`](https://github.com/nodejs/node/commit/91b6ba1973)] - **test**: refactor test-benchmark-timers (Rich Trott) [#14464](https://github.com/nodejs/node/pull/14464)
* [[`c2853893cf`](https://github.com/nodejs/node/commit/c2853893cf)] - **test**: refactor test-http-parser.js (Rich Trott) [#14431](https://github.com/nodejs/node/pull/14431)
* [[`4ff562f41e`](https://github.com/nodejs/node/commit/4ff562f41e)] - **test**: make flaky crypto test more deterministic (Ben Noordhuis) [#14451](https://github.com/nodejs/node/pull/14451)
* [[`100e862dfa`](https://github.com/nodejs/node/commit/100e862dfa)] - **test**: rename crypto test (Ben Noordhuis) [#14451](https://github.com/nodejs/node/pull/14451)
* [[`f8c2302a66`](https://github.com/nodejs/node/commit/f8c2302a66)] - **test**: use common.mustCall() instead of exit handle (笑斌) [#14262](https://github.com/nodejs/node/pull/14262)
* [[`0ff19b0c4c`](https://github.com/nodejs/node/commit/0ff19b0c4c)] - **test**: changed error message validator (Pratik Jain) [#14443](https://github.com/nodejs/node/pull/14443)
* [[`14f6a5a367`](https://github.com/nodejs/node/commit/14f6a5a367)] - **test**: fix flaky test-force-repl (Rich Trott) [#14439](https://github.com/nodejs/node/pull/14439)
* [[`5057c7a953`](https://github.com/nodejs/node/commit/5057c7a953)] - **test**: replace concatenation with template literal (rockcoder23) [#14270](https://github.com/nodejs/node/pull/14270)
* [[`6420a73f3e`](https://github.com/nodejs/node/commit/6420a73f3e)] - **test**: replace concatenation with template literal (Ching Hsu) [#14284](https://github.com/nodejs/node/pull/14284)
* [[`cd0fffd86a`](https://github.com/nodejs/node/commit/cd0fffd86a)] - **test**: convert table in test doc to markdown table (vixony) [#14291](https://github.com/nodejs/node/pull/14291)
* [[`1c6135f431`](https://github.com/nodejs/node/commit/1c6135f431)] - **test**: fix flaky http(s)-set-server-timeout tests (Rich Trott) [#14380](https://github.com/nodejs/node/pull/14380)
* [[`de3d73c88c`](https://github.com/nodejs/node/commit/de3d73c88c)] - **test**: replace CRLF by LF in a fixture (Vse Mozhet Byt) [#14437](https://github.com/nodejs/node/pull/14437)
* [[`aeb8d66eec`](https://github.com/nodejs/node/commit/aeb8d66eec)] - **test**: fix test-async-wrap-getasyncid flakyness (Julien Gilli) [#14329](https://github.com/nodejs/node/pull/14329)
* [[`3c50c592a5`](https://github.com/nodejs/node/commit/3c50c592a5)] - **test**: replace concatenation with template literals (笑斌) [#14293](https://github.com/nodejs/node/pull/14293)
* [[`1813467d27`](https://github.com/nodejs/node/commit/1813467d27)] - **test**: upgrade tests to work with master’s `common` (Anna Henningsen) [#14459](https://github.com/nodejs/node/pull/14459)
* [[`d89bb1c6f3`](https://github.com/nodejs/node/commit/d89bb1c6f3)] - **test**: bump test/common to master (Anna Henningsen) [#14459](https://github.com/nodejs/node/pull/14459)
* [[`d7a1637897`](https://github.com/nodejs/node/commit/d7a1637897)] - **test**: change isAix to isAIX (章礼平) [#14263](https://github.com/nodejs/node/pull/14263)
* [[`552d2be625`](https://github.com/nodejs/node/commit/552d2be625)] - **test**: improve test-util-inspect (Peter Marshall) [#14003](https://github.com/nodejs/node/pull/14003)
* [[`0418a70d7c`](https://github.com/nodejs/node/commit/0418a70d7c)] - **test**: add non-internet resolveAny tests (Anna Henningsen) [#13883](https://github.com/nodejs/node/pull/13883)
* [[`265f159881`](https://github.com/nodejs/node/commit/265f159881)] - **test**: replace concatenation with template literals (Song, Bintao Garfield) [#14295](https://github.com/nodejs/node/pull/14295)
* [[`3414e42127`](https://github.com/nodejs/node/commit/3414e42127)] - **test**: replace concatenation with template literals (Zongmin Lei) [#14298](https://github.com/nodejs/node/pull/14298)
* [[`953736cdde`](https://github.com/nodejs/node/commit/953736cdde)] - **test**: move timing-dependent tests to sequential (Alexey Orlenko) [#14377](https://github.com/nodejs/node/pull/14377)
* [[`9b22acc29e`](https://github.com/nodejs/node/commit/9b22acc29e)] - **test**: fix flaky test-net-write-after-close (Rich Trott) [#14361](https://github.com/nodejs/node/pull/14361)
* [[`11ae8c33bd`](https://github.com/nodejs/node/commit/11ae8c33bd)] - **test**: delete obsolete test-sendfd.js (decareano) [#14334](https://github.com/nodejs/node/pull/14334)
* [[`99104e1b58`](https://github.com/nodejs/node/commit/99104e1b58)] - **test**: improve fs.exists coverage (jkzing) [#14301](https://github.com/nodejs/node/pull/14301)
* [[`e237720537`](https://github.com/nodejs/node/commit/e237720537)] - **test**: replace string concatenation with template (ziyun) [#14286](https://github.com/nodejs/node/pull/14286)
* [[`3c92b787d7`](https://github.com/nodejs/node/commit/3c92b787d7)] - **test**: use path.join in async-hooks/test-tlswrap.js (Vincent Xue) [#14319](https://github.com/nodejs/node/pull/14319)
* [[`0197ba00a5`](https://github.com/nodejs/node/commit/0197ba00a5)] - **test**: add comments for whatwg-url tests (Gautam Arora) [#14355](https://github.com/nodejs/node/pull/14355)
* [[`956a473107`](https://github.com/nodejs/node/commit/956a473107)] - **test**: move test-fs-largefile to pummel (Rich Trott) [#14338](https://github.com/nodejs/node/pull/14338)
* [[`c866c9078b`](https://github.com/nodejs/node/commit/c866c9078b)] - **test**: use path.join for long path concatenation (zzz) [#14280](https://github.com/nodejs/node/pull/14280)
* [[`94c7331277`](https://github.com/nodejs/node/commit/94c7331277)] - **test**: replace string concatenation with path.join (jkzing) [#14272](https://github.com/nodejs/node/pull/14272)
* [[`def98c6959`](https://github.com/nodejs/node/commit/def98c6959)] - **test**: replace string concatenation with template (Nathan Jiang) [#14342](https://github.com/nodejs/node/pull/14342)
* [[`3bc7d2a5ea`](https://github.com/nodejs/node/commit/3bc7d2a5ea)] - **test**: replace string concat in test-fs-watchfile.js (Helianthus21) [#14287](https://github.com/nodejs/node/pull/14287)
* [[`72febfd3b6`](https://github.com/nodejs/node/commit/72febfd3b6)] - **test**: replace concatenation with template literals (SkyAo) [#14296](https://github.com/nodejs/node/pull/14296)
* [[`b5d0a03a9e`](https://github.com/nodejs/node/commit/b5d0a03a9e)] - **test**: fix error handling test-http-full-response (Rich Trott) [#14252](https://github.com/nodejs/node/pull/14252)
* [[`e90af29604`](https://github.com/nodejs/node/commit/e90af29604)] - **tls**: fix empty issuer/subject/infoAccess parsing (Ben Noordhuis) [#14473](https://github.com/nodejs/node/pull/14473)
* [[`767644def5`](https://github.com/nodejs/node/commit/767644def5)] - **tools**: simplify no-unescaped-regexp-dot rule (Rich Trott) [#14561](https://github.com/nodejs/node/pull/14561)
* [[`9f319d5dfb`](https://github.com/nodejs/node/commit/9f319d5dfb)] - **tools**: replace assert-throw-arguments custom lint (Rich Trott) [#14547](https://github.com/nodejs/node/pull/14547)
* [[`fa8c5f4372`](https://github.com/nodejs/node/commit/fa8c5f4372)] - **tools**: remove legacy indentation linting (Rich Trott) [#14515](https://github.com/nodejs/node/pull/14515)
* [[`d11840c180`](https://github.com/nodejs/node/commit/d11840c180)] - **tools**: enable stricter linting in lib directory (Rich Trott) [#14403](https://github.com/nodejs/node/pull/14403)
* [[`5e952182e7`](https://github.com/nodejs/node/commit/5e952182e7)] - **tools**: update to ESLint 4.3.0 (Rich Trott) [#14417](https://github.com/nodejs/node/pull/14417)
* [[`ebb90900af`](https://github.com/nodejs/node/commit/ebb90900af)] - **tools**: skip workaround for newer llvm (nanaya) [#14077](https://github.com/nodejs/node/pull/14077)
* [[`c0ea5d8ce5`](https://github.com/nodejs/node/commit/c0ea5d8ce5)] - **tools**: always include llvm_version in config (nanaya) [#14077](https://github.com/nodejs/node/pull/14077)
* [[`32259421ca`](https://github.com/nodejs/node/commit/32259421ca)] - **url**: update sort() behavior with no params (Timothy Gu) [#14185](https://github.com/nodejs/node/pull/14185)
* [[`9a3fc10dd4`](https://github.com/nodejs/node/commit/9a3fc10dd4)] - **(SEMVER-MINOR)** **util**: implement WHATWG Encoding Standard API (James M Snell) [#13644](https://github.com/nodejs/node/pull/13644)
* [[`f593960d35`](https://github.com/nodejs/node/commit/f593960d35)] - **util**: refactor util module (James M Snell) [#13803](https://github.com/nodejs/node/pull/13803)
* [[`357873ddb0`](https://github.com/nodejs/node/commit/357873ddb0)] - **(SEMVER-MINOR)** **v8**: fix stack overflow in recursive method (Ben Noordhuis) [#14004](https://github.com/nodejs/node/pull/14004)
* [[`a8132943c5`](https://github.com/nodejs/node/commit/a8132943c5)] - **zlib**: fix crash when initializing failed (Anna Henningsen) [#14666](https://github.com/nodejs/node/pull/14666)
* [[`e529914e70`](https://github.com/nodejs/node/commit/e529914e70)] - **zlib**: fix interaction of flushing and needDrain (Anna Henningsen) [#14527](https://github.com/nodejs/node/pull/14527)

<a id="8.2.1"></a>
## 2017-07-20, Version 8.2.1 (Current), @fishrock123

### Notable changes

* **http**: Writes no longer abort if the Socket is missing.
* **process, async_hooks**: Avoid problems when `triggerAsyncId` is `undefined`.
* **zlib**: Streams no longer attempt to process data when destroyed.

### Commits

* [[`8d426bbeec`](https://github.com/nodejs/node/commit/8d426bbeec)] - **http**: do not abort if socket is missing (Matteo Collina) [#14387](https://github.com/nodejs/node/pull/14387)
* [[`302c19b006`](https://github.com/nodejs/node/commit/302c19b006)] - **process**: triggerAsyncId can be undefined (Matteo Collina) [#14387](https://github.com/nodejs/node/pull/14387)
* [[`6fce1a314e`](https://github.com/nodejs/node/commit/6fce1a314e)] - **zlib**: check if the stream is destroyed before push (Matteo Collina) [#14330](https://github.com/nodejs/node/pull/14330)

<a id="8.2.0"></a>
## 2017-07-19, Version 8.2.0 (Current), @fishrock123

Big thanks to @addaleax who prepared the vast majority of this release.

### Notable changes

* **Async Hooks**
  * Multiple improvements to Promise support in `async_hooks` have been made.

* **Build**
  * The compiler version requirement to build Node with GCC has been raised to
    GCC 4.9.4.
    [[`820b011ed6`](https://github.com/nodejs/node/commit/820b011ed6)]
    [#13466](https://github.com/nodejs/node/pull/13466)

* **Cluster**
  * Users now have more fine-grained control over the inspector port used by
    individual cluster workers. Previously, cluster workers were restricted to
    incrementing from the master's debug port.
    [[`dfc46e262a`](https://github.com/nodejs/node/commit/dfc46e262a)]
    [#14140](https://github.com/nodejs/node/pull/14140)

* **DNS**
  * The server used for DNS queries can now use a custom port.
    [[`ebe7bb29aa`](https://github.com/nodejs/node/commit/ebe7bb29aa)]
    [#13723](https://github.com/nodejs/node/pull/13723)
  * Support for `dns.resolveAny()` has been added.
    [[`6e30e2558e`](https://github.com/nodejs/node/commit/6e30e2558e)]
    [#13137](https://github.com/nodejs/node/pull/13137)

* **npm**
  * The `npm` CLI has been updated to version 5.3.0. In particular, it now comes
    with the `npx` binary, which is also shipped with Node.
    [[`dc3f6b9ac1`](https://github.com/nodejs/node/commit/dc3f6b9ac1)]
    [#14235](https://github.com/nodejs/node/pull/14235)
  * `npm` Changelogs:
      - [v5.0.4](https://github.com/npm/npm/releases/tag/v5.0.4)
      - [v5.1.0](https://github.com/npm/npm/releases/tag/v5.1.0)
      - [v5.2.0](https://github.com/npm/npm/releases/tag/v5.2.0)
      - [v5.3.0](https://github.com/npm/npm/releases/tag/v5.3.0)

### Commits

* [[`53c52ac38e`](https://github.com/nodejs/node/commit/53c52ac38e)] - **N-API**: Reuse ObjectTemplate instances (Gabriel Schulhof) [#13999](https://github.com/nodejs/node/pull/13999)
* [[`86c06c01ec`](https://github.com/nodejs/node/commit/86c06c01ec)] - **async-hooks,net**: ensure asyncId=null if no handle (Matt Sergeant) [#13938](https://github.com/nodejs/node/pull/13938)
* [[`71ee15d340`](https://github.com/nodejs/node/commit/71ee15d340)] - **async_hooks**: make AsyncResource match emitInit (Andreas Madsen) [#14152](https://github.com/nodejs/node/pull/14152)
* [[`1aac2c09e7`](https://github.com/nodejs/node/commit/1aac2c09e7)] - **async_hooks**: rename internal emit functions (Andreas Madsen) [#14152](https://github.com/nodejs/node/pull/14152)
* [[`0c69ec12a9`](https://github.com/nodejs/node/commit/0c69ec12a9)] - **async_hooks**: fix nested hooks mutation (Andreas Madsen) [#14143](https://github.com/nodejs/node/pull/14143)
* [[`3211eff935`](https://github.com/nodejs/node/commit/3211eff935)] - **async_hooks**: move restoreTmpHooks call to init (Ruben Bridgewater) [#14054](https://github.com/nodejs/node/pull/14054)
* [[`76ba1b59bc`](https://github.com/nodejs/node/commit/76ba1b59bc)] - **async_hooks**: C++ Embedder API overhaul (Andreas Madsen) [#14040](https://github.com/nodejs/node/pull/14040)
* [[`544300ee48`](https://github.com/nodejs/node/commit/544300ee48)] - **async_hooks**: require parameter in emitBefore (Andreas Madsen) [#14050](https://github.com/nodejs/node/pull/14050)
* [[`9f66f1924f`](https://github.com/nodejs/node/commit/9f66f1924f)] - **async_hooks**: use common emitBefore and emitAfter (Andreas Madsen) [#14050](https://github.com/nodejs/node/pull/14050)
* [[`7b369d12cf`](https://github.com/nodejs/node/commit/7b369d12cf)] - **async_hooks**: fix default nextTick triggerAsyncId (Andreas Madsen) [#14026](https://github.com/nodejs/node/pull/14026)
* [[`2eabd92639`](https://github.com/nodejs/node/commit/2eabd92639)] - **async_hooks**: reduce duplication with factory (Ruben Bridgewater) [#13755](https://github.com/nodejs/node/pull/13755)
* [[`8f37f5dd01`](https://github.com/nodejs/node/commit/8f37f5dd01)] - **async_hooks**: proper id stacking for Promises (Anna Henningsen) [#13585](https://github.com/nodejs/node/pull/13585)
* [[`3bb4ec80ae`](https://github.com/nodejs/node/commit/3bb4ec80ae)] - **(SEMVER-MINOR)** **async_hooks**: rename currentId and triggerId (Andreas Madsen) [#13490](https://github.com/nodejs/node/pull/13490)
* [[`8b57b09c15`](https://github.com/nodejs/node/commit/8b57b09c15)] - ***Revert*** "**async_hooks**: only set up hooks if used" (Trevor Norris) [#13509](https://github.com/nodejs/node/pull/13509)
* [[`a44260326c`](https://github.com/nodejs/node/commit/a44260326c)] - **(SEMVER-MINOR)** **async_hooks**: use resource objects for Promises (Anna Henningsen) [#13452](https://github.com/nodejs/node/pull/13452)
* [[`2122e2fe89`](https://github.com/nodejs/node/commit/2122e2fe89)] - **async_wrap**: use kTotals to enable PromiseHook (Trevor Norris) [#13509](https://github.com/nodejs/node/pull/13509)
* [[`96279e83e7`](https://github.com/nodejs/node/commit/96279e83e7)] - **async_wrap**: expose enable/disablePromiseHook API (Anna Henningsen) [#13509](https://github.com/nodejs/node/pull/13509)
* [[`1c0f20fcf3`](https://github.com/nodejs/node/commit/1c0f20fcf3)] - **benchmark**: fix typo in inspect-proxy (Vse Mozhet Byt) [#14237](https://github.com/nodejs/node/pull/14237)
* [[`65a2e80596`](https://github.com/nodejs/node/commit/65a2e80596)] - **benchmark**: Improve event performance tests. (Benedikt Meurer) [#14052](https://github.com/nodejs/node/pull/14052)
* [[`3d0b66a7c2`](https://github.com/nodejs/node/commit/3d0b66a7c2)] - **benchmark,lib,test**: use braces for multiline block (Rich Trott) [#13995](https://github.com/nodejs/node/pull/13995)
* [[`bed13444b1`](https://github.com/nodejs/node/commit/bed13444b1)] - **buffer**: remove MAX_SAFE_INTEGER check on length (Rich Trott) [#14131](https://github.com/nodejs/node/pull/14131)
* [[`683f743e61`](https://github.com/nodejs/node/commit/683f743e61)] - **(SEMVER-MINOR)** **buffer**: support boxed strings and toPrimitive (James M Snell) [#13725](https://github.com/nodejs/node/pull/13725)
* [[`7794030700`](https://github.com/nodejs/node/commit/7794030700)] - **(SEMVER-MINOR)** **buffer**: add constants object (Anna Henningsen) [#13467](https://github.com/nodejs/node/pull/13467)
* [[`1444601a57`](https://github.com/nodejs/node/commit/1444601a57)] - **build**: prevent VsDevCmd.bat from changing cwd (Nikolai Vavilov) [#14303](https://github.com/nodejs/node/pull/14303)
* [[`6b052e7c42`](https://github.com/nodejs/node/commit/6b052e7c42)] - **(SEMVER-MINOR)** **build**: add npx to installers (Kat Marchán) [#14235](https://github.com/nodejs/node/pull/14235)
* [[`922f58f8ca`](https://github.com/nodejs/node/commit/922f58f8ca)] - **build**: run test-hash-seed at the end of test-v8 (Michaël Zasso) [#14219](https://github.com/nodejs/node/pull/14219)
* [[`b757105862`](https://github.com/nodejs/node/commit/b757105862)] - **build**: allow enabling the --trace-maps flag in V8 (Evan Lucas) [#14018](https://github.com/nodejs/node/pull/14018)
* [[`9ee271d92b`](https://github.com/nodejs/node/commit/9ee271d92b)] - **build**: split up cpplint to avoid long cmd lines (Kyle Farnung) [#14116](https://github.com/nodejs/node/pull/14116)
* [[`651af59e6b`](https://github.com/nodejs/node/commit/651af59e6b)] - **build**: add async-hooks testing to vcbuild.bat (Refael Ackermann) [#13381](https://github.com/nodejs/node/pull/13381)
* [[`c972364848`](https://github.com/nodejs/node/commit/c972364848)] - **build**: remove dependency on icu io library (Ben Noordhuis) [#13656](https://github.com/nodejs/node/pull/13656)
* [[`f2d7b803f1`](https://github.com/nodejs/node/commit/f2d7b803f1)] - **build**: clean up config_fips.gypi (Daniel Bevenius) [#13837](https://github.com/nodejs/node/pull/13837)
* [[`897405d62c`](https://github.com/nodejs/node/commit/897405d62c)] - **build,win**: skip `vcvarsall.bat` if env is set (Refael Ackermann) [#13806](https://github.com/nodejs/node/pull/13806)
* [[`dc0ae8be56`](https://github.com/nodejs/node/commit/dc0ae8be56)] - **build,win**: respect VS version for building addons (João Reis) [#13911](https://github.com/nodejs/node/pull/13911)
* [[`cd9ef939ba`](https://github.com/nodejs/node/commit/cd9ef939ba)] - **build,win**: use latest installed VS by default (João Reis) [#13911](https://github.com/nodejs/node/pull/13911)
* [[`79ead795b9`](https://github.com/nodejs/node/commit/79ead795b9)] - **build,windows**: restore DISTTYPEDIR (Refael Ackermann) [#13969](https://github.com/nodejs/node/pull/13969)
* [[`949f7be5a0`](https://github.com/nodejs/node/commit/949f7be5a0)] - **build,windows**: implement PEP514 python detection (Refael Ackermann) [#13900](https://github.com/nodejs/node/pull/13900)
* [[`096080b69c`](https://github.com/nodejs/node/commit/096080b69c)] - **child_process**: refactor normalizeSpawnArguments() (Rich Trott) [#14149](https://github.com/nodejs/node/pull/14149)
* [[`09eb58894e`](https://github.com/nodejs/node/commit/09eb58894e)] - **child_process**: fix handleless NODE_HANDLE handling (Santiago Gimeno) [#13235](https://github.com/nodejs/node/pull/13235)
* [[`16f2600ecf`](https://github.com/nodejs/node/commit/16f2600ecf)] - **child_process**: emit IPC messages on next tick (cjihrig) [#13856](https://github.com/nodejs/node/pull/13856)
* [[`dfc46e262a`](https://github.com/nodejs/node/commit/dfc46e262a)] - **(SEMVER-MINOR)** **cluster**: overriding inspector port (cornholio) [#14140](https://github.com/nodejs/node/pull/14140)
* [[`26f85e75f9`](https://github.com/nodejs/node/commit/26f85e75f9)] - **cluster**: remove obsolete todo (Ruben Bridgewater) [#13734](https://github.com/nodejs/node/pull/13734)
* [[`816f98f5d0`](https://github.com/nodejs/node/commit/816f98f5d0)] - **console**: use a plain object for the the error stack (Ruben Bridgewater) [#13743](https://github.com/nodejs/node/pull/13743)
* [[`932791063b`](https://github.com/nodejs/node/commit/932791063b)] - **(SEMVER-MINOR)** **deps**: hotfix to bump npx version (Kat Marchán) [#14235](https://github.com/nodejs/node/pull/14235)
* [[`dc3f6b9ac1`](https://github.com/nodejs/node/commit/dc3f6b9ac1)] - **(SEMVER-MINOR)** **deps**: upgrade npm to 5.3.0 (Kat Marchán) [#14235](https://github.com/nodejs/node/pull/14235)
* [[`fe6ca44f84`](https://github.com/nodejs/node/commit/fe6ca44f84)] - **deps**: upgrade libuv to 1.13.1 (cjihrig) [#14117](https://github.com/nodejs/node/pull/14117)
* [[`46cc80abf5`](https://github.com/nodejs/node/commit/46cc80abf5)] - **deps**: delete deps/icu-small/source/io (Ben Noordhuis) [#13656](https://github.com/nodejs/node/pull/13656)
* [[`6e30e2558e`](https://github.com/nodejs/node/commit/6e30e2558e)] - **(SEMVER-MINOR)** **dns**: add resolveAny support (XadillaX) [#13137](https://github.com/nodejs/node/pull/13137)
* [[`ebe7bb29aa`](https://github.com/nodejs/node/commit/ebe7bb29aa)] - **(SEMVER-MINOR)** **dns**: make `dns.setServers` support customized port (XadillaX) [#13723](https://github.com/nodejs/node/pull/13723)
* [[`7df10f529d`](https://github.com/nodejs/node/commit/7df10f529d)] - **doc**: fix inspectPort documentation in cluster.md (Anna Henningsen) [#14349](https://github.com/nodejs/node/pull/14349)
* [[`7a116d4a60`](https://github.com/nodejs/node/commit/7a116d4a60)] - **doc**: add guidance on testing new errors (Michael Dawson) [#14207](https://github.com/nodejs/node/pull/14207)
* [[`6f13d7da67`](https://github.com/nodejs/node/commit/6f13d7da67)] - **doc**: move LTS README link to increase prominence (Gibson Fahnestock) [#14259](https://github.com/nodejs/node/pull/14259)
* [[`c0703f0d4c`](https://github.com/nodejs/node/commit/c0703f0d4c)] - **(SEMVER-MINOR)** **doc**: fixes in cluster.md (cornholio) [#14140](https://github.com/nodejs/node/pull/14140)
* [[`e91a7a447d`](https://github.com/nodejs/node/commit/e91a7a447d)] - **doc**: update umask for clarity (James Sumners) [#14170](https://github.com/nodejs/node/pull/14170)
* [[`157ef23fc3`](https://github.com/nodejs/node/commit/157ef23fc3)] - **doc**: add notice about useGlobal option in repl docs (starkwang) [#13866](https://github.com/nodejs/node/pull/13866)
* [[`1b3cf97198`](https://github.com/nodejs/node/commit/1b3cf97198)] - **doc**: prefix of the stacktrace in errors.md (Roman Shoryn) [#14150](https://github.com/nodejs/node/pull/14150)
* [[`eb90ad61fb`](https://github.com/nodejs/node/commit/eb90ad61fb)] - **doc**: add missing space (Timothy Gu) [#14181](https://github.com/nodejs/node/pull/14181)
* [[`01b98a769f`](https://github.com/nodejs/node/commit/01b98a769f)] - **doc**: removed redundant mentions to error codes (jklepatch) [#13627](https://github.com/nodejs/node/pull/13627)
* [[`575dcdcf0e`](https://github.com/nodejs/node/commit/575dcdcf0e)] - **doc**: correct stream Duplex allowHalfOpen doc (Rich Trott) [#14127](https://github.com/nodejs/node/pull/14127)
* [[`cfa5e0c3b6`](https://github.com/nodejs/node/commit/cfa5e0c3b6)] - **doc**: note 'resize' event conditions on Windows (Dean Coakley) [#13576](https://github.com/nodejs/node/pull/13576)
* [[`217e1dc7b1`](https://github.com/nodejs/node/commit/217e1dc7b1)] - **doc**: fix mistake in http.md (Moogen Tian) [#14126](https://github.com/nodejs/node/pull/14126)
* [[`32ddb666b6`](https://github.com/nodejs/node/commit/32ddb666b6)] - **doc**: match debugger output & instructions to master behavior (Jan Krems) [#13885](https://github.com/nodejs/node/pull/13885)
* [[`9e6a4d6e27`](https://github.com/nodejs/node/commit/9e6a4d6e27)] - **doc**: add documentation on ICU (Timothy Gu) [#13916](https://github.com/nodejs/node/pull/13916)
* [[`23c67de3df`](https://github.com/nodejs/node/commit/23c67de3df)] - **doc**: fix padding mode of crypto.publicDecrypt (MoonBall) [#14036](https://github.com/nodejs/node/pull/14036)
* [[`99f0a6bdb5`](https://github.com/nodejs/node/commit/99f0a6bdb5)] - **doc**: add CTC members to Collaborators list (Rich Trott) [#13284](https://github.com/nodejs/node/pull/13284)
* [[`199e905249`](https://github.com/nodejs/node/commit/199e905249)] - **doc**: fix example in child_process.md (Ruslan Iusupov) [#13716](https://github.com/nodejs/node/pull/13716)
* [[`310040c89e`](https://github.com/nodejs/node/commit/310040c89e)] - **doc**: add default values to functions in fs.md (Matej Krajčovič) [#13767](https://github.com/nodejs/node/pull/13767)
* [[`26ed901730`](https://github.com/nodejs/node/commit/26ed901730)] - **doc**: fix some broken references (Alexander Gromnitsky) [#13811](https://github.com/nodejs/node/pull/13811)
* [[`e36561a828`](https://github.com/nodejs/node/commit/e36561a828)] - **doc**: move module-specific "globals" to modules.md (Tobias Nießen) [#13962](https://github.com/nodejs/node/pull/13962)
* [[`f1d92fb489`](https://github.com/nodejs/node/commit/f1d92fb489)] - **doc**: fix indentation issues in sample code (Rich Trott) [#13950](https://github.com/nodejs/node/pull/13950)
* [[`f53bfe4945`](https://github.com/nodejs/node/commit/f53bfe4945)] - **doc**: use stricter indentation checking for docs (Rich Trott) [#13950](https://github.com/nodejs/node/pull/13950)
* [[`adb0f4601d`](https://github.com/nodejs/node/commit/adb0f4601d)] - **doc**: note that fs.futimes only works on AIX \>7.1 (Gibson Fahnestock) [#13659](https://github.com/nodejs/node/pull/13659)
* [[`8fe77225ab`](https://github.com/nodejs/node/commit/8fe77225ab)] - **doc**: add @nodejs/documentation to CC table (Vse Mozhet Byt) [#13952](https://github.com/nodejs/node/pull/13952)
* [[`4c43ff271f`](https://github.com/nodejs/node/commit/4c43ff271f)] - **doc**: doc lifetime of n-api last error info (Michael Dawson) [#13939](https://github.com/nodejs/node/pull/13939)
* [[`7332e7ef5c`](https://github.com/nodejs/node/commit/7332e7ef5c)] - **doc**: add gireeshpunathil to collaborators (Gireesh Punathil) [#13967](https://github.com/nodejs/node/pull/13967)
* [[`9ff5212d5f`](https://github.com/nodejs/node/commit/9ff5212d5f)] - **doc**: fix mistake in path.relative (Tobias Nießen) [#13912](https://github.com/nodejs/node/pull/13912)
* [[`0fc7a5077f`](https://github.com/nodejs/node/commit/0fc7a5077f)] - **doc**: unify ERR_FALSY_VALUE_REJECTION description (Tobias Nießen) [#13869](https://github.com/nodejs/node/pull/13869)
* [[`502be7c085`](https://github.com/nodejs/node/commit/502be7c085)] - **doc**: fixed formatting issue in cli docs (Chris Young) [#13808](https://github.com/nodejs/node/pull/13808)
* [[`12b6765cd1`](https://github.com/nodejs/node/commit/12b6765cd1)] - **doc**: fix link in async_hooks.md (Azard) [#13930](https://github.com/nodejs/node/pull/13930)
* [[`04bca73bd7`](https://github.com/nodejs/node/commit/04bca73bd7)] - **doc**: add missing zlib link to stream API docs (Rob Wu) [#13838](https://github.com/nodejs/node/pull/13838)
* [[`f1b7e8d50d`](https://github.com/nodejs/node/commit/f1b7e8d50d)] - **doc**: fix nits in guides/using-internal-errors.md (Vse Mozhet Byt) [#13820](https://github.com/nodejs/node/pull/13820)
* [[`46756acb95`](https://github.com/nodejs/node/commit/46756acb95)] - **doc**: document res.connection and res.socket (Justin Beckwith) [#13617](https://github.com/nodejs/node/pull/13617)
* [[`70f3935130`](https://github.com/nodejs/node/commit/70f3935130)] - **doc**: fix api docs style (Daijiro Wachi) [#13700](https://github.com/nodejs/node/pull/13700)
* [[`820b011ed6`](https://github.com/nodejs/node/commit/820b011ed6)] - **doc**: update minimum g++ version to 4.9.4 (Ben Noordhuis) [#13466](https://github.com/nodejs/node/pull/13466)
* [[`d4a6ca6ed3`](https://github.com/nodejs/node/commit/d4a6ca6ed3)] - **doc, util, console**: clarify ambiguous docs (Natanael Log) [#14027](https://github.com/nodejs/node/pull/14027)
* [[`4f0eb6f024`](https://github.com/nodejs/node/commit/4f0eb6f024)] - **doc,test**: fs - reserved characters under win32 (XadillaX) [#13875](https://github.com/nodejs/node/pull/13875)
* [[`ad8b1588a2`](https://github.com/nodejs/node/commit/ad8b1588a2)] - **errors**: prevent stack recalculation (Ruben Bridgewater) [#13743](https://github.com/nodejs/node/pull/13743)
* [[`e8780ba7ae`](https://github.com/nodejs/node/commit/e8780ba7ae)] - **errors**: add missing ERR_ prefix on util.callbackify error (James M Snell) [#13750](https://github.com/nodejs/node/pull/13750)
* [[`2a02868934`](https://github.com/nodejs/node/commit/2a02868934)] - **fs**: two minor optimizations (Ruben Bridgewater) [#14055](https://github.com/nodejs/node/pull/14055)
* [[`4587f21716`](https://github.com/nodejs/node/commit/4587f21716)] - **gyp**: implement LD/LDXX for ninja and FIPS (Sam Roberts) [#14227](https://github.com/nodejs/node/pull/14227)
* [[`63aee3b4c8`](https://github.com/nodejs/node/commit/63aee3b4c8)] - **http**: OutgoingMessage change writable after end (Roee Kasher) [#14024](https://github.com/nodejs/node/pull/14024)
* [[`c652845a61`](https://github.com/nodejs/node/commit/c652845a61)] - **http**: guard against failed sockets creation (Refael Ackermann) [#13839](https://github.com/nodejs/node/pull/13839)
* [[`b22a04b2c6`](https://github.com/nodejs/node/commit/b22a04b2c6)] - **http**: always cork outgoing writes (Brian White) [#13522](https://github.com/nodejs/node/pull/13522)
* [[`74741fa52b`](https://github.com/nodejs/node/commit/74741fa52b)] - **(SEMVER-MINOR)** **https**: make opts optional & immutable when create (XadillaX) [#13599](https://github.com/nodejs/node/pull/13599)
* [[`a45792a383`](https://github.com/nodejs/node/commit/a45792a383)] - **inspector**: perform DNS lookup for host (Eugene Ostroukhov) [#13478](https://github.com/nodejs/node/pull/13478)
* [[`b0db2b9fc2`](https://github.com/nodejs/node/commit/b0db2b9fc2)] - **inspector, test**: Fix test bug detected by Coverity (Eugene Ostroukhov) [#13799](https://github.com/nodejs/node/pull/13799)
* [[`6361565915`](https://github.com/nodejs/node/commit/6361565915)] - **lib**: update indentation of ternaries (Rich Trott) [#14247](https://github.com/nodejs/node/pull/14247)
* [[`b12b8c2f7c`](https://github.com/nodejs/node/commit/b12b8c2f7c)] - **lib**: normalize indentation in parentheses (Rich Trott) [#14125](https://github.com/nodejs/node/pull/14125)
* [[`a0866b6b0c`](https://github.com/nodejs/node/commit/a0866b6b0c)] - **lib**: remove excess indentation (Rich Trott) [#14090](https://github.com/nodejs/node/pull/14090)
* [[`07642552cb`](https://github.com/nodejs/node/commit/07642552cb)] - **lib**: use consistent indentation for ternaries (Rich Trott) [#14078](https://github.com/nodejs/node/pull/14078)
* [[`4bb1a3a8ac`](https://github.com/nodejs/node/commit/4bb1a3a8ac)] - **lib**: fix typos (Ruben Bridgewater) [#14044](https://github.com/nodejs/node/pull/14044)
* [[`3bd18c51e0`](https://github.com/nodejs/node/commit/3bd18c51e0)] - **n-api**: add napi_fatal_error API (Kyle Farnung) [#13971](https://github.com/nodejs/node/pull/13971)
* [[`b1eb6d5485`](https://github.com/nodejs/node/commit/b1eb6d5485)] - **n-api**: wrap test macros in do/while (Kyle Farnung) [#14095](https://github.com/nodejs/node/pull/14095)
* [[`f2054f330a`](https://github.com/nodejs/node/commit/f2054f330a)] - **n-api**: Implement stricter wrapping (Gabriel Schulhof) [#13872](https://github.com/nodejs/node/pull/13872)
* [[`e25c5ef7da`](https://github.com/nodejs/node/commit/e25c5ef7da)] - **n-api**: fix warning in test_general (Daniel Bevenius) [#14104](https://github.com/nodejs/node/pull/14104)
* [[`2a86650562`](https://github.com/nodejs/node/commit/2a86650562)] - **n-api**: add napi_has_own_property() (cjihrig) [#14063](https://github.com/nodejs/node/pull/14063)
* [[`f3933049e5`](https://github.com/nodejs/node/commit/f3933049e5)] - **n-api**: fix -Wmaybe-uninitialized compiler warning (Ben Noordhuis) [#14053](https://github.com/nodejs/node/pull/14053)
* [[`de744ba232`](https://github.com/nodejs/node/commit/de744ba232)] - **n-api**: use Maybe version of Object::SetPrototype() (Ben Noordhuis) [#14053](https://github.com/nodejs/node/pull/14053)
* [[`820d97df5d`](https://github.com/nodejs/node/commit/820d97df5d)] - **n-api**: add napi_delete_property() (cjihrig) [#13934](https://github.com/nodejs/node/pull/13934)
* [[`6316c9a0f8`](https://github.com/nodejs/node/commit/6316c9a0f8)] - **n-api**: add napi_delete_element() (cjihrig) [#13949](https://github.com/nodejs/node/pull/13949)
* [[`4843d4da8c`](https://github.com/nodejs/node/commit/4843d4da8c)] - **n-api**: fix section title typo (Kyle Farnung) [#13972](https://github.com/nodejs/node/pull/13972)
* [[`a839aede3e`](https://github.com/nodejs/node/commit/a839aede3e)] - **(SEMVER-MINOR)** **net**: return this from getConnections() (Sam Roberts) [#13553](https://github.com/nodejs/node/pull/13553)
* [[`69f806cc55`](https://github.com/nodejs/node/commit/69f806cc55)] - **(SEMVER-MINOR)** **net**: return this from destroy() (Sam Roberts) [#13530](https://github.com/nodejs/node/pull/13530)
* [[`e30fc2c5ba`](https://github.com/nodejs/node/commit/e30fc2c5ba)] - **process**: improve nextTick() performance (Brian White) [#13446](https://github.com/nodejs/node/pull/13446)
* [[`c56a89013c`](https://github.com/nodejs/node/commit/c56a89013c)] - **querystring**: fix up lastPos usage (Timothy Gu) [#14151](https://github.com/nodejs/node/pull/14151)
* [[`b4b27b2edd`](https://github.com/nodejs/node/commit/b4b27b2edd)] - **readline**: properly handle 0-width characters (Timothy Gu) [#13918](https://github.com/nodejs/node/pull/13918)
* [[`3683f6b787`](https://github.com/nodejs/node/commit/3683f6b787)] - **repl**: fix crash with large buffer tab completion (XadillaX) [#13817](https://github.com/nodejs/node/pull/13817)
* [[`f237ad55ff`](https://github.com/nodejs/node/commit/f237ad55ff)] - **src**: fix memory leak in DH key setters (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`0bbdb78962`](https://github.com/nodejs/node/commit/0bbdb78962)] - **src**: reduce allocations in exportPublicKey() (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`e4b70199b3`](https://github.com/nodejs/node/commit/e4b70199b3)] - **src**: guard against double free in randomBytes() (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`ad0669bfe6`](https://github.com/nodejs/node/commit/ad0669bfe6)] - **src**: simplify PBKDF2Request (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`8f4b84ba42`](https://github.com/nodejs/node/commit/8f4b84ba42)] - **src**: remove PBKDF2Request::release() (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`b5802c7bf1`](https://github.com/nodejs/node/commit/b5802c7bf1)] - **src**: avoid heap allocation in crypto.pbkdf2() (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`1c3e090eba`](https://github.com/nodejs/node/commit/1c3e090eba)] - **src**: make array arg length compile-time checkable (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`41f79fb22f`](https://github.com/nodejs/node/commit/41f79fb22f)] - **src**: refactor PBKDF2Request (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`233740c594`](https://github.com/nodejs/node/commit/233740c594)] - **src**: remove extra heap allocations in DH functions (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`8e51d3151d`](https://github.com/nodejs/node/commit/8e51d3151d)] - **src**: avoid heap allocation in hmac.digest() (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`8be9bd139f`](https://github.com/nodejs/node/commit/8be9bd139f)] - **src**: remove extra heap allocation in GetSession() (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`8dd6866303`](https://github.com/nodejs/node/commit/8dd6866303)] - **src**: make CipherBase::kind_ const (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`0fcb8b1029`](https://github.com/nodejs/node/commit/0fcb8b1029)] - **src**: remove unused Local (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`db65422f0d`](https://github.com/nodejs/node/commit/db65422f0d)] - **src**: remove superfluous cipher_ data member (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`1af064bf7c`](https://github.com/nodejs/node/commit/1af064bf7c)] - **src**: don't heap allocate GCM cipher auth tag (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`174f8c8d91`](https://github.com/nodejs/node/commit/174f8c8d91)] - **src**: avoid heap allocation in sign.final() (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`efb7aef676`](https://github.com/nodejs/node/commit/efb7aef676)] - **src**: remove unneeded const_cast (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`2ee31aa261`](https://github.com/nodejs/node/commit/2ee31aa261)] - **src**: remove extra heap allocations in CipherBase (Ben Noordhuis) [#14122](https://github.com/nodejs/node/pull/14122)
* [[`50913b168d`](https://github.com/nodejs/node/commit/50913b168d)] - **(SEMVER-MINOR)** **src**: whitelist v8 options with `'_'` or `'-'` (Sam Roberts) [#14093](https://github.com/nodejs/node/pull/14093)
* [[`b799498e8a`](https://github.com/nodejs/node/commit/b799498e8a)] - **src**: document --abort-on-uncaught-exception (Sam Roberts) [#13931](https://github.com/nodejs/node/pull/13931)
* [[`21ee4b1b97`](https://github.com/nodejs/node/commit/21ee4b1b97)] - **src**: --abort-on-uncaught-exception in NODE_OPTIONS (Sam Roberts) [#13932](https://github.com/nodejs/node/pull/13932)
* [[`ef67f7c8ca`](https://github.com/nodejs/node/commit/ef67f7c8ca)] - **src**: move crypto_bio/clienthello to crypto ns (Daniel Bevenius) [#13957](https://github.com/nodejs/node/pull/13957)
* [[`dff506c5c5`](https://github.com/nodejs/node/commit/dff506c5c5)] - **src**: add missing new line to printed message (Timothy Gu) [#13940](https://github.com/nodejs/node/pull/13940)
* [[`98cb59e9f0`](https://github.com/nodejs/node/commit/98cb59e9f0)] - **src**: revise character width calculation (Timothy Gu) [#13918](https://github.com/nodejs/node/pull/13918)
* [[`5579bc8fb6`](https://github.com/nodejs/node/commit/5579bc8fb6)] - **src,fs**: calculate times as unsigned long (Refael Ackermann) [#13281](https://github.com/nodejs/node/pull/13281)
* [[`864abc567e`](https://github.com/nodejs/node/commit/864abc567e)] - **src,lib,test,doc**: correct misspellings (Roman Reiss) [#13719](https://github.com/nodejs/node/pull/13719)
* [[`6eb53e5611`](https://github.com/nodejs/node/commit/6eb53e5611)] - **stream**: avoid possible slow path w UInt8Array (Matteo Collina) [#13956](https://github.com/nodejs/node/pull/13956)
* [[`6512fd7614`](https://github.com/nodejs/node/commit/6512fd7614)] - **stream**: improve Transform performance (Brian White) [#13322](https://github.com/nodejs/node/pull/13322)
* [[`86e55eff27`](https://github.com/nodejs/node/commit/86e55eff27)] - **test**: add test for http outgoing internal headers (Gergely Nemeth) [#13980](https://github.com/nodejs/node/pull/13980)
* [[`0f52b41cbd`](https://github.com/nodejs/node/commit/0f52b41cbd)] - **test**: use regex error check in test-crypto-random (Zhang Weijie) [#14273](https://github.com/nodejs/node/pull/14273)
* [[`bf663a8550`](https://github.com/nodejs/node/commit/bf663a8550)] - **test**: check error with regex in test-signal-safety (shaman) [#14285](https://github.com/nodejs/node/pull/14285)
* [[`784102f2d1`](https://github.com/nodejs/node/commit/784102f2d1)] - **test**: use regex error checks in test-util-format (Superwoods) [#14299](https://github.com/nodejs/node/pull/14299)
* [[`f9b292c954`](https://github.com/nodejs/node/commit/f9b292c954)] - **test**: change style in test-cli-bad-options (boydfd) [#14274](https://github.com/nodejs/node/pull/14274)
* [[`9257e7ef70`](https://github.com/nodejs/node/commit/9257e7ef70)] - **test**: use template literals in test-writewrap (vercent deng) [#14292](https://github.com/nodejs/node/pull/14292)
* [[`f5e8342057`](https://github.com/nodejs/node/commit/f5e8342057)] - **test**: improve regexps for error checking (xinglong.wangwxl) [#14271](https://github.com/nodejs/node/pull/14271)
* [[`337a8652c7`](https://github.com/nodejs/node/commit/337a8652c7)] - **test**: replace string concatenation with template (weiyuanyue) [#14279](https://github.com/nodejs/node/pull/14279)
* [[`85c181ab78`](https://github.com/nodejs/node/commit/85c181ab78)] - **test**: use template literals as appropriate (blade254353074) [#14289](https://github.com/nodejs/node/pull/14289)
* [[`65bccd519e`](https://github.com/nodejs/node/commit/65bccd519e)] - **test**: use template literal for string concat (tobewhatwewant) [#14288](https://github.com/nodejs/node/pull/14288)
* [[`802783d34a`](https://github.com/nodejs/node/commit/802783d34a)] - **test**: simplify string concatenation (jiangplus) [#14278](https://github.com/nodejs/node/pull/14278)
* [[`76a4671729`](https://github.com/nodejs/node/commit/76a4671729)] - **test**: use regexp to confir error message (Bang Wu) [#14268](https://github.com/nodejs/node/pull/14268)
* [[`e37510a0c7`](https://github.com/nodejs/node/commit/e37510a0c7)] - **test**: use regluar expression in vm test (akira.xue) [#14266](https://github.com/nodejs/node/pull/14266)
* [[`a338b94214`](https://github.com/nodejs/node/commit/a338b94214)] - **test**: use regular expression to match error msg (Flandre) [#14265](https://github.com/nodejs/node/pull/14265)
* [[`c8087c05e8`](https://github.com/nodejs/node/commit/c8087c05e8)] - **test**: replace string concat with template literal (Song, Bintao Garfield) [#14269](https://github.com/nodejs/node/pull/14269)
* [[`c44d899ca1`](https://github.com/nodejs/node/commit/c44d899ca1)] - **test**: check complete error message (Fraser Xu) [#14264](https://github.com/nodejs/node/pull/14264)
* [[`bf9457276b`](https://github.com/nodejs/node/commit/bf9457276b)] - **test**: fix flaky test-net-can-reset-timeout (Rich Trott) [#14257](https://github.com/nodejs/node/pull/14257)
* [[`9efd328d5d`](https://github.com/nodejs/node/commit/9efd328d5d)] - **test**: disable MultipleEnvironmentsPerIsolate (Refael Ackermann) [#14246](https://github.com/nodejs/node/pull/14246)
* [[`724e7e1acf`](https://github.com/nodejs/node/commit/724e7e1acf)] - **test**: make common.PIPE process unique (Refael Ackermann) [#14168](https://github.com/nodejs/node/pull/14168)
* [[`d651a01641`](https://github.com/nodejs/node/commit/d651a01641)] - **(SEMVER-MINOR)** **test**: reduce offset in test-inspector-port-cluster (cornholio) [#14140](https://github.com/nodejs/node/pull/14140)
* [[`f5bea638df`](https://github.com/nodejs/node/commit/f5bea638df)] - **test**: http outgoing \_renderHeaders (Peter Czibik) [#13981](https://github.com/nodejs/node/pull/13981)
* [[`1671fe4506`](https://github.com/nodejs/node/commit/1671fe4506)] - **test**: decrease duration of test-cli-syntax (Evan Lucas) [#14187](https://github.com/nodejs/node/pull/14187)
* [[`3fcc7e6772`](https://github.com/nodejs/node/commit/3fcc7e6772)] - **test**: handle missing V8 tests in n-api test (cjihrig) [#14123](https://github.com/nodejs/node/pull/14123)
* [[`3bc713e45a`](https://github.com/nodejs/node/commit/3bc713e45a)] - **test**: reduce run time for test-benchmark-crypto (Rich Trott) [#14189](https://github.com/nodejs/node/pull/14189)
* [[`73257045a5`](https://github.com/nodejs/node/commit/73257045a5)] - **test**: reduce run time for test-benchmark-http (Rich Trott) [#14180](https://github.com/nodejs/node/pull/14180)
* [[`cd9eba9da8`](https://github.com/nodejs/node/commit/cd9eba9da8)] - **test**: reduce test-benchmark-net run duration (Rich Trott) [#14183](https://github.com/nodejs/node/pull/14183)
* [[`de842498fa`](https://github.com/nodejs/node/commit/de842498fa)] - **test**: fix flaky test-https-set-timeout-server (Rich Trott) [#14134](https://github.com/nodejs/node/pull/14134)
* [[`e879a56aec`](https://github.com/nodejs/node/commit/e879a56aec)] - **test**: remove common.noop (Rich Trott) [#12822](https://github.com/nodejs/node/pull/12822)
* [[`697ea62f39`](https://github.com/nodejs/node/commit/697ea62f39)] - **test**: add get/set effective uid/gid tests (Evan Lucas) [#14091](https://github.com/nodejs/node/pull/14091)
* [[`d0e4e2b5c5`](https://github.com/nodejs/node/commit/d0e4e2b5c5)] - **test**: fix cctest failure on Windows (Jimmy Thomson) [#14111](https://github.com/nodejs/node/pull/14111)
* [[`e080fb349e`](https://github.com/nodejs/node/commit/e080fb349e)] - **test**: ignore connection errors for hostname check (Refael Ackermann) [#14073](https://github.com/nodejs/node/pull/14073)
* [[`9cfa52a568`](https://github.com/nodejs/node/commit/9cfa52a568)] - **test**: check and fail inspector-cluster-port-clash (Daniel Bevenius) [#14074](https://github.com/nodejs/node/pull/14074)
* [[`2a91d59c49`](https://github.com/nodejs/node/commit/2a91d59c49)] - **test**: add coverage for napi_typeof (Michael Dawson) [#13990](https://github.com/nodejs/node/pull/13990)
* [[`e71b98f9f7`](https://github.com/nodejs/node/commit/e71b98f9f7)] - **test**: restore no-op function in test (Rich Trott) [#14065](https://github.com/nodejs/node/pull/14065)
* [[`d288cf10cc`](https://github.com/nodejs/node/commit/d288cf10cc)] - **test**: skip test-fs-readdir-ucs2 if no support (Rich Trott) [#14029](https://github.com/nodejs/node/pull/14029)
* [[`32a8f368ab`](https://github.com/nodejs/node/commit/32a8f368ab)] - **test**: simplify test skipping (Vse Mozhet Byt) [#14021](https://github.com/nodejs/node/pull/14021)
* [[`0cc12fc646`](https://github.com/nodejs/node/commit/0cc12fc646)] - **test**: fix require nits in some test-tls-* tests (Vse Mozhet Byt) [#14008](https://github.com/nodejs/node/pull/14008)
* [[`0707a6b2b5`](https://github.com/nodejs/node/commit/0707a6b2b5)] - **test**: refactor test-http-hostname-typechecking (Rich Trott) [#13993](https://github.com/nodejs/node/pull/13993)
* [[`534ae446c6`](https://github.com/nodejs/node/commit/534ae446c6)] - **test**: refactor test-http(s)-set-timeout-server (Alexey Orlenko) [#13935](https://github.com/nodejs/node/pull/13935)
* [[`81c644795d`](https://github.com/nodejs/node/commit/81c644795d)] - **test**: refactor test-http-invalidheaderfield (Rich Trott) [#13996](https://github.com/nodejs/node/pull/13996)
* [[`8edde98f16`](https://github.com/nodejs/node/commit/8edde98f16)] - **test**: change var to const in ./common (Ruben Bridgewater) [#13732](https://github.com/nodejs/node/pull/13732)
* [[`cfb6f94b30`](https://github.com/nodejs/node/commit/cfb6f94b30)] - **test**: mark test-npm-install flaky on arm (Refael Ackermann) [#14035](https://github.com/nodejs/node/pull/14035)
* [[`50ee4bd598`](https://github.com/nodejs/node/commit/50ee4bd598)] - **test**: replace indexOf with includes and startsWith (Nataly Shrits) [#13852](https://github.com/nodejs/node/pull/13852)
* [[`f1ef692454`](https://github.com/nodejs/node/commit/f1ef692454)] - **test**: refactor test-fs-options-immutable (Rich Trott) [#13977](https://github.com/nodejs/node/pull/13977)
* [[`bb198dcda9`](https://github.com/nodejs/node/commit/bb198dcda9)] - **test**: refactor test-crypto-pbkdf2 (Rich Trott) [#13975](https://github.com/nodejs/node/pull/13975)
* [[`4ba1d32609`](https://github.com/nodejs/node/commit/4ba1d32609)] - **test**: remove undef NDEBUG from at-exit addons test (Daniel Bevenius) [#13998](https://github.com/nodejs/node/pull/13998)
* [[`f400939206`](https://github.com/nodejs/node/commit/f400939206)] - **test**: verify napi_get_property() walks prototype (cjihrig) [#13961](https://github.com/nodejs/node/pull/13961)
* [[`100ccf9ad4`](https://github.com/nodejs/node/commit/100ccf9ad4)] - **test**: refactor test-fs-watchfile (Rich Trott) [#13721](https://github.com/nodejs/node/pull/13721)
* [[`f7383eb80e`](https://github.com/nodejs/node/commit/f7383eb80e)] - **test**: verify isNativeError accepts internal errors (cjihrig) [#13965](https://github.com/nodejs/node/pull/13965)
* [[`071ecb0dd2`](https://github.com/nodejs/node/commit/071ecb0dd2)] - **test**: refactor test-child-process-send-type-error (Rich Trott) [#13904](https://github.com/nodejs/node/pull/13904)
* [[`e5d32b8b13`](https://github.com/nodejs/node/commit/e5d32b8b13)] - **test**: mark test-fs-readdir-ucs2 flaky (João Reis) [#13989](https://github.com/nodejs/node/pull/13989)
* [[`fa9e647385`](https://github.com/nodejs/node/commit/fa9e647385)] - **test**: fix failure in test-icu-data-dir.js (Tobias Nießen) [#13987](https://github.com/nodejs/node/pull/13987)
* [[`b43547acc6`](https://github.com/nodejs/node/commit/b43547acc6)] - **test**: refactor test-cluster-basic (Rich Trott) [#13905](https://github.com/nodejs/node/pull/13905)
* [[`98ec8aaa30`](https://github.com/nodejs/node/commit/98ec8aaa30)] - **test**: refactor test-vm-sigint (Rich Trott) [#13902](https://github.com/nodejs/node/pull/13902)
* [[`949d1b1d4a`](https://github.com/nodejs/node/commit/949d1b1d4a)] - **test**: refactor test-tls-two-cas-one-string (Rich Trott) [#13896](https://github.com/nodejs/node/pull/13896)
* [[`c4018e8a48`](https://github.com/nodejs/node/commit/c4018e8a48)] - **test**: remove unneeded HandleScope usage (Ezequiel Garcia) [#13859](https://github.com/nodejs/node/pull/13859)
* [[`6120a0de6c`](https://github.com/nodejs/node/commit/6120a0de6c)] - **test**: skip fips tests using OpenSSL config file (Daniel Bevenius) [#13786](https://github.com/nodejs/node/pull/13786)
* [[`74aed0b6bd`](https://github.com/nodejs/node/commit/74aed0b6bd)] - **test**: refactor test-tls-invoked-queued (Rich Trott) [#13893](https://github.com/nodejs/node/pull/13893)
* [[`a767367123`](https://github.com/nodejs/node/commit/a767367123)] - **test**: refactor test-tls-env-extra-ca (Rich Trott) [#13886](https://github.com/nodejs/node/pull/13886)
* [[`265957334c`](https://github.com/nodejs/node/commit/265957334c)] - **test**: make http(s)-set-timeout-server more similar (Julien Klepatch) [#13822](https://github.com/nodejs/node/pull/13822)
* [[`587c905d11`](https://github.com/nodejs/node/commit/587c905d11)] - **test**: check uv_ip4_addr return value (Eugene Ostroukhov) [#13878](https://github.com/nodejs/node/pull/13878)
* [[`005e343339`](https://github.com/nodejs/node/commit/005e343339)] - **test**: remove `require('buffer')` from 4 test files (XadillaX) [#13844](https://github.com/nodejs/node/pull/13844)
* [[`df3c2929b9`](https://github.com/nodejs/node/commit/df3c2929b9)] - **test**: remove unnecessary require('buffer').Buffer (lena) [#13851](https://github.com/nodejs/node/pull/13851)
* [[`ec3761b1da`](https://github.com/nodejs/node/commit/ec3761b1da)] - **test**: remove `require('buffer')` from 4 test files (Zongmin Lei) [#13846](https://github.com/nodejs/node/pull/13846)
* [[`c3c6699bb3`](https://github.com/nodejs/node/commit/c3c6699bb3)] - **test**: remove require('buffer') from 4 buffer tests (OriLev) [#13855](https://github.com/nodejs/node/pull/13855)
* [[`4a6604193f`](https://github.com/nodejs/node/commit/4a6604193f)] - **test**: remove require('buffer') on 6 fs test files (sallen450) [#13845](https://github.com/nodejs/node/pull/13845)
* [[`76cdaec2b3`](https://github.com/nodejs/node/commit/76cdaec2b3)] - **test**: remove unnecessary Buffer import (Steven Winston) [#13860](https://github.com/nodejs/node/pull/13860)
* [[`b15378cc90`](https://github.com/nodejs/node/commit/b15378cc90)] - **test**: improve async-hooks/test-callback-error (Refael Ackermann) [#13559](https://github.com/nodejs/node/pull/13559)
* [[`7e3bab779a`](https://github.com/nodejs/node/commit/7e3bab779a)] - **test**: use string instead of RegExp in split() (Vse Mozhet Byt) [#13710](https://github.com/nodejs/node/pull/13710)
* [[`0e857a5ee4`](https://github.com/nodejs/node/commit/0e857a5ee4)] - **test**: remove needless RegExp flags (Vse Mozhet Byt) [#13690](https://github.com/nodejs/node/pull/13690)
* [[`022c6d080c`](https://github.com/nodejs/node/commit/022c6d080c)] - **test**: add crypto check to test-tls-wrap-econnreset (Daniel Bevenius) [#13691](https://github.com/nodejs/node/pull/13691)
* [[`bf22514ae4`](https://github.com/nodejs/node/commit/bf22514ae4)] - **test**: increase util.callbackify() coverage (cjihrig) [#13705](https://github.com/nodejs/node/pull/13705)
* [[`b717609e86`](https://github.com/nodejs/node/commit/b717609e86)] - **test,async_hooks**: match test-ttywrap.readstream (Trevor Norris) [#13991](https://github.com/nodejs/node/pull/13991)
* [[`1fc5c29f28`](https://github.com/nodejs/node/commit/1fc5c29f28)] - **test,async_hooks**: skip whether TTY is available (Trevor Norris) [#13991](https://github.com/nodejs/node/pull/13991)
* [[`3d9bc01734`](https://github.com/nodejs/node/commit/3d9bc01734)] - **test,async_hooks**: stabilize tests on Windows (Refael Ackermann) [#13381](https://github.com/nodejs/node/pull/13381)
* [[`b9e07f9fec`](https://github.com/nodejs/node/commit/b9e07f9fec)] - **test,fs**: delay unlink in test-regress-GH-4027.js (Jaime Bernardo) [#14010](https://github.com/nodejs/node/pull/14010)
* [[`e2d325403f`](https://github.com/nodejs/node/commit/e2d325403f)] - **(SEMVER-MINOR)** **tls**: add host and port info to ECONNRESET errors (José F. Romaniello) [#7476](https://github.com/nodejs/node/pull/7476)
* [[`55438024a6`](https://github.com/nodejs/node/commit/55438024a6)] - **tools**: update package.json `engine` field (AJ Jordan) [#14165](https://github.com/nodejs/node/pull/14165)
* [[`36c267cbe9`](https://github.com/nodejs/node/commit/36c267cbe9)] - **tools**: increase test timeouts (Rich Trott) [#14197](https://github.com/nodejs/node/pull/14197)
* [[`ef53149203`](https://github.com/nodejs/node/commit/ef53149203)] - **tools**: update ESLint to 4.2.0 (Rich Trott) [#14155](https://github.com/nodejs/node/pull/14155)
* [[`b97e140241`](https://github.com/nodejs/node/commit/b97e140241)] - **tools**: generate template literal for addon tests (Rich Trott) [#14094](https://github.com/nodejs/node/pull/14094)
* [[`e17fb82c06`](https://github.com/nodejs/node/commit/e17fb82c06)] - **tools**: fix error in eslintrc comment (Rich Trott) [#14108](https://github.com/nodejs/node/pull/14108)
* [[`f8d76dcc82`](https://github.com/nodejs/node/commit/f8d76dcc82)] - **tools**: remove align-multiline-assignment lint rule (Rich Trott) [#14079](https://github.com/nodejs/node/pull/14079)
* [[`7d7da98703`](https://github.com/nodejs/node/commit/7d7da98703)] - **tools**: eslint - use `error` and `off` (Refael Ackermann) [#14061](https://github.com/nodejs/node/pull/14061)
* [[`aa4a700ddb`](https://github.com/nodejs/node/commit/aa4a700ddb)] - **tools**: update: eslint-plugin-markdown@1.0.0-beta.7 (Vse Mozhet Byt) [#14047](https://github.com/nodejs/node/pull/14047)
* [[`e03774236a`](https://github.com/nodejs/node/commit/e03774236a)] - **tools**: use no-use-before-define ESLint rule (Vse Mozhet Byt) [#14032](https://github.com/nodejs/node/pull/14032)
* [[`d69527f426`](https://github.com/nodejs/node/commit/d69527f426)] - **tools**: change var to const in ./eslint-rules (Ruben Bridgewater) [#13732](https://github.com/nodejs/node/pull/13732)
* [[`d454add7ce`](https://github.com/nodejs/node/commit/d454add7ce)] - **tools**: change var to const in ./doc/html (Ruben Bridgewater) [#13732](https://github.com/nodejs/node/pull/13732)
* [[`7ed7b22e67`](https://github.com/nodejs/node/commit/7ed7b22e67)] - **tools**: change var to const in ./license2rtf (Ruben Bridgewater) [#13732](https://github.com/nodejs/node/pull/13732)
* [[`f3bff93e21`](https://github.com/nodejs/node/commit/f3bff93e21)] - **tools**: change var to const in ./doc/preprocess (Ruben Bridgewater) [#13732](https://github.com/nodejs/node/pull/13732)
* [[`148f49fcdc`](https://github.com/nodejs/node/commit/148f49fcdc)] - **tools**: change var to const in ./doc/json (Ruben Bridgewater) [#13732](https://github.com/nodejs/node/pull/13732)
* [[`b89c27d360`](https://github.com/nodejs/node/commit/b89c27d360)] - **tools**: change var to const in ./doc/addon-verify (Ruben Bridgewater) [#13732](https://github.com/nodejs/node/pull/13732)
* [[`17636f64db`](https://github.com/nodejs/node/commit/17636f64db)] - **tools**: update to ESLint 4.1.1 (Rich Trott) [#13946](https://github.com/nodejs/node/pull/13946)
* [[`42ef8f9161`](https://github.com/nodejs/node/commit/42ef8f9161)] - **tools**: remove comment in eslint rule (Daniel Bevenius) [#13945](https://github.com/nodejs/node/pull/13945)
* [[`84b1641182`](https://github.com/nodejs/node/commit/84b1641182)] - **tools**: disable legacy indentation linting in tools (Rich Trott) [#13895](https://github.com/nodejs/node/pull/13895)
* [[`c732bf613d`](https://github.com/nodejs/node/commit/c732bf613d)] - **tools**: add script to update ESLint (Rich Trott) [#13895](https://github.com/nodejs/node/pull/13895)
* [[`6a5c37655d`](https://github.com/nodejs/node/commit/6a5c37655d)] - **tools**: update to ESLint 4.1.0 (Rich Trott) [#13895](https://github.com/nodejs/node/pull/13895)
* [[`4ecff6cad7`](https://github.com/nodejs/node/commit/4ecff6cad7)] - **tools,benchmark**: use stricter indentation linting (Rich Trott) [#13895](https://github.com/nodejs/node/pull/13895)
* [[`d23c49f951`](https://github.com/nodejs/node/commit/d23c49f951)] - **url**: do not use HandleScope in ToObject (Bradley Farias) [#14096](https://github.com/nodejs/node/pull/14096)
* [[`cf6afe3331`](https://github.com/nodejs/node/commit/cf6afe3331)] - **url**: normalize port on scheme change (Timothy Gu) [#13997](https://github.com/nodejs/node/pull/13997)
* [[`783cf50a76`](https://github.com/nodejs/node/commit/783cf50a76)] - **util**: delete unused argument 'depth' (kadoufall) [#14267](https://github.com/nodejs/node/pull/14267)
* [[`a675c3d3b7`](https://github.com/nodejs/node/commit/a675c3d3b7)] - **util**: remove redundant declaration (Vse Mozhet Byt) [#14199](https://github.com/nodejs/node/pull/14199)
* [[`8cba959a93`](https://github.com/nodejs/node/commit/8cba959a93)] - **util**: add callbackify (Refael Ackermann) [#13750](https://github.com/nodejs/node/pull/13750)

<a id="8.1.4"></a>
## 2017-07-11, Version 8.1.4 (Current), @evanlucas

This is a security release. All Node.js users should consult the security release summary at https://nodejs.org/en/blog/vulnerability/july-2017-security-releases/ for details on patched vulnerabilities.

### Notable changes

* **build**:
  - Disable V8 snapshots - The hashseed embedded in the snapshot is currently the same for all runs of the binary. This opens node up to collision attacks which could result in a Denial of Service. We have temporarily disabled snapshots until a more robust solution is found (Ali Ijaz Sheikh)
* **deps**:
  - CVE-2017-1000381 - The c-ares function ares_parse_naptr_reply(), which is used for parsing NAPTR responses, could be triggered to read memory outside of the given input buffer if the passed in DNS response packet was crafted in a particular way. This patch checks that there is enough data for the required elements of an NAPTR record (2 int16, 3 bytes for string lengths) before processing a record. (David Drysdale)

### Commits

* [[`51d69d2bec`](https://github.com/nodejs/node/commit/51d69d2bec)] - **build**: disable V8 snapshots (Ali Ijaz Sheikh) [nodejs/node-private#84](https://github.com/nodejs/node-private/pull/84)
* [[`d70fac47af`](https://github.com/nodejs/node/commit/d70fac47af)] - **deps**: cherry-pick 9478908a49 from cares upstream (David Drysdale) [nodejs/node-private#88](https://github.com/nodejs/node-private/pull/88)
* [[`803d689873`](https://github.com/nodejs/node/commit/803d689873)] - **test**: verify hash seed uniqueness (Ali Ijaz Sheikh) [nodejs/node-private#84](https://github.com/nodejs/node-private/pull/84)

<a id="8.1.3"></a>
## 2017-06-29, Version 8.1.3 (Current), @addaleax

### Notable changes

* **Stream**
  Two regressions with the `stream` module have been fixed:
  * The `finish` event will now always be emitted after the `error` event
    if one is emitted:
    [[`0a9e96e86c`](https://github.com/nodejs/node/commit/0a9e96e86c)]
    [#13850](https://github.com/nodejs/node/pull/13850)
  * In object mode, readable streams can now use `undefined` again.
    [[`5840138e70`](https://github.com/nodejs/node/commit/5840138e70)]
    [#13760](https://github.com/nodejs/node/pull/13760)

### Commits

* [[`11f45623ac`](https://github.com/nodejs/node/commit/11f45623ac)] - **benchmark**: remove needless RegExp capturing (Vse Mozhet Byt) [#13718](https://github.com/nodejs/node/pull/13718)
* [[`2ce236e173`](https://github.com/nodejs/node/commit/2ce236e173)] - **build**: check for linter in bin rather than lib (Rich Trott) [#13645](https://github.com/nodejs/node/pull/13645)
* [[`18f073f0fe`](https://github.com/nodejs/node/commit/18f073f0fe)] - **build**: fail linter if linting not available (Gibson Fahnestock) [#13658](https://github.com/nodejs/node/pull/13658)
* [[`465bd48b14`](https://github.com/nodejs/node/commit/465bd48b14)] - **configure**: add mips64el to valid_arch (Aditya Anand) [#13620](https://github.com/nodejs/node/pull/13620)
* [[`1fe455f525`](https://github.com/nodejs/node/commit/1fe455f525)] - **dgram**: change parameter name in set(Multicast)TTL (Tobias Nießen) [#13747](https://github.com/nodejs/node/pull/13747)
* [[`a63e54a94c`](https://github.com/nodejs/node/commit/a63e54a94c)] - **doc**: update backporting guide (Refael Ackermann) [#13749](https://github.com/nodejs/node/pull/13749)
* [[`0bb53a7aa2`](https://github.com/nodejs/node/commit/0bb53a7aa2)] - **doc**: make socket IPC examples more robust (cjihrig) [#13196](https://github.com/nodejs/node/pull/13196)
* [[`57b7285400`](https://github.com/nodejs/node/commit/57b7285400)] - **doc**: mention rebasing of v?.x-staging post release (Anna Henningsen) [#13742](https://github.com/nodejs/node/pull/13742)
* [[`cb932835d5`](https://github.com/nodejs/node/commit/cb932835d5)] - **doc**: `path.relative` uses `cwd` (DuanPengfei) [#13714](https://github.com/nodejs/node/pull/13714)
* [[`61714acbe5`](https://github.com/nodejs/node/commit/61714acbe5)] - **doc**: add hasIntl to test/common/README.md (Daniel Bevenius) [#13699](https://github.com/nodejs/node/pull/13699)
* [[`2a95cfb4ef`](https://github.com/nodejs/node/commit/2a95cfb4ef)] - **doc**: fix typo in changelog (Teddy Katz) [#13713](https://github.com/nodejs/node/pull/13713)
* [[`31ae193b99`](https://github.com/nodejs/node/commit/31ae193b99)] - **doc**: small makeover for onboarding.md (Anna Henningsen) [#13413](https://github.com/nodejs/node/pull/13413)
* [[`c27ffadf8e`](https://github.com/nodejs/node/commit/c27ffadf8e)] - **doc**: fix a few n-api doc issues (Michael Dawson) [#13650](https://github.com/nodejs/node/pull/13650)
* [[`c142f1d316`](https://github.com/nodejs/node/commit/c142f1d316)] - **doc**: fix minor issues reported in #9538 (Tobias Nießen) [#13491](https://github.com/nodejs/node/pull/13491)
* [[`f28dd8e680`](https://github.com/nodejs/node/commit/f28dd8e680)] - **doc**: fixes a typo in the async_hooks documentation (Chris Young) [#13666](https://github.com/nodejs/node/pull/13666)
* [[`58e177cde1`](https://github.com/nodejs/node/commit/58e177cde1)] - **doc**: document and test that methods return this (Sam Roberts) [#13531](https://github.com/nodejs/node/pull/13531)
* [[`f5f2a0e968`](https://github.com/nodejs/node/commit/f5f2a0e968)] - **doc**: sort and update /cc list for inspector issues (Aditya Anand) [#13632](https://github.com/nodejs/node/pull/13632)
* [[`dc06a0a85a`](https://github.com/nodejs/node/commit/dc06a0a85a)] - **doc**: note that EoL platforms are not supported (Gibson Fahnestock) [#12672](https://github.com/nodejs/node/pull/12672)
* [[`9b74dded0d`](https://github.com/nodejs/node/commit/9b74dded0d)] - **doc**: update async_hooks providers list (Anna Henningsen) [#13561](https://github.com/nodejs/node/pull/13561)
* [[`cc922310e3`](https://github.com/nodejs/node/commit/cc922310e3)] - **doc**: fix out of date napi_callback doc (XadillaX) [#13570](https://github.com/nodejs/node/pull/13570)
* [[`8cb7d96569`](https://github.com/nodejs/node/commit/8cb7d96569)] - **fs**: don't conflate data and callback in appendFile (Nikolai Vavilov) [#11607](https://github.com/nodejs/node/pull/11607)
* [[`233545a81c`](https://github.com/nodejs/node/commit/233545a81c)] - **inspector,cluster**: fix inspect port assignment (cornholio) [#13619](https://github.com/nodejs/node/pull/13619)
* [[`cbe7c5c617`](https://github.com/nodejs/node/commit/cbe7c5c617)] - **lib**: correct typo in createSecureContext (Daniel Bevenius) [#13653](https://github.com/nodejs/node/pull/13653)
* [[`f49dd21b2f`](https://github.com/nodejs/node/commit/f49dd21b2f)] - **n-api**: avoid crash in napi_escape_scope() (Michael Dawson) [#13651](https://github.com/nodejs/node/pull/13651)
* [[`28166770bd`](https://github.com/nodejs/node/commit/28166770bd)] - **net**: fix abort on bad address input (Ruben Bridgewater) [#13726](https://github.com/nodejs/node/pull/13726)
* [[`e786926de9`](https://github.com/nodejs/node/commit/e786926de9)] - **readline,repl,url,util**: remove needless capturing (Vse Mozhet Byt) [#13718](https://github.com/nodejs/node/pull/13718)
* [[`3322191d2f`](https://github.com/nodejs/node/commit/3322191d2f)] - **src**: don't set --icu_case_mapping flag on startup (Ben Noordhuis) [#13698](https://github.com/nodejs/node/pull/13698)
* [[`a27a35b997`](https://github.com/nodejs/node/commit/a27a35b997)] - **src**: fix decoding base64 with whitespace (Nikolai Vavilov) [#13660](https://github.com/nodejs/node/pull/13660)
* [[`5b3e5fac38`](https://github.com/nodejs/node/commit/5b3e5fac38)] - **src**: remove void casts for clear_error_on_return (Daniel Bevenius) [#13669](https://github.com/nodejs/node/pull/13669)
* [[`0a9e96e86c`](https://github.com/nodejs/node/commit/0a9e96e86c)] - **stream**: finish must always follow error (Matteo Collina) [#13850](https://github.com/nodejs/node/pull/13850)
* [[`5840138e70`](https://github.com/nodejs/node/commit/5840138e70)] - **stream**: fix `undefined` in Readable object mode (Anna Henningsen) [#13760](https://github.com/nodejs/node/pull/13760)
* [[`f1d96f0b2a`](https://github.com/nodejs/node/commit/f1d96f0b2a)] - **test**: refactor test-http-set-timeout-server (Rich Trott) [#13802](https://github.com/nodejs/node/pull/13802)
* [[`b23f2461cb`](https://github.com/nodejs/node/commit/b23f2461cb)] - **test**: refactor test-stream2-writable (Rich Trott) [#13823](https://github.com/nodejs/node/pull/13823)
* [[`9ff9782f66`](https://github.com/nodejs/node/commit/9ff9782f66)] - **test**: remove common module from test it thwarts (Rich Trott) [#13748](https://github.com/nodejs/node/pull/13748)
* [[`1f32d9ef5b`](https://github.com/nodejs/node/commit/1f32d9ef5b)] - **test**: fix RegExp nits (Vse Mozhet Byt) [#13770](https://github.com/nodejs/node/pull/13770)
* [[`3306fd1d97`](https://github.com/nodejs/node/commit/3306fd1d97)] - **test**: accommodate AIX by watching file (Rich Trott) [#13766](https://github.com/nodejs/node/pull/13766)
* [[`c8b134bc6d`](https://github.com/nodejs/node/commit/c8b134bc6d)] - **test**: remove node-tap lookalike (cjihrig) [#13707](https://github.com/nodejs/node/pull/13707)
* [[`d4a05b2d9c`](https://github.com/nodejs/node/commit/d4a05b2d9c)] - **test**: make test-http(s)-set-timeout-server alike (jklepatch) [#13625](https://github.com/nodejs/node/pull/13625)
* [[`d0f39cc38a`](https://github.com/nodejs/node/commit/d0f39cc38a)] - **test**: delete outdated fixtures/stdio-filter.js (Vse Mozhet Byt) [#13712](https://github.com/nodejs/node/pull/13712)
* [[`b2a5399760`](https://github.com/nodejs/node/commit/b2a5399760)] - **test**: refactor test-fs-watch-stop-sync (Rich Trott) [#13689](https://github.com/nodejs/node/pull/13689)
* [[`10aee10c0c`](https://github.com/nodejs/node/commit/10aee10c0c)] - **test**: check zlib version for createDeflateRaw (Daniel Bevenius) [#13697](https://github.com/nodejs/node/pull/13697)
* [[`0d3b52e9de`](https://github.com/nodejs/node/commit/0d3b52e9de)] - **test**: add hasIntl to failing test (Daniel Bevenius) [#13699](https://github.com/nodejs/node/pull/13699)
* [[`70fb1bd038`](https://github.com/nodejs/node/commit/70fb1bd038)] - **test**: improve http test reliability (Brian White) [#13693](https://github.com/nodejs/node/pull/13693)
* [[`5e59c2d21d`](https://github.com/nodejs/node/commit/5e59c2d21d)] - **test**: increase coverage for internal/module.js (Tamás Hódi) [#13673](https://github.com/nodejs/node/pull/13673)
* [[`ba20627520`](https://github.com/nodejs/node/commit/ba20627520)] - **test**: refactor test-fs-read-stream (Rich Trott) [#13643](https://github.com/nodejs/node/pull/13643)
* [[`e203e392d7`](https://github.com/nodejs/node/commit/e203e392d7)] - **test**: refactor test-cluster-worker-isconnected.js (cjihrig) [#13685](https://github.com/nodejs/node/pull/13685)
* [[`80e6524ff0`](https://github.com/nodejs/node/commit/80e6524ff0)] - **test**: fix nits in test-fs-mkdir-rmdir.js (Vse Mozhet Byt) [#13680](https://github.com/nodejs/node/pull/13680)
* [[`406c09aacb`](https://github.com/nodejs/node/commit/406c09aacb)] - **test**: fix test-inspector-port-zero-cluster (Refael Ackermann) [#13373](https://github.com/nodejs/node/pull/13373)
* [[`af46cf621b`](https://github.com/nodejs/node/commit/af46cf621b)] - **test**: refactor test-fs-watch-stop-async (Rich Trott) [#13661](https://github.com/nodejs/node/pull/13661)
* [[`6920d5c9f9`](https://github.com/nodejs/node/commit/6920d5c9f9)] - **test**: change deprecated method to recommended (Rich Trott) [#13649](https://github.com/nodejs/node/pull/13649)
* [[`0d87b3102a`](https://github.com/nodejs/node/commit/0d87b3102a)] - **test**: refactor test-fs-read-stream-inherit (Rich Trott) [#13618](https://github.com/nodejs/node/pull/13618)
* [[`80fa13b93f`](https://github.com/nodejs/node/commit/80fa13b93f)] - **test**: use mustNotCall() in test-fs-watch (Rich Trott) [#13595](https://github.com/nodejs/node/pull/13595)
* [[`7874360ca2`](https://github.com/nodejs/node/commit/7874360ca2)] - **test**: add mustCall() to child-process test (Rich Trott) [#13605](https://github.com/nodejs/node/pull/13605)
* [[`5cb3fac396`](https://github.com/nodejs/node/commit/5cb3fac396)] - **test**: use mustNotCall in test-http-eof-on-connect (Rich Trott) [#13587](https://github.com/nodejs/node/pull/13587)
* [[`4afa7483b1`](https://github.com/nodejs/node/commit/4afa7483b1)] - **test**: increase bufsize in child process write test (Rich Trott) [#13626](https://github.com/nodejs/node/pull/13626)
* [[`0ef687e858`](https://github.com/nodejs/node/commit/0ef687e858)] - **tools**: fix error in custom ESLint rule (Rich Trott) [#13758](https://github.com/nodejs/node/pull/13758)
* [[`b171e728e5`](https://github.com/nodejs/node/commit/b171e728e5)] - **tools**: apply stricter indentation rules to tools (Rich Trott) [#13758](https://github.com/nodejs/node/pull/13758)
* [[`9c2abc3e29`](https://github.com/nodejs/node/commit/9c2abc3e29)] - **tools**: fix indentation in required-modules.js (Rich Trott) [#13758](https://github.com/nodejs/node/pull/13758)
* [[`ff568d4b63`](https://github.com/nodejs/node/commit/ff568d4b63)] - **tools**: update ESLint to v4.0.0 (Rich Trott) [#13645](https://github.com/nodejs/node/pull/13645)
* [[`c046a21321`](https://github.com/nodejs/node/commit/c046a21321)] - **util**: ignore invalid format specifiers (Michaël Zasso) [#13674](https://github.com/nodejs/node/pull/13674)
* [[`c68e472b76`](https://github.com/nodejs/node/commit/c68e472b76)] - **v8**: fix RegExp nits in v8_prof_polyfill.js (Vse Mozhet Byt) [#13709](https://github.com/nodejs/node/pull/13709)

<a id="8.1.2"></a>
## 2017-06-15, Version 8.1.2 (Current), @rvagg

### Notable changes

Release to fix broken `process.release` properties
Ref: https://github.com/nodejs/node/issues/13667

<a id="8.1.1"></a>
## 2017-06-13, Version 8.1.1 (Current), @addaleax

### Notable changes

* **Child processes**
  * `stdout` and `stderr` are now available on the error output of a
    failed call to the `util.promisify()`ed version of
    `child_process.exec`.
    [[`d66d4fc94c`](https://github.com/nodejs/node/commit/d66d4fc94c)]
    [#13388](https://github.com/nodejs/node/pull/13388)

* **HTTP**
  * A regression that broke certain scenarios in which HTTP is used together
    with the `cluster` module has been fixed.
    [[`fff8a56d6f`](https://github.com/nodejs/node/commit/fff8a56d6f)]
    [#13578](https://github.com/nodejs/node/pull/13578)

* **HTTPS**
  * The `rejectUnauthorized` option now works properly for unix sockets.
    [[`c4cbd99d37`](https://github.com/nodejs/node/commit/c4cbd99d37)]
    [#13505](https://github.com/nodejs/node/pull/13505)

* **Readline**
  * A change that broke `npm init` and other code which uses `readline`
    multiple times on the same input stream is reverted.
    [[`0df6c0b5f0`](https://github.com/nodejs/node/commit/0df6c0b5f0)]
    [#13560](https://github.com/nodejs/node/pull/13560)

### Commits

* [[`61c73085ba`](https://github.com/nodejs/node/commit/61c73085ba)] - **async_hooks**: minor refactor to callback invocation (Anna Henningsen) [#13419](https://github.com/nodejs/node/pull/13419)
* [[`bf61d97742`](https://github.com/nodejs/node/commit/bf61d97742)] - **async_hooks**: make sure `.{en|dis}able() === this` (Anna Henningsen) [#13418](https://github.com/nodejs/node/pull/13418)
* [[`32c87ac6f3`](https://github.com/nodejs/node/commit/32c87ac6f3)] - **benchmark**: fix some RegExp nits (Vse Mozhet Byt) [#13551](https://github.com/nodejs/node/pull/13551)
* [[`b967b4cbc5`](https://github.com/nodejs/node/commit/b967b4cbc5)] - **build**: merge test suite groups (Refael Ackermann) [#13378](https://github.com/nodejs/node/pull/13378)
* [[`00d2f7c818`](https://github.com/nodejs/node/commit/00d2f7c818)] - **build,windows**: check for VS version and arch (Refael Ackermann) [#13485](https://github.com/nodejs/node/pull/13485)
* [[`d66d4fc94c`](https://github.com/nodejs/node/commit/d66d4fc94c)] - **child_process**: promisify includes stdio in error (Gil Tayar) [#13388](https://github.com/nodejs/node/pull/13388)
* [[`0ca4bd1e18`](https://github.com/nodejs/node/commit/0ca4bd1e18)] - **child_process**: reduce nextTick() usage (Brian White) [#13459](https://github.com/nodejs/node/pull/13459)
* [[`d1fa59fbb7`](https://github.com/nodejs/node/commit/d1fa59fbb7)] - **child_process**: simplify send() result handling (Brian White) [#13459](https://github.com/nodejs/node/pull/13459)
* [[`d51b1c2e6f`](https://github.com/nodejs/node/commit/d51b1c2e6f)] - **cluster, dns, repl, tls, util**: fix RegExp nits (Vse Mozhet Byt) [#13536](https://github.com/nodejs/node/pull/13536)
* [[`68c0518e48`](https://github.com/nodejs/node/commit/68c0518e48)] - **doc**: fix links and typos in fs.md (Vse Mozhet Byt) [#13573](https://github.com/nodejs/node/pull/13573)
* [[`70432f2111`](https://github.com/nodejs/node/commit/70432f2111)] - **doc**: fix incorrect fs.utimes() link (Justin Beckwith) [#13608](https://github.com/nodejs/node/pull/13608)
* [[`26d76307d5`](https://github.com/nodejs/node/commit/26d76307d5)] - **doc**: fs constants for Node \< v6.3.0 in fs.md (Anshul Guleria) [#12690](https://github.com/nodejs/node/pull/12690)
* [[`52f5e3f804`](https://github.com/nodejs/node/commit/52f5e3f804)] - **doc**: use HTTPS URL for suggested upstream remote (Nikolai Vavilov) [#13602](https://github.com/nodejs/node/pull/13602)
* [[`2c1133d5fe`](https://github.com/nodejs/node/commit/2c1133d5fe)] - **doc**: add readline.emitKeypressEvents note (Samuel Reed) [#9447](https://github.com/nodejs/node/pull/9447)
* [[`53ec50d971`](https://github.com/nodejs/node/commit/53ec50d971)] - **doc**: fix napi_create_*_error signatures in n-api (Jamen Marzonie) [#13544](https://github.com/nodejs/node/pull/13544)
* [[`98d7f25181`](https://github.com/nodejs/node/commit/98d7f25181)] - **doc**: fix out of date sections in n-api doc (Michael Dawson) [#13508](https://github.com/nodejs/node/pull/13508)
* [[`85cac4ed53`](https://github.com/nodejs/node/commit/85cac4ed53)] - **doc**: update new CTC members (Refael Ackermann) [#13534](https://github.com/nodejs/node/pull/13534)
* [[`8c5407d321`](https://github.com/nodejs/node/commit/8c5407d321)] - **doc**: corrects reference to tlsClientError (Tarun) [#13533](https://github.com/nodejs/node/pull/13533)
* [[`3d12e1b455`](https://github.com/nodejs/node/commit/3d12e1b455)] - **doc**: emphasize Collaborators in GOVERNANCE.md (Rich Trott) [#13423](https://github.com/nodejs/node/pull/13423)
* [[`a9be8fff58`](https://github.com/nodejs/node/commit/a9be8fff58)] - **doc**: minimal documentation for Emeritus status (Rich Trott) [#13421](https://github.com/nodejs/node/pull/13421)
* [[`2778256680`](https://github.com/nodejs/node/commit/2778256680)] - **doc**: remove note highlighting in GOVERNANCE doc (Rich Trott) [#13420](https://github.com/nodejs/node/pull/13420)
* [[`2cb6f2b281`](https://github.com/nodejs/node/commit/2cb6f2b281)] - **http**: fix timeout reset after keep-alive timeout (Alexey Orlenko) [#13549](https://github.com/nodejs/node/pull/13549)
* [[`fff8a56d6f`](https://github.com/nodejs/node/commit/fff8a56d6f)] - **http**: handle cases where socket.server is null (Luigi Pinca) [#13578](https://github.com/nodejs/node/pull/13578)
* [[`c4cbd99d37`](https://github.com/nodejs/node/commit/c4cbd99d37)] - **https**: support rejectUnauthorized for unix sockets (cjihrig) [#13505](https://github.com/nodejs/node/pull/13505)
* [[`6a696d15ff`](https://github.com/nodejs/node/commit/6a696d15ff)] - **inspector**: fix crash on exception (Nikolai Vavilov) [#13455](https://github.com/nodejs/node/pull/13455)
* [[`50e1f931a9`](https://github.com/nodejs/node/commit/50e1f931a9)] - **profiler**: declare missing `printErr` (Fedor Indutny) [#13590](https://github.com/nodejs/node/pull/13590)
* [[`0df6c0b5f0`](https://github.com/nodejs/node/commit/0df6c0b5f0)] - ***Revert*** "**readline**: clean up event listener in onNewListener" (Anna Henningsen) [#13560](https://github.com/nodejs/node/pull/13560)
* [[`a5f415fe83`](https://github.com/nodejs/node/commit/a5f415fe83)] - **src**: merge `fn_name` in NODE_SET_PROTOTYPE_METHOD (XadillaX) [#13547](https://github.com/nodejs/node/pull/13547)
* [[`4a96ed4896`](https://github.com/nodejs/node/commit/4a96ed4896)] - **src**: check whether inspector is doing io (Sam Roberts) [#13504](https://github.com/nodejs/node/pull/13504)
* [[`f134c9d147`](https://github.com/nodejs/node/commit/f134c9d147)] - **src**: correct indentation for X509ToObject (Daniel Bevenius) [#13543](https://github.com/nodejs/node/pull/13543)
* [[`dd158b096f`](https://github.com/nodejs/node/commit/dd158b096f)] - **src**: make IsConstructCall checks consistent (Daniel Bevenius) [#13473](https://github.com/nodejs/node/pull/13473)
* [[`bf065344cf`](https://github.com/nodejs/node/commit/bf065344cf)] - **stream**: ensure that instanceof fast-path is hit. (Benedikt Meurer) [#13403](https://github.com/nodejs/node/pull/13403)
* [[`e713482147`](https://github.com/nodejs/node/commit/e713482147)] - **test**: fix typo in test-cli-node-options.js (Vse Mozhet Byt) [#13558](https://github.com/nodejs/node/pull/13558)
* [[`4c5457fae5`](https://github.com/nodejs/node/commit/4c5457fae5)] - **test**: fix flaky test-http-client-get-url (Sebastian Plesciuc) [#13516](https://github.com/nodejs/node/pull/13516)
* [[`812e0b0fbf`](https://github.com/nodejs/node/commit/812e0b0fbf)] - **test**: refactor async-hooks test-callback-error (Rich Trott) [#13554](https://github.com/nodejs/node/pull/13554)
* [[`2ea529b797`](https://github.com/nodejs/node/commit/2ea529b797)] - **test**: add regression test for 13557 (Anna Henningsen) [#13560](https://github.com/nodejs/node/pull/13560)
* [[`4d27930faf`](https://github.com/nodejs/node/commit/4d27930faf)] - **test**: fix flaky test-tls-socket-close (Rich Trott) [#13529](https://github.com/nodejs/node/pull/13529)
* [[`3da56ac9fb`](https://github.com/nodejs/node/commit/3da56ac9fb)] - **test**: harden test-dgram-bind-shared-ports (Refael Ackermann) [#13100](https://github.com/nodejs/node/pull/13100)
* [[`f686f73465`](https://github.com/nodejs/node/commit/f686f73465)] - **test**: add coverage for AsyncResource constructor (Gergely Nemeth) [#13327](https://github.com/nodejs/node/pull/13327)
* [[`12036a1d73`](https://github.com/nodejs/node/commit/12036a1d73)] - **test**: exercise once() with varying arguments (cjihrig) [#13524](https://github.com/nodejs/node/pull/13524)
* [[`1f88cbd620`](https://github.com/nodejs/node/commit/1f88cbd620)] - **test**: refactor test-http-server-keep-alive-timeout (realwakka) [#13448](https://github.com/nodejs/node/pull/13448)
* [[`bdbeb33dcb`](https://github.com/nodejs/node/commit/bdbeb33dcb)] - **test**: add hijackStdout and hijackStderr (XadillaX) [#13439](https://github.com/nodejs/node/pull/13439)
* [[`1c7f9171c0`](https://github.com/nodejs/node/commit/1c7f9171c0)] - **test**: add coverage for napi_property_descriptor (Michael Dawson) [#13510](https://github.com/nodejs/node/pull/13510)
* [[`c8db0475e0`](https://github.com/nodejs/node/commit/c8db0475e0)] - **test**: refactor test-fs-read-* (Rich Trott) [#13501](https://github.com/nodejs/node/pull/13501)
* [[`ad07c46b00`](https://github.com/nodejs/node/commit/ad07c46b00)] - **test**: refactor domain tests (Rich Trott) [#13480](https://github.com/nodejs/node/pull/13480)
* [[`fe5ea3feb0`](https://github.com/nodejs/node/commit/fe5ea3feb0)] - **test**: check callback not invoked on lookup error (Rich Trott) [#13456](https://github.com/nodejs/node/pull/13456)
* [[`216cb3f6e9`](https://github.com/nodejs/node/commit/216cb3f6e9)] - **test,benchmark**: stabilize child-process (Refael Ackermann) [#13457](https://github.com/nodejs/node/pull/13457)
* [[`a0f8faa3a4`](https://github.com/nodejs/node/commit/a0f8faa3a4)] - **v8**: fix debug builds on Windows (Bartosz Sosnowski) [#13634](https://github.com/nodejs/node/pull/13634)
* [[`38a1cfb5e6`](https://github.com/nodejs/node/commit/38a1cfb5e6)] - **v8**: add a js class for Serializer/Dserializer (Rajaram Gaunker) [#13541](https://github.com/nodejs/node/pull/13541)

<a id="8.1.0"></a>
## 2017-06-07, Version 8.1.0 (Current), @jasnell

### Notable Changes

* **Async Hooks**
  * When one `Promise` leads to the creation of a new `Promise`, the parent
    `Promise` will be identified as the trigger
    [[`135f4e6643`](https://github.com/nodejs/node/commit/135f4e6643)]
    [#13367](https://github.com/nodejs/node/pull/13367).
* **Dependencies**
  * libuv has been updated to 1.12.0
    [[`968596ec77`](https://github.com/nodejs/node/commit/968596ec77)]
    [#13306](https://github.com/nodejs/node/pull/13306).
  * npm has been updated to 5.0.3
    [[`ffa7debd7a`](https://github.com/nodejs/node/commit/ffa7debd7a)]
    [#13487](https://github.com/nodejs/node/pull/13487).
* **File system**
  * The `fs.exists()` function now works correctly with `util.promisify()`
    [[`6e0eccd7a1`](https://github.com/nodejs/node/commit/6e0eccd7a1)]
    [#13316](https://github.com/nodejs/node/pull/13316).
  * fs.Stats times are now also available as numbers
    [[`c756efb25a`](https://github.com/nodejs/node/commit/c756efb25a)]
    [#13173](https://github.com/nodejs/node/pull/13173).
* **Inspector**
  * It is now possible to bind to a random port using `--inspect=0`
    [[`cc6ec2fb27`](https://github.com/nodejs/node/commit/cc6ec2fb27)]
    [#5025](https://github.com/nodejs/node/pull/5025).
* **Zlib**
  * A regression in the Zlib module that made it impossible to properly
    subclasses `zlib.Deflate` and other Zlib classes has been fixed.
    [[`6aeb555cc4`](https://github.com/nodejs/node/commit/6aeb555cc4)]
    [#13374](https://github.com/nodejs/node/pull/13374).

### Commits

* [[`b8e90ddf53`](https://github.com/nodejs/node/commit/b8e90ddf53)] - **assert**: fix deepEqual similar sets and maps bug (Joseph Gentle) [#13426](https://github.com/nodejs/node/pull/13426)
* [[`47c9de9842`](https://github.com/nodejs/node/commit/47c9de9842)] - **assert**: fix deepEqual RangeError: Maximum call stack size exceeded (rmdm) [#13318](https://github.com/nodejs/node/pull/13318)
* [[`135f4e6643`](https://github.com/nodejs/node/commit/135f4e6643)] - **(SEMVER-MINOR)** **async_hooks**: use parent promise as triggerId (JiaLi.Passion) [#13367](https://github.com/nodejs/node/pull/13367)
* [[`9db02dcc85`](https://github.com/nodejs/node/commit/9db02dcc85)] - **async_hooks,http**: fix socket reuse with Agent (Anna Henningsen) [#13348](https://github.com/nodejs/node/pull/13348)
* [[`6917df2a80`](https://github.com/nodejs/node/commit/6917df2a80)] - **async_wrap**: run destroy in uv_timer_t (Trevor Norris) [#13369](https://github.com/nodejs/node/pull/13369)
* [[`65f22e481b`](https://github.com/nodejs/node/commit/65f22e481b)] - **build**: use existing variable to reduce complexity (Bryce Baril) [#2883](https://github.com/nodejs/node/pull/2883)
* [[`291669e7d8`](https://github.com/nodejs/node/commit/291669e7d8)] - **build**: streamline JS test suites in Makefile (Rich Trott) [#13340](https://github.com/nodejs/node/pull/13340)
* [[`dcadeb4fef`](https://github.com/nodejs/node/commit/dcadeb4fef)] - **build**: fix typo (Nikolai Vavilov) [#13396](https://github.com/nodejs/node/pull/13396)
* [[`50b5f8bac0`](https://github.com/nodejs/node/commit/50b5f8bac0)] - **crypto**: clear err stack after ECDH::BufferToPoint (Ryan Kelly) [#13275](https://github.com/nodejs/node/pull/13275)
* [[`968596ec77`](https://github.com/nodejs/node/commit/968596ec77)] - **deps**: upgrade libuv to 1.12.0 (cjihrig) [#13306](https://github.com/nodejs/node/pull/13306)
* [[`ffa7debd7a`](https://github.com/nodejs/node/commit/ffa7debd7a)] - **deps**: upgrade npm to 5.0.3 (Kat Marchán) [#13487](https://github.com/nodejs/node/pull/13487)
* [[`035a81b2e6`](https://github.com/nodejs/node/commit/035a81b2e6)] - **deps**: update openssl asm and asm_obsolete files (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`6f57554650`](https://github.com/nodejs/node/commit/6f57554650)] - **deps**: update openssl config files (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`1b8b82d076`](https://github.com/nodejs/node/commit/1b8b82d076)] - **deps**: add -no_rand_screen to openssl s_client (Shigeki Ohtsu) [nodejs/io.js#1836](https://github.com/nodejs/io.js/pull/1836)
* [[`783294add1`](https://github.com/nodejs/node/commit/783294add1)] - **deps**: fix asm build error of openssl in x86_win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`db7419bead`](https://github.com/nodejs/node/commit/db7419bead)] - **deps**: fix openssl assembly error on ia32 win32 (Fedor Indutny) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`dd93fa677a`](https://github.com/nodejs/node/commit/dd93fa677a)] - **deps**: copy all openssl header files to include dir (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`d9191f6e18`](https://github.com/nodejs/node/commit/d9191f6e18)] - **deps**: upgrade openssl sources to 1.0.2l (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`d985ca7d4a`](https://github.com/nodejs/node/commit/d985ca7d4a)] - **deps**: float patch on npm to fix citgm (Myles Borins) [#13305](https://github.com/nodejs/node/pull/13305)
* [[`92de432780`](https://github.com/nodejs/node/commit/92de432780)] - **dns**: use faster IP address type check on results (Brian White) [#13261](https://github.com/nodejs/node/pull/13261)
* [[`007a033820`](https://github.com/nodejs/node/commit/007a033820)] - **dns**: improve callback performance (Brian White) [#13261](https://github.com/nodejs/node/pull/13261)
* [[`26e5b6411f`](https://github.com/nodejs/node/commit/26e5b6411f)] - **doc**: update linux supported versions (cjihrig) [#13306](https://github.com/nodejs/node/pull/13306)
* [[`a117bcc0a7`](https://github.com/nodejs/node/commit/a117bcc0a7)] - **doc**: Add URL argument with http/https request (Vladimir Trifonov) [#13405](https://github.com/nodejs/node/pull/13405)
* [[`e6e42c1f75`](https://github.com/nodejs/node/commit/e6e42c1f75)] - **doc**: fix typo "ndapi" in n-api.md (Jamen Marz) [#13484](https://github.com/nodejs/node/pull/13484)
* [[`e991cd79f3`](https://github.com/nodejs/node/commit/e991cd79f3)] - **doc**: add ref to option to enable n-api (Michael Dawson) [#13406](https://github.com/nodejs/node/pull/13406)
* [[`414da1b7a1`](https://github.com/nodejs/node/commit/414da1b7a1)] - **doc**: fix nits in code examples of async_hooks.md (Vse Mozhet Byt) [#13400](https://github.com/nodejs/node/pull/13400)
* [[`159294d7d5`](https://github.com/nodejs/node/commit/159294d7d5)] - **doc**: use prefer-rest-params eslint rule in docs (Vse Mozhet Byt) [#13389](https://github.com/nodejs/node/pull/13389)
* [[`641979b213`](https://github.com/nodejs/node/commit/641979b213)] - **doc**: resume a stream after pipe() and unpipe() (Matteo Collina) [#13329](https://github.com/nodejs/node/pull/13329)
* [[`6c56bbdf13`](https://github.com/nodejs/node/commit/6c56bbdf13)] - **doc**: add missing backticks to doc/api/tls.md (Paul Bininda) [#13394](https://github.com/nodejs/node/pull/13394)
* [[`837ecc01eb`](https://github.com/nodejs/node/commit/837ecc01eb)] - **doc**: update who to cc for async_hooks (Anna Henningsen) [#13332](https://github.com/nodejs/node/pull/13332)
* [[`52c0c47856`](https://github.com/nodejs/node/commit/52c0c47856)] - **doc**: suggest xcode-select --install (Gibson Fahnestock) [#13264](https://github.com/nodejs/node/pull/13264)
* [[`11e428dd99`](https://github.com/nodejs/node/commit/11e428dd99)] - **doc**: add require modules in url.md (Daijiro Wachi) [#13365](https://github.com/nodejs/node/pull/13365)
* [[`2d25e09b0f`](https://github.com/nodejs/node/commit/2d25e09b0f)] - **doc**: add object-curly-spacing to doc/.eslintrc (Vse Mozhet Byt) [#13354](https://github.com/nodejs/node/pull/13354)
* [[`6cd5312b22`](https://github.com/nodejs/node/commit/6cd5312b22)] - **doc**: unify spaces in object literals (Vse Mozhet Byt) [#13354](https://github.com/nodejs/node/pull/13354)
* [[`4e687605ee`](https://github.com/nodejs/node/commit/4e687605ee)] - **doc**: use destructuring in code examples (Vse Mozhet Byt) [#13349](https://github.com/nodejs/node/pull/13349)
* [[`1b192f936a`](https://github.com/nodejs/node/commit/1b192f936a)] - **doc**: fix code examples in zlib.md (Vse Mozhet Byt) [#13342](https://github.com/nodejs/node/pull/13342)
* [[`a872399ddb`](https://github.com/nodejs/node/commit/a872399ddb)] - **doc**: update who to cc for n-api (Michael Dawson) [#13335](https://github.com/nodejs/node/pull/13335)
* [[`90417e8ced`](https://github.com/nodejs/node/commit/90417e8ced)] - **doc**: add missing make command to UPGRADING.md (Daniel Bevenius) [#13233](https://github.com/nodejs/node/pull/13233)
* [[`3c55d1aea4`](https://github.com/nodejs/node/commit/3c55d1aea4)] - **doc**: refine spaces in example from vm.md (Vse Mozhet Byt) [#13334](https://github.com/nodejs/node/pull/13334)
* [[`1729574cd7`](https://github.com/nodejs/node/commit/1729574cd7)] - **doc**: fix link in CHANGELOG_V8 (James, please) [#13313](https://github.com/nodejs/node/pull/13313)
* [[`16605cc3e4`](https://github.com/nodejs/node/commit/16605cc3e4)] - **doc**: add async_hooks, n-api to _toc.md and all.md (Vse Mozhet Byt) [#13379](https://github.com/nodejs/node/pull/13379)
* [[`eb6e9a0c9a`](https://github.com/nodejs/node/commit/eb6e9a0c9a)] - **doc**: remove 'you' from writing-tests.md (Michael Dawson) [#13319](https://github.com/nodejs/node/pull/13319)
* [[`e4f37568e2`](https://github.com/nodejs/node/commit/e4f37568e2)] - **doc**: fix date for 8.0.0 changelog (Myles Borins) [#13360](https://github.com/nodejs/node/pull/13360)
* [[`41f0af524d`](https://github.com/nodejs/node/commit/41f0af524d)] - **doc**: async-hooks documentation (Thorsten Lorenz) [#13287](https://github.com/nodejs/node/pull/13287)
* [[`b8b0bfb1a7`](https://github.com/nodejs/node/commit/b8b0bfb1a7)] - **doc**: add tniessen to collaborators (Tobias Nießen) [#13371](https://github.com/nodejs/node/pull/13371)
* [[`561c14ba12`](https://github.com/nodejs/node/commit/561c14ba12)] - **doc**: modernize and fix code examples in util.md (Vse Mozhet Byt) [#13298](https://github.com/nodejs/node/pull/13298)
* [[`c2d7b41ac7`](https://github.com/nodejs/node/commit/c2d7b41ac7)] - **doc**: fix code examples in url.md (Vse Mozhet Byt) [#13288](https://github.com/nodejs/node/pull/13288)
* [[`243643e5e4`](https://github.com/nodejs/node/commit/243643e5e4)] - **doc**: fix typo in n-api.md (JongChan Choi) [#13323](https://github.com/nodejs/node/pull/13323)
* [[`bee1421501`](https://github.com/nodejs/node/commit/bee1421501)] - **doc**: fix doc styles (Daijiro Wachi) [#13270](https://github.com/nodejs/node/pull/13270)
* [[`44c8ea32df`](https://github.com/nodejs/node/commit/44c8ea32df)] - **doc,stream**: clarify 'data', pipe() and 'readable' (Matteo Collina) [#13432](https://github.com/nodejs/node/pull/13432)
* [[`8f2b82a2b4`](https://github.com/nodejs/node/commit/8f2b82a2b4)] - **errors,tty**: migrate to use internal/errors.js (Gautam Mittal) [#13240](https://github.com/nodejs/node/pull/13240)
* [[`a666238ffe`](https://github.com/nodejs/node/commit/a666238ffe)] - **events**: fix potential permanent deopt (Brian White) [#13384](https://github.com/nodejs/node/pull/13384)
* [[`c756efb25a`](https://github.com/nodejs/node/commit/c756efb25a)] - **(SEMVER-MINOR)** **fs**: expose Stats times as Numbers (Refael Ackermann) [#13173](https://github.com/nodejs/node/pull/13173)
* [[`5644dd76a5`](https://github.com/nodejs/node/commit/5644dd76a5)] - **fs**: replace a bind() with a top-level function (Matteo Collina) [#13474](https://github.com/nodejs/node/pull/13474)
* [[`6e0eccd7a1`](https://github.com/nodejs/node/commit/6e0eccd7a1)] - **(SEMVER-MINOR)** **fs**: promisify exists correctly (Dan Fabulich) [#13316](https://github.com/nodejs/node/pull/13316)
* [[`0caa09da60`](https://github.com/nodejs/node/commit/0caa09da60)] - **gitignore**: add libuv book and GitHub template (cjihrig) [#13306](https://github.com/nodejs/node/pull/13306)
* [[`8efaa554f2`](https://github.com/nodejs/node/commit/8efaa554f2)] - **(SEMVER-MINOR)** **http**: overridable keep-alive behavior of `Agent` (Fedor Indutny) [#13005](https://github.com/nodejs/node/pull/13005)
* [[`afe91ec957`](https://github.com/nodejs/node/commit/afe91ec957)] - **http**: assert parser.consume argument's type (Gireesh Punathil) [#12288](https://github.com/nodejs/node/pull/12288)
* [[`b3c9bff254`](https://github.com/nodejs/node/commit/b3c9bff254)] - **http**: describe parse err in debug output (Sam Roberts) [#13206](https://github.com/nodejs/node/pull/13206)
* [[`c7ebf6ea70`](https://github.com/nodejs/node/commit/c7ebf6ea70)] - **http**: suppress data event if req aborted (Yihong Wang) [#13260](https://github.com/nodejs/node/pull/13260)
* [[`9be8b6373e`](https://github.com/nodejs/node/commit/9be8b6373e)] - **(SEMVER-MINOR)** **inspector**: allow --inspect=host:port from js (Sam Roberts) [#13228](https://github.com/nodejs/node/pull/13228)
* [[`376ac5fc3e`](https://github.com/nodejs/node/commit/376ac5fc3e)] - **inspector**: Allows reentry when paused (Eugene Ostroukhov) [#13350](https://github.com/nodejs/node/pull/13350)
* [[`7f0aa3f4bd`](https://github.com/nodejs/node/commit/7f0aa3f4bd)] - **inspector**: refactor to rename and comment methods (Sam Roberts) [#13321](https://github.com/nodejs/node/pull/13321)
* [[`cc6ec2fb27`](https://github.com/nodejs/node/commit/cc6ec2fb27)] - **(SEMVER-MINOR)** **inspector**: bind to random port with --inspect=0 (Ben Noordhuis) [#5025](https://github.com/nodejs/node/pull/5025)
* [[`4b2c756bfc`](https://github.com/nodejs/node/commit/4b2c756bfc)] - **(SEMVER-MINOR)** **lib**: return this from net.Socket.end() (Sam Roberts) [#13481](https://github.com/nodejs/node/pull/13481)
* [[`b3fb909d06`](https://github.com/nodejs/node/commit/b3fb909d06)] - **lib**: "iff" changed to "if and only if" (Jacob Jones) [#13496](https://github.com/nodejs/node/pull/13496)
* [[`a95f080160`](https://github.com/nodejs/node/commit/a95f080160)] - **n-api**: enable napi_wrap() to work with any object (Jason Ginchereau) [#13250](https://github.com/nodejs/node/pull/13250)
* [[`41eaa4b6a6`](https://github.com/nodejs/node/commit/41eaa4b6a6)] - **net**: fix permanent deopt (Brian White) [#13384](https://github.com/nodejs/node/pull/13384)
* [[`b5409abf9a`](https://github.com/nodejs/node/commit/b5409abf9a)] - **openssl**: fix keypress requirement in apps on win32 (Shigeki Ohtsu) [iojs/io.js#1389](https://github.com/iojs/io.js/pull/1389)
* [[`103de0e69a`](https://github.com/nodejs/node/commit/103de0e69a)] - **process**: fix permanent deopt (Brian White) [#13384](https://github.com/nodejs/node/pull/13384)
* [[`81ddeb98f6`](https://github.com/nodejs/node/commit/81ddeb98f6)] - **readline**: clean up event listener in onNewListener (Gibson Fahnestock) [#13266](https://github.com/nodejs/node/pull/13266)
* [[`791b5b5cbe`](https://github.com/nodejs/node/commit/791b5b5cbe)] - **src**: remove `'` print modifier (Refael Ackermann) [#13447](https://github.com/nodejs/node/pull/13447)
* [[`640101b780`](https://github.com/nodejs/node/commit/640101b780)] - **src**: remove process._inspectorEnbale (cjihrig) [#13460](https://github.com/nodejs/node/pull/13460)
* [[`8620aad573`](https://github.com/nodejs/node/commit/8620aad573)] - **src**: added newline in help message (Josh Ferge) [#13315](https://github.com/nodejs/node/pull/13315)
* [[`71a3d2c87e`](https://github.com/nodejs/node/commit/71a3d2c87e)] - **test**: refactor test-dgram-oob-buffer (Rich Trott) [#13443](https://github.com/nodejs/node/pull/13443)
* [[`54ae7d8931`](https://github.com/nodejs/node/commit/54ae7d8931)] - **test**: pass env vars through to test-benchmark-http (Gibson Fahnestock) [#13390](https://github.com/nodejs/node/pull/13390)
* [[`757ae521b5`](https://github.com/nodejs/node/commit/757ae521b5)] - **test**: validate full error messages (aniketshukla) [#13453](https://github.com/nodejs/node/pull/13453)
* [[`68e06e6945`](https://github.com/nodejs/node/commit/68e06e6945)] - **test**: increase coverage of async_hooks (David Cai) [#13336](https://github.com/nodejs/node/pull/13336)
* [[`7be1a1cd47`](https://github.com/nodejs/node/commit/7be1a1cd47)] - **test**: fix build warning in addons-napi/test_object (Jason Ginchereau) [#13412](https://github.com/nodejs/node/pull/13412)
* [[`fb73070068`](https://github.com/nodejs/node/commit/fb73070068)] - **test**: consolidate n-api test addons - part2 (Michael Dawson) [#13380](https://github.com/nodejs/node/pull/13380)
* [[`339d220eed`](https://github.com/nodejs/node/commit/339d220eed)] - **test**: rearrange inspector headers into convention (Sam Roberts) [#13428](https://github.com/nodejs/node/pull/13428)
* [[`8c7f9da489`](https://github.com/nodejs/node/commit/8c7f9da489)] - **test**: improve async hooks test error messages (Anna Henningsen) [#13243](https://github.com/nodejs/node/pull/13243)
* [[`818c935add`](https://github.com/nodejs/node/commit/818c935add)] - **test**: test async-hook triggerId properties (Dávid Szakállas) [#13328](https://github.com/nodejs/node/pull/13328)
* [[`29f19b6d39`](https://github.com/nodejs/node/commit/29f19b6d39)] - **test**: add documentation for common.mustNotCall() (Rich Trott) [#13359](https://github.com/nodejs/node/pull/13359)
* [[`c208f9d51f`](https://github.com/nodejs/node/commit/c208f9d51f)] - **test**: check destroy hooks are called before exit (Anna Henningsen) [#13369](https://github.com/nodejs/node/pull/13369)
* [[`406c2cd8e4`](https://github.com/nodejs/node/commit/406c2cd8e4)] - **test**: make test-fs-watchfile reliable (Rich Trott) [#13385](https://github.com/nodejs/node/pull/13385)
* [[`93e91a4f3f`](https://github.com/nodejs/node/commit/93e91a4f3f)] - **test**: check inspector support in test/inspector (Daniel Bevenius) [#13324](https://github.com/nodejs/node/pull/13324)
* [[`d1b39d92d6`](https://github.com/nodejs/node/commit/d1b39d92d6)] - **test**: add known_test request with Unicode in the URL (David D Lowe) [#13297](https://github.com/nodejs/node/pull/13297)
* [[`dccd1d2d31`](https://github.com/nodejs/node/commit/dccd1d2d31)] - **test**: improve dns internet test case (Brian White) [#13261](https://github.com/nodejs/node/pull/13261)
* [[`e20f3577d0`](https://github.com/nodejs/node/commit/e20f3577d0)] - **test**: improve test-https-server-keep-alive-timeout (Rich Trott) [#13312](https://github.com/nodejs/node/pull/13312)
* [[`2a29c07d9e`](https://github.com/nodejs/node/commit/2a29c07d9e)] - **test**: mark inspector-port-zero-cluster as flaky (Refael Ackermann)
* [[`b16dd98387`](https://github.com/nodejs/node/commit/b16dd98387)] - **test**: consolidate n-api test addons (Michael Dawson) [#13317](https://github.com/nodejs/node/pull/13317)
* [[`830049f784`](https://github.com/nodejs/node/commit/830049f784)] - **test**: refactor test-net-server-bind (Rich Trott) [#13273](https://github.com/nodejs/node/pull/13273)
* [[`9df8e2a3e9`](https://github.com/nodejs/node/commit/9df8e2a3e9)] - **test**: use mustCall() in test-readline-interface (Rich Trott) [#13259](https://github.com/nodejs/node/pull/13259)
* [[`25a05e5db1`](https://github.com/nodejs/node/commit/25a05e5db1)] - **test**: fix flaky test-fs-watchfile on macOS (Rich Trott) [#13252](https://github.com/nodejs/node/pull/13252)
* [[`ec357bf88f`](https://github.com/nodejs/node/commit/ec357bf88f)] - **test**: use mustNotCall() in test-stream2-objects (Rich Trott) [#13249](https://github.com/nodejs/node/pull/13249)
* [[`5369359d52`](https://github.com/nodejs/node/commit/5369359d52)] - **test**: Make N-API weak-ref GC tests asynchronous (Jason Ginchereau) [#13121](https://github.com/nodejs/node/pull/13121)
* [[`7cc6fd8403`](https://github.com/nodejs/node/commit/7cc6fd8403)] - **test**: improve n-api coverage for typed arrays (Michael Dawson) [#13244](https://github.com/nodejs/node/pull/13244)
* [[`a2d49545a7`](https://github.com/nodejs/node/commit/a2d49545a7)] - **test**: support candidate V8 versions (Michaël Zasso) [#13282](https://github.com/nodejs/node/pull/13282)
* [[`f0ad3bb695`](https://github.com/nodejs/node/commit/f0ad3bb695)] - **test**: hasCrypto https-server-keep-alive-timeout (Daniel Bevenius) [#13253](https://github.com/nodejs/node/pull/13253)
* [[`658560ee5b`](https://github.com/nodejs/node/commit/658560ee5b)] - **test,fs**: test fs.watch for `filename` (Refael Ackermann) [#13411](https://github.com/nodejs/node/pull/13411)
* [[`2e3b758006`](https://github.com/nodejs/node/commit/2e3b758006)] - **test,module**: make message check MUI dependent (Refael Ackermann) [#13393](https://github.com/nodejs/node/pull/13393)
* [[`01278bdd64`](https://github.com/nodejs/node/commit/01278bdd64)] - **tools**: fix order of ESLint rules (Michaël Zasso) [#13363](https://github.com/nodejs/node/pull/13363)
* [[`48cad9cde6`](https://github.com/nodejs/node/commit/48cad9cde6)] - **tools**: fix node args passing in test runner (Brian White) [#13384](https://github.com/nodejs/node/pull/13384)
* [[`bccda4f2b8`](https://github.com/nodejs/node/commit/bccda4f2b8)] - **tools**: be explicit about including key-id (Myles Borins) [#13309](https://github.com/nodejs/node/pull/13309)
* [[`61eb085c6a`](https://github.com/nodejs/node/commit/61eb085c6a)] - **tools, test**: update test-npm-package paths (Gibson Fahnestock) [#13441](https://github.com/nodejs/node/pull/13441)
* [[`ba817d3312`](https://github.com/nodejs/node/commit/ba817d3312)] - **url**: update IDNA handling (Timothy Gu) [#13362](https://github.com/nodejs/node/pull/13362)
* [[`d4d138c6e9`](https://github.com/nodejs/node/commit/d4d138c6e9)] - **url**: do not pass WHATWG host to http.request (Tobias Nießen) [#13409](https://github.com/nodejs/node/pull/13409)
* [[`315c3aaf43`](https://github.com/nodejs/node/commit/315c3aaf43)] - **url**: more precise URLSearchParams constructor (Timothy Gu) [#13026](https://github.com/nodejs/node/pull/13026)
* [[`1bcda5efda`](https://github.com/nodejs/node/commit/1bcda5efda)] - **util**: refactor format method.Performance improved. (Jesus Seijas) [#12407](https://github.com/nodejs/node/pull/12407)
* [[`f47ce01dfb`](https://github.com/nodejs/node/commit/f47ce01dfb)] - **win, doc**: document per-drive current working dir (Bartosz Sosnowski) [#13330](https://github.com/nodejs/node/pull/13330)
* [[`6aeb555cc4`](https://github.com/nodejs/node/commit/6aeb555cc4)] - **zlib**: revert back to Functions (James M Snell) [#13374](https://github.com/nodejs/node/pull/13374)
* [[`cc3174a937`](https://github.com/nodejs/node/commit/cc3174a937)] - **(SEMVER-MINOR)** **zlib**: expose amount of data read for engines (Alexander O'Mara) [#13088](https://github.com/nodejs/node/pull/13088)
* [[`bb77d6c1cc`](https://github.com/nodejs/node/commit/bb77d6c1cc)] - **(SEMVER-MINOR)** **zlib**: option for engine in convenience methods (Alexander O'Mara) [#13089](https://github.com/nodejs/node/pull/13089)


<a id="8.0.0"></a>
## 2017-05-30, Version 8.0.0 (Current), @jasnell

Node.js 8.0.0 is a major new release that includes a significant number of
`semver-major` and `semver-minor` changes. Notable changes are listed below.

The Node.js 8.x release branch is scheduled to become the *next* actively
maintained Long Term Support (LTS) release line in October, 2017 under the
LTS codename `'Carbon'`. Note that the
[LTS lifespan](https://github.com/nodejs/lts) for 8.x will end on December 31st,
2019.

### Notable Changes

* **Async Hooks**
  * The `async_hooks` module has landed in core
    [[`4a7233c178`](https://github.com/nodejs/node/commit/4a7233c178)]
    [#12892](https://github.com/nodejs/node/pull/12892).

* **Buffer**
  * Using the `--pending-deprecation` flag will cause Node.js to emit a
    deprecation warning when using `new Buffer(num)` or `Buffer(num)`.
    [[`d2d32ea5a2`](https://github.com/nodejs/node/commit/d2d32ea5a2)]
    [#11968](https://github.com/nodejs/node/pull/11968).
  * `new Buffer(num)` and `Buffer(num)` will zero-fill new `Buffer` instances
    [[`7eb1b4658e`](https://github.com/nodejs/node/commit/7eb1b4658e)]
    [#12141](https://github.com/nodejs/node/pull/12141).
  * Many `Buffer` methods now accept `Uint8Array` as input
    [[`beca3244e2`](https://github.com/nodejs/node/commit/beca3244e2)]
    [#10236](https://github.com/nodejs/node/pull/10236).

* **Child Process**
  * Argument and kill signal validations have been improved
    [[`97a77288ce`](https://github.com/nodejs/node/commit/97a77288ce)]
    [#12348](https://github.com/nodejs/node/pull/12348),
    [[`d75fdd96aa`](https://github.com/nodejs/node/commit/d75fdd96aa)]
    [#10423](https://github.com/nodejs/node/pull/10423).
  * Child Process methods accept `Uint8Array` as input
    [[`627ecee9ed`](https://github.com/nodejs/node/commit/627ecee9ed)]
    [#10653](https://github.com/nodejs/node/pull/10653).

* **Console**
  * Error events emitted when using `console` methods are now supressed.
    [[`f18e08d820`](https://github.com/nodejs/node/commit/f18e08d820)]
    [#9744](https://github.com/nodejs/node/pull/9744).

* **Dependencies**
  * The npm client has been updated to 5.0.0
    [[`3c3b36af0f`](https://github.com/nodejs/node/commit/3c3b36af0f)]
    [#12936](https://github.com/nodejs/node/pull/12936).
  * V8 has been updated to 5.8 with forward ABI stability to 6.0
    [[`60d1aac8d2`](https://github.com/nodejs/node/commit/60d1aac8d2)]
    [#12784](https://github.com/nodejs/node/pull/12784).

* **Domains**
  * Native `Promise` instances are now `Domain` aware
    [[`84dabe8373`](https://github.com/nodejs/node/commit/84dabe8373)]
    [#12489](https://github.com/nodejs/node/pull/12489).

* **Errors**
  * We have started assigning static error codes to errors generated by Node.js.
    This has been done through multiple commits and is still a work in
    progress.

* **File System**
  * The utility class `fs.SyncWriteStream` has been deprecated
    [[`7a55e34ef4`](https://github.com/nodejs/node/commit/7a55e34ef4)]
    [#10467](https://github.com/nodejs/node/pull/10467).
  * The deprecated `fs.read()` string interface has been removed
    [[`3c2a9361ff`](https://github.com/nodejs/node/commit/3c2a9361ff)]
    [#9683](https://github.com/nodejs/node/pull/9683).

* **HTTP**
  * Improved support for userland implemented Agents
    [[`90403dd1d0`](https://github.com/nodejs/node/commit/90403dd1d0)]
    [#11567](https://github.com/nodejs/node/pull/11567).
  * Outgoing Cookie headers are concatenated into a single string
    [[`d3480776c7`](https://github.com/nodejs/node/commit/d3480776c7)]
    [#11259](https://github.com/nodejs/node/pull/11259).
  * The `httpResponse.writeHeader()` method has been deprecated
    [[`fb71ba4921`](https://github.com/nodejs/node/commit/fb71ba4921)]
    [#11355](https://github.com/nodejs/node/pull/11355).
  * New methods for accessing HTTP headers have been added to `OutgoingMessage`
    [[`3e6f1032a4`](https://github.com/nodejs/node/commit/3e6f1032a4)]
    [#10805](https://github.com/nodejs/node/pull/10805).

* **Lib**
  * All deprecation messages have been assigned static identifiers
    [[`5de3cf099c`](https://github.com/nodejs/node/commit/5de3cf099c)]
    [#10116](https://github.com/nodejs/node/pull/10116).
  * The legacy `linkedlist` module has been removed
    [[`84a23391f6`](https://github.com/nodejs/node/commit/84a23391f6)]
    [#12113](https://github.com/nodejs/node/pull/12113).

* **N-API**
  * Experimental support for the new N-API API has been added
    [[`56e881d0b0`](https://github.com/nodejs/node/commit/56e881d0b0)]
    [#11975](https://github.com/nodejs/node/pull/11975).

* **Process**
  * Process warning output can be redirected to a file using the
    `--redirect-warnings` command-line argument
    [[`03e89b3ff2`](https://github.com/nodejs/node/commit/03e89b3ff2)]
    [#10116](https://github.com/nodejs/node/pull/10116).
  * Process warnings may now include additional detail
    [[`dd20e68b0f`](https://github.com/nodejs/node/commit/dd20e68b0f)]
    [#12725](https://github.com/nodejs/node/pull/12725).

* **REPL**
  * REPL magic mode has been deprecated
    [[`3f27f02da0`](https://github.com/nodejs/node/commit/3f27f02da0)]
    [#11599](https://github.com/nodejs/node/pull/11599).

* **Src**
  * `NODE_MODULE_VERSION` has been updated to 57
    [[`ec7cbaf266`](https://github.com/nodejs/node/commit/ec7cbaf266)]
    [#12995](https://github.com/nodejs/node/pull/12995).
  * Add `--pending-deprecation` command-line argument and
    `NODE_PENDING_DEPRECATION` environment variable
    [[`a16b570f8c`](https://github.com/nodejs/node/commit/a16b570f8c)]
    [#11968](https://github.com/nodejs/node/pull/11968).
  * The `--debug` command-line argument has been deprecated. Note that
    using `--debug` will enable the *new* Inspector-based debug protocol
    as the legacy Debugger protocol previously used by Node.js has been
    removed. [[`010f864426`](https://github.com/nodejs/node/commit/010f864426)]
    [#12949](https://github.com/nodejs/node/pull/12949).
  * Throw when the `-c` and `-e` command-line arguments are used at the same
    time [[`a5f91ab230`](https://github.com/nodejs/node/commit/a5f91ab230)]
    [#11689](https://github.com/nodejs/node/pull/11689).
  * Throw when the `--use-bundled-ca` and `--use-openssl-ca` command-line
    arguments are used at the same time.
    [[`8a7db9d4b5`](https://github.com/nodejs/node/commit/8a7db9d4b5)]
    [#12087](https://github.com/nodejs/node/pull/12087).

* **Stream**
  * `Stream` now supports `destroy()` and `_destroy()` APIs
    [[`b6e1d22fa6`](https://github.com/nodejs/node/commit/b6e1d22fa6)]
    [#12925](https://github.com/nodejs/node/pull/12925).
  * `Stream` now supports the `_final()` API
    [[`07c7f198db`](https://github.com/nodejs/node/commit/07c7f198db)]
    [#12828](https://github.com/nodejs/node/pull/12828).

* **TLS**
  * The `rejectUnauthorized` option now defaults to `true`
    [[`348cc80a3c`](https://github.com/nodejs/node/commit/348cc80a3c)]
    [#5923](https://github.com/nodejs/node/pull/5923).
  * The `tls.createSecurePair()` API now emits a runtime deprecation
    [[`a2ae08999b`](https://github.com/nodejs/node/commit/a2ae08999b)]
    [#11349](https://github.com/nodejs/node/pull/11349).
  * A runtime deprecation will now be emitted when `dhparam` is less than
    2048 bits [[`d523eb9c40`](https://github.com/nodejs/node/commit/d523eb9c40)]
    [#11447](https://github.com/nodejs/node/pull/11447).

* **URL**
  * The WHATWG URL implementation is now a fully-supported Node.js API
    [[`d080ead0f9`](https://github.com/nodejs/node/commit/d080ead0f9)]
    [#12710](https://github.com/nodejs/node/pull/12710).

* **Util**
  * `Symbol` keys are now displayed by default when using `util.inspect()`
    [[`5bfd13b81e`](https://github.com/nodejs/node/commit/5bfd13b81e)]
    [#9726](https://github.com/nodejs/node/pull/9726).
  * `toJSON` errors will be thrown when formatting `%j`
    [[`455e6f1dd8`](https://github.com/nodejs/node/commit/455e6f1dd8)]
    [#11708](https://github.com/nodejs/node/pull/11708).
  * Convert `inspect.styles` and `inspect.colors` to prototype-less objects
    [[`aab0d202f8`](https://github.com/nodejs/node/commit/aab0d202f8)]
    [#11624](https://github.com/nodejs/node/pull/11624).
  * The new `util.promisify()` API has been added
    [[`99da8e8e02`](https://github.com/nodejs/node/commit/99da8e8e02)]
    [#12442](https://github.com/nodejs/node/pull/12442).

* **Zlib**
  * Support `Uint8Array` in Zlib convenience methods
    [[`91383e47fd`](https://github.com/nodejs/node/commit/91383e47fd)]
    [#12001](https://github.com/nodejs/node/pull/12001).
  * Zlib errors now use `RangeError` and `TypeError` consistently
    [[`b514bd231e`](https://github.com/nodejs/node/commit/b514bd231e)]
    [#11391](https://github.com/nodejs/node/pull/11391).

### Commits

#### Semver-Major Commits

* [[`e48d58b8b2`](https://github.com/nodejs/node/commit/e48d58b8b2)] - **(SEMVER-MAJOR)** **assert**: fix AssertionError, assign error code (James M Snell) [#12651](https://github.com/nodejs/node/pull/12651)
* [[`758b8b6e5d`](https://github.com/nodejs/node/commit/758b8b6e5d)] - **(SEMVER-MAJOR)** **assert**: improve assert.fail() API (Rich Trott) [#12293](https://github.com/nodejs/node/pull/12293)
* [[`6481c93aef`](https://github.com/nodejs/node/commit/6481c93aef)] - **(SEMVER-MAJOR)** **assert**: add support for Map and Set in deepEqual (Joseph Gentle) [#12142](https://github.com/nodejs/node/pull/12142)
* [[`efec14a7d1`](https://github.com/nodejs/node/commit/efec14a7d1)] - **(SEMVER-MAJOR)** **assert**: enforce type check in deepStrictEqual (Joyee Cheung) [#10282](https://github.com/nodejs/node/pull/10282)
* [[`562cf5a81c`](https://github.com/nodejs/node/commit/562cf5a81c)] - **(SEMVER-MAJOR)** **assert**: fix premature deep strict comparison (Joyee Cheung) [#11128](https://github.com/nodejs/node/pull/11128)
* [[`0af41834f1`](https://github.com/nodejs/node/commit/0af41834f1)] - **(SEMVER-MAJOR)** **assert**: fix misformatted error message (Rich Trott) [#11254](https://github.com/nodejs/node/pull/11254)
* [[`190dc69c89`](https://github.com/nodejs/node/commit/190dc69c89)] - **(SEMVER-MAJOR)** **benchmark**: add parameter for module benchmark (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`b888bfe81d`](https://github.com/nodejs/node/commit/b888bfe81d)] - **(SEMVER-MAJOR)** **benchmark**: allow zero when parsing http req/s (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`f53a6fb48b`](https://github.com/nodejs/node/commit/f53a6fb48b)] - **(SEMVER-MAJOR)** **benchmark**: add http header setting scenarios (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`d2d32ea5a2`](https://github.com/nodejs/node/commit/d2d32ea5a2)] - **(SEMVER-MAJOR)** **buffer**: add pending deprecation warning (James M Snell) [#11968](https://github.com/nodejs/node/pull/11968)
* [[`7eb1b4658e`](https://github.com/nodejs/node/commit/7eb1b4658e)] - **(SEMVER-MAJOR)** **buffer**: zero fill Buffer(num) by default (James M Snell) [#12141](https://github.com/nodejs/node/pull/12141)
* [[`682573c11d`](https://github.com/nodejs/node/commit/682573c11d)] - **(SEMVER-MAJOR)** **buffer**: remove error for malformatted hex string (Rich Trott) [#12012](https://github.com/nodejs/node/pull/12012)
* [[`9a0829d728`](https://github.com/nodejs/node/commit/9a0829d728)] - **(SEMVER-MAJOR)** **buffer**: stricter argument checking in toString (Nikolai Vavilov) [#11120](https://github.com/nodejs/node/pull/11120)
* [[`beca3244e2`](https://github.com/nodejs/node/commit/beca3244e2)] - **(SEMVER-MAJOR)** **buffer**: allow Uint8Array input to methods (Anna Henningsen) [#10236](https://github.com/nodejs/node/pull/10236)
* [[`3d353c749c`](https://github.com/nodejs/node/commit/3d353c749c)] - **(SEMVER-MAJOR)** **buffer**: consistent error for length \> kMaxLength (Joyee Cheung) [#10152](https://github.com/nodejs/node/pull/10152)
* [[`bf5c309b5e`](https://github.com/nodejs/node/commit/bf5c309b5e)] - **(SEMVER-MAJOR)** **build**: fix V8 build on FreeBSD (Michaël Zasso) [#12784](https://github.com/nodejs/node/pull/12784)
* [[`a1028d5e3e`](https://github.com/nodejs/node/commit/a1028d5e3e)] - **(SEMVER-MAJOR)** **build**: remove cares headers from tarball (Gibson Fahnestock) [#10283](https://github.com/nodejs/node/pull/10283)
* [[`d08836003c`](https://github.com/nodejs/node/commit/d08836003c)] - **(SEMVER-MAJOR)** **build**: build an x64 build by default on Windows (Nikolai Vavilov) [#11537](https://github.com/nodejs/node/pull/11537)
* [[`92ed1ab450`](https://github.com/nodejs/node/commit/92ed1ab450)] - **(SEMVER-MAJOR)** **build**: change nosign flag to sign and flips logic (Joe Doyle) [#10156](https://github.com/nodejs/node/pull/10156)
* [[`97a77288ce`](https://github.com/nodejs/node/commit/97a77288ce)] - **(SEMVER-MAJOR)** **child_process**: improve ChildProcess validation (cjihrig) [#12348](https://github.com/nodejs/node/pull/12348)
* [[`a9111f9738`](https://github.com/nodejs/node/commit/a9111f9738)] - **(SEMVER-MAJOR)** **child_process**: minor cleanup of internals (cjihrig) [#12348](https://github.com/nodejs/node/pull/12348)
* [[`d75fdd96aa`](https://github.com/nodejs/node/commit/d75fdd96aa)] - **(SEMVER-MAJOR)** **child_process**: improve killSignal validations (Sakthipriyan Vairamani (thefourtheye)) [#10423](https://github.com/nodejs/node/pull/10423)
* [[`4cafa60c99`](https://github.com/nodejs/node/commit/4cafa60c99)] - **(SEMVER-MAJOR)** **child_process**: align fork/spawn stdio error msg (Sam Roberts) [#11044](https://github.com/nodejs/node/pull/11044)
* [[`3268863ebc`](https://github.com/nodejs/node/commit/3268863ebc)] - **(SEMVER-MAJOR)** **child_process**: add string shortcut for fork stdio (Javis Sullivan) [#10866](https://github.com/nodejs/node/pull/10866)
* [[`8f3ff98f0e`](https://github.com/nodejs/node/commit/8f3ff98f0e)] - **(SEMVER-MAJOR)** **child_process**: allow Infinity as maxBuffer value (cjihrig) [#10769](https://github.com/nodejs/node/pull/10769)
* [[`627ecee9ed`](https://github.com/nodejs/node/commit/627ecee9ed)] - **(SEMVER-MAJOR)** **child_process**: support Uint8Array input to methods (Anna Henningsen) [#10653](https://github.com/nodejs/node/pull/10653)
* [[`fc7b0dda85`](https://github.com/nodejs/node/commit/fc7b0dda85)] - **(SEMVER-MAJOR)** **child_process**: improve input validation (cjihrig) [#8312](https://github.com/nodejs/node/pull/8312)
* [[`49d1c366d8`](https://github.com/nodejs/node/commit/49d1c366d8)] - **(SEMVER-MAJOR)** **child_process**: remove extra newline in errors (cjihrig) [#9343](https://github.com/nodejs/node/pull/9343)
* [[`f18e08d820`](https://github.com/nodejs/node/commit/f18e08d820)] - **(SEMVER-MAJOR)** **console**: do not emit error events (Anna Henningsen) [#9744](https://github.com/nodejs/node/pull/9744)
* [[`a8f460f12d`](https://github.com/nodejs/node/commit/a8f460f12d)] - **(SEMVER-MAJOR)** **crypto**: support all ArrayBufferView types (Timothy Gu) [#12223](https://github.com/nodejs/node/pull/12223)
* [[`0db49fef41`](https://github.com/nodejs/node/commit/0db49fef41)] - **(SEMVER-MAJOR)** **crypto**: support Uint8Array prime in createDH (Anna Henningsen) [#11983](https://github.com/nodejs/node/pull/11983)
* [[`443691a5ae`](https://github.com/nodejs/node/commit/443691a5ae)] - **(SEMVER-MAJOR)** **crypto**: fix default encoding of LazyTransform (Lukas Möller) [#8611](https://github.com/nodejs/node/pull/8611)
* [[`9f74184e98`](https://github.com/nodejs/node/commit/9f74184e98)] - **(SEMVER-MAJOR)** **crypto**: upgrade pbkdf2 without digest to an error (James M Snell) [#11305](https://github.com/nodejs/node/pull/11305)
* [[`e90f38270c`](https://github.com/nodejs/node/commit/e90f38270c)] - **(SEMVER-MAJOR)** **crypto**: throw error in CipherBase::SetAutoPadding (Kirill Fomichev) [#9405](https://github.com/nodejs/node/pull/9405)
* [[`1ef401ce92`](https://github.com/nodejs/node/commit/1ef401ce92)] - **(SEMVER-MAJOR)** **crypto**: use check macros in CipherBase::SetAuthTag (Kirill Fomichev) [#9395](https://github.com/nodejs/node/pull/9395)
* [[`7599b0ef9d`](https://github.com/nodejs/node/commit/7599b0ef9d)] - **(SEMVER-MAJOR)** **debug**: activate inspector with _debugProcess (Eugene Ostroukhov) [#11431](https://github.com/nodejs/node/pull/11431)
* [[`549e81bfa1`](https://github.com/nodejs/node/commit/549e81bfa1)] - **(SEMVER-MAJOR)** **debugger**: remove obsolete _debug_agent.js (Rich Trott) [#12582](https://github.com/nodejs/node/pull/12582)
* [[`3c3b36af0f`](https://github.com/nodejs/node/commit/3c3b36af0f)] - **(SEMVER-MAJOR)** **deps**: upgrade npm beta to 5.0.0-beta.56 (Kat Marchán) [#12936](https://github.com/nodejs/node/pull/12936)
* [[`6690415696`](https://github.com/nodejs/node/commit/6690415696)] - **(SEMVER-MAJOR)** **deps**: cherry-pick a927f81c7 from V8 upstream (Anna Henningsen) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`60d1aac8d2`](https://github.com/nodejs/node/commit/60d1aac8d2)] - **(SEMVER-MAJOR)** **deps**: update V8 to 5.8.283.38 (Michaël Zasso) [#12784](https://github.com/nodejs/node/pull/12784)
* [[`b7608ac707`](https://github.com/nodejs/node/commit/b7608ac707)] - **(SEMVER-MAJOR)** **deps**: cherry-pick node-inspect#43 (Ali Ijaz Sheikh) [#11441](https://github.com/nodejs/node/pull/11441)
* [[`9c9e2d7f4a`](https://github.com/nodejs/node/commit/9c9e2d7f4a)] - **(SEMVER-MAJOR)** **deps**: backport 3297130 from upstream V8 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`07088e6fc1`](https://github.com/nodejs/node/commit/07088e6fc1)] - **(SEMVER-MAJOR)** **deps**: backport 39642fa from upstream V8 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`8394b05e20`](https://github.com/nodejs/node/commit/8394b05e20)] - **(SEMVER-MAJOR)** **deps**: cherry-pick c5c570f from upstream V8 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`fcc58bf0da`](https://github.com/nodejs/node/commit/fcc58bf0da)] - **(SEMVER-MAJOR)** **deps**: cherry-pick a927f81c7 from V8 upstream (Anna Henningsen) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`83bf2975ec`](https://github.com/nodejs/node/commit/83bf2975ec)] - **(SEMVER-MAJOR)** **deps**: cherry-pick V8 ValueSerializer changes (Anna Henningsen) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`c459d8ea5d`](https://github.com/nodejs/node/commit/c459d8ea5d)] - **(SEMVER-MAJOR)** **deps**: update V8 to 5.7.492.69 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`7c0c7baff3`](https://github.com/nodejs/node/commit/7c0c7baff3)] - **(SEMVER-MAJOR)** **deps**: fix gyp configuration for v8-inspector (Michaël Zasso) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`00a2aa0af5`](https://github.com/nodejs/node/commit/00a2aa0af5)] - **(SEMVER-MAJOR)** **deps**: fix gyp build configuration for Windows (Michaël Zasso) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`b30ec59855`](https://github.com/nodejs/node/commit/b30ec59855)] - **(SEMVER-MAJOR)** **deps**: switch to v8_inspector in V8 (Ali Ijaz Sheikh) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`7a77daf243`](https://github.com/nodejs/node/commit/7a77daf243)] - **(SEMVER-MAJOR)** **deps**: update V8 to 5.6.326.55 (Michaël Zasso) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`c9e5178f3c`](https://github.com/nodejs/node/commit/c9e5178f3c)] - **(SEMVER-MAJOR)** **deps**: hide zlib internal symbols (Sam Roberts) [#11082](https://github.com/nodejs/node/pull/11082)
* [[`2739185b79`](https://github.com/nodejs/node/commit/2739185b79)] - **(SEMVER-MAJOR)** **deps**: update V8 to 5.5.372.40 (Michaël Zasso) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`f2e3a670af`](https://github.com/nodejs/node/commit/f2e3a670af)] - **(SEMVER-MAJOR)** **dgram**: convert to using internal/errors (Michael Dawson) [#12926](https://github.com/nodejs/node/pull/12926)
* [[`2dc1053b0a`](https://github.com/nodejs/node/commit/2dc1053b0a)] - **(SEMVER-MAJOR)** **dgram**: support Uint8Array input to send() (Anna Henningsen) [#11985](https://github.com/nodejs/node/pull/11985)
* [[`32679c73c1`](https://github.com/nodejs/node/commit/32679c73c1)] - **(SEMVER-MAJOR)** **dgram**: improve signature of Socket.prototype.send (Christopher Hiller) [#10473](https://github.com/nodejs/node/pull/10473)
* [[`5587ff1ccd`](https://github.com/nodejs/node/commit/5587ff1ccd)] - **(SEMVER-MAJOR)** **dns**: handle implicit bind DNS errors (cjihrig) [#11036](https://github.com/nodejs/node/pull/11036)
* [[`eb535c5154`](https://github.com/nodejs/node/commit/eb535c5154)] - **(SEMVER-MAJOR)** **doc**: deprecate vm.runInDebugContext (Josh Gavant) [#12243](https://github.com/nodejs/node/pull/12243)
* [[`75c471a026`](https://github.com/nodejs/node/commit/75c471a026)] - **(SEMVER-MAJOR)** **doc**: drop PPC BE from supported platforms (Michael Dawson) [#12309](https://github.com/nodejs/node/pull/12309)
* [[`86996c5838`](https://github.com/nodejs/node/commit/86996c5838)] - **(SEMVER-MAJOR)** **doc**: deprecate private http properties (Brian White) [#10941](https://github.com/nodejs/node/pull/10941)
* [[`3d8379ae60`](https://github.com/nodejs/node/commit/3d8379ae60)] - **(SEMVER-MAJOR)** **doc**: improve assert.md regarding ECMAScript terms (Joyee Cheung) [#11128](https://github.com/nodejs/node/pull/11128)
* [[`d708700c68`](https://github.com/nodejs/node/commit/d708700c68)] - **(SEMVER-MAJOR)** **doc**: deprecate buffer's parent property (Sakthipriyan Vairamani (thefourtheye)) [#8332](https://github.com/nodejs/node/pull/8332)
* [[`03d440e3ce`](https://github.com/nodejs/node/commit/03d440e3ce)] - **(SEMVER-MAJOR)** **doc**: document buffer.buffer property (Sakthipriyan Vairamani (thefourtheye)) [#8332](https://github.com/nodejs/node/pull/8332)
* [[`f0b702555a`](https://github.com/nodejs/node/commit/f0b702555a)] - **(SEMVER-MAJOR)** **errors**: use lazy assert to avoid issues on startup (James M Snell) [#11300](https://github.com/nodejs/node/pull/11300)
* [[`251e5ed8ee`](https://github.com/nodejs/node/commit/251e5ed8ee)] - **(SEMVER-MAJOR)** **errors**: assign error code to bootstrap_node created error (James M Snell) [#11298](https://github.com/nodejs/node/pull/11298)
* [[`e75bc87d22`](https://github.com/nodejs/node/commit/e75bc87d22)] - **(SEMVER-MAJOR)** **errors**: port internal/process errors to internal/errors (James M Snell) [#11294](https://github.com/nodejs/node/pull/11294)
* [[`76327613af`](https://github.com/nodejs/node/commit/76327613af)] - **(SEMVER-MAJOR)** **errors, child_process**: migrate to using internal/errors (James M Snell) [#11300](https://github.com/nodejs/node/pull/11300)
* [[`1c834e78ff`](https://github.com/nodejs/node/commit/1c834e78ff)] - **(SEMVER-MAJOR)** **errors,test**: migrating error to internal/errors (larissayvette) [#11505](https://github.com/nodejs/node/pull/11505)
* [[`2141d37452`](https://github.com/nodejs/node/commit/2141d37452)] - **(SEMVER-MAJOR)** **events**: update and clarify error message (Chris Burkhart) [#10387](https://github.com/nodejs/node/pull/10387)
* [[`221b03ad20`](https://github.com/nodejs/node/commit/221b03ad20)] - **(SEMVER-MAJOR)** **events, doc**: check input in defaultMaxListeners (DavidCai) [#11938](https://github.com/nodejs/node/pull/11938)
* [[`eed87b1637`](https://github.com/nodejs/node/commit/eed87b1637)] - **(SEMVER-MAJOR)** **fs**: (+/-)Infinity and NaN invalid unixtimestamp (Luca Maraschi) [#11919](https://github.com/nodejs/node/pull/11919)
* [[`71097744b2`](https://github.com/nodejs/node/commit/71097744b2)] - **(SEMVER-MAJOR)** **fs**: more realpath*() optimizations (Brian White) [#11665](https://github.com/nodejs/node/pull/11665)
* [[`6a5ab5d550`](https://github.com/nodejs/node/commit/6a5ab5d550)] - **(SEMVER-MAJOR)** **fs**: include more fs.stat*() optimizations (Brian White) [#11665](https://github.com/nodejs/node/pull/11665)
* [[`1c3df96570`](https://github.com/nodejs/node/commit/1c3df96570)] - **(SEMVER-MAJOR)** **fs**: replace regexp with function (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`34c9fc2e4e`](https://github.com/nodejs/node/commit/34c9fc2e4e)] - **(SEMVER-MAJOR)** **fs**: avoid multiple conversions to string (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`21b2440176`](https://github.com/nodejs/node/commit/21b2440176)] - **(SEMVER-MAJOR)** **fs**: avoid recompilation of closure (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`7a55e34ef4`](https://github.com/nodejs/node/commit/7a55e34ef4)] - **(SEMVER-MAJOR)** **fs**: runtime deprecation for fs.SyncWriteStream (James M Snell) [#10467](https://github.com/nodejs/node/pull/10467)
* [[`b1fc7745f2`](https://github.com/nodejs/node/commit/b1fc7745f2)] - **(SEMVER-MAJOR)** **fs**: avoid emitting error EBADF for double close (Matteo Collina) [#11225](https://github.com/nodejs/node/pull/11225)
* [[`3c2a9361ff`](https://github.com/nodejs/node/commit/3c2a9361ff)] - **(SEMVER-MAJOR)** **fs**: remove fs.read's string interface (Nikolai Vavilov) [#9683](https://github.com/nodejs/node/pull/9683)
* [[`f3cf8e9808`](https://github.com/nodejs/node/commit/f3cf8e9808)] - **(SEMVER-MAJOR)** **fs**: do not pass Buffer when toString() fails (Brian White) [#9670](https://github.com/nodejs/node/pull/9670)
* [[`85a4e25775`](https://github.com/nodejs/node/commit/85a4e25775)] - **(SEMVER-MAJOR)** **http**: add type checking for hostname (James M Snell) [#12494](https://github.com/nodejs/node/pull/12494)
* [[`90403dd1d0`](https://github.com/nodejs/node/commit/90403dd1d0)] - **(SEMVER-MAJOR)** **http**: should support userland Agent (fengmk2) [#11567](https://github.com/nodejs/node/pull/11567)
* [[`d3480776c7`](https://github.com/nodejs/node/commit/d3480776c7)] - **(SEMVER-MAJOR)** **http**: concatenate outgoing Cookie headers (Brian White) [#11259](https://github.com/nodejs/node/pull/11259)
* [[`6b2cef65c9`](https://github.com/nodejs/node/commit/6b2cef65c9)] - **(SEMVER-MAJOR)** **http**: append Cookie header values with semicolon (Brian White) [#11259](https://github.com/nodejs/node/pull/11259)
* [[`8243ca0e0e`](https://github.com/nodejs/node/commit/8243ca0e0e)] - **(SEMVER-MAJOR)** **http**: reuse existing StorageObject (Brian White) [#10941](https://github.com/nodejs/node/pull/10941)
* [[`b377034359`](https://github.com/nodejs/node/commit/b377034359)] - **(SEMVER-MAJOR)** **http**: support old private properties and function (Brian White) [#10941](https://github.com/nodejs/node/pull/10941)
* [[`940b5303be`](https://github.com/nodejs/node/commit/940b5303be)] - **(SEMVER-MAJOR)** **http**: use Symbol for outgoing headers (Brian White) [#10941](https://github.com/nodejs/node/pull/10941)
* [[`fb71ba4921`](https://github.com/nodejs/node/commit/fb71ba4921)] - **(SEMVER-MAJOR)** **http**: docs-only deprecation of res.writeHeader() (James M Snell) [#11355](https://github.com/nodejs/node/pull/11355)
* [[`a4bb9fdb89`](https://github.com/nodejs/node/commit/a4bb9fdb89)] - **(SEMVER-MAJOR)** **http**: include provided status code in range error (cjihrig) [#11221](https://github.com/nodejs/node/pull/11221)
* [[`fc7025c9c0`](https://github.com/nodejs/node/commit/fc7025c9c0)] - **(SEMVER-MAJOR)** **http**: throw an error for unexpected agent values (brad-decker) [#10053](https://github.com/nodejs/node/pull/10053)
* [[`176cdc2823`](https://github.com/nodejs/node/commit/176cdc2823)] - **(SEMVER-MAJOR)** **http**: misc optimizations and style fixes (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`73d9445782`](https://github.com/nodejs/node/commit/73d9445782)] - **(SEMVER-MAJOR)** **http**: try to avoid lowercasing incoming headers (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`c77ed327d9`](https://github.com/nodejs/node/commit/c77ed327d9)] - **(SEMVER-MAJOR)** **http**: avoid using object for removed header status (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`c00e4adbb4`](https://github.com/nodejs/node/commit/c00e4adbb4)] - **(SEMVER-MAJOR)** **http**: optimize header storage and matching (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`ec8910bcea`](https://github.com/nodejs/node/commit/ec8910bcea)] - **(SEMVER-MAJOR)** **http**: check statusCode early (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`a73ab9de0d`](https://github.com/nodejs/node/commit/a73ab9de0d)] - **(SEMVER-MAJOR)** **http**: remove pointless use of arguments (cjihrig) [#10664](https://github.com/nodejs/node/pull/10664)
* [[`df3978421b`](https://github.com/nodejs/node/commit/df3978421b)] - **(SEMVER-MAJOR)** **http**: verify client method is a string (Luca Maraschi) [#10111](https://github.com/nodejs/node/pull/10111)
* [[`90476ac6ee`](https://github.com/nodejs/node/commit/90476ac6ee)] - **(SEMVER-MAJOR)** **lib**: remove _debugger.js (Ben Noordhuis) [#12495](https://github.com/nodejs/node/pull/12495)
* [[`3209a8ebf3`](https://github.com/nodejs/node/commit/3209a8ebf3)] - **(SEMVER-MAJOR)** **lib**: ensure --check flag works for piped-in code (Teddy Katz) [#11689](https://github.com/nodejs/node/pull/11689)
* [[`c67207731f`](https://github.com/nodejs/node/commit/c67207731f)] - **(SEMVER-MAJOR)** **lib**: simplify Module._resolveLookupPaths (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`28dc848e70`](https://github.com/nodejs/node/commit/28dc848e70)] - **(SEMVER-MAJOR)** **lib**: improve method of function calling (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`a851b868c0`](https://github.com/nodejs/node/commit/a851b868c0)] - **(SEMVER-MAJOR)** **lib**: remove sources of permanent deopts (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`62e96096fa`](https://github.com/nodejs/node/commit/62e96096fa)] - **(SEMVER-MAJOR)** **lib**: more consistent use of module.exports = {} model (James M Snell) [#11406](https://github.com/nodejs/node/pull/11406)
* [[`88c3f57cc3`](https://github.com/nodejs/node/commit/88c3f57cc3)] - **(SEMVER-MAJOR)** **lib**: refactor internal/socket_list (James M Snell) [#11406](https://github.com/nodejs/node/pull/11406)
* [[`f04387e9f2`](https://github.com/nodejs/node/commit/f04387e9f2)] - **(SEMVER-MAJOR)** **lib**: refactor internal/freelist (James M Snell) [#11406](https://github.com/nodejs/node/pull/11406)
* [[`d61a511728`](https://github.com/nodejs/node/commit/d61a511728)] - **(SEMVER-MAJOR)** **lib**: refactor internal/linkedlist (James M Snell) [#11406](https://github.com/nodejs/node/pull/11406)
* [[`2ba4eeadbb`](https://github.com/nodejs/node/commit/2ba4eeadbb)] - **(SEMVER-MAJOR)** **lib**: remove simd support from util.format() (Ben Noordhuis) [#11346](https://github.com/nodejs/node/pull/11346)
* [[`dfdd911e17`](https://github.com/nodejs/node/commit/dfdd911e17)] - **(SEMVER-MAJOR)** **lib**: deprecate node --debug at runtime (Josh Gavant) [#10970](https://github.com/nodejs/node/pull/10970)
* [[`5de3cf099c`](https://github.com/nodejs/node/commit/5de3cf099c)] - **(SEMVER-MAJOR)** **lib**: add static identifier codes for all deprecations (James M Snell) [#10116](https://github.com/nodejs/node/pull/10116)
* [[`4775942957`](https://github.com/nodejs/node/commit/4775942957)] - **(SEMVER-MAJOR)** **lib, test**: fix server.listen error message (Joyee Cheung) [#11693](https://github.com/nodejs/node/pull/11693)
* [[`caf9ae748b`](https://github.com/nodejs/node/commit/caf9ae748b)] - **(SEMVER-MAJOR)** **lib,src**: make constants not inherit from Object (Sakthipriyan Vairamani (thefourtheye)) [#10458](https://github.com/nodejs/node/pull/10458)
* [[`e0b076a949`](https://github.com/nodejs/node/commit/e0b076a949)] - **(SEMVER-MAJOR)** **lib,src,test**: update --debug/debug-brk comments (Ben Noordhuis) [#12495](https://github.com/nodejs/node/pull/12495)
* [[`b40dab553f`](https://github.com/nodejs/node/commit/b40dab553f)] - **(SEMVER-MAJOR)** **linkedlist**: remove unused methods (Brian White) [#11726](https://github.com/nodejs/node/pull/11726)
* [[`84a23391f6`](https://github.com/nodejs/node/commit/84a23391f6)] - **(SEMVER-MAJOR)** **linkedlist**: remove public module (Brian White) [#12113](https://github.com/nodejs/node/pull/12113)
* [[`e32425bfcd`](https://github.com/nodejs/node/commit/e32425bfcd)] - **(SEMVER-MAJOR)** **module**: avoid JSON.stringify() for cache key (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`403b89e72b`](https://github.com/nodejs/node/commit/403b89e72b)] - **(SEMVER-MAJOR)** **module**: avoid hasOwnProperty() (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`298a40e04e`](https://github.com/nodejs/node/commit/298a40e04e)] - **(SEMVER-MAJOR)** **module**: use "clean" objects (Brian White) [#10789](https://github.com/nodejs/node/pull/10789)
* [[`cf980b0311`](https://github.com/nodejs/node/commit/cf980b0311)] - **(SEMVER-MAJOR)** **net**: check and throw on error for getsockname (Daniel Bevenius) [#12871](https://github.com/nodejs/node/pull/12871)
* [[`473572ea25`](https://github.com/nodejs/node/commit/473572ea25)] - **(SEMVER-MAJOR)** **os**: refactor os structure, add Symbol.toPrimitive (James M Snell) [#12654](https://github.com/nodejs/node/pull/12654)
* [[`03e89b3ff2`](https://github.com/nodejs/node/commit/03e89b3ff2)] - **(SEMVER-MAJOR)** **process**: add --redirect-warnings command line argument (James M Snell) [#10116](https://github.com/nodejs/node/pull/10116)
* [[`5e1f32fd94`](https://github.com/nodejs/node/commit/5e1f32fd94)] - **(SEMVER-MAJOR)** **process**: add optional code to warnings + type checking (James M Snell) [#10116](https://github.com/nodejs/node/pull/10116)
* [[`a647d82f83`](https://github.com/nodejs/node/commit/a647d82f83)] - **(SEMVER-MAJOR)** **process**: improve process.hrtime (Joyee Cheung) [#10764](https://github.com/nodejs/node/pull/10764)
* [[`4e259b21a3`](https://github.com/nodejs/node/commit/4e259b21a3)] - **(SEMVER-MAJOR)** **querystring, url**: handle repeated sep in search (Daijiro Wachi) [#10967](https://github.com/nodejs/node/pull/10967)
* [[`39d9afe279`](https://github.com/nodejs/node/commit/39d9afe279)] - **(SEMVER-MAJOR)** **repl**: refactor `LineParser` implementation (Blake Embrey) [#6171](https://github.com/nodejs/node/pull/6171)
* [[`3f27f02da0`](https://github.com/nodejs/node/commit/3f27f02da0)] - **(SEMVER-MAJOR)** **repl**: docs-only deprecation of magic mode (Timothy Gu) [#11599](https://github.com/nodejs/node/pull/11599)
* [[`b77c89022b`](https://github.com/nodejs/node/commit/b77c89022b)] - **(SEMVER-MAJOR)** **repl**: remove magic mode semantics (Timothy Gu) [#11599](https://github.com/nodejs/node/pull/11599)
* [[`007386ee81`](https://github.com/nodejs/node/commit/007386ee81)] - **(SEMVER-MAJOR)** **repl**: remove workaround for function redefinition (Michaël Zasso) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`5b63fabfd8`](https://github.com/nodejs/node/commit/5b63fabfd8)] - **(SEMVER-MAJOR)** **src**: update NODE_MODULE_VERSION to 55 (Michaël Zasso) [#12784](https://github.com/nodejs/node/pull/12784)
* [[`a16b570f8c`](https://github.com/nodejs/node/commit/a16b570f8c)] - **(SEMVER-MAJOR)** **src**: add --pending-deprecation and NODE_PENDING_DEPRECATION (James M Snell) [#11968](https://github.com/nodejs/node/pull/11968)
* [[`faa447b256`](https://github.com/nodejs/node/commit/faa447b256)] - **(SEMVER-MAJOR)** **src**: allow ArrayBufferView as instance of Buffer (Timothy Gu) [#12223](https://github.com/nodejs/node/pull/12223)
* [[`47f8f7462f`](https://github.com/nodejs/node/commit/47f8f7462f)] - **(SEMVER-MAJOR)** **src**: remove support for --debug (Jan Krems) [#12197](https://github.com/nodejs/node/pull/12197)
* [[`a5f91ab230`](https://github.com/nodejs/node/commit/a5f91ab230)] - **(SEMVER-MAJOR)** **src**: throw when -c and -e are used simultaneously (Teddy Katz) [#11689](https://github.com/nodejs/node/pull/11689)
* [[`8a7db9d4b5`](https://github.com/nodejs/node/commit/8a7db9d4b5)] - **(SEMVER-MAJOR)** **src**: add --use-bundled-ca --use-openssl-ca check (Daniel Bevenius) [#12087](https://github.com/nodejs/node/pull/12087)
* [[`ed12ea371c`](https://github.com/nodejs/node/commit/ed12ea371c)] - **(SEMVER-MAJOR)** **src**: update inspector code to match upstream API (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`89d8dc9afd`](https://github.com/nodejs/node/commit/89d8dc9afd)] - **(SEMVER-MAJOR)** **src**: update NODE_MODULE_VERSION to 54 (Michaël Zasso) [#11752](https://github.com/nodejs/node/pull/11752)
* [[`1125c8a814`](https://github.com/nodejs/node/commit/1125c8a814)] - **(SEMVER-MAJOR)** **src**: fix typos in node_lttng_provider.h (Benjamin Fleischer) [#11723](https://github.com/nodejs/node/pull/11723)
* [[`aae8f683b4`](https://github.com/nodejs/node/commit/aae8f683b4)] - **(SEMVER-MAJOR)** **src**: update NODE_MODULE_VERSION to 53 (Michaël Zasso) [#10992](https://github.com/nodejs/node/pull/10992)
* [[`91ab09fe2a`](https://github.com/nodejs/node/commit/91ab09fe2a)] - **(SEMVER-MAJOR)** **src**: update NODE_MODULE_VERSION to 52 (Michaël Zasso) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`334be0feba`](https://github.com/nodejs/node/commit/334be0feba)] - **(SEMVER-MAJOR)** **src**: fix space for module version mismatch error (Yann Pringault) [#10606](https://github.com/nodejs/node/pull/10606)
* [[`45c9ca7fd4`](https://github.com/nodejs/node/commit/45c9ca7fd4)] - **(SEMVER-MAJOR)** **src**: remove redundant spawn/spawnSync type checks (cjihrig) [#8312](https://github.com/nodejs/node/pull/8312)
* [[`b374ee8c3d`](https://github.com/nodejs/node/commit/b374ee8c3d)] - **(SEMVER-MAJOR)** **src**: add handle check to spawn_sync (cjihrig) [#8312](https://github.com/nodejs/node/pull/8312)
* [[`3295a7feba`](https://github.com/nodejs/node/commit/3295a7feba)] - **(SEMVER-MAJOR)** **src**: allow getting Symbols on process.env (Anna Henningsen) [#9631](https://github.com/nodejs/node/pull/9631)
* [[`1aa595e5bd`](https://github.com/nodejs/node/commit/1aa595e5bd)] - **(SEMVER-MAJOR)** **src**: throw on process.env symbol usage (cjihrig) [#9446](https://github.com/nodejs/node/pull/9446)
* [[`a235ccd168`](https://github.com/nodejs/node/commit/a235ccd168)] - **(SEMVER-MAJOR)** **src,test**: debug is now an alias for inspect (Ali Ijaz Sheikh) [#11441](https://github.com/nodejs/node/pull/11441)
* [[`b6e1d22fa6`](https://github.com/nodejs/node/commit/b6e1d22fa6)] - **(SEMVER-MAJOR)** **stream**: add destroy and _destroy methods. (Matteo Collina) [#12925](https://github.com/nodejs/node/pull/12925)
* [[`f36c970cf3`](https://github.com/nodejs/node/commit/f36c970cf3)] - **(SEMVER-MAJOR)** **stream**: improve multiple callback error message (cjihrig) [#12520](https://github.com/nodejs/node/pull/12520)
* [[`202b07f414`](https://github.com/nodejs/node/commit/202b07f414)] - **(SEMVER-MAJOR)** **stream**: fix comment for sync flag of ReadableState (Wang Xinyong) [#11139](https://github.com/nodejs/node/pull/11139)
* [[`1004b9b4ad`](https://github.com/nodejs/node/commit/1004b9b4ad)] - **(SEMVER-MAJOR)** **stream**: remove unused `ranOut` from ReadableState (Wang Xinyong) [#11139](https://github.com/nodejs/node/pull/11139)
* [[`03b9f6fe26`](https://github.com/nodejs/node/commit/03b9f6fe26)] - **(SEMVER-MAJOR)** **stream**: avoid instanceof (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`a3539ae3be`](https://github.com/nodejs/node/commit/a3539ae3be)] - **(SEMVER-MAJOR)** **stream**: use plain objects for write/corked reqs (Brian White) [#10558](https://github.com/nodejs/node/pull/10558)
* [[`24ef1e6775`](https://github.com/nodejs/node/commit/24ef1e6775)] - **(SEMVER-MAJOR)** **string_decoder**: align UTF-8 handling with V8 (Brian White) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`23fc082409`](https://github.com/nodejs/node/commit/23fc082409)] - **(SEMVER-MAJOR)** **test**: remove extra console output from test-os.js (James M Snell) [#12654](https://github.com/nodejs/node/pull/12654)
* [[`59c6230861`](https://github.com/nodejs/node/commit/59c6230861)] - **(SEMVER-MAJOR)** **test**: cleanup test-child-process-constructor.js (cjihrig) [#12348](https://github.com/nodejs/node/pull/12348)
* [[`06c29a66d4`](https://github.com/nodejs/node/commit/06c29a66d4)] - **(SEMVER-MAJOR)** **test**: remove common.fail() (Rich Trott) [#12293](https://github.com/nodejs/node/pull/12293)
* [[`0c539faac3`](https://github.com/nodejs/node/commit/0c539faac3)] - **(SEMVER-MAJOR)** **test**: add common.getArrayBufferViews(buf) (Timothy Gu) [#12223](https://github.com/nodejs/node/pull/12223)
* [[`c5d1851ac4`](https://github.com/nodejs/node/commit/c5d1851ac4)] - **(SEMVER-MAJOR)** **test**: drop v5.x-specific code path from simd test (Ben Noordhuis) [#11346](https://github.com/nodejs/node/pull/11346)
* [[`c2c6ae52ea`](https://github.com/nodejs/node/commit/c2c6ae52ea)] - **(SEMVER-MAJOR)** **test**: move test-vm-function-redefinition to parallel (Franziska Hinkelmann) [#9618](https://github.com/nodejs/node/pull/9618)
* [[`5b30c4f24d`](https://github.com/nodejs/node/commit/5b30c4f24d)] - **(SEMVER-MAJOR)** **test**: refactor test-child-process-spawnsync-maxbuf (cjihrig) [#10769](https://github.com/nodejs/node/pull/10769)
* [[`348cc80a3c`](https://github.com/nodejs/node/commit/348cc80a3c)] - **(SEMVER-MAJOR)** **tls**: make rejectUnauthorized default to true (ghaiklor) [#5923](https://github.com/nodejs/node/pull/5923)
* [[`a2ae08999b`](https://github.com/nodejs/node/commit/a2ae08999b)] - **(SEMVER-MAJOR)** **tls**: runtime deprecation for tls.createSecurePair() (James M Snell) [#11349](https://github.com/nodejs/node/pull/11349)
* [[`d523eb9c40`](https://github.com/nodejs/node/commit/d523eb9c40)] - **(SEMVER-MAJOR)** **tls**: use emitWarning() for dhparam \< 2048 bits (James M Snell) [#11447](https://github.com/nodejs/node/pull/11447)
* [[`e03a929648`](https://github.com/nodejs/node/commit/e03a929648)] - **(SEMVER-MAJOR)** **tools**: update test-npm.sh paths (Kat Marchán) [#12936](https://github.com/nodejs/node/pull/12936)
* [[`6f202ef857`](https://github.com/nodejs/node/commit/6f202ef857)] - **(SEMVER-MAJOR)** **tools**: remove assert.fail() lint rule (Rich Trott) [#12293](https://github.com/nodejs/node/pull/12293)
* [[`615789b723`](https://github.com/nodejs/node/commit/615789b723)] - **(SEMVER-MAJOR)** **tools**: enable ES2017 syntax support in ESLint (Michaël Zasso) [#11210](https://github.com/nodejs/node/pull/11210)
* [[`1b63fa1096`](https://github.com/nodejs/node/commit/1b63fa1096)] - **(SEMVER-MAJOR)** **tty**: remove NODE_TTY_UNSAFE_ASYNC (Jeremiah Senkpiel) [#12057](https://github.com/nodejs/node/pull/12057)
* [[`78182458e6`](https://github.com/nodejs/node/commit/78182458e6)] - **(SEMVER-MAJOR)** **url**: fix error message of url.format (DavidCai) [#11162](https://github.com/nodejs/node/pull/11162)
* [[`c65d55f087`](https://github.com/nodejs/node/commit/c65d55f087)] - **(SEMVER-MAJOR)** **url**: do not truncate long hostnames (Junshu Okamoto) [#9292](https://github.com/nodejs/node/pull/9292)
* [[`3cc3e099be`](https://github.com/nodejs/node/commit/3cc3e099be)] - **(SEMVER-MAJOR)** **util**: show External values explicitly in inspect (Anna Henningsen) [#12151](https://github.com/nodejs/node/pull/12151)
* [[`4a5a9445b5`](https://github.com/nodejs/node/commit/4a5a9445b5)] - **(SEMVER-MAJOR)** **util**: use `\[Array\]` for deeply nested arrays (Anna Henningsen) [#12046](https://github.com/nodejs/node/pull/12046)
* [[`5bfd13b81e`](https://github.com/nodejs/node/commit/5bfd13b81e)] - **(SEMVER-MAJOR)** **util**: display Symbol keys in inspect by default (Shahar Or) [#9726](https://github.com/nodejs/node/pull/9726)
* [[`455e6f1dd8`](https://github.com/nodejs/node/commit/455e6f1dd8)] - **(SEMVER-MAJOR)** **util**: throw toJSON errors when formatting %j (Timothy Gu) [#11708](https://github.com/nodejs/node/pull/11708)
* [[`ec2f098156`](https://github.com/nodejs/node/commit/ec2f098156)] - **(SEMVER-MAJOR)** **util**: change sparse arrays inspection format (Alexey Orlenko) [#11576](https://github.com/nodejs/node/pull/11576)
* [[`aab0d202f8`](https://github.com/nodejs/node/commit/aab0d202f8)] - **(SEMVER-MAJOR)** **util**: convert inspect.styles and inspect.colors to prototype-less objects (Nemanja Stojanovic) [#11624](https://github.com/nodejs/node/pull/11624)
* [[`4151ab398b`](https://github.com/nodejs/node/commit/4151ab398b)] - **(SEMVER-MAJOR)** **util**: add createClassWrapper to internal/util (James M Snell) [#11391](https://github.com/nodejs/node/pull/11391)
* [[`f65aa08b52`](https://github.com/nodejs/node/commit/f65aa08b52)] - **(SEMVER-MAJOR)** **util**: improve inspect for (Async|Generator)Function (Michaël Zasso) [#11210](https://github.com/nodejs/node/pull/11210)
* [[`efae43f0ee`](https://github.com/nodejs/node/commit/efae43f0ee)] - **(SEMVER-MAJOR)** **zlib**: fix node crashing on invalid options (Alexey Orlenko) [#13098](https://github.com/nodejs/node/pull/13098)
* [[`2ced07ccaf`](https://github.com/nodejs/node/commit/2ced07ccaf)] - **(SEMVER-MAJOR)** **zlib**: support all ArrayBufferView types (Timothy Gu) [#12223](https://github.com/nodejs/node/pull/12223)
* [[`91383e47fd`](https://github.com/nodejs/node/commit/91383e47fd)] - **(SEMVER-MAJOR)** **zlib**: support Uint8Array in convenience methods (Timothy Gu) [#12001](https://github.com/nodejs/node/pull/12001)
* [[`b514bd231e`](https://github.com/nodejs/node/commit/b514bd231e)] - **(SEMVER-MAJOR)** **zlib**: use RangeError/TypeError consistently (James M Snell) [#11391](https://github.com/nodejs/node/pull/11391)
* [[`8e69f7e385`](https://github.com/nodejs/node/commit/8e69f7e385)] - **(SEMVER-MAJOR)** **zlib**: refactor zlib module (James M Snell) [#11391](https://github.com/nodejs/node/pull/11391)
* [[`dd928b04fc`](https://github.com/nodejs/node/commit/dd928b04fc)] - **(SEMVER-MAJOR)** **zlib**: be strict about what strategies are accepted (Rich Trott) [#10934](https://github.com/nodejs/node/pull/10934)

#### Semver-minor Commits

* [[`7e3a3c962f`](https://github.com/nodejs/node/commit/7e3a3c962f)] - **(SEMVER-MINOR)** **async_hooks**: initial async_hooks implementation (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`60a2fe7d47`](https://github.com/nodejs/node/commit/60a2fe7d47)] - **(SEMVER-MINOR)** **async_hooks**: implement C++ embedder API (Anna Henningsen) [#13142](https://github.com/nodejs/node/pull/13142)
* [[`f1ed19d98f`](https://github.com/nodejs/node/commit/f1ed19d98f)] - **(SEMVER-MINOR)** **async_wrap**: use more specific providers (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`0432c6e91e`](https://github.com/nodejs/node/commit/0432c6e91e)] - **(SEMVER-MINOR)** **async_wrap**: use double, not int64_t, for async id (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`fe2df3b842`](https://github.com/nodejs/node/commit/fe2df3b842)] - **(SEMVER-MINOR)** **async_wrap,src**: add GetAsyncId() method (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`6d93508369`](https://github.com/nodejs/node/commit/6d93508369)] - **(SEMVER-MINOR)** **buffer**: expose FastBuffer on internal/buffer (Anna Henningsen) [#11048](https://github.com/nodejs/node/pull/11048)
* [[`fe5ca3ff27`](https://github.com/nodejs/node/commit/fe5ca3ff27)] - **(SEMVER-MINOR)** **child_process**: support promisified `exec(File)` (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`f146fe4ed4`](https://github.com/nodejs/node/commit/f146fe4ed4)] - **(SEMVER-MINOR)** **cmd**: support dash as stdin alias (Ebrahim Byagowi) [#13012](https://github.com/nodejs/node/pull/13012)
* [[`d9f3ec8e09`](https://github.com/nodejs/node/commit/d9f3ec8e09)] - **(SEMVER-MINOR)** **crypto**: use named FunctionTemplate (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`0e710aada4`](https://github.com/nodejs/node/commit/0e710aada4)] - **(SEMVER-MINOR)** **crypto**: add sign/verify support for RSASSA-PSS (Tobias Nießen) [#11705](https://github.com/nodejs/node/pull/11705)
* [[`faf6654ff7`](https://github.com/nodejs/node/commit/faf6654ff7)] - **(SEMVER-MINOR)** **dns**: support promisified `lookup(Service)` (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`5077cde71f`](https://github.com/nodejs/node/commit/5077cde71f)] - **(SEMVER-MINOR)** **doc**: restructure url.md (James M Snell) [#12710](https://github.com/nodejs/node/pull/12710)
* [[`d080ead0f9`](https://github.com/nodejs/node/commit/d080ead0f9)] - **(SEMVER-MINOR)** **doc**: graduate WHATWG URL from Experimental (James M Snell) [#12710](https://github.com/nodejs/node/pull/12710)
* [[`c505b8109e`](https://github.com/nodejs/node/commit/c505b8109e)] - **(SEMVER-MINOR)** **doc**: document URLSearchParams constructor (Timothy Gu) [#11060](https://github.com/nodejs/node/pull/11060)
* [[`84dabe8373`](https://github.com/nodejs/node/commit/84dabe8373)] - **(SEMVER-MINOR)** **domain**: support promises (Anna Henningsen) [#12489](https://github.com/nodejs/node/pull/12489)
* [[`fbcb4f50b8`](https://github.com/nodejs/node/commit/fbcb4f50b8)] - **(SEMVER-MINOR)** **fs**: support util.promisify for fs.read/fs.write (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`a7f5c9cb7d`](https://github.com/nodejs/node/commit/a7f5c9cb7d)] - **(SEMVER-MINOR)** **http**: destroy sockets after keepAliveTimeout (Timur Shemsedinov) [#2534](https://github.com/nodejs/node/pull/2534)
* [[`3e6f1032a4`](https://github.com/nodejs/node/commit/3e6f1032a4)] - **(SEMVER-MINOR)** **http**: add new functions to OutgoingMessage (Brian White) [#10805](https://github.com/nodejs/node/pull/10805)
* [[`c7182b9d9c`](https://github.com/nodejs/node/commit/c7182b9d9c)] - **(SEMVER-MINOR)** **inspector**: JavaScript bindings for the inspector (Eugene Ostroukhov) [#12263](https://github.com/nodejs/node/pull/12263)
* [[`4a7233c178`](https://github.com/nodejs/node/commit/4a7233c178)] - **(SEMVER-MINOR)** **lib**: implement async_hooks API in core (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`c68ebe8436`](https://github.com/nodejs/node/commit/c68ebe8436)] - **(SEMVER-MINOR)** **makefile**: add async-hooks to test and test-ci (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`45139e59f3`](https://github.com/nodejs/node/commit/45139e59f3)] - **(SEMVER-MINOR)** **n-api**: add napi_get_version (Michael Dawson) [#13207](https://github.com/nodejs/node/pull/13207)
* [[`56e881d0b0`](https://github.com/nodejs/node/commit/56e881d0b0)] - **(SEMVER-MINOR)** **n-api**: add support for abi stable module API (Jason Ginchereau) [#11975](https://github.com/nodejs/node/pull/11975)
* [[`dd20e68b0f`](https://github.com/nodejs/node/commit/dd20e68b0f)] - **(SEMVER-MINOR)** **process**: add optional detail to process emitWarning (James M Snell) [#12725](https://github.com/nodejs/node/pull/12725)
* [[`c0bde73f1b`](https://github.com/nodejs/node/commit/c0bde73f1b)] - **(SEMVER-MINOR)** **src**: implement native changes for async_hooks (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`e5a25cbc85`](https://github.com/nodejs/node/commit/e5a25cbc85)] - **(SEMVER-MINOR)** **src**: expose `node::AddPromiseHook` (Anna Henningsen) [#12489](https://github.com/nodejs/node/pull/12489)
* [[`ec53921d2e`](https://github.com/nodejs/node/commit/ec53921d2e)] - **(SEMVER-MINOR)** **src**: make AtExit callback's per Environment (Daniel Bevenius) [#9163](https://github.com/nodejs/node/pull/9163)
* [[`ba4847e879`](https://github.com/nodejs/node/commit/ba4847e879)] - **(SEMVER-MINOR)** **src**: Node Tracing Controller (misterpoe) [#9304](https://github.com/nodejs/node/pull/9304)
* [[`6ff3b03240`](https://github.com/nodejs/node/commit/6ff3b03240)] - **(SEMVER-MINOR)** **src, inspector**: add --inspect-brk option (Josh Gavant) [#8979](https://github.com/nodejs/node/pull/8979)
* [[`220186c4a8`](https://github.com/nodejs/node/commit/220186c4a8)] - **(SEMVER-MINOR)** **stream**: support Uint8Array input to methods (Anna Henningsen) [#11608](https://github.com/nodejs/node/pull/11608)
* [[`07c7f198db`](https://github.com/nodejs/node/commit/07c7f198db)] - **(SEMVER-MINOR)** **stream**: add final method (Calvin Metcalf) [#12828](https://github.com/nodejs/node/pull/12828)
* [[`11918c4aed`](https://github.com/nodejs/node/commit/11918c4aed)] - **(SEMVER-MINOR)** **stream**: fix highWaterMark integer overflow (Tobias Nießen) [#12593](https://github.com/nodejs/node/pull/12593)
* [[`c56d6046ec`](https://github.com/nodejs/node/commit/c56d6046ec)] - **(SEMVER-MINOR)** **test**: add AsyncResource addon test (Anna Henningsen) [#13142](https://github.com/nodejs/node/pull/13142)
* [[`e3e56f1d71`](https://github.com/nodejs/node/commit/e3e56f1d71)] - **(SEMVER-MINOR)** **test**: adding tests for initHooks API (Thorsten Lorenz) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`732620cfe9`](https://github.com/nodejs/node/commit/732620cfe9)] - **(SEMVER-MINOR)** **test**: remove unneeded tests (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`e965ed16c1`](https://github.com/nodejs/node/commit/e965ed16c1)] - **(SEMVER-MINOR)** **test**: add test for promisify customPromisifyArgs (Gil Tayar) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`3ea2301e38`](https://github.com/nodejs/node/commit/3ea2301e38)] - **(SEMVER-MINOR)** **test**: add a bunch of tests from bluebird (Madara Uchiha) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`dca08152cb`](https://github.com/nodejs/node/commit/dca08152cb)] - **(SEMVER-MINOR)** **test**: introduce `common.crashOnUnhandledRejection` (Anna Henningsen) [#12489](https://github.com/nodejs/node/pull/12489)
* [[`e7c51454b0`](https://github.com/nodejs/node/commit/e7c51454b0)] - **(SEMVER-MINOR)** **timers**: add promisify support (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`e600fbe576`](https://github.com/nodejs/node/commit/e600fbe576)] - **(SEMVER-MINOR)** **tls**: accept `lookup` option for `tls.connect()` (Fedor Indutny) [#12839](https://github.com/nodejs/node/pull/12839)
* [[`c3efe72669`](https://github.com/nodejs/node/commit/c3efe72669)] - **(SEMVER-MINOR)** **tls**: support Uint8Arrays for protocol list buffers (Anna Henningsen) [#11984](https://github.com/nodejs/node/pull/11984)
* [[`29f758731f`](https://github.com/nodejs/node/commit/29f758731f)] - **(SEMVER-MINOR)** **tools**: add MDN link for Iterable (Timothy Gu) [#11060](https://github.com/nodejs/node/pull/11060)
* [[`4b9d84df51`](https://github.com/nodejs/node/commit/4b9d84df51)] - **(SEMVER-MINOR)** **tty_wrap**: throw when uv_tty_init() returns error (Trevor Norris) [#12892](https://github.com/nodejs/node/pull/12892)
* [[`cc48f21c83`](https://github.com/nodejs/node/commit/cc48f21c83)] - **(SEMVER-MINOR)** **url**: extend URLSearchParams constructor (Timothy Gu) [#11060](https://github.com/nodejs/node/pull/11060)
* [[`99da8e8e02`](https://github.com/nodejs/node/commit/99da8e8e02)] - **(SEMVER-MINOR)** **util**: add util.promisify() (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`059f296050`](https://github.com/nodejs/node/commit/059f296050)] - **(SEMVER-MINOR)** **util**: add internal bindings for promise handling (Anna Henningsen) [#12442](https://github.com/nodejs/node/pull/12442)
* [[`1fde98bb4f`](https://github.com/nodejs/node/commit/1fde98bb4f)] - **(SEMVER-MINOR)** **v8**: expose new V8 serialization API (Anna Henningsen) [#11048](https://github.com/nodejs/node/pull/11048)
* [[`70beef97bd`](https://github.com/nodejs/node/commit/70beef97bd)] - **(SEMVER-MINOR)** **v8**: add cachedDataVersionTag (Andres Suarez) [#11515](https://github.com/nodejs/node/pull/11515)

#### Semver-patch commits

* [[`276720921b`](https://github.com/nodejs/node/commit/276720921b)] - **addons**: remove semicolons from after module definition (Gabriel Schulhof) [#12919](https://github.com/nodejs/node/pull/12919)
* [[`f6247a945c`](https://github.com/nodejs/node/commit/f6247a945c)] - **assert**: restore TypeError if no arguments (Rich Trott) [#12843](https://github.com/nodejs/node/pull/12843)
* [[`7e5f500c98`](https://github.com/nodejs/node/commit/7e5f500c98)] - **assert**: improve deepEqual perf for large input (Anna Henningsen) [#12849](https://github.com/nodejs/node/pull/12849)
* [[`3863c3ae66`](https://github.com/nodejs/node/commit/3863c3ae66)] - **async_hooks**: rename AsyncEvent to AsyncResource (Anna Henningsen) [#13192](https://github.com/nodejs/node/pull/13192)
* [[`e554bb449e`](https://github.com/nodejs/node/commit/e554bb449e)] - **async_hooks**: only set up hooks if used (Anna Henningsen) [#13177](https://github.com/nodejs/node/pull/13177)
* [[`6fb27af70a`](https://github.com/nodejs/node/commit/6fb27af70a)] - **async_hooks**: add constructor check to async-hooks (Shadowbeetle) [#13096](https://github.com/nodejs/node/pull/13096)
* [[`a6023ee0b5`](https://github.com/nodejs/node/commit/a6023ee0b5)] - **async_wrap**: fix Promises with later enabled hooks (Anna Henningsen) [#13242](https://github.com/nodejs/node/pull/13242)
* [[`6bfdeedce5`](https://github.com/nodejs/node/commit/6bfdeedce5)] - **async_wrap**: add `asyncReset` to `TLSWrap` (Refael Ackermann) [#13092](https://github.com/nodejs/node/pull/13092)
* [[`4a8ea63b3b`](https://github.com/nodejs/node/commit/4a8ea63b3b)] - **async_wrap,src**: wrap promises directly (Matt Loring) [#13242](https://github.com/nodejs/node/pull/13242)
* [[`6e4394fb0b`](https://github.com/nodejs/node/commit/6e4394fb0b)] - **async_wrap,src**: promise hook integration (Matt Loring) [#13000](https://github.com/nodejs/node/pull/13000)
* [[`72429b3981`](https://github.com/nodejs/node/commit/72429b3981)] - **benchmark**: allow no duration in benchmark tests (Rich Trott) [#13110](https://github.com/nodejs/node/pull/13110)
* [[`f2ba06db92`](https://github.com/nodejs/node/commit/f2ba06db92)] - **benchmark**: remove redundant timers benchmark (Rich Trott) [#13009](https://github.com/nodejs/node/pull/13009)
* [[`3fa5d80eda`](https://github.com/nodejs/node/commit/3fa5d80eda)] - **benchmark**: chunky http client should exit with 0 (Joyee Cheung) [#12916](https://github.com/nodejs/node/pull/12916)
* [[`a82e0e6f36`](https://github.com/nodejs/node/commit/a82e0e6f36)] - **benchmark**: check for time precision in common.js (Rich Trott) [#12934](https://github.com/nodejs/node/pull/12934)
* [[`65d6249979`](https://github.com/nodejs/node/commit/65d6249979)] - **benchmark**: update an obsolete path (Vse Mozhet Byt) [#12904](https://github.com/nodejs/node/pull/12904)
* [[`d8965d5b0e`](https://github.com/nodejs/node/commit/d8965d5b0e)] - **benchmark**: fix typo in _http-benchmarkers.js (Vse Mozhet Byt) [#12455](https://github.com/nodejs/node/pull/12455)
* [[`a3778cb9b1`](https://github.com/nodejs/node/commit/a3778cb9b1)] - **benchmark**: fix URL in _http-benchmarkers.js (Vse Mozhet Byt) [#12455](https://github.com/nodejs/node/pull/12455)
* [[`22aa3d4899`](https://github.com/nodejs/node/commit/22aa3d4899)] - **benchmark**: reduce string concatenations (Vse Mozhet Byt) [#12455](https://github.com/nodejs/node/pull/12455)
* [[`3e3414f45f`](https://github.com/nodejs/node/commit/3e3414f45f)] - **benchmark**: control HTTP benchmarks run time (Joyee Cheung) [#12121](https://github.com/nodejs/node/pull/12121)
* [[`a3e71a8901`](https://github.com/nodejs/node/commit/a3e71a8901)] - **benchmark**: add test double HTTP benchmarker (Joyee Cheung) [#12121](https://github.com/nodejs/node/pull/12121)
* [[`a6e69f8c08`](https://github.com/nodejs/node/commit/a6e69f8c08)] - **benchmark**: add more options to map-bench (Timothy Gu) [#11930](https://github.com/nodejs/node/pull/11930)
* [[`ae8a8691e6`](https://github.com/nodejs/node/commit/ae8a8691e6)] - **benchmark**: add final clean-up to module-loader.js (Vse Mozhet Byt) [#11924](https://github.com/nodejs/node/pull/11924)
* [[`6df23fa39f`](https://github.com/nodejs/node/commit/6df23fa39f)] - **benchmark**: fix punycode and get-ciphers benchmark (Bartosz Sosnowski) [#11720](https://github.com/nodejs/node/pull/11720)
* [[`75cdc895ec`](https://github.com/nodejs/node/commit/75cdc895ec)] - **benchmark**: cleanup after forced optimization drop (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`ca86aa5323`](https://github.com/nodejs/node/commit/ca86aa5323)] - **benchmark**: remove forced optimization from util (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`c5958d20fd`](https://github.com/nodejs/node/commit/c5958d20fd)] - **benchmark**: remove forced optimization from url (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`ea61ce518b`](https://github.com/nodejs/node/commit/ea61ce518b)] - **benchmark**: remove forced optimization from tls (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`541119c6ee`](https://github.com/nodejs/node/commit/541119c6ee)] - **benchmark**: remove streams forced optimization (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`57b5ce1d8e`](https://github.com/nodejs/node/commit/57b5ce1d8e)] - **benchmark**: remove querystring forced optimization (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`eba2c62bb1`](https://github.com/nodejs/node/commit/eba2c62bb1)] - **benchmark**: remove forced optimization from path (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`7587a11adc`](https://github.com/nodejs/node/commit/7587a11adc)] - **benchmark**: remove forced optimization from misc (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`ef8cc301fe`](https://github.com/nodejs/node/commit/ef8cc301fe)] - **benchmark**: remove forced optimization from es (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`17c85ffd80`](https://github.com/nodejs/node/commit/17c85ffd80)] - **benchmark**: remove forced optimization from crypto (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`05ac6e1b01`](https://github.com/nodejs/node/commit/05ac6e1b01)] - **benchmark**: remove forced optimization from buffer (Bartosz Sosnowski) [#9615](https://github.com/nodejs/node/pull/9615)
* [[`6123ed5b25`](https://github.com/nodejs/node/commit/6123ed5b25)] - **benchmark**: add USVString conversion benchmark (Timothy Gu) [#11436](https://github.com/nodejs/node/pull/11436)
* [[`28ddac2ec2`](https://github.com/nodejs/node/commit/28ddac2ec2)] - **buffer**: fix indexOf for empty searches (Anna Henningsen) [#13024](https://github.com/nodejs/node/pull/13024)
* [[`9d723e85fb`](https://github.com/nodejs/node/commit/9d723e85fb)] - **buffer**: remove pointless C++ utility methods (Anna Henningsen) [#12760](https://github.com/nodejs/node/pull/12760)
* [[`7cd0d4f644`](https://github.com/nodejs/node/commit/7cd0d4f644)] - **buffer**: fix backwards incompatibility (Brian White) [#12439](https://github.com/nodejs/node/pull/12439)
* [[`3ee4a1a281`](https://github.com/nodejs/node/commit/3ee4a1a281)] - **buffer**: optimize toString() (Brian White) [#12361](https://github.com/nodejs/node/pull/12361)
* [[`4a86803f60`](https://github.com/nodejs/node/commit/4a86803f60)] - **buffer**: optimize from() and byteLength() (Brian White) [#12361](https://github.com/nodejs/node/pull/12361)
* [[`00c86cc8e9`](https://github.com/nodejs/node/commit/00c86cc8e9)] - **buffer**: remove Uint8Array check (Nikolai Vavilov) [#11324](https://github.com/nodejs/node/pull/11324)
* [[`fb6cf2f861`](https://github.com/nodejs/node/commit/fb6cf2f861)] - **build**: xz tarball extreme compression (Peter Dave Hello) [#10626](https://github.com/nodejs/node/pull/10626)
* [[`4f4d5d24f4`](https://github.com/nodejs/node/commit/4f4d5d24f4)] - **build**: ignore more VC++ artifacts (Refael Ackermann) [#13208](https://github.com/nodejs/node/pull/13208)
* [[`735902f326`](https://github.com/nodejs/node/commit/735902f326)] - **build**: support dtrace on ARM (Bradley T. Hughes) [#12193](https://github.com/nodejs/node/pull/12193)
* [[`46bd32e7e8`](https://github.com/nodejs/node/commit/46bd32e7e8)] - **build**: fix openssl link error on windows (Daniel Bevenius) [#13078](https://github.com/nodejs/node/pull/13078)
* [[`77dfa2b1da`](https://github.com/nodejs/node/commit/77dfa2b1da)] - **build**: avoid /docs/api and /docs/doc/api upload (Rod Vagg) [#12957](https://github.com/nodejs/node/pull/12957)
* [[`6342988053`](https://github.com/nodejs/node/commit/6342988053)] - **build**: clean up napi build in test-addons-clean (Joyee Cheung) [#13034](https://github.com/nodejs/node/pull/13034)
* [[`ad7b98baa8`](https://github.com/nodejs/node/commit/ad7b98baa8)] - **build**: don't print directory for GNUMake (Daniel Bevenius) [#13042](https://github.com/nodejs/node/pull/13042)
* [[`80355271c3`](https://github.com/nodejs/node/commit/80355271c3)] - **build**: simplify `if` in setting of arg_paths (Refael Ackermann) [#12653](https://github.com/nodejs/node/pull/12653)
* [[`4aff0563aa`](https://github.com/nodejs/node/commit/4aff0563aa)] - **build**: reduce one level of spawning in node_gyp (Refael Ackermann) [#12653](https://github.com/nodejs/node/pull/12653)
* [[`9fd22bc4d4`](https://github.com/nodejs/node/commit/9fd22bc4d4)] - **build**: fix ninja build failure (GYP patch) (Daniel Bevenius) [#12484](https://github.com/nodejs/node/pull/12484)
* [[`bb88caec06`](https://github.com/nodejs/node/commit/bb88caec06)] - **build**: fix ninja build failure (Daniel Bevenius) [#12484](https://github.com/nodejs/node/pull/12484)
* [[`e488857402`](https://github.com/nodejs/node/commit/e488857402)] - **build**: add static option to vcbuild.bat (Tony Rice) [#12764](https://github.com/nodejs/node/pull/12764)
* [[`d727d5d2cf`](https://github.com/nodejs/node/commit/d727d5d2cf)] - **build**: enable cctest to use objects (gyp part) (Daniel Bevenius) [#12450](https://github.com/nodejs/node/pull/12450)
* [[`ea44b8b283`](https://github.com/nodejs/node/commit/ea44b8b283)] - **build**: disable -O3 for C++ coverage (Anna Henningsen) [#12406](https://github.com/nodejs/node/pull/12406)
* [[`baa2602539`](https://github.com/nodejs/node/commit/baa2602539)] - **build**: add test-gc-clean and test-gc PHONY rules (Joyee Cheung) [#12059](https://github.com/nodejs/node/pull/12059)
* [[`c694633328`](https://github.com/nodejs/node/commit/c694633328)] - **build**: sort phony rules (Joyee Cheung) [#12059](https://github.com/nodejs/node/pull/12059)
* [[`4dde87620a`](https://github.com/nodejs/node/commit/4dde87620a)] - **build**: don't test addons-napi twice (Gibson Fahnestock) [#12201](https://github.com/nodejs/node/pull/12201)
* [[`d19809a3c5`](https://github.com/nodejs/node/commit/d19809a3c5)] - **build**: avoid passing kill empty input in Makefile (Gibson Fahnestock) [#12158](https://github.com/nodejs/node/pull/12158)
* [[`c68da89694`](https://github.com/nodejs/node/commit/c68da89694)] - **build**: always use V8_ENABLE_CHECKS in debug mode (Anna Henningsen) [#12029](https://github.com/nodejs/node/pull/12029)
* [[`7cd6a7ddcb`](https://github.com/nodejs/node/commit/7cd6a7ddcb)] - **build**: notify about the redundancy of "nosign" (Nikolai Vavilov) [#11119](https://github.com/nodejs/node/pull/11119)
* [[`dd81d539e2`](https://github.com/nodejs/node/commit/dd81d539e2)] - **child_process**: fix deoptimizing use of arguments (Vse Mozhet Byt) [#11535](https://github.com/nodejs/node/pull/11535)
* [[`dc3bbb45a7`](https://github.com/nodejs/node/commit/dc3bbb45a7)] - **cluster**: remove debug arg handling (Rich Trott) [#12738](https://github.com/nodejs/node/pull/12738)
* [[`c969047d62`](https://github.com/nodejs/node/commit/c969047d62)] - **console**: fixup `console.dir()` error handling (Anna Henningsen) [#11443](https://github.com/nodejs/node/pull/11443)
* [[`9fa148909c`](https://github.com/nodejs/node/commit/9fa148909c)] - **crypto**: update root certificates (Ben Noordhuis) [#13279](https://github.com/nodejs/node/pull/13279)
* [[`945916195c`](https://github.com/nodejs/node/commit/945916195c)] - **crypto**: return CHECK_OK in VerifyCallback (Daniel Bevenius) [#13241](https://github.com/nodejs/node/pull/13241)
* [[`7b97f07340`](https://github.com/nodejs/node/commit/7b97f07340)] - **crypto**: remove root_cert_store from node_crypto.h (Daniel Bevenius) [#13194](https://github.com/nodejs/node/pull/13194)
* [[`f06f8365e4`](https://github.com/nodejs/node/commit/f06f8365e4)] - **crypto**: remove unnecessary template class (Daniel Bevenius) [#12993](https://github.com/nodejs/node/pull/12993)
* [[`6c2daf0ce9`](https://github.com/nodejs/node/commit/6c2daf0ce9)] - **crypto**: throw proper errors if out enc is UTF-16 (Anna Henningsen) [#12752](https://github.com/nodejs/node/pull/12752)
* [[`eaa0542eff`](https://github.com/nodejs/node/commit/eaa0542eff)] - **crypto**: remove unused C++ parameter in sign/verify (Tobias Nießen) [#12397](https://github.com/nodejs/node/pull/12397)
* [[`6083c4dc10`](https://github.com/nodejs/node/commit/6083c4dc10)] - **deps**: upgrade npm to 5.0.0 (Kat Marchán) [#13276](https://github.com/nodejs/node/pull/13276)
* [[`f204945495`](https://github.com/nodejs/node/commit/f204945495)] - **deps**: add example of comparing OpenSSL changes (Daniel Bevenius) [#13234](https://github.com/nodejs/node/pull/13234)
* [[`9302f512f8`](https://github.com/nodejs/node/commit/9302f512f8)] - **deps**: cherry-pick 6803eef from V8 upstream (Matt Loring) [#13175](https://github.com/nodejs/node/pull/13175)
* [[`2648c8de30`](https://github.com/nodejs/node/commit/2648c8de30)] - **deps**: backport 6d38f89d from upstream V8 (Ali Ijaz Sheikh) [#13162](https://github.com/nodejs/node/pull/13162)
* [[`6e1407c3b3`](https://github.com/nodejs/node/commit/6e1407c3b3)] - **deps**: backport 4fdf9fd4813 from upstream v8 (Jochen Eisinger) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`385499ccf8`](https://github.com/nodejs/node/commit/385499ccf8)] - **deps**: backport 4acdb5eec2c from upstream v8 (jbroman) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`69161b5f94`](https://github.com/nodejs/node/commit/69161b5f94)] - **deps**: backport 3700a01c82 from upstream v8 (jbroman) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`9edd6d8ddb`](https://github.com/nodejs/node/commit/9edd6d8ddb)] - **deps**: backport 2cd2f5feff3 from upstream v8 (Jochen Eisinger) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`19c0c07446`](https://github.com/nodejs/node/commit/19c0c07446)] - **deps**: backport de1461b7efd from upstream v8 (addaleax) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`95c4b0d8f6`](https://github.com/nodejs/node/commit/95c4b0d8f6)] - **deps**: backport 78867ad8707a016 from v8 upstream (Michael Lippautz) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`986e1d2c6f`](https://github.com/nodejs/node/commit/986e1d2c6f)] - **deps**: cherry-pick f5fad6d from upstream v8 (daniel.bevenius) [#12826](https://github.com/nodejs/node/pull/12826)
* [[`e896898dea`](https://github.com/nodejs/node/commit/e896898dea)] - **deps**: update openssl asm and asm_obsolete files (Shigeki Ohtsu) [#12913](https://github.com/nodejs/node/pull/12913)
* [[`f4390650e3`](https://github.com/nodejs/node/commit/f4390650e3)] - **deps**: cherry-pick 4ae5993 from upstream OpenSSL (Shigeki Ohtsu) [#12913](https://github.com/nodejs/node/pull/12913)
* [[`5d0a770c12`](https://github.com/nodejs/node/commit/5d0a770c12)] - **deps**: ICU 59.1 bump (Steven R. Loomis) [#12486](https://github.com/nodejs/node/pull/12486)
* [[`d74a545535`](https://github.com/nodejs/node/commit/d74a545535)] - **deps**: cherry-pick bfae9db from upstream v8 (Ben Noordhuis) [#12722](https://github.com/nodejs/node/pull/12722)
* [[`6ed791c665`](https://github.com/nodejs/node/commit/6ed791c665)] - **deps**: cherry-pick bfae9db from upstream v8 (Ben Noordhuis) [#12722](https://github.com/nodejs/node/pull/12722)
* [[`0084260448`](https://github.com/nodejs/node/commit/0084260448)] - **deps**: upgrade npm to 4.5.0 (Rebecca Turner) [#12480](https://github.com/nodejs/node/pull/12480)
* [[`021719738e`](https://github.com/nodejs/node/commit/021719738e)] - **deps**: update node-inspect to v1.11.2 (Jan Krems) [#12363](https://github.com/nodejs/node/pull/12363)
* [[`3471d6312d`](https://github.com/nodejs/node/commit/3471d6312d)] - **deps**: cherry-pick 0ba513f05 from V8 upstream (Franziska Hinkelmann) [#11712](https://github.com/nodejs/node/pull/11712)
* [[`dd8982dc74`](https://github.com/nodejs/node/commit/dd8982dc74)] - **deps**: cherry-pick 09de996 from V8 upstream (Franziska Hinkelmann) [#11905](https://github.com/nodejs/node/pull/11905)
* [[`a44aff4770`](https://github.com/nodejs/node/commit/a44aff4770)] - **deps**: cherry-pick 0ba513f05 from V8 upstream (Franziska Hinkelmann) [#11712](https://github.com/nodejs/node/pull/11712)
* [[`2b541471db`](https://github.com/nodejs/node/commit/2b541471db)] - **dns**: fix `resolve` failed starts without network (XadillaX) [#13076](https://github.com/nodejs/node/pull/13076)
* [[`5a948f6f64`](https://github.com/nodejs/node/commit/5a948f6f64)] - **dns**: fix crash using dns.setServers after resolve4 (XadillaX) [#13050](https://github.com/nodejs/node/pull/13050)
* [[`dd14ab608e`](https://github.com/nodejs/node/commit/dd14ab608e)] - **doc**: create list of CTC emeriti (Rich Trott) [#13232](https://github.com/nodejs/node/pull/13232)
* [[`40572c5def`](https://github.com/nodejs/node/commit/40572c5def)] - **doc**: remove Gitter badge from README (Rich Trott) [#13231](https://github.com/nodejs/node/pull/13231)
* [[`686dd36930`](https://github.com/nodejs/node/commit/686dd36930)] - **doc**: fix api docs style (Daijiro Wachi) [#13236](https://github.com/nodejs/node/pull/13236)
* [[`0be029ec97`](https://github.com/nodejs/node/commit/0be029ec97)] - **doc**: make spelling of behavior consistent (Michael Dawson) [#13245](https://github.com/nodejs/node/pull/13245)
* [[`c0a7c8e0d2`](https://github.com/nodejs/node/commit/c0a7c8e0d2)] - **doc**: fix code example in inspector.md (Vse Mozhet Byt) [#13182](https://github.com/nodejs/node/pull/13182)
* [[`cd2824cc93`](https://github.com/nodejs/node/commit/cd2824cc93)] - **doc**: make the style of notes consistent (Alexey Orlenko) [#13133](https://github.com/nodejs/node/pull/13133)
* [[`d4e9e0f7e4`](https://github.com/nodejs/node/commit/d4e9e0f7e4)] - **doc**: add jasongin & kunalspathak to collaborators (Jason Ginchereau) [#13200](https://github.com/nodejs/node/pull/13200)
* [[`db90b505e8`](https://github.com/nodejs/node/commit/db90b505e8)] - **doc**: don't use useless constructors in stream.md (Vse Mozhet Byt) [#13145](https://github.com/nodejs/node/pull/13145)
* [[`2c45e6fd68`](https://github.com/nodejs/node/commit/2c45e6fd68)] - **doc**: update code example for Windows in stream.md (Vse Mozhet Byt) [#13138](https://github.com/nodejs/node/pull/13138)
* [[`3c91145f31`](https://github.com/nodejs/node/commit/3c91145f31)] - **doc**: improve formatting of STYLE_GUIDE.md (Alexey Orlenko) [#13135](https://github.com/nodejs/node/pull/13135)
* [[`1d587ef982`](https://github.com/nodejs/node/commit/1d587ef982)] - **doc**: fix incorrect keyboard shortcut (Alexey Orlenko) [#13134](https://github.com/nodejs/node/pull/13134)
* [[`336d33b646`](https://github.com/nodejs/node/commit/336d33b646)] - **doc**: fix title/function name mismatch (Michael Dawson) [#13123](https://github.com/nodejs/node/pull/13123)
* [[`9f01b34bf9`](https://github.com/nodejs/node/commit/9f01b34bf9)] - **doc**: link to `common` docs in test writing guide (Anna Henningsen) [#13118](https://github.com/nodejs/node/pull/13118)
* [[`1aa68f9a8d`](https://github.com/nodejs/node/commit/1aa68f9a8d)] - **doc**: list macOS as supporting filename argument (Chris Young) [#13111](https://github.com/nodejs/node/pull/13111)
* [[`ef71824740`](https://github.com/nodejs/node/commit/ef71824740)] - **doc**: edit Error.captureStackTrace html comment (Artur Vieira) [#12962](https://github.com/nodejs/node/pull/12962)
* [[`bfade5aacd`](https://github.com/nodejs/node/commit/bfade5aacd)] - **doc**: remove unused/duplicated reference links (Daijiro Wachi) [#13066](https://github.com/nodejs/node/pull/13066)
* [[`4a7b7e8097`](https://github.com/nodejs/node/commit/4a7b7e8097)] - **doc**: add reference to node_api.h in docs (Michael Dawson) [#13084](https://github.com/nodejs/node/pull/13084)
* [[`3702ae732e`](https://github.com/nodejs/node/commit/3702ae732e)] - **doc**: add additional useful ci job to list (Michael Dawson) [#13086](https://github.com/nodejs/node/pull/13086)
* [[`847688018c`](https://github.com/nodejs/node/commit/847688018c)] - **doc**: don't suggest setEncoding for binary streams (Rick Bullotta) [#11363](https://github.com/nodejs/node/pull/11363)
* [[`eff9252181`](https://github.com/nodejs/node/commit/eff9252181)] - **doc**: update doc of publicEncrypt method (Faiz Halde) [#12947](https://github.com/nodejs/node/pull/12947)
* [[`ab34f9dec2`](https://github.com/nodejs/node/commit/ab34f9dec2)] - **doc**: update doc to remove usage of "you" (Michael Dawson) [#13067](https://github.com/nodejs/node/pull/13067)
* [[`5de722ab6d`](https://github.com/nodejs/node/commit/5de722ab6d)] - **doc**: fix links from ToC to subsection for 4.8.x (Frank Lanitz) [#13039](https://github.com/nodejs/node/pull/13039)
* [[`92f3b301ab`](https://github.com/nodejs/node/commit/92f3b301ab)] - **doc**: document method for reverting commits (Gibson Fahnestock) [#13015](https://github.com/nodejs/node/pull/13015)
* [[`1b28022de0`](https://github.com/nodejs/node/commit/1b28022de0)] - **doc**: clarify operation of napi_cancel_async_work (Michael Dawson) [#12974](https://github.com/nodejs/node/pull/12974)
* [[`1d5f5aa7e1`](https://github.com/nodejs/node/commit/1d5f5aa7e1)] - **doc**: update COLLABORATOR_GUIDE.md (morrme) [#12555](https://github.com/nodejs/node/pull/12555)
* [[`d7d16f7b8b`](https://github.com/nodejs/node/commit/d7d16f7b8b)] - **doc**: Change options at STEP 5 in CONTRIBUTING.md (kysnm) [#12830](https://github.com/nodejs/node/pull/12830)
* [[`c79deaab82`](https://github.com/nodejs/node/commit/c79deaab82)] - **doc**: update to add ref to supported platforms (Michael Dawson) [#12931](https://github.com/nodejs/node/pull/12931)
* [[`abfd4bf9df`](https://github.com/nodejs/node/commit/abfd4bf9df)] - **doc**: clarify node.js addons are c++ (Beth Griggs) [#12898](https://github.com/nodejs/node/pull/12898)
* [[`13487c437c`](https://github.com/nodejs/node/commit/13487c437c)] - **doc**: add docs for server.address() for pipe case (Flarna) [#12907](https://github.com/nodejs/node/pull/12907)
* [[`147048a0d3`](https://github.com/nodejs/node/commit/147048a0d3)] - **doc**: fix broken links in n-api doc (Michael Dawson) [#12889](https://github.com/nodejs/node/pull/12889)
* [[`e429f9a42a`](https://github.com/nodejs/node/commit/e429f9a42a)] - **doc**: fix typo in streams.md (Glenn Schlereth) [#12924](https://github.com/nodejs/node/pull/12924)
* [[`ea1b8a5cbc`](https://github.com/nodejs/node/commit/ea1b8a5cbc)] - **doc**: sort bottom-of-file markdown links (Sam Roberts) [#12726](https://github.com/nodejs/node/pull/12726)
* [[`cbd6fde9a3`](https://github.com/nodejs/node/commit/cbd6fde9a3)] - **doc**: improve path.posix.normalize docs (Steven Lehn) [#12700](https://github.com/nodejs/node/pull/12700)
* [[`a398516b4f`](https://github.com/nodejs/node/commit/a398516b4f)] - **doc**: remove test-npm from general build doc (Rich Trott) [#12840](https://github.com/nodejs/node/pull/12840)
* [[`4703824276`](https://github.com/nodejs/node/commit/4703824276)] - **doc**: fix commit guideline url (Thomas Watson) [#12862](https://github.com/nodejs/node/pull/12862)
* [[`2614d247fb`](https://github.com/nodejs/node/commit/2614d247fb)] - **doc**: update readFileSync in fs.md (Aditya Anand) [#12800](https://github.com/nodejs/node/pull/12800)
* [[`0258aed9d2`](https://github.com/nodejs/node/commit/0258aed9d2)] - **doc**: edit CONTRIBUTING.md for clarity etc. (Rich Trott) [#12796](https://github.com/nodejs/node/pull/12796)
* [[`c1b3b95939`](https://github.com/nodejs/node/commit/c1b3b95939)] - **doc**: add WHATWG file URLs in fs module (Olivier Martin) [#12670](https://github.com/nodejs/node/pull/12670)
* [[`2bf461e6f5`](https://github.com/nodejs/node/commit/2bf461e6f5)] - **doc**: document vm timeout option perf impact (Anna Henningsen) [#12751](https://github.com/nodejs/node/pull/12751)
* [[`d8f8096ec6`](https://github.com/nodejs/node/commit/d8f8096ec6)] - **doc**: update example in repl.md (Vse Mozhet Byt) [#12685](https://github.com/nodejs/node/pull/12685)
* [[`deb9622b11`](https://github.com/nodejs/node/commit/deb9622b11)] - **doc**: Add initial documentation for N-API (Michael Dawson) [#12549](https://github.com/nodejs/node/pull/12549)
* [[`71911be1de`](https://github.com/nodejs/node/commit/71911be1de)] - **doc**: clarify arch support for power platforms (Michael Dawson) [#12679](https://github.com/nodejs/node/pull/12679)
* [[`71f22c842b`](https://github.com/nodejs/node/commit/71f22c842b)] - **doc**: replace uses of `you` and other style nits (James M Snell) [#12673](https://github.com/nodejs/node/pull/12673)
* [[`35d2137715`](https://github.com/nodejs/node/commit/35d2137715)] - **doc**: modernize and fix code examples in repl.md (Vse Mozhet Byt) [#12634](https://github.com/nodejs/node/pull/12634)
* [[`6ee6aaefa1`](https://github.com/nodejs/node/commit/6ee6aaefa1)] - **doc**: add no-var, prefer-const in doc eslintrc (Vse Mozhet Byt) [#12563](https://github.com/nodejs/node/pull/12563)
* [[`b4fea2a3d6`](https://github.com/nodejs/node/commit/b4fea2a3d6)] - **doc**: add eslint-plugin-markdown (Vse Mozhet Byt) [#12563](https://github.com/nodejs/node/pull/12563)
* [[`e2c3e4727d`](https://github.com/nodejs/node/commit/e2c3e4727d)] - **doc**: conform to rules for eslint-plugin-markdown (Vse Mozhet Byt) [#12563](https://github.com/nodejs/node/pull/12563)
* [[`211813c99c`](https://github.com/nodejs/node/commit/211813c99c)] - **doc**: unify quotes in an assert.md code example (Vse Mozhet Byt) [#12505](https://github.com/nodejs/node/pull/12505)
* [[`b4f59a7460`](https://github.com/nodejs/node/commit/b4f59a7460)] - **doc**: upgrade Clang requirement to 3.4.2 (Michaël Zasso) [#12388](https://github.com/nodejs/node/pull/12388)
* [[`b837bd2792`](https://github.com/nodejs/node/commit/b837bd2792)] - **doc**: fix typo in CHANGELOG.md (Gautam krishna.R) [#12434](https://github.com/nodejs/node/pull/12434)
* [[`fe1be39b28`](https://github.com/nodejs/node/commit/fe1be39b28)] - **doc**: child_process example for special chars (Cody Deckard)
* [[`e72ea0da0b`](https://github.com/nodejs/node/commit/e72ea0da0b)] - **doc**: modernize and fix code examples in process.md (Vse Mozhet Byt) [#12381](https://github.com/nodejs/node/pull/12381)
* [[`c6e0ba31ec`](https://github.com/nodejs/node/commit/c6e0ba31ec)] - **doc**: update OS level support for AIX (Michael Dawson) [#12235](https://github.com/nodejs/node/pull/12235)
* [[`6ebc806a47`](https://github.com/nodejs/node/commit/6ebc806a47)] - **doc**: correct markdown file line lengths (JR McEntee) [#12106](https://github.com/nodejs/node/pull/12106)
* [[`7a5d07c7fb`](https://github.com/nodejs/node/commit/7a5d07c7fb)] - **doc**: change Mac OS X to macOS (JR McEntee) [#12106](https://github.com/nodejs/node/pull/12106)
* [[`c79b081367`](https://github.com/nodejs/node/commit/c79b081367)] - **doc**: fix typo in CHANGELOG_V6.md (Zero King) [#12206](https://github.com/nodejs/node/pull/12206)
* [[`ba0e3ac53d`](https://github.com/nodejs/node/commit/ba0e3ac53d)] - **doc**: minor improvements in BUILDING.md (Sakthipriyan Vairamani (thefourtheye)) [#11963](https://github.com/nodejs/node/pull/11963)
* [[`e40ac30e05`](https://github.com/nodejs/node/commit/e40ac30e05)] - **doc**: document extent of crypto Uint8Array support (Anna Henningsen) [#11982](https://github.com/nodejs/node/pull/11982)
* [[`ef4768754c`](https://github.com/nodejs/node/commit/ef4768754c)] - **doc**: add supported platforms list (Michael Dawson) [#11872](https://github.com/nodejs/node/pull/11872)
* [[`73e2d0bce6`](https://github.com/nodejs/node/commit/73e2d0bce6)] - **doc**: argument types for crypto methods (Amelia Clarke) [#11799](https://github.com/nodejs/node/pull/11799)
* [[`df97727272`](https://github.com/nodejs/node/commit/df97727272)] - **doc**: improve net.md on sockets and connections (Joyee Cheung) [#11700](https://github.com/nodejs/node/pull/11700)
* [[`3b05153cdc`](https://github.com/nodejs/node/commit/3b05153cdc)] - **doc**: various improvements to net.md (Joyee Cheung) [#11636](https://github.com/nodejs/node/pull/11636)
* [[`289e53265a`](https://github.com/nodejs/node/commit/289e53265a)] - **doc**: add missing entry in v6 changelog table (Luigi Pinca) [#11534](https://github.com/nodejs/node/pull/11534)
* [[`5da952472b`](https://github.com/nodejs/node/commit/5da952472b)] - **doc**: document pending semver-major API changes (Anna Henningsen) [#11489](https://github.com/nodejs/node/pull/11489)
* [[`52b253677a`](https://github.com/nodejs/node/commit/52b253677a)] - **doc**: fix sorting in API references (Vse Mozhet Byt) [#11331](https://github.com/nodejs/node/pull/11331)
* [[`ca8c30a35c`](https://github.com/nodejs/node/commit/ca8c30a35c)] - **doc**: update output examples in debugger.md (Vse Mozhet Byt) [#10944](https://github.com/nodejs/node/pull/10944)
* [[`c5a0dcedd3`](https://github.com/nodejs/node/commit/c5a0dcedd3)] - **doc**: fix math error in process.md (Diego Rodríguez Baquero) [#11158](https://github.com/nodejs/node/pull/11158)
* [[`93c4820458`](https://github.com/nodejs/node/commit/93c4820458)] - ***Revert*** "**doc**: correct vcbuild options for windows testing" (Gibson Fahnestock) [#10839](https://github.com/nodejs/node/pull/10839)
* [[`6d31bdb872`](https://github.com/nodejs/node/commit/6d31bdb872)] - **doc**: move Boron releases to LTS column (Anna Henningsen) [#10827](https://github.com/nodejs/node/pull/10827)
* [[`f3f2468bdc`](https://github.com/nodejs/node/commit/f3f2468bdc)] - **doc**: fix CHANGELOG.md table formatting (Сковорода Никита Андреевич) [#10743](https://github.com/nodejs/node/pull/10743)
* [[`65e7f62400`](https://github.com/nodejs/node/commit/65e7f62400)] - **doc**: fix heading type for v4.6.2 changelog (Myles Borins) [#9515](https://github.com/nodejs/node/pull/9515)
* [[`e1cabf6fbd`](https://github.com/nodejs/node/commit/e1cabf6fbd)] - **doc, test**: add note to response.getHeaders (Refael Ackermann) [#12887](https://github.com/nodejs/node/pull/12887)
* [[`42dca99cd7`](https://github.com/nodejs/node/commit/42dca99cd7)] - **doc, tools**: add doc linting to CI (Vse Mozhet Byt) [#12640](https://github.com/nodejs/node/pull/12640)
* [[`81b9b857aa`](https://github.com/nodejs/node/commit/81b9b857aa)] - **doc,build**: update configure help messages (Gibson Fahnestock) [#12978](https://github.com/nodejs/node/pull/12978)
* [[`50af2b95e0`](https://github.com/nodejs/node/commit/50af2b95e0)] - **errors**: AssertionError moved to internal/error (Faiz Halde) [#12906](https://github.com/nodejs/node/pull/12906)
* [[`7b4a72d797`](https://github.com/nodejs/node/commit/7b4a72d797)] - **errors**: add space between error name and code (James M Snell) [#12099](https://github.com/nodejs/node/pull/12099)
* [[`58066d16d5`](https://github.com/nodejs/node/commit/58066d16d5)] - **events**: remove unreachable code (cjihrig) [#12501](https://github.com/nodejs/node/pull/12501)
* [[`ea9eed5643`](https://github.com/nodejs/node/commit/ea9eed5643)] - **freelist**: simplify export (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`d99b7bc8c9`](https://github.com/nodejs/node/commit/d99b7bc8c9)] - **fs**: fix realpath{Sync} on resolving pipes/sockets (Ebrahim Byagowi) [#13028](https://github.com/nodejs/node/pull/13028)
* [[`6f449db60f`](https://github.com/nodejs/node/commit/6f449db60f)] - **fs**: refactor deprecated functions for readability (Rich Trott) [#12910](https://github.com/nodejs/node/pull/12910)
* [[`08809f28ad`](https://github.com/nodejs/node/commit/08809f28ad)] - **fs**: simplify constant decls (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`2264d9d4ba`](https://github.com/nodejs/node/commit/2264d9d4ba)] - **http**: improve outgoing string write performance (Brian White) [#13013](https://github.com/nodejs/node/pull/13013)
* [[`414f93ecb3`](https://github.com/nodejs/node/commit/414f93ecb3)] - **http**: fix IPv6 Host header check (Brian White) [#13122](https://github.com/nodejs/node/pull/13122)
* [[`55c95b1644`](https://github.com/nodejs/node/commit/55c95b1644)] - **http**: fix first body chunk fast case for UTF-16 (Anna Henningsen) [#12747](https://github.com/nodejs/node/pull/12747)
* [[`e283319969`](https://github.com/nodejs/node/commit/e283319969)] - **http**: fix permanent deoptimizations (Brian White) [#12456](https://github.com/nodejs/node/pull/12456)
* [[`e0a9ad1af2`](https://github.com/nodejs/node/commit/e0a9ad1af2)] - **http**: avoid retaining unneeded memory (Luigi Pinca) [#11926](https://github.com/nodejs/node/pull/11926)
* [[`74c1e02642`](https://github.com/nodejs/node/commit/74c1e02642)] - **http**: replace uses of self (James M Snell) [#11594](https://github.com/nodejs/node/pull/11594)
* [[`5425e0dcbe`](https://github.com/nodejs/node/commit/5425e0dcbe)] - **http**: use more efficient module.exports pattern (James M Snell) [#11594](https://github.com/nodejs/node/pull/11594)
* [[`69f3db4571`](https://github.com/nodejs/node/commit/69f3db4571)] - **http,https**: avoid instanceof for WHATWG URL (Brian White) [#12983](https://github.com/nodejs/node/pull/12983)
* [[`9ce2271e81`](https://github.com/nodejs/node/commit/9ce2271e81)] - **https**: support agent construction without new (cjihrig) [#12927](https://github.com/nodejs/node/pull/12927)
* [[`010f864426`](https://github.com/nodejs/node/commit/010f864426)] - **inspector**: --debug* deprecation and invalidation (Refael Ackermann) [#12949](https://github.com/nodejs/node/pull/12949)
* [[`bb77cce7a1`](https://github.com/nodejs/node/commit/bb77cce7a1)] - **inspector**: add missing virtual destructor (Eugene Ostroukhov) [#13198](https://github.com/nodejs/node/pull/13198)
* [[`39785c7780`](https://github.com/nodejs/node/commit/39785c7780)] - **inspector**: document bad usage for --inspect-port (Sam Roberts) [#12581](https://github.com/nodejs/node/pull/12581)
* [[`77d5e6f8da`](https://github.com/nodejs/node/commit/77d5e6f8da)] - **inspector**: fix process._debugEnd() for inspector (Eugene Ostroukhov) [#12777](https://github.com/nodejs/node/pull/12777)
* [[`7c3a23b9c1`](https://github.com/nodejs/node/commit/7c3a23b9c1)] - **inspector**: handle socket close before close frame (Eugene Ostroukhov) [#12937](https://github.com/nodejs/node/pull/12937)
* [[`15e160e626`](https://github.com/nodejs/node/commit/15e160e626)] - **inspector**: report when main context is destroyed (Eugene Ostroukhov) [#12814](https://github.com/nodejs/node/pull/12814)
* [[`3f48ab3042`](https://github.com/nodejs/node/commit/3f48ab3042)] - **inspector**: do not add 'inspector' property (Eugene Ostroukhov) [#12656](https://github.com/nodejs/node/pull/12656)
* [[`439b35aade`](https://github.com/nodejs/node/commit/439b35aade)] - **inspector**: remove AgentImpl (Eugene Ostroukhov) [#12576](https://github.com/nodejs/node/pull/12576)
* [[`42be835e05`](https://github.com/nodejs/node/commit/42be835e05)] - **inspector**: fix Coverity defects (Eugene Ostroukhov) [#12272](https://github.com/nodejs/node/pull/12272)
* [[`7954d2a4c7`](https://github.com/nodejs/node/commit/7954d2a4c7)] - **inspector**: use inspector API for "break on start" (Eugene Ostroukhov) [#12076](https://github.com/nodejs/node/pull/12076)
* [[`b170fb7c55`](https://github.com/nodejs/node/commit/b170fb7c55)] - **inspector**: proper WS URLs when bound to 0.0.0.0 (Eugene Ostroukhov) [#11755](https://github.com/nodejs/node/pull/11755)
* [[`54d331895c`](https://github.com/nodejs/node/commit/54d331895c)] - **lib**: add guard to originalConsole (Daniel Bevenius) [#12881](https://github.com/nodejs/node/pull/12881)
* [[`824fb49a70`](https://github.com/nodejs/node/commit/824fb49a70)] - **lib**: remove useless default caught (Jackson Tian) [#12884](https://github.com/nodejs/node/pull/12884)
* [[`9077b48271`](https://github.com/nodejs/node/commit/9077b48271)] - **lib**: refactor internal/util (James M Snell) [#11404](https://github.com/nodejs/node/pull/11404)
* [[`cfc8422a68`](https://github.com/nodejs/node/commit/cfc8422a68)] - **lib**: use Object.create(null) directly (Timothy Gu) [#11930](https://github.com/nodejs/node/pull/11930)
* [[`4eb194a2b1`](https://github.com/nodejs/node/commit/4eb194a2b1)] - **lib**: Use regex to compare error message (Kunal Pathak) [#11854](https://github.com/nodejs/node/pull/11854)
* [[`989713d343`](https://github.com/nodejs/node/commit/989713d343)] - **lib**: avoid using forEach (James M Snell) [#11582](https://github.com/nodejs/node/pull/11582)
* [[`4d090855c6`](https://github.com/nodejs/node/commit/4d090855c6)] - **lib**: avoid using forEach in LazyTransform (James M Snell) [#11582](https://github.com/nodejs/node/pull/11582)
* [[`3becb0206c`](https://github.com/nodejs/node/commit/3becb0206c)] - **lib,src**: improve writev() performance for Buffers (Brian White) [#13187](https://github.com/nodejs/node/pull/13187)
* [[`6bcf65d4a7`](https://github.com/nodejs/node/commit/6bcf65d4a7)] - **lib,test**: use regular expression literals (Rich Trott) [#12807](https://github.com/nodejs/node/pull/12807)
* [[`dd0624676c`](https://github.com/nodejs/node/commit/dd0624676c)] - **meta**: fix nits in README.md collaborators list (Vse Mozhet Byt) [#12866](https://github.com/nodejs/node/pull/12866)
* [[`98e54b0bd4`](https://github.com/nodejs/node/commit/98e54b0bd4)] - **meta**: restore original copyright header (James M Snell) [#10155](https://github.com/nodejs/node/pull/10155)
* [[`ed0716f0e9`](https://github.com/nodejs/node/commit/ed0716f0e9)] - **module**: refactor internal/module export style (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`f97156623a`](https://github.com/nodejs/node/commit/f97156623a)] - **module**: standardize strip shebang behaviour (Sebastian Van Sande) [#12202](https://github.com/nodejs/node/pull/12202)
* [[`a63b245b0a`](https://github.com/nodejs/node/commit/a63b245b0a)] - **n-api**: Retain last code when getting error info (Jason Ginchereau) [#13087](https://github.com/nodejs/node/pull/13087)
* [[`008301167e`](https://github.com/nodejs/node/commit/008301167e)] - **n-api**: remove compiler warning (Anna Henningsen) [#13014](https://github.com/nodejs/node/pull/13014)
* [[`2e3fef7628`](https://github.com/nodejs/node/commit/2e3fef7628)] - **n-api**: Handle fatal exception in async callback (Jason Ginchereau) [#12838](https://github.com/nodejs/node/pull/12838)
* [[`2bbabb1f85`](https://github.com/nodejs/node/commit/2bbabb1f85)] - **n-api**: napi_get_cb_info should fill array (Jason Ginchereau) [#12863](https://github.com/nodejs/node/pull/12863)
* [[`cd32b77567`](https://github.com/nodejs/node/commit/cd32b77567)] - **n-api**: remove unnecessary try-catch bracket from certain APIs (Gabriel Schulhof) [#12705](https://github.com/nodejs/node/pull/12705)
* [[`972bfe1514`](https://github.com/nodejs/node/commit/972bfe1514)] - **n-api**: Sync with back-compat changes (Jason Ginchereau) [#12674](https://github.com/nodejs/node/pull/12674)
* [[`427125491f`](https://github.com/nodejs/node/commit/427125491f)] - **n-api**: Reference and external tests (Jason Ginchereau) [#12551](https://github.com/nodejs/node/pull/12551)
* [[`b7a341d7e5`](https://github.com/nodejs/node/commit/b7a341d7e5)] - **n-api**: Enable scope and ref APIs during exception (Jason Ginchereau) [#12524](https://github.com/nodejs/node/pull/12524)
* [[`ba7bac5c37`](https://github.com/nodejs/node/commit/ba7bac5c37)] - **n-api**: tighten null-checking and clean up last error (Gabriel Schulhof) [#12539](https://github.com/nodejs/node/pull/12539)
* [[`468275ac79`](https://github.com/nodejs/node/commit/468275ac79)] - **n-api**: remove napi_get_value_string_length() (Jason Ginchereau) [#12496](https://github.com/nodejs/node/pull/12496)
* [[`46f202690b`](https://github.com/nodejs/node/commit/46f202690b)] - **n-api**: fix coverity scan report (Michael Dawson) [#12365](https://github.com/nodejs/node/pull/12365)
* [[`ad5f987558`](https://github.com/nodejs/node/commit/ad5f987558)] - **n-api**: add string api for latin1 encoding (Sampson Gao) [#12368](https://github.com/nodejs/node/pull/12368)
* [[`affe0f2d2a`](https://github.com/nodejs/node/commit/affe0f2d2a)] - **n-api**: fix -Wmismatched-tags compiler warning (Ben Noordhuis) [#12333](https://github.com/nodejs/node/pull/12333)
* [[`9decfb1521`](https://github.com/nodejs/node/commit/9decfb1521)] - **n-api**: implement async helper methods (taylor.woll) [#12250](https://github.com/nodejs/node/pull/12250)
* [[`ca786c3734`](https://github.com/nodejs/node/commit/ca786c3734)] - **n-api**: change napi_callback to return napi_value (Taylor Woll) [#12248](https://github.com/nodejs/node/pull/12248)
* [[`8fbace163a`](https://github.com/nodejs/node/commit/8fbace163a)] - **n-api**: cache Symbol.hasInstance (Gabriel Schulhof) [#12246](https://github.com/nodejs/node/pull/12246)
* [[`84602845c6`](https://github.com/nodejs/node/commit/84602845c6)] - **n-api**: Update property attrs enum to match JS spec (Jason Ginchereau) [#12240](https://github.com/nodejs/node/pull/12240)
* [[`0a5bf4aee3`](https://github.com/nodejs/node/commit/0a5bf4aee3)] - **n-api**: create napi_env as a real structure (Gabriel Schulhof) [#12195](https://github.com/nodejs/node/pull/12195)
* [[`4a21e398d6`](https://github.com/nodejs/node/commit/4a21e398d6)] - **n-api**: break dep between v8 and napi attributes (Michael Dawson) [#12191](https://github.com/nodejs/node/pull/12191)
* [[`afd5966fa9`](https://github.com/nodejs/node/commit/afd5966fa9)] - **napi**: initialize and check status properly (Gabriel Schulhof) [#12283](https://github.com/nodejs/node/pull/12283)
* [[`491d59da84`](https://github.com/nodejs/node/commit/491d59da84)] - **napi**: supress invalid coverity leak message (Michael Dawson) [#12192](https://github.com/nodejs/node/pull/12192)
* [[`4fabcfc5a2`](https://github.com/nodejs/node/commit/4fabcfc5a2)] - ***Revert*** "**net**: remove unnecessary process.nextTick()" (Evan Lucas) [#12854](https://github.com/nodejs/node/pull/12854)
* [[`51664fc265`](https://github.com/nodejs/node/commit/51664fc265)] - **net**: add symbol to normalized connect() args (cjihrig) [#13069](https://github.com/nodejs/node/pull/13069)
* [[`212a7a609d`](https://github.com/nodejs/node/commit/212a7a609d)] - **net**: ensure net.connect calls Socket connect (Thomas Watson) [#12861](https://github.com/nodejs/node/pull/12861)
* [[`879d6663ea`](https://github.com/nodejs/node/commit/879d6663ea)] - **net**: remove an unused internal module `assertPort` (Daijiro Wachi) [#11812](https://github.com/nodejs/node/pull/11812)
* [[`896be833c8`](https://github.com/nodejs/node/commit/896be833c8)] - **node**: add missing option to --help output (Ruslan Bekenev) [#12763](https://github.com/nodejs/node/pull/12763)
* [[`579ff2a487`](https://github.com/nodejs/node/commit/579ff2a487)] - **process**: refactor internal/process.js export style (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`776028c46b`](https://github.com/nodejs/node/commit/776028c46b)] - **querystring**: improve unescapeBuffer() performance (Jesus Seijas) [#12525](https://github.com/nodejs/node/pull/12525)
* [[`c98a8022b7`](https://github.com/nodejs/node/commit/c98a8022b7)] - **querystring**: move isHexTable to internal (Timothy Gu) [#11858](https://github.com/nodejs/node/pull/11858)
* [[`ff785fd517`](https://github.com/nodejs/node/commit/ff785fd517)] - **querystring**: fix empty pairs and optimize parse() (Brian White) [#11234](https://github.com/nodejs/node/pull/11234)
* [[`4c070d4897`](https://github.com/nodejs/node/commit/4c070d4897)] - **readline**: move escape codes into internal/readline (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`4ac7a68ccd`](https://github.com/nodejs/node/commit/4ac7a68ccd)] - **readline**: multiple code cleanups (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`392a8987c6`](https://github.com/nodejs/node/commit/392a8987c6)] - **readline**: use module.exports = {} on internal/readline (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`9318f82937`](https://github.com/nodejs/node/commit/9318f82937)] - **readline**: use module.exports = {} (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`c20e87a10e`](https://github.com/nodejs/node/commit/c20e87a10e)] - **repl**: fix /dev/null history file regression (Brian White) [#12762](https://github.com/nodejs/node/pull/12762)
* [[`b45abfda5f`](https://github.com/nodejs/node/commit/b45abfda5f)] - **repl**: fix permanent deoptimizations (Brian White) [#12456](https://github.com/nodejs/node/pull/12456)
* [[`c7b60165a6`](https://github.com/nodejs/node/commit/c7b60165a6)] - **repl**: Empty command should be sent to eval function (Jan Krems) [#11871](https://github.com/nodejs/node/pull/11871)
* [[`ac2e8820c4`](https://github.com/nodejs/node/commit/ac2e8820c4)] - **src**: reduce duplicate code in SafeGetenv() (cjihrig) [#13220](https://github.com/nodejs/node/pull/13220)
* [[`ec7cbaf266`](https://github.com/nodejs/node/commit/ec7cbaf266)] - **src**: update NODE_MODULE_VERSION to 57 (Michaël Zasso) [#12995](https://github.com/nodejs/node/pull/12995)
* [[`9d922c6c0e`](https://github.com/nodejs/node/commit/9d922c6c0e)] - **src**: fix InspectorStarted macro guard (Daniel Bevenius) [#13167](https://github.com/nodejs/node/pull/13167)
* [[`e7d098cea6`](https://github.com/nodejs/node/commit/e7d098cea6)] - **src**: ignore unused warning for inspector-agent.cc (Daniel Bevenius) [#13188](https://github.com/nodejs/node/pull/13188)
* [[`145ab052df`](https://github.com/nodejs/node/commit/145ab052df)] - **src**: add comment for TicketKeyCallback (Anna Henningsen) [#13193](https://github.com/nodejs/node/pull/13193)
* [[`b4f6ea06eb`](https://github.com/nodejs/node/commit/b4f6ea06eb)] - **src**: make StreamBase::GetAsyncWrap pure virtual (Anna Henningsen) [#13174](https://github.com/nodejs/node/pull/13174)
* [[`4fa2ee16bb`](https://github.com/nodejs/node/commit/4fa2ee16bb)] - **src**: add linux getauxval(AT_SECURE) in SafeGetenv (Daniel Bevenius) [#12548](https://github.com/nodejs/node/pull/12548)
* [[`287b11dc8c`](https://github.com/nodejs/node/commit/287b11dc8c)] - **src**: allow --tls-cipher-list in NODE_OPTIONS (Sam Roberts) [#13172](https://github.com/nodejs/node/pull/13172)
* [[`3ccef8e267`](https://github.com/nodejs/node/commit/3ccef8e267)] - **src**: correct endif comment SRC_NODE_API_H__ (Daniel Bevenius) [#13190](https://github.com/nodejs/node/pull/13190)
* [[`4cbdac3183`](https://github.com/nodejs/node/commit/4cbdac3183)] - **src**: redirect-warnings to file, not path (Sam Roberts) [#13120](https://github.com/nodejs/node/pull/13120)
* [[`85e2d56df1`](https://github.com/nodejs/node/commit/85e2d56df1)] - **src**: fix typo (Brian White) [#13085](https://github.com/nodejs/node/pull/13085)
* [[`1263b70e9e`](https://github.com/nodejs/node/commit/1263b70e9e)] - **src**: remove unused parameters (Brian White) [#13085](https://github.com/nodejs/node/pull/13085)
* [[`1acd4d2cc4`](https://github.com/nodejs/node/commit/1acd4d2cc4)] - **src**: assert that uv_async_init() succeeds (cjihrig) [#13116](https://github.com/nodejs/node/pull/13116)
* [[`f81281737c`](https://github.com/nodejs/node/commit/f81281737c)] - **src**: remove unnecessary forward declaration (Daniel Bevenius) [#13081](https://github.com/nodejs/node/pull/13081)
* [[`60132e83c3`](https://github.com/nodejs/node/commit/60132e83c3)] - **src**: check IsConstructCall in TLSWrap constructor (Daniel Bevenius) [#13097](https://github.com/nodejs/node/pull/13097)
* [[`57b9b9d7d6`](https://github.com/nodejs/node/commit/57b9b9d7d6)] - **src**: remove unnecessary return statement (Daniel Bevenius) [#13094](https://github.com/nodejs/node/pull/13094)
* [[`94eca79d5d`](https://github.com/nodejs/node/commit/94eca79d5d)] - **src**: remove unused node_buffer.h include (Daniel Bevenius) [#13095](https://github.com/nodejs/node/pull/13095)
* [[`46e773c5db`](https://github.com/nodejs/node/commit/46e773c5db)] - **src**: check if --icu-data-dir= points to valid dir (Ben Noordhuis)
* [[`29d89c9855`](https://github.com/nodejs/node/commit/29d89c9855)] - **src**: split CryptoPemCallback into two functions (Daniel Bevenius) [#12827](https://github.com/nodejs/node/pull/12827)
* [[`d6cd466a25`](https://github.com/nodejs/node/commit/d6cd466a25)] - **src**: whitelist new options for NODE_OPTIONS (Sam Roberts) [#13002](https://github.com/nodejs/node/pull/13002)
* [[`53dae83833`](https://github.com/nodejs/node/commit/53dae83833)] - **src**: fix --abort_on_uncaught_exception arg parsing (Sam Roberts) [#13004](https://github.com/nodejs/node/pull/13004)
* [[`fefab9026b`](https://github.com/nodejs/node/commit/fefab9026b)] - **src**: only call FatalException if not verbose (Daniel Bevenius) [#12826](https://github.com/nodejs/node/pull/12826)
* [[`32f01c8c11`](https://github.com/nodejs/node/commit/32f01c8c11)] - **src**: remove unused uv.h include in async-wrap.h (Daniel Bevenius) [#12973](https://github.com/nodejs/node/pull/12973)
* [[`60f0dc7d42`](https://github.com/nodejs/node/commit/60f0dc7d42)] - **src**: rename CONNECTION provider to SSLCONNECTION (Daniel Bevenius) [#12989](https://github.com/nodejs/node/pull/12989)
* [[`15410797f2`](https://github.com/nodejs/node/commit/15410797f2)] - **src**: add HAVE_OPENSSL guard to crypto providers (Daniel Bevenius) [#12967](https://github.com/nodejs/node/pull/12967)
* [[`9f8e030f1b`](https://github.com/nodejs/node/commit/9f8e030f1b)] - **src**: add/move hasCrypto checks for async tests (Daniel Bevenius) [#12968](https://github.com/nodejs/node/pull/12968)
* [[`b6001a2da5`](https://github.com/nodejs/node/commit/b6001a2da5)] - **src**: make SIGPROF message a real warning (cjihrig) [#12709](https://github.com/nodejs/node/pull/12709)
* [[`dd6e3f69a7`](https://github.com/nodejs/node/commit/dd6e3f69a7)] - **src**: fix comments re PER_ISOLATE macros (Josh Gavant) [#12899](https://github.com/nodejs/node/pull/12899)
* [[`6ade7f3601`](https://github.com/nodejs/node/commit/6ade7f3601)] - **src**: update --inspect hint text (Josh Gavant) [#11207](https://github.com/nodejs/node/pull/11207)
* [[`d0c968ea57`](https://github.com/nodejs/node/commit/d0c968ea57)] - **src**: make root_cert_vector function scoped (Daniel Bevenius) [#12788](https://github.com/nodejs/node/pull/12788)
* [[`ebcd8c6bb8`](https://github.com/nodejs/node/commit/ebcd8c6bb8)] - **src**: rename CryptoPemCallback -\> PasswordCallback (Daniel Bevenius) [#12787](https://github.com/nodejs/node/pull/12787)
* [[`d56a7e640f`](https://github.com/nodejs/node/commit/d56a7e640f)] - **src**: do proper StringBytes error handling (Anna Henningsen) [#12765](https://github.com/nodejs/node/pull/12765)
* [[`9990be2919`](https://github.com/nodejs/node/commit/9990be2919)] - **src**: turn buffer type-CHECK into exception (Anna Henningsen) [#12753](https://github.com/nodejs/node/pull/12753)
* [[`21653b6901`](https://github.com/nodejs/node/commit/21653b6901)] - **src**: add --napi-modules to whitelist (Michael Dawson) [#12733](https://github.com/nodejs/node/pull/12733)
* [[`0f58d3cbef`](https://github.com/nodejs/node/commit/0f58d3cbef)] - **src**: support domains with empty labels (Daijiro Wachi) [#12707](https://github.com/nodejs/node/pull/12707)
* [[`719247ff95`](https://github.com/nodejs/node/commit/719247ff95)] - **src**: remove debugger dead code (Michaël Zasso) [#12621](https://github.com/nodejs/node/pull/12621)
* [[`892ce06dbd`](https://github.com/nodejs/node/commit/892ce06dbd)] - **src**: fix incorrect macro comment (Daniel Bevenius) [#12688](https://github.com/nodejs/node/pull/12688)
* [[`5bb06e8596`](https://github.com/nodejs/node/commit/5bb06e8596)] - **src**: remove GTEST_DONT_DEFINE_ASSERT_EQ in util.h (Daniel Bevenius) [#12638](https://github.com/nodejs/node/pull/12638)
* [[`f2282bb812`](https://github.com/nodejs/node/commit/f2282bb812)] - **src**: allow CLI args in env with NODE_OPTIONS (Sam Roberts) [#12028](https://github.com/nodejs/node/pull/12028)
* [[`6a1275dfec`](https://github.com/nodejs/node/commit/6a1275dfec)] - **src**: add missing "http_parser.h" include (Anna Henningsen) [#12464](https://github.com/nodejs/node/pull/12464)
* [[`5ef6000afd`](https://github.com/nodejs/node/commit/5ef6000afd)] - **src**: don't call uv_run() after 'exit' event (Ben Noordhuis) [#12344](https://github.com/nodejs/node/pull/12344)
* [[`ade80eeb1a`](https://github.com/nodejs/node/commit/ade80eeb1a)] - **src**: clean up WHATWG WG parser (Timothy Gu) [#12251](https://github.com/nodejs/node/pull/12251)
* [[`b2803637e8`](https://github.com/nodejs/node/commit/b2803637e8)] - **src**: replace IsConstructCall functions with lambda (Daniel Bevenius) [#12384](https://github.com/nodejs/node/pull/12384)
* [[`9d522225e7`](https://github.com/nodejs/node/commit/9d522225e7)] - **src**: reduce number of exported symbols (Anna Henningsen) [#12366](https://github.com/nodejs/node/pull/12366)
* [[`a4125a3c49`](https://github.com/nodejs/node/commit/a4125a3c49)] - **src**: remove experimental warning for inspect (Josh Gavant) [#12352](https://github.com/nodejs/node/pull/12352)
* [[`8086cb68ae`](https://github.com/nodejs/node/commit/8086cb68ae)] - **src**: use option parser for expose_internals (Sam Roberts) [#12245](https://github.com/nodejs/node/pull/12245)
* [[`e505c079e0`](https://github.com/nodejs/node/commit/e505c079e0)] - **src**: supply missing comments for CLI options (Sam Roberts) [#12245](https://github.com/nodejs/node/pull/12245)
* [[`de168b4b4a`](https://github.com/nodejs/node/commit/de168b4b4a)] - **src**: guard bundled_ca/openssl_ca with HAVE_OPENSSL (Daniel Bevenius) [#12302](https://github.com/nodejs/node/pull/12302)
* [[`cecdf7c118`](https://github.com/nodejs/node/commit/cecdf7c118)] - **src**: use a std::vector for preload_modules (Sam Roberts) [#12241](https://github.com/nodejs/node/pull/12241)
* [[`65a6e05da5`](https://github.com/nodejs/node/commit/65a6e05da5)] - **src**: only block SIGUSR1 when HAVE_INSPECTOR (Daniel Bevenius) [#12266](https://github.com/nodejs/node/pull/12266)
* [[`ebeee853e6`](https://github.com/nodejs/node/commit/ebeee853e6)] - **src**: Update trace event macros to V8 5.7 version (Matt Loring) [#12127](https://github.com/nodejs/node/pull/12127)
* [[`7c0079f1be`](https://github.com/nodejs/node/commit/7c0079f1be)] - **src**: add .FromJust(), fix -Wunused-result warnings (Ben Noordhuis) [#12118](https://github.com/nodejs/node/pull/12118)
* [[`4ddd23f0ec`](https://github.com/nodejs/node/commit/4ddd23f0ec)] - **src**: WHATWG URL C++ parser cleanup (Timothy Gu) [#11917](https://github.com/nodejs/node/pull/11917)
* [[`d099f8e317`](https://github.com/nodejs/node/commit/d099f8e317)] - **src**: remove explicit UTF-8 validity check in url (Timothy Gu) [#11859](https://github.com/nodejs/node/pull/11859)
* [[`e2f151f5b2`](https://github.com/nodejs/node/commit/e2f151f5b2)] - **src**: make process.env work with symbols in/delete (Timothy Gu) [#11709](https://github.com/nodejs/node/pull/11709)
* [[`e1d8899c28`](https://github.com/nodejs/node/commit/e1d8899c28)] - **src**: add HAVE_OPENSSL directive to openssl_config (Daniel Bevenius) [#11618](https://github.com/nodejs/node/pull/11618)
* [[`a7f7724167`](https://github.com/nodejs/node/commit/a7f7724167)] - **src**: remove misleading flag in TwoByteValue (Timothy Gu) [#11436](https://github.com/nodejs/node/pull/11436)
* [[`046f66a554`](https://github.com/nodejs/node/commit/046f66a554)] - **src**: fix building --without-v8-plartform (Myk Melez) [#11088](https://github.com/nodejs/node/pull/11088)
* [[`d317184f97`](https://github.com/nodejs/node/commit/d317184f97)] - **src**: bump version to v8.0.0 for master (Rod Vagg) [#8956](https://github.com/nodejs/node/pull/8956)
* [[`f077e51c92`](https://github.com/nodejs/node/commit/f077e51c92)] - **src,fs**: calculate fs times without truncation (Daniel Pihlstrom) [#12607](https://github.com/nodejs/node/pull/12607)
* [[`b8b6c2c262`](https://github.com/nodejs/node/commit/b8b6c2c262)] - **stream**: emit finish when using writev and cork (Matteo Collina) [#13195](https://github.com/nodejs/node/pull/13195)
* [[`c15fe8b78e`](https://github.com/nodejs/node/commit/c15fe8b78e)] - **stream**: remove dup property (Calvin Metcalf) [#13216](https://github.com/nodejs/node/pull/13216)
* [[`87cef63ccb`](https://github.com/nodejs/node/commit/87cef63ccb)] - **stream**: fix destroy(err, cb) regression (Matteo Collina) [#13156](https://github.com/nodejs/node/pull/13156)
* [[`8914f7b4b7`](https://github.com/nodejs/node/commit/8914f7b4b7)] - **stream**: improve readable push performance (Brian White) [#13113](https://github.com/nodejs/node/pull/13113)
* [[`6993eb0897`](https://github.com/nodejs/node/commit/6993eb0897)] - **stream**: fix y.pipe(x)+y.pipe(x)+y.unpipe(x) (Anna Henningsen) [#12746](https://github.com/nodejs/node/pull/12746)
* [[`d6a6bcdc47`](https://github.com/nodejs/node/commit/d6a6bcdc47)] - **stream**: remove unnecessary parameter (Leo) [#12767](https://github.com/nodejs/node/pull/12767)
* [[`e2199e0fc2`](https://github.com/nodejs/node/commit/e2199e0fc2)] - **streams**: refactor BufferList into ES6 class (James M Snell) [#12644](https://github.com/nodejs/node/pull/12644)
* [[`ea6941f703`](https://github.com/nodejs/node/commit/ea6941f703)] - **test**: refactor test-fs-assert-encoding-error (Rich Trott) [#13226](https://github.com/nodejs/node/pull/13226)
* [[`8d193919fb`](https://github.com/nodejs/node/commit/8d193919fb)] - **test**: replace `indexOf` with `includes` (Aditya Anand) [#13215](https://github.com/nodejs/node/pull/13215)
* [[`2c5c2bda61`](https://github.com/nodejs/node/commit/2c5c2bda61)] - **test**: check noop invocation with mustNotCall() (Rich Trott) [#13205](https://github.com/nodejs/node/pull/13205)
* [[`d0dbd53eb0`](https://github.com/nodejs/node/commit/d0dbd53eb0)] - **test**: add coverage for socket write after close (cjihrig) [#13171](https://github.com/nodejs/node/pull/13171)
* [[`686e753b7e`](https://github.com/nodejs/node/commit/686e753b7e)] - **test**: use common.mustNotCall in test-crypto-random (Rich Trott) [#13183](https://github.com/nodejs/node/pull/13183)
* [[`4030aed8ce`](https://github.com/nodejs/node/commit/4030aed8ce)] - **test**: skip test-bindings if inspector is disabled (Daniel Bevenius) [#13186](https://github.com/nodejs/node/pull/13186)
* [[`a590709909`](https://github.com/nodejs/node/commit/a590709909)] - **test**: add coverage for napi_has_named_property (Michael Dawson) [#13178](https://github.com/nodejs/node/pull/13178)
* [[`72a319e417`](https://github.com/nodejs/node/commit/72a319e417)] - **test**: refactor event-emitter-remove-all-listeners (Rich Trott) [#13165](https://github.com/nodejs/node/pull/13165)
* [[`c4502728fb`](https://github.com/nodejs/node/commit/c4502728fb)] - **test**: refactor event-emitter-check-listener-leaks (Rich Trott) [#13164](https://github.com/nodejs/node/pull/13164)
* [[`597aff0846`](https://github.com/nodejs/node/commit/597aff0846)] - **test**: cover dgram handle send failures (cjihrig) [#13158](https://github.com/nodejs/node/pull/13158)
* [[`5ad4170cd9`](https://github.com/nodejs/node/commit/5ad4170cd9)] - **test**: cover util.format() format placeholders (cjihrig) [#13159](https://github.com/nodejs/node/pull/13159)
* [[`b781fa7b06`](https://github.com/nodejs/node/commit/b781fa7b06)] - **test**: add override to ServerDone function (Daniel Bevenius) [#13166](https://github.com/nodejs/node/pull/13166)
* [[`a985ed66c4`](https://github.com/nodejs/node/commit/a985ed66c4)] - **test**: refactor test-dns (Rich Trott) [#13163](https://github.com/nodejs/node/pull/13163)
* [[`7fe5303983`](https://github.com/nodejs/node/commit/7fe5303983)] - **test**: fix disabled test-fs-largefile (Rich Trott) [#13147](https://github.com/nodejs/node/pull/13147)
* [[`e012f5a412`](https://github.com/nodejs/node/commit/e012f5a412)] - **test**: move stream2 test from pummel to parallel (Rich Trott) [#13146](https://github.com/nodejs/node/pull/13146)
* [[`9100cac146`](https://github.com/nodejs/node/commit/9100cac146)] - **test**: simplify assert usage in test-stream2-basic (Rich Trott) [#13146](https://github.com/nodejs/node/pull/13146)
* [[`cd70a520d2`](https://github.com/nodejs/node/commit/cd70a520d2)] - **test**: check noop function invocations (Rich Trott) [#13146](https://github.com/nodejs/node/pull/13146)
* [[`110a3b2657`](https://github.com/nodejs/node/commit/110a3b2657)] - **test**: confirm callback is invoked in fs test (Rich Trott) [#13132](https://github.com/nodejs/node/pull/13132)
* [[`1da674e2c0`](https://github.com/nodejs/node/commit/1da674e2c0)] - **test**: check number of message events (Rich Trott) [#13125](https://github.com/nodejs/node/pull/13125)
* [[`4ccfd7cf15`](https://github.com/nodejs/node/commit/4ccfd7cf15)] - **test**: increase n-api constructor coverage (Michael Dawson) [#13124](https://github.com/nodejs/node/pull/13124)
* [[`6cfb876d54`](https://github.com/nodejs/node/commit/6cfb876d54)] - **test**: add regression test for immediate socket errors (Evan Lucas) [#12854](https://github.com/nodejs/node/pull/12854)
* [[`268a39ac2a`](https://github.com/nodejs/node/commit/268a39ac2a)] - **test**: add hasCrypto check to async-wrap-GH13045 (Daniel Bevenius) [#13141](https://github.com/nodejs/node/pull/13141)
* [[`e6c03c78f7`](https://github.com/nodejs/node/commit/e6c03c78f7)] - **test**: fix sequential test-net-connect-local-error (Sebastian Plesciuc) [#13064](https://github.com/nodejs/node/pull/13064)
* [[`511ee24310`](https://github.com/nodejs/node/commit/511ee24310)] - **test**: remove common.PORT from dgram test (Artur Vieira) [#12944](https://github.com/nodejs/node/pull/12944)
* [[`8a4f3b7dfc`](https://github.com/nodejs/node/commit/8a4f3b7dfc)] - **test**: bind to 0 in dgram-send-callback-buffer-length (Artur Vieira) [#12943](https://github.com/nodejs/node/pull/12943)
* [[`9fc47de8e6`](https://github.com/nodejs/node/commit/9fc47de8e6)] - **test**: use dynamic port in test-dgram-send-address-types (Artur Vieira) [#13007](https://github.com/nodejs/node/pull/13007)
* [[`8ef4fe0af2`](https://github.com/nodejs/node/commit/8ef4fe0af2)] - **test**: use dynamic port in test-dgram-send-callback-buffer (Artur Vieira) [#12942](https://github.com/nodejs/node/pull/12942)
* [[`96925e1b93`](https://github.com/nodejs/node/commit/96925e1b93)] - **test**: replace common.PORT in dgram test (Artur Vieira) [#12929](https://github.com/nodejs/node/pull/12929)
* [[`1af8b70c57`](https://github.com/nodejs/node/commit/1af8b70c57)] - **test**: allow for absent nobody user in setuid test (Rich Trott) [#13112](https://github.com/nodejs/node/pull/13112)
* [[`e29477ab25`](https://github.com/nodejs/node/commit/e29477ab25)] - **test**: shorten test-benchmark-http (Rich Trott) [#13109](https://github.com/nodejs/node/pull/13109)
* [[`595e5e3b23`](https://github.com/nodejs/node/commit/595e5e3b23)] - **test**: port disabled readline test (Rich Trott) [#13091](https://github.com/nodejs/node/pull/13091)
* [[`c60a7fa738`](https://github.com/nodejs/node/commit/c60a7fa738)] - **test**: move net reconnect error test to sequential (Artur G Vieira) [#13033](https://github.com/nodejs/node/pull/13033)
* [[`525497596a`](https://github.com/nodejs/node/commit/525497596a)] - **test**: refactor test-net-GH-5504 (Rich Trott) [#13025](https://github.com/nodejs/node/pull/13025)
* [[`658741b9d9`](https://github.com/nodejs/node/commit/658741b9d9)] - **test**: refactor test-https-set-timeout-server (Rich Trott) [#13032](https://github.com/nodejs/node/pull/13032)
* [[`fccc0bf6e6`](https://github.com/nodejs/node/commit/fccc0bf6e6)] - **test**: add mustCallAtLeast (Refael Ackermann) [#12935](https://github.com/nodejs/node/pull/12935)
* [[`6f216710eb`](https://github.com/nodejs/node/commit/6f216710eb)] - **test**: ignore spurious 'EMFILE' (Refael Ackermann) [#12698](https://github.com/nodejs/node/pull/12698)
* [[`6b1819cff5`](https://github.com/nodejs/node/commit/6b1819cff5)] - **test**: use dynamic port in test-cluster-dgram-reuse (Artur Vieira) [#12901](https://github.com/nodejs/node/pull/12901)
* [[`a593c74f81`](https://github.com/nodejs/node/commit/a593c74f81)] - **test**: refactor test-vm-new-script-new-context (Akshay Iyer) [#13035](https://github.com/nodejs/node/pull/13035)
* [[`7e5ed8bad9`](https://github.com/nodejs/node/commit/7e5ed8bad9)] - **test**: track callback invocations (Rich Trott) [#13010](https://github.com/nodejs/node/pull/13010)
* [[`47e3d00241`](https://github.com/nodejs/node/commit/47e3d00241)] - **test**: refactor test-dns-regress-6244.js (Rich Trott) [#13058](https://github.com/nodejs/node/pull/13058)
* [[`6933419cb9`](https://github.com/nodejs/node/commit/6933419cb9)] - **test**: add hasCrypto to tls-lookup (Daniel Bevenius) [#13047](https://github.com/nodejs/node/pull/13047)
* [[`0dd8b9a965`](https://github.com/nodejs/node/commit/0dd8b9a965)] - **test**: Improve N-API test coverage (Michael Dawson) [#13044](https://github.com/nodejs/node/pull/13044)
* [[`5debcceafc`](https://github.com/nodejs/node/commit/5debcceafc)] - **test**: add hasCrypto to tls-wrap-event-emmiter (Daniel Bevenius) [#13041](https://github.com/nodejs/node/pull/13041)
* [[`7906ed50fa`](https://github.com/nodejs/node/commit/7906ed50fa)] - **test**: add regex check in test-url-parse-invalid-input (Andrei Cioromila) [#12879](https://github.com/nodejs/node/pull/12879)
* [[`0c2edd27e6`](https://github.com/nodejs/node/commit/0c2edd27e6)] - **test**: fixed flaky test-net-connect-local-error (Sebastian Plesciuc) [#12964](https://github.com/nodejs/node/pull/12964)
* [[`47c3c58704`](https://github.com/nodejs/node/commit/47c3c58704)] - **test**: improve N-API test coverage (Michael Dawson) [#13006](https://github.com/nodejs/node/pull/13006)
* [[`88d2e699d8`](https://github.com/nodejs/node/commit/88d2e699d8)] - **test**: remove unneeded string splitting (Vse Mozhet Byt) [#12992](https://github.com/nodejs/node/pull/12992)
* [[`72e3dda93c`](https://github.com/nodejs/node/commit/72e3dda93c)] - **test**: use mustCall in tls-connect-given-socket (vperezma) [#12592](https://github.com/nodejs/node/pull/12592)
* [[`b7bc09fd60`](https://github.com/nodejs/node/commit/b7bc09fd60)] - **test**: add not-called check to heap-profiler test (Rich Trott) [#12985](https://github.com/nodejs/node/pull/12985)
* [[`b5ae22dd1c`](https://github.com/nodejs/node/commit/b5ae22dd1c)] - **test**: add hasCrypto check to https-agent-constructor (Daniel Bevenius) [#12987](https://github.com/nodejs/node/pull/12987)
* [[`945f208081`](https://github.com/nodejs/node/commit/945f208081)] - **test**: make the rest of tests path-independent (Vse Mozhet Byt) [#12972](https://github.com/nodejs/node/pull/12972)
* [[`9516aa19c1`](https://github.com/nodejs/node/commit/9516aa19c1)] - **test**: add common.mustCall() to NAPI exception test (Rich Trott) [#12959](https://github.com/nodejs/node/pull/12959)
* [[`84fc069b95`](https://github.com/nodejs/node/commit/84fc069b95)] - **test**: move test-dgram-bind-shared-ports to sequential (Rafael Fragoso) [#12452](https://github.com/nodejs/node/pull/12452)
* [[`642bd4dd6d`](https://github.com/nodejs/node/commit/642bd4dd6d)] - **test**: add a simple abort check in windows (Sreepurna Jasti) [#12914](https://github.com/nodejs/node/pull/12914)
* [[`56812c81a3`](https://github.com/nodejs/node/commit/56812c81a3)] - **test**: use dynamic port in test-https-connect-address-family (Artur G Vieira) [#12915](https://github.com/nodejs/node/pull/12915)
* [[`529e4f206a`](https://github.com/nodejs/node/commit/529e4f206a)] - **test**: make a test path-independent (Vse Mozhet Byt) [#12945](https://github.com/nodejs/node/pull/12945)
* [[`631cb42b4e`](https://github.com/nodejs/node/commit/631cb42b4e)] - **test**: favor deepStrictEqual over deepEqual (Rich Trott) [#12883](https://github.com/nodejs/node/pull/12883)
* [[`654afa2c19`](https://github.com/nodejs/node/commit/654afa2c19)] - **test**: improve n-api array func coverage (Michael Dawson) [#12890](https://github.com/nodejs/node/pull/12890)
* [[`bee250c232`](https://github.com/nodejs/node/commit/bee250c232)] - **test**: dynamic port in cluster disconnect (Sebastian Plesciuc) [#12545](https://github.com/nodejs/node/pull/12545)
* [[`6914aeaefd`](https://github.com/nodejs/node/commit/6914aeaefd)] - **test**: detect all types of aborts in windows (Gireesh Punathil) [#12856](https://github.com/nodejs/node/pull/12856)
* [[`cfe7b34058`](https://github.com/nodejs/node/commit/cfe7b34058)] - **test**: use assert regexp in tls no cert test (Artur Vieira) [#12891](https://github.com/nodejs/node/pull/12891)
* [[`317180ffe5`](https://github.com/nodejs/node/commit/317180ffe5)] - **test**: fix flaky test-https-client-get-url (Sebastian Plesciuc) [#12876](https://github.com/nodejs/node/pull/12876)
* [[`57a08e2f70`](https://github.com/nodejs/node/commit/57a08e2f70)] - **test**: remove obsolete lint config comments (Rich Trott) [#12868](https://github.com/nodejs/node/pull/12868)
* [[`94eed0fb11`](https://github.com/nodejs/node/commit/94eed0fb11)] - **test**: use dynamic port instead of common.PORT (Aditya Anand) [#12473](https://github.com/nodejs/node/pull/12473)
* [[`f72376d323`](https://github.com/nodejs/node/commit/f72376d323)] - **test**: add skipIfInspectorDisabled to debugger-pid (Daniel Bevenius) [#12882](https://github.com/nodejs/node/pull/12882)
* [[`771568a5a5`](https://github.com/nodejs/node/commit/771568a5a5)] - **test**: add test for timers benchmarks (Joyee Cheung) [#12851](https://github.com/nodejs/node/pull/12851)
* [[`dc4313c620`](https://github.com/nodejs/node/commit/dc4313c620)] - **test**: remove unused testpy code (Rich Trott) [#12844](https://github.com/nodejs/node/pull/12844)
* [[`0a734fec88`](https://github.com/nodejs/node/commit/0a734fec88)] - **test**: fix napi test_reference for recent V8 (Michaël Zasso) [#12864](https://github.com/nodejs/node/pull/12864)
* [[`42958d1a75`](https://github.com/nodejs/node/commit/42958d1a75)] - **test**: refactor test-querystring (Łukasz Szewczak) [#12661](https://github.com/nodejs/node/pull/12661)
* [[`152966dbb5`](https://github.com/nodejs/node/commit/152966dbb5)] - **test**: refactoring test with common.mustCall (weewey) [#12702](https://github.com/nodejs/node/pull/12702)
* [[`6058c4349f`](https://github.com/nodejs/node/commit/6058c4349f)] - **test**: refactored test-repl-persistent-history (cool88) [#12703](https://github.com/nodejs/node/pull/12703)
* [[`dac9f42a7e`](https://github.com/nodejs/node/commit/dac9f42a7e)] - **test**: remove common.PORT in test tls ticket cluster (Oscar Martinez) [#12715](https://github.com/nodejs/node/pull/12715)
* [[`d37f27a008`](https://github.com/nodejs/node/commit/d37f27a008)] - **test**: expand test coverage of readline (James M Snell) [#12755](https://github.com/nodejs/node/pull/12755)
* [[`a710e443a2`](https://github.com/nodejs/node/commit/a710e443a2)] - **test**: complete coverage of buffer (David Cai) [#12831](https://github.com/nodejs/node/pull/12831)
* [[`3fd890a06e`](https://github.com/nodejs/node/commit/3fd890a06e)] - **test**: add mustCall in timers-unrefed-in-callback (Zahidul Islam) [#12594](https://github.com/nodejs/node/pull/12594)
* [[`73d9c0f903`](https://github.com/nodejs/node/commit/73d9c0f903)] - **test**: port test for make_callback to n-api (Hitesh Kanwathirtha) [#12409](https://github.com/nodejs/node/pull/12409)
* [[`68c933c01e`](https://github.com/nodejs/node/commit/68c933c01e)] - **test**: fix flakyness with `yes.exe` (Refael Ackermann) [#12821](https://github.com/nodejs/node/pull/12821)
* [[`8b76c3e60c`](https://github.com/nodejs/node/commit/8b76c3e60c)] - **test**: reduce string concatenations (Vse Mozhet Byt) [#12735](https://github.com/nodejs/node/pull/12735)
* [[`f1d593cda1`](https://github.com/nodejs/node/commit/f1d593cda1)] - **test**: make tests cwd-independent (Vse Mozhet Byt) [#12812](https://github.com/nodejs/node/pull/12812)
* [[`94a120cf65`](https://github.com/nodejs/node/commit/94a120cf65)] - **test**: add coverage for error apis (Michael Dawson) [#12729](https://github.com/nodejs/node/pull/12729)
* [[`bc05436a89`](https://github.com/nodejs/node/commit/bc05436a89)] - **test**: add regex check in test-vm-is-context (jeyanthinath) [#12785](https://github.com/nodejs/node/pull/12785)
* [[`665695fbea`](https://github.com/nodejs/node/commit/665695fbea)] - **test**: add callback to fs.close() in test-fs-stat (Vse Mozhet Byt) [#12804](https://github.com/nodejs/node/pull/12804)
* [[`712596fc45`](https://github.com/nodejs/node/commit/712596fc45)] - **test**: add callback to fs.close() in test-fs-chmod (Vse Mozhet Byt) [#12795](https://github.com/nodejs/node/pull/12795)
* [[`f971916885`](https://github.com/nodejs/node/commit/f971916885)] - **test**: fix too optimistic guess in setproctitle (Vse Mozhet Byt) [#12792](https://github.com/nodejs/node/pull/12792)
* [[`4677766d21`](https://github.com/nodejs/node/commit/4677766d21)] - **test**: enable test-debugger-pid (Rich Trott) [#12770](https://github.com/nodejs/node/pull/12770)
* [[`ff001c12b0`](https://github.com/nodejs/node/commit/ff001c12b0)] - **test**: move WPT to its own testing module (Rich Trott) [#12736](https://github.com/nodejs/node/pull/12736)
* [[`b2ab41e5ae`](https://github.com/nodejs/node/commit/b2ab41e5ae)] - **test**: increase readline coverage (Anna Henningsen) [#12761](https://github.com/nodejs/node/pull/12761)
* [[`8aca66a1f3`](https://github.com/nodejs/node/commit/8aca66a1f3)] - **test**: fix warning in n-api reference test (Michael Dawson) [#12730](https://github.com/nodejs/node/pull/12730)
* [[`04796ee97f`](https://github.com/nodejs/node/commit/04796ee97f)] - **test**: increase coverage of buffer (David Cai) [#12714](https://github.com/nodejs/node/pull/12714)
* [[`133fb0c3b7`](https://github.com/nodejs/node/commit/133fb0c3b7)] - **test**: minor fixes to test-module-loading.js (Walter Huang) [#12728](https://github.com/nodejs/node/pull/12728)
* [[`9f7b54945e`](https://github.com/nodejs/node/commit/9f7b54945e)] - ***Revert*** "**test**: remove eslint comments" (Joyee Cheung) [#12743](https://github.com/nodejs/node/pull/12743)
* [[`10ccf56f89`](https://github.com/nodejs/node/commit/10ccf56f89)] - **test**: skipIfInspectorDisabled cluster-inspect-brk (Daniel Bevenius) [#12757](https://github.com/nodejs/node/pull/12757)
* [[`0142276977`](https://github.com/nodejs/node/commit/0142276977)] - **test**: replace indexOf with includes (gwer) [#12604](https://github.com/nodejs/node/pull/12604)
* [[`0324ac686c`](https://github.com/nodejs/node/commit/0324ac686c)] - **test**: add inspect-brk option to cluster module (dave-k) [#12503](https://github.com/nodejs/node/pull/12503)
* [[`d5db4d25dc`](https://github.com/nodejs/node/commit/d5db4d25dc)] - **test**: cleanup handles in test_environment (Anna Henningsen) [#12621](https://github.com/nodejs/node/pull/12621)
* [[`427cd293d5`](https://github.com/nodejs/node/commit/427cd293d5)] - **test**: add hasCrypto check to test-cli-node-options (Daniel Bevenius) [#12692](https://github.com/nodejs/node/pull/12692)
* [[`0101a8f1a6`](https://github.com/nodejs/node/commit/0101a8f1a6)] - **test**: add relative path to accommodate limit (coreybeaumont) [#12601](https://github.com/nodejs/node/pull/12601)
* [[`b16869c4e4`](https://github.com/nodejs/node/commit/b16869c4e4)] - **test**: remove AIX guard in fs-options-immutable (Sakthipriyan Vairamani (thefourtheye)) [#12687](https://github.com/nodejs/node/pull/12687)
* [[`a4fd9e5e6d`](https://github.com/nodejs/node/commit/a4fd9e5e6d)] - **test**: chdir before running test-cli-node-options (Daniel Bevenius) [#12660](https://github.com/nodejs/node/pull/12660)
* [[`d289678352`](https://github.com/nodejs/node/commit/d289678352)] - **test**: dynamic port in dgram tests (Sebastian Plesciuc) [#12623](https://github.com/nodejs/node/pull/12623)
* [[`28f535a923`](https://github.com/nodejs/node/commit/28f535a923)] - **test**: fixup test-http-hostname-typechecking (Anna Henningsen) [#12627](https://github.com/nodejs/node/pull/12627)
* [[`e927809eec`](https://github.com/nodejs/node/commit/e927809eec)] - **test**: dynamic port in parallel regress tests (Sebastian Plesciuc) [#12639](https://github.com/nodejs/node/pull/12639)
* [[`1d968030d4`](https://github.com/nodejs/node/commit/1d968030d4)] - **test**: add coverage for napi_cancel_async_work (Michael Dawson) [#12575](https://github.com/nodejs/node/pull/12575)
* [[`4241577112`](https://github.com/nodejs/node/commit/4241577112)] - **test**: test doc'd napi_get_value_int32 behaviour (Michael Dawson) [#12633](https://github.com/nodejs/node/pull/12633)
* [[`bda34bde56`](https://github.com/nodejs/node/commit/bda34bde56)] - **test**: remove obsolete lint comment (Rich Trott) [#12659](https://github.com/nodejs/node/pull/12659)
* [[`c8c5a528da`](https://github.com/nodejs/node/commit/c8c5a528da)] - **test**: make tests pass when built without inspector (Michaël Zasso) [#12622](https://github.com/nodejs/node/pull/12622)
* [[`d1d9ecfe6e`](https://github.com/nodejs/node/commit/d1d9ecfe6e)] - **test**: support unreleased V8 versions (Michaël Zasso) [#12619](https://github.com/nodejs/node/pull/12619)
* [[`75bfdad037`](https://github.com/nodejs/node/commit/75bfdad037)] - **test**: check that pending warning is emitted once (Rich Trott) [#12527](https://github.com/nodejs/node/pull/12527)
* [[`5e095f699e`](https://github.com/nodejs/node/commit/5e095f699e)] - **test**: verify listener leak is only emitted once (cjihrig) [#12502](https://github.com/nodejs/node/pull/12502)
* [[`4bcbefccce`](https://github.com/nodejs/node/commit/4bcbefccce)] - **test**: add coverage for vm's breakOnSigint option (cjihrig) [#12512](https://github.com/nodejs/node/pull/12512)
* [[`f3f9dd73aa`](https://github.com/nodejs/node/commit/f3f9dd73aa)] - **test**: skip tests using ca flags (Daniel Bevenius) [#12485](https://github.com/nodejs/node/pull/12485)
* [[`86a3ba0c4e`](https://github.com/nodejs/node/commit/86a3ba0c4e)] - **test**: dynamic port in cluster worker wait close (Sebastian Plesciuc) [#12466](https://github.com/nodejs/node/pull/12466)
* [[`6c912a8216`](https://github.com/nodejs/node/commit/6c912a8216)] - **test**: fix coverity UNINIT_CTOR cctest warning (Ben Noordhuis) [#12387](https://github.com/nodejs/node/pull/12387)
* [[`4fc11998b4`](https://github.com/nodejs/node/commit/4fc11998b4)] - **test**: add cwd ENOENT known issue test (cjihrig) [#12343](https://github.com/nodejs/node/pull/12343)
* [[`2e5188de92`](https://github.com/nodejs/node/commit/2e5188de92)] - **test**: remove common.PORT from multiple tests (Tarun Batra) [#12451](https://github.com/nodejs/node/pull/12451)
* [[`7044065f1a`](https://github.com/nodejs/node/commit/7044065f1a)] - **test**: change == to === in crypto test (Fabio Campinho) [#12405](https://github.com/nodejs/node/pull/12405)
* [[`f98db78f77`](https://github.com/nodejs/node/commit/f98db78f77)] - **test**: add internal/fs tests (DavidCai) [#12306](https://github.com/nodejs/node/pull/12306)
* [[`3d2181c5f0`](https://github.com/nodejs/node/commit/3d2181c5f0)] - **test**: run the addon tests last (Sebastian Van Sande) [#12062](https://github.com/nodejs/node/pull/12062)
* [[`8bd26d3aea`](https://github.com/nodejs/node/commit/8bd26d3aea)] - **test**: fix compiler warning in n-api test (Anna Henningsen) [#12318](https://github.com/nodejs/node/pull/12318)
* [[`3900cf66a5`](https://github.com/nodejs/node/commit/3900cf66a5)] - **test**: remove disabled test-dgram-send-error (Rich Trott) [#12330](https://github.com/nodejs/node/pull/12330)
* [[`9de2e159c4`](https://github.com/nodejs/node/commit/9de2e159c4)] - **test**: add second argument to assert.throws (Michaël Zasso) [#12270](https://github.com/nodejs/node/pull/12270)
* [[`0ec0272e10`](https://github.com/nodejs/node/commit/0ec0272e10)] - **test**: improve test coverage for n-api (Michael Dawson) [#12327](https://github.com/nodejs/node/pull/12327)
* [[`569f988be7`](https://github.com/nodejs/node/commit/569f988be7)] - **test**: remove disabled tls_server.js (Rich Trott) [#12275](https://github.com/nodejs/node/pull/12275)
* [[`2555780aa6`](https://github.com/nodejs/node/commit/2555780aa6)] - **test**: check curve algorithm is supported (Karl Cheng) [#12265](https://github.com/nodejs/node/pull/12265)
* [[`2d3d4ccb98`](https://github.com/nodejs/node/commit/2d3d4ccb98)] - **test**: add http benchmark test (Joyee Cheung) [#12121](https://github.com/nodejs/node/pull/12121)
* [[`b03f1f0c01`](https://github.com/nodejs/node/commit/b03f1f0c01)] - **test**: add basic cctest for base64.h (Alexey Orlenko) [#12238](https://github.com/nodejs/node/pull/12238)
* [[`971fe67dce`](https://github.com/nodejs/node/commit/971fe67dce)] - **test**: complete coverage for lib/assert.js (Rich Trott) [#12239](https://github.com/nodejs/node/pull/12239)
* [[`65c100ae8b`](https://github.com/nodejs/node/commit/65c100ae8b)] - **test**: remove disabled debugger test (Rich Trott) [#12199](https://github.com/nodejs/node/pull/12199)
* [[`610ac7d858`](https://github.com/nodejs/node/commit/610ac7d858)] - **test**: increase coverage of internal/socket_list (DavidCai) [#12066](https://github.com/nodejs/node/pull/12066)
* [[`2ff107dad7`](https://github.com/nodejs/node/commit/2ff107dad7)] - **test**: add case for url.parse throwing a URIError (Lovell Fuller) [#12135](https://github.com/nodejs/node/pull/12135)
* [[`5ccaba49f0`](https://github.com/nodejs/node/commit/5ccaba49f0)] - **test**: add variable arguments support for Argv (Daniel Bevenius) [#12166](https://github.com/nodejs/node/pull/12166)
* [[`9348f31c2a`](https://github.com/nodejs/node/commit/9348f31c2a)] - **test**: fix test-cli-syntax assertions on windows (Teddy Katz) [#12212](https://github.com/nodejs/node/pull/12212)
* [[`53828e8bff`](https://github.com/nodejs/node/commit/53828e8bff)] - **test**: extended test to makeCallback cb type check (Luca Maraschi) [#12140](https://github.com/nodejs/node/pull/12140)
* [[`9b05393362`](https://github.com/nodejs/node/commit/9b05393362)] - **test**: fix V8 test on big-endian machines (Anna Henningsen) [#12186](https://github.com/nodejs/node/pull/12186)
* [[`50bfef66f0`](https://github.com/nodejs/node/commit/50bfef66f0)] - **test**: synchronize WPT url test data (Daijiro Wachi) [#12058](https://github.com/nodejs/node/pull/12058)
* [[`92de91d570`](https://github.com/nodejs/node/commit/92de91d570)] - **test**: fix truncation of argv (Daniel Bevenius) [#12110](https://github.com/nodejs/node/pull/12110)
* [[`51b007aaa7`](https://github.com/nodejs/node/commit/51b007aaa7)] - **test**: add cctest for native URL class (James M Snell) [#12042](https://github.com/nodejs/node/pull/12042)
* [[`4f2e372716`](https://github.com/nodejs/node/commit/4f2e372716)] - **test**: add common.noop, default for common.mustCall() (James M Snell) [#12027](https://github.com/nodejs/node/pull/12027)
* [[`4929d12e99`](https://github.com/nodejs/node/commit/4929d12e99)] - **test**: add internal/socket_list tests (DavidCai) [#11989](https://github.com/nodejs/node/pull/11989)
* [[`64d0a73574`](https://github.com/nodejs/node/commit/64d0a73574)] - **test**: minor fixups for REPL eval tests (Anna Henningsen) [#11946](https://github.com/nodejs/node/pull/11946)
* [[`6aed32c579`](https://github.com/nodejs/node/commit/6aed32c579)] - **test**: add tests for unixtimestamp generation (Luca Maraschi) [#11886](https://github.com/nodejs/node/pull/11886)
* [[`1ff6796083`](https://github.com/nodejs/node/commit/1ff6796083)] - **test**: added net.connect lookup type check (Luca Maraschi) [#11873](https://github.com/nodejs/node/pull/11873)
* [[`7b830f4e4a`](https://github.com/nodejs/node/commit/7b830f4e4a)] - **test**: add more and refactor test cases to net.connect (Joyee Cheung) [#11847](https://github.com/nodejs/node/pull/11847)
* [[`474e9d64b5`](https://github.com/nodejs/node/commit/474e9d64b5)] - **test**: add more test cases of server.listen option (Joyee Cheung)
* [[`78cdd4baa4`](https://github.com/nodejs/node/commit/78cdd4baa4)] - **test**: include all stdio strings for fork() (Rich Trott) [#11783](https://github.com/nodejs/node/pull/11783)
* [[`b98004b79c`](https://github.com/nodejs/node/commit/b98004b79c)] - **test**: add hasCrypto check to tls-legacy-deprecated (Daniel Bevenius) [#11747](https://github.com/nodejs/node/pull/11747)
* [[`60c8115f63`](https://github.com/nodejs/node/commit/60c8115f63)] - **test**: clean up comments in test-url-format (Rich Trott) [#11679](https://github.com/nodejs/node/pull/11679)
* [[`1402fef098`](https://github.com/nodejs/node/commit/1402fef098)] - **test**: make tests pass when configured without-ssl (Daniel Bevenius) [#11631](https://github.com/nodejs/node/pull/11631)
* [[`acc3a80546`](https://github.com/nodejs/node/commit/acc3a80546)] - **test**: add two test cases for querystring (Daijiro Wachi) [#11551](https://github.com/nodejs/node/pull/11551)
* [[`a218fa381f`](https://github.com/nodejs/node/commit/a218fa381f)] - **test**: fix WPT.test()'s error handling (Timothy Gu) [#11436](https://github.com/nodejs/node/pull/11436)
* [[`dd2e135560`](https://github.com/nodejs/node/commit/dd2e135560)] - **test**: add two test cases for querystring (Daijiro Wachi) [#11481](https://github.com/nodejs/node/pull/11481)
* [[`82ddf96828`](https://github.com/nodejs/node/commit/82ddf96828)] - **test**: turn on WPT tests on empty param pairs (Joyee Cheung) [#11369](https://github.com/nodejs/node/pull/11369)
* [[`8bcc122349`](https://github.com/nodejs/node/commit/8bcc122349)] - **test**: improve querystring.parse assertion messages (Brian White) [#11234](https://github.com/nodejs/node/pull/11234)
* [[`dd1cf8bb37`](https://github.com/nodejs/node/commit/dd1cf8bb37)] - **test**: refactor test-http-response-statuscode (Rich Trott) [#11274](https://github.com/nodejs/node/pull/11274)
* [[`1544d8f04b`](https://github.com/nodejs/node/commit/1544d8f04b)] - **test**: improve test-buffer-includes.js (toboid) [#11203](https://github.com/nodejs/node/pull/11203)
* [[`f8cdaaa16a`](https://github.com/nodejs/node/commit/f8cdaaa16a)] - **test**: validate error message from buffer.equals (Sebastian Roeder) [#11215](https://github.com/nodejs/node/pull/11215)
* [[`901cb8cb5e`](https://github.com/nodejs/node/commit/901cb8cb5e)] - **test**: increase coverage of buffer (DavidCai) [#11122](https://github.com/nodejs/node/pull/11122)
* [[`78545039d6`](https://github.com/nodejs/node/commit/78545039d6)] - **test**: remove unnecessary eslint-disable max-len (Joyee Cheung) [#11049](https://github.com/nodejs/node/pull/11049)
* [[`6af10907a2`](https://github.com/nodejs/node/commit/6af10907a2)] - **test**: add msg validation to test-buffer-compare (Josh Hollandsworth) [#10807](https://github.com/nodejs/node/pull/10807)
* [[`775de9cc96`](https://github.com/nodejs/node/commit/775de9cc96)] - **test**: improve module version mismatch error check (cjihrig) [#10636](https://github.com/nodejs/node/pull/10636)
* [[`904b66d870`](https://github.com/nodejs/node/commit/904b66d870)] - **test**: increase coverage of Buffer.transcode (Joyee Cheung) [#10437](https://github.com/nodejs/node/pull/10437)
* [[`a180259e42`](https://github.com/nodejs/node/commit/a180259e42)] - **test,lib,doc**: use function declarations (Rich Trott) [#12711](https://github.com/nodejs/node/pull/12711)
* [[`98609fc1c4`](https://github.com/nodejs/node/commit/98609fc1c4)] - **timers**: do not use user object call/apply (Rich Trott) [#12960](https://github.com/nodejs/node/pull/12960)
* [[`b23d414c7e`](https://github.com/nodejs/node/commit/b23d414c7e)] - **tls**: do not wrap net.Socket with StreamWrap (Ruslan Bekenev) [#12799](https://github.com/nodejs/node/pull/12799)
* [[`bfa27d22f5`](https://github.com/nodejs/node/commit/bfa27d22f5)] - **tools**: update certdata.txt (Ben Noordhuis) [#13279](https://github.com/nodejs/node/pull/13279)
* [[`feb90d37ff`](https://github.com/nodejs/node/commit/feb90d37ff)] - **tools**: relax lint rule for regexps (Rich Trott) [#12807](https://github.com/nodejs/node/pull/12807)
* [[`53c88fa411`](https://github.com/nodejs/node/commit/53c88fa411)] - **tools**: remove unused code from test.py (Rich Trott) [#12806](https://github.com/nodejs/node/pull/12806)
* [[`595d4ec114`](https://github.com/nodejs/node/commit/595d4ec114)] - **tools**: ignore node_trace.*.log (Daijiro Wachi) [#12754](https://github.com/nodejs/node/pull/12754)
* [[`aea7269c45`](https://github.com/nodejs/node/commit/aea7269c45)] - **tools**: require function declarations (Rich Trott) [#12711](https://github.com/nodejs/node/pull/12711)
* [[`e7c3f4a97b`](https://github.com/nodejs/node/commit/e7c3f4a97b)] - **tools**: fix gyp to work on MacOSX without XCode (Shigeki Ohtsu) [iojs/io.js#1325](https://github.com/iojs/io.js/pull/1325)
* [[`a4b9c585b3`](https://github.com/nodejs/node/commit/a4b9c585b3)] - **tools**: enforce two arguments in assert.throws (Michaël Zasso) [#12270](https://github.com/nodejs/node/pull/12270)
* [[`b3f2e3b7e2`](https://github.com/nodejs/node/commit/b3f2e3b7e2)] - **tools**: replace custom assert.fail lint rule (Rich Trott) [#12287](https://github.com/nodejs/node/pull/12287)
* [[`8191af5b29`](https://github.com/nodejs/node/commit/8191af5b29)] - **tools**: replace custom new-with-error rule (Rich Trott) [#12249](https://github.com/nodejs/node/pull/12249)
* [[`61ebfa8d1f`](https://github.com/nodejs/node/commit/61ebfa8d1f)] - **tools**: add unescaped regexp dot rule to linter (Brian White) [#11834](https://github.com/nodejs/node/pull/11834)
* [[`20b18236de`](https://github.com/nodejs/node/commit/20b18236de)] - **tools**: add rule prefering common.mustNotCall() (James M Snell) [#12027](https://github.com/nodejs/node/pull/12027)
* [[`096508dfa9`](https://github.com/nodejs/node/commit/096508dfa9)] - **tools,lib**: enable strict equality lint rule (Rich Trott) [#12446](https://github.com/nodejs/node/pull/12446)
* [[`70cdfc5eb1`](https://github.com/nodejs/node/commit/70cdfc5eb1)] - **url**: expose WHATWG url.origin as ASCII (Timothy Gu) [#13126](https://github.com/nodejs/node/pull/13126)
* [[`06a617aa21`](https://github.com/nodejs/node/commit/06a617aa21)] - **url**: update IDNA error conditions (Rajaram Gaunker) [#12966](https://github.com/nodejs/node/pull/12966)
* [[`841bb4c61f`](https://github.com/nodejs/node/commit/841bb4c61f)] - **url**: fix C0 control and whitespace handling (Timothy Gu) [#12846](https://github.com/nodejs/node/pull/12846)
* [[`943dd5f9ed`](https://github.com/nodejs/node/commit/943dd5f9ed)] - **url**: handle windows drive letter in the file state (Daijiro Wachi) [#12808](https://github.com/nodejs/node/pull/12808)
* [[`8491c705b1`](https://github.com/nodejs/node/commit/8491c705b1)] - **url**: fix permanent deoptimizations (Brian White) [#12456](https://github.com/nodejs/node/pull/12456)
* [[`97ec72b76d`](https://github.com/nodejs/node/commit/97ec72b76d)] - **url**: refactor binding imports in internal/url (James M Snell) [#12717](https://github.com/nodejs/node/pull/12717)
* [[`b331ba6ca9`](https://github.com/nodejs/node/commit/b331ba6ca9)] - **url**: move to module.exports = {} pattern (James M Snell) [#12717](https://github.com/nodejs/node/pull/12717)
* [[`d457a986a0`](https://github.com/nodejs/node/commit/d457a986a0)] - **url**: port WHATWG URL API to internal/errors (Timothy Gu) [#12574](https://github.com/nodejs/node/pull/12574)
* [[`061c5da010`](https://github.com/nodejs/node/commit/061c5da010)] - **url**: use internal/util's getConstructorOf (Timothy Gu) [#12526](https://github.com/nodejs/node/pull/12526)
* [[`2841f478e4`](https://github.com/nodejs/node/commit/2841f478e4)] - **url**: improve WHATWG URL inspection (Timothy Gu) [#12253](https://github.com/nodejs/node/pull/12253)
* [[`aff5cc92b9`](https://github.com/nodejs/node/commit/aff5cc92b9)] - **url**: clean up WHATWG URL origin generation (Timothy Gu) [#12252](https://github.com/nodejs/node/pull/12252)
* [[`1b99d8ffe9`](https://github.com/nodejs/node/commit/1b99d8ffe9)] - **url**: disallow invalid IPv4 in IPv6 parser (Daijiro Wachi) [#12315](https://github.com/nodejs/node/pull/12315)
* [[`eb0492d51e`](https://github.com/nodejs/node/commit/eb0492d51e)] - **url**: remove javascript URL special case (Daijiro Wachi) [#12331](https://github.com/nodejs/node/pull/12331)
* [[`b470a85f07`](https://github.com/nodejs/node/commit/b470a85f07)] - **url**: trim leading slashes of file URL paths (Daijiro Wachi) [#12203](https://github.com/nodejs/node/pull/12203)
* [[`b76a350a19`](https://github.com/nodejs/node/commit/b76a350a19)] - **url**: avoid instanceof for WHATWG URL (Brian White) [#11690](https://github.com/nodejs/node/pull/11690)
* [[`c4469c49ec`](https://github.com/nodejs/node/commit/c4469c49ec)] - **url**: error when domainTo*() is called w/o argument (Timothy Gu) [#12134](https://github.com/nodejs/node/pull/12134)
* [[`f8f46f9917`](https://github.com/nodejs/node/commit/f8f46f9917)] - **url**: change path parsing for non-special URLs (Daijiro Wachi) [#12058](https://github.com/nodejs/node/pull/12058)
* [[`7139b93a8b`](https://github.com/nodejs/node/commit/7139b93a8b)] - **url**: add ToObject method to native URL class (James M Snell) [#12056](https://github.com/nodejs/node/pull/12056)
* [[`14a91957f8`](https://github.com/nodejs/node/commit/14a91957f8)] - **url**: use a class for WHATWG url\[context\] (Timothy Gu) [#11930](https://github.com/nodejs/node/pull/11930)
* [[`c515a985ea`](https://github.com/nodejs/node/commit/c515a985ea)] - **url**: spec-compliant URLSearchParams parser (Timothy Gu) [#11858](https://github.com/nodejs/node/pull/11858)
* [[`d77a7588cf`](https://github.com/nodejs/node/commit/d77a7588cf)] - **url**: spec-compliant URLSearchParams serializer (Timothy Gu) [#11626](https://github.com/nodejs/node/pull/11626)
* [[`99b27ce99a`](https://github.com/nodejs/node/commit/99b27ce99a)] - **url**: prioritize toString when stringifying (Timothy Gu) [#11737](https://github.com/nodejs/node/pull/11737)
* [[`b610a4db1c`](https://github.com/nodejs/node/commit/b610a4db1c)] - **url**: enforce valid UTF-8 in WHATWG parser (Timothy Gu) [#11436](https://github.com/nodejs/node/pull/11436)
* [[`147d2a6419`](https://github.com/nodejs/node/commit/147d2a6419)] - **url, test**: break up test-url.js (Joyee Cheung) [#11049](https://github.com/nodejs/node/pull/11049)
* [[`ef16319eff`](https://github.com/nodejs/node/commit/ef16319eff)] - **util**: fixup internal util exports (James M Snell) [#12998](https://github.com/nodejs/node/pull/12998)
* [[`d5925af8d7`](https://github.com/nodejs/node/commit/d5925af8d7)] - **util**: fix permanent deoptimizations (Brian White) [#12456](https://github.com/nodejs/node/pull/12456)
* [[`3c0dd45c88`](https://github.com/nodejs/node/commit/3c0dd45c88)] - **util**: move getConstructorOf() to internal (Timothy Gu) [#12526](https://github.com/nodejs/node/pull/12526)
* [[`a37273c1e4`](https://github.com/nodejs/node/commit/a37273c1e4)] - **util**: use V8 C++ API for inspecting Promises (Timothy Gu) [#12254](https://github.com/nodejs/node/pull/12254)
* [[`c8be718749`](https://github.com/nodejs/node/commit/c8be718749)] - **v8**: backport pieces from 18a26cfe174 from upstream v8 (Peter Marshall) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`cfdcd6cf33`](https://github.com/nodejs/node/commit/cfdcd6cf33)] - **v8**: backport 43791ce02c8 from upstream v8 (kozyatinskiy) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`1061e43739`](https://github.com/nodejs/node/commit/1061e43739)] - **v8**: backport faf5f52627c from upstream v8 (Peter Marshall) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`a56f9698cb`](https://github.com/nodejs/node/commit/a56f9698cb)] - **v8**: backport 4f82f1d948c from upstream v8 (hpayer) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`13a961e9dc`](https://github.com/nodejs/node/commit/13a961e9dc)] - **v8**: backport 4f82f1d948c from upstream v8 (hpayer) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`188630b84c`](https://github.com/nodejs/node/commit/188630b84c)] - **v8**: backport a9e56f4f36d from upstream v8 (Peter Marshall) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`0f3bfaf530`](https://github.com/nodejs/node/commit/0f3bfaf530)] - **v8**: backport bd59e7452be from upstream v8 (Michael Achenbach) [#13217](https://github.com/nodejs/node/pull/13217)
* [[`6d5ca4feb0`](https://github.com/nodejs/node/commit/6d5ca4feb0)] - **v8**: backport pieces of dab18fb0bbcdd (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`62eaa2a186`](https://github.com/nodejs/node/commit/62eaa2a186)] - **v8**: do not test v8 with -Werror (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`f118f7ae90`](https://github.com/nodejs/node/commit/f118f7ae90)] - **v8**: backport header diff from 2e4a68733803 (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`a947cf9a03`](https://github.com/nodejs/node/commit/a947cf9a03)] - **v8**: backport header diff from 94283dcf4459f (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`1bb880b595`](https://github.com/nodejs/node/commit/1bb880b595)] - **v8**: backport pieces of bf463c4dc0 and dc662e5b74 (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`04e646be52`](https://github.com/nodejs/node/commit/04e646be52)] - **v8**: backport header diff from da5b745dba387 (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`39834bc441`](https://github.com/nodejs/node/commit/39834bc441)] - **v8**: backport pieces of 6226576efa82ee (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`25430fd247`](https://github.com/nodejs/node/commit/25430fd247)] - **v8**: backport pieces from 99743ad460e (Anna Henningsen) [#12875](https://github.com/nodejs/node/pull/12875)
* [[`0f3e69db41`](https://github.com/nodejs/node/commit/0f3e69db41)] - **v8**: fix gcc 7 build errors (Zuzana Svetlikova) [#12676](https://github.com/nodejs/node/pull/12676)
* [[`b07e1a828c`](https://github.com/nodejs/node/commit/b07e1a828c)] - **v8**: fix gcc 7 build errors (Zuzana Svetlikova) [#12676](https://github.com/nodejs/node/pull/12676)
* [[`1052383f7c`](https://github.com/nodejs/node/commit/1052383f7c)] - **v8**: refactor struture of v8 module (James M Snell) [#12681](https://github.com/nodejs/node/pull/12681)
* [[`33a19b46ca`](https://github.com/nodejs/node/commit/33a19b46ca)] - **v8**: fix offsets for TypedArray deserialization (Anna Henningsen) [#12143](https://github.com/nodejs/node/pull/12143)
* [[`6b25c75cda`](https://github.com/nodejs/node/commit/6b25c75cda)] - **vm**: fix race condition with timeout param (Marcel Laverdet) [#13074](https://github.com/nodejs/node/pull/13074)
* [[`191bb5a358`](https://github.com/nodejs/node/commit/191bb5a358)] - **vm**: fix displayErrors in runIn.. functions (Marcel Laverdet) [#13074](https://github.com/nodejs/node/pull/13074)
* [[`1c93e8c94b`](https://github.com/nodejs/node/commit/1c93e8c94b)] - **win**: make buildable on VS2017 (Refael Ackermann) [#11852](https://github.com/nodejs/node/pull/11852)
* [[`ea01cd7adb`](https://github.com/nodejs/node/commit/ea01cd7adb)] - **zlib**: remove unused declaration (Anna Henningsen) [#12432](https://github.com/nodejs/node/pull/12432)
