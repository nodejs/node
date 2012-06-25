title: Node v0.8.0
date: Mon Jun 25 2012 09:00:00 GMT-0700 (PDT)
version: 0.8.0
category: release
author: Isaac Z. Schlueter
slug: node-v0-8-0
status: publish

I am thrilled to announce the arrival of a new stable version of
Node.js.

Compared with the v0.6 releases of Node, this release brings significant
improvements in many key performance metrics, as well as
cleanup in several core APIs, and the addition of new debugging
features.

## tl;dr

With version 0.8.0:

1. Node got a lot faster.
2. Node got more stable.
3. You can do stuff with file descriptors again.
4. The [cluster module](http://nodejs.org/api/cluster.html) is much more
   awesome.
5. The [domain module](http://nodejs.org/api/domain.html) was added.
6. The repl is better.
7. The build system changed from waf to gyp.
8. [Some other stuff changed,
   too.](https://github.com/joyent/node/wiki/API-changes-between-v0.6-and-v0.8)
9. Scroll to the bottom for the links to install it.

## Performance

This version brings a few key enhancements in V8 and libuv that result
in significantly improved throughput.

All of these benchmarks were run on my OS X laptop, but the results are
typical of what we're seeing on SmartOS, Linux, and Windows.

```
# io.js

# 0.6.19, writes
Wrote 1024 byte buffers: 19.428793471925395 mB/s
Wrote 4096 byte buffers: 59.737156511350065 mB/s
Wrote 16384 byte buffers: 83.97010664203543 mB/s
Wrote 65536 byte buffers: 97.4184120798831 mB/s

# 0.8.0, writes
Wrote 1024 byte buffers: 61.236987140232706 mB/s +215.19%
Wrote 4096 byte buffers: 109.05125408942203 mB/s +82.55%
Wrote 16384 byte buffers: 182.18254691200585 mB/s +116.96%
Wrote 65536 byte buffers: 181.91740949608877 mB/s +86.74%

# v0.6.19, reads
Read 1024 byte buffers: 29.96883241428914 mB/s
Read 4096 byte buffers: 62.34413965087282 mB/s
Read 16384 byte buffers: 165.7550140891762 mB/s
Read 65536 byte buffers: 266.73779674579885 mB/s

# v0.8.0, reads
Read 1024 byte buffers: 57.63688760806916 mB/s +92.32%
Read 4096 byte buffers: 136.7801942278758 mB/s +119.40%
Read 16384 byte buffers: 244.8579823702253 mB/s +47.72%
Read 65536 byte buffers: 302.2974607013301 mB/s +13.33%
```

The difference is not small.  If you are writing network programs with
node, and pushing a lot of traffic, you will notice this improvement.

The speed of reading files got quite a bit faster as well:

```
# v0.6.19
read the file 110948 times (higher is better)
90141.32 ns per read (lower is better)
11093.69 reads per sec (higher is better)

# v0.8.0
read the file 158193 times (higher is better) +42.58%
63217.16 ns per read (lower is better) -29.87%
15818.48 reads per sec (higher is better) +42.59%
```

And of course, the ubiquitous 'hello, world' http server benchmark got
significantly faster, especially for large message sizes:

```
$ TYPE=bytes LENGTH=123 bash benchmark/http.sh  2>&1 | grep Req
# 0.6.19
Requests per second:    3317.24 [#/sec] (mean)
# 0.8.0
Requests per second:    3795.34 [#/sec] (mean) +14.41%


$ TYPE=bytes LENGTH=1024 bash benchmark/http.sh  2>&1 | grep Req
# v0.6.19
Requests per second:    3258.42 [#/sec] (mean)
# 0.8.0
Requests per second:    3585.62 [#/sec] (mean) +10.04%


$ TYPE=bytes LENGTH=123456 bash benchmark/http.sh  2>&1 | grep Req
# v0.6.19
Requests per second:    218.51 [#/sec] (mean)
# 0.8.0
Requests per second:    749.17 [#/sec] (mean) +242.85%
```

The difference with Unicode responses is even more pronounced:

```
$ TYPE=unicode LENGTH=1024 bash benchmark/http.sh  2>&1 | grep Req
# v0.6.19
Requests per second:    3228.23 [#/sec] (mean)
# v0.8.0
Requests per second:    3317.60 [#/sec] (mean) +2.77%

$ TYPE=unicode LENGTH=12345 bash benchmark/http.sh  2>&1 | grep Req
# v0.6.19
Requests per second:    1703.96 [#/sec] (mean)
# v0.8.0
Requests per second:    2431.61 [#/sec] (mean) +42.70%

$ TYPE=unicode LENGTH=55555 bash benchmark/http.sh  2>&1 | grep Req
#v0.6.19
Requests per second:    161.65 [#/sec] (mean)
#v0.8.0
Requests per second:    980.38 [#/sec] (mean) +506.48%

$ TYPE=unicode LENGTH=99999 bash benchmark/http.sh  2>&1 | grep Req
# v0.6.19
^C  # lost patience after a few hours
# v0.8.0
Requests per second:    252.69 [#/sec] (mean)
```

The more bytes you're pushing, and the more work you're doing, the more
win you'll see with node 0.8 over 0.6.

The vast majority of the performance boost is due to improvements in V8.
They've been very responsive to the needs of the Node.js project.  A lot
of Node's success is due to being built on such a stellar VM.


## Build System

Since its inception Node has used the WAF build system which is a Python
based system similar to SCons.  The Chrome project recently changed to
the GYP meta-build system from SCons. GYP generates Makefiles, Visual
Studio project files, or XCode files depending on the target. V8, being
part of the Chrome project, now defines its build in GYP.  By using GYP,
Node is able to:

- integrate with the optimal build system on all platforms,
- easily able to integrate V8's build process into its own, and
- define its compilation declaratively for better manageability

GYP was used already in Node v0.6 to build on Windows, but now it
defines the build on all platforms. Node is still in the process of
migrating external addon modules to GYP, and node-gyp is included with
npm.  In future releases, node-waf will be officially deprecated.  If
you are currently using a wscript in your addon, please migrate to gyp
as soon as possible.


## Stabler

The transition from libev and libeio to libuv in 0.6 was somewhat
destabilizing for many node internals.  The gambit paid off: libuv is
the obvious choice in cross-platform asynchronous IO libraries, and
Node.js is impressively performant on both Windows and Unix.  But it
made the transition from 0.4 to 0.6 was very rocky for a lot of users.
Libuv wasn't as mature as node, and it showed in those early releases.

At this point, with very few exceptions, if your v0.6 program doesn't
run on v0.8, it should be easy and obvious to make whatever changes are
necessary.  Libuv has come a very long way, and Node 0.8 is a simpler
and more efficient machine as a result.

See the [migration
wiki](https://github.com/joyent/node/wiki/API-changes-between-v0.6-and-v0.8)
for details on the specific APIs that changed.

## The Return of File Descriptors

In Node 0.4, there was a `listenFD` method that servers could use to
listen on a specific file descriptor that was already bound to a socket
or port. In 0.6, that functionality went away, largely because it was
very Unix-specific, and couldn't be easily made to work with the new
cross-platform libuv base.

Since the most common use case for listenFD was as a method for having
servers in multiple node processes share the same underlying handle, the
`cluster` module was added in its place.  However, this still left a lot
of use cases unaddressed, and was a reason why some people could not use
node 0.6 for their programs.

In 0.8, we've replaced this functionality, as `server.listen({ fd:
number })`.

The other feature in node 0.4 that got dropped in 0.6 was the ability to
pass arbitrary file descriptors as a child process's stdio, using the
`customFds` array.  In Node 0.6, `customFds` could be used to inherit
the parent's stdio handles, but not to pass arbitrary handles or file
descriptors to the child's stdio.  Also, there was never a way to pass
more than the standard `in, out, err` trio, so programs that expected
FD 4 to be opened in some specific way were out of luck.

In 0.8, we've added the `stdio` array on the `child_process.spawn`
options.  Pass as many file descriptors, handles, etc. as you like, and
the child process will see them as already-opened FDs.

## More Powerful Cluster

The cluster module in 0.8 is so much improved over 0.6, it's basically a
complete rewrite.  The API is mostly backwards compatible, but not
entirely.  (See the [migration
wiki](https://github.com/joyent/node/wiki/API-changes-between-v0.6-and-v0.8)
for details.)

Barring these very minor API changes, if you were using cluster in 0.6,
then your program will still work, but it'll be faster and more
well-behaved now.  And if you aren't taking advantage of the new
features in 0.8's cluster, you're really missing out.

There's too much even to do it justice here.  Go read [the API
docs](http://nodejs.org/api/cluster.html).

## Domains

The original idea for Domains was a way to join multiple different IO
actions, so that you can have some context when an error occurs.

Since Ryan discussed the feature with node users at NodeConf Summer Camp
last year, the domains feature has gone through many revisions.  The
problem is fairly well understood, but most attempts to solve it
resulted in serious performance regressions, or uncovered difficult edge
cases.

What we ended up with in 0.8 is a very stripped-down version of this
idea.  It's entirely opt-in, with minimal performance impact when it's
used (and none when it isn't).  There are a lot of examples in [the API
documentation](http://nodejs.org/api/domain.html), so check them out,
and start handling your crashes smarter.

The domain module is still experimental.  We are looking forward to your
feedback, so please use it and let us know what you think.

## Repl, Readline, TTY

The Repl, Readline, and TTY modules have all had a major facelift.  The
interfaces between these three modules are cleaned up and refactored,
removing a lot of common pain points and making it easier to use for
debugging your programs.

It may seem minor at times, but a good repl dramatically increases the
quality of the overall experience.  My personal favorites are:

1. Typing `fs` or `net` or `path` will automatically load the module.
2. Typing `npm install ...` will give you a helpful message.
3. It doesn't do that stupid thing where long lines wrap and then the
   backspace makes it get all confused and crazy.  Instead of that, it
   does the right thing.

## Looking Forward

Like other even-numbered version families before it, v0.8 will maintain
API and ABI stability throughout its lifetime.

The v0.6 release family will continue to see releases for critical
bugfixes and security issues through the end of 2012.  However, it will
not be the main focus of the core team's attention.

The v0.9 releases will start in the next couple weeks.  The main focus
of v0.9 will be:

* The HTTP implementation - It has seen a lot of real-world use now, but
  the http module is in dire need of a cleanup and refactor.  Special
  attention will be paid to making the interfaces more consistent,
  improve performance, and increase correctness in more edge cases.
* The Streams API - The concept of the Stream API is very core to node.
  However, it is also (like HTTP) a feature that grew up organically,
  and is now in need of a cleanup.  It is currently too hard to get
  right, especially regarding error handling.
* Libuv Streams - The handle interfaces in libuv are going to be
  refactored for added consistency throughout the codebase and across
  platforms.

Looking past that, there are a few areas where Node.js still has room
for improvement in terms of internal consistency, idiomatic JavaScript
usage, and performance.  None of these are fully-fleshed out ideas yet,
but these are some of the items on our radar:

* We ought to move to TypedArrays in favor of Buffers.  Buffers will
  continue to work, but since TypedArray is a JavaScript native, it
  makes sense to move towards that as the preferred API.
* SSL performance leaves much to be desired at the moment.  Node's
  interface with OpenSSL is somewhat naive and leaves a lot of potential
  optimization on the table.
* The VM module needs massive improvement.  It lacks features required
  to emulate a web browser JavaScript context, which means that it is
  inadequate.
* The Crypto module still uses some very dated APIs.  In 0.8, it can
  accept Buffers for many things (finally!) but it still does not
  present a Node-like streaming interface.

At this point, the scope of Node's feature set is pretty much locked
down.  We may move things around internally for these cleanup tasks, but
as you can see, there are no major new features planned.  We've drawn
our boundaries, and now it's time to continue focusing on improving
stability and performance of the core, so that more innovation can
happen in **your** programs.

And now, for those of you who may be wondering what was added since
v0.7.12, your regularly scheduled release announcement:

## 2012.06.25, Version 0.8.0 (stable)

* V8: upgrade to v3.11.10.10

* npm: Upgrade to 1.1.32

* Deprecate iowatcher (Ben Noordhuis)

* windows: update icon (Bert Belder)

* http: Hush 'MUST NOT have a body' warnings to debug() (isaacs)

* Move blog.nodejs.org content into repository (isaacs)

* Fix #3503: stdin: resume() on pipe(dest) (isaacs)

* crypto: fix error reporting in SetKey() (Fedor Indutny)

* Add --no-deprecation and --trace-deprecation command-line flags (isaacs)

* fs: fix fs.watchFile() (Ben Noordhuis)

* fs: Fix fs.readfile() on pipes (isaacs)

* Rename GYP variable node_use_system_openssl to be consistent (Ryan Dahl)


Source Code: http://nodejs.org/dist/v0.8.0/node-v0.8.0.tar.gz

Macintosh Installer (Universal): http://nodejs.org/dist/v0.8.0/node-v0.8.0.pkg

Windows Installer: http://nodejs.org/dist/v0.8.0/node-v0.8.0-x86.msi

Windows x64 Installer: http://nodejs.org/dist/v0.8.0/x64/node-v0.8.0-x64.msi

Windows x64 Files: http://nodejs.org/dist/v0.8.0/x64/

Other release files: http://nodejs.org/dist/v0.8.0/

Website: http://nodejs.org/docs/v0.8.0/

Documentation: http://nodejs.org/docs/v0.8.0/api/

Shasums:

```
b92208b291ad420025c65661a7df51fc618e21ca  license.rtf
0786bcda79bd651b9981682527a1bbabe0250700  node-v0.8.0-x86.msi
8f160a742a01fdfc1b1423b3fc742d184f1ab70c  node-v0.8.0-x86.wixpdb
6035d6d59304add21e462cd7eb89491570b4970d  node-v0.8.0.pkg
5171fb46fbfee5ac7129c4b17207a3f35a1f57e8  node-v0.8.0.tar.gz
742100a4ee4cd4d190031a30d9b22b2b69b6872e  node.exe
52d20d285e9aec53043af0843f5ecc4153210693  node.exp
6d67a64274d844548cc6099c76181a50feafc233  node.lib
aa2af08d5ab869e6c8b67f01ed67129c1cad8bce  node.pdb
b92208b291ad420025c65661a7df51fc618e21ca  x64/license.rtf
c4d4164d4f78ea68e0e2a85b96f9b355f3b1df8b  x64/node-v0.8.0-x64.msi
df8bb178ee4cb9562d93fe80bbe59b2acf1b9e6b  x64/node-v0.8.0-x64.wixpdb
fc07b475d943f7681e1904d6d7d666b41874a6fa  x64/node.exe
895002806dfb6d5bb141ef0f43cad3b540a4ff6c  x64/node.exp
686c60d5ae5dad7fcffcdc88049c63b2cd23cffc  x64/node.lib
75549cffab0c11107348a66ab0d94d4897bd6a27  x64/node.pdb
```

<ins>Edited by Tim Oxley to provide percentage differences in the
benchmarks.</ins>
