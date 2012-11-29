title: libuv status report
author: ryandahl
date: Fri Sep 23 2011 12:45:50 GMT-0700 (PDT)
status: publish
category: Uncategorized
slug: libuv-status-report

We <a href="http://blog.nodejs.org/2011/06/23/porting-node-to-windows-with-microsoft%E2%80%99s-help/">announced</a> back in July that with Microsoft's support Joyent would be porting Node to Windows. This effort is ongoing but I thought it would be nice to make a status report post about the new platform library <code><a href="https://github.com/joyent/libuv">libuv</a></code> which has resulted from porting Node to Windows.

<code>libuv</code>'s purpose is to abstract platform-dependent code in Node into one place where it can be tested for correctness and performance before bindings to V8 are added. Since Node is totally non-blocking, <code>libuv</code> turns out to be a rather useful library itself: a BSD-licensed, minimal, high-performance, cross-platform networking library.

We attempt to not reinvent the wheel where possible. The entire Unix backend sits heavily on Marc Lehmann's beautiful libraries <a href="http://software.schmorp.de/pkg/libev.html">libev</a> and <a href="http://software.schmorp.de/pkg/libeio.html">libeio</a>. For DNS we integrated with Daniel Stenberg's <a href="http://c-ares.haxx.se/">C-Ares</a>. For cross-platform build-system support we're relying on Chrome's <a href="http://code.google.com/p/gyp/">GYP</a> meta-build system.

The current implemented features are:
<ul>
	<li>Non-blocking TCP sockets (using IOCP on Windows)</li>
	<li>Non-blocking named pipes</li>
	<li>UDP</li>
	<li>Timers</li>
	<li>Child process spawning</li>
	<li>Asynchronous DNS via <a href="http://c-ares.haxx.se/">c-ares</a> or <code>uv_getaddrinfo</code>.</li>
	<li>Asynchronous file system APIs <code>uv_fs_*</code></li>
	<li>High resolution time <code>uv_hrtime</code></li>
	<li>Current executable path look up <code>uv_exepath</code></li>
	<li>Thread pool scheduling <code>uv_queue_work</code></li>
</ul>
The features we are working on still are
<ul>
	<li>File system events (Currently supports inotify, <code>ReadDirectoryChangesW</code> and will support kqueue and event ports in the near future.) <code>uv_fs_event_t</code></li>
	<li>VT100 TTY <code>uv_tty_t</code></li>
	<li>Socket sharing between processes <code>uv_ipc_t (<a href="https://gist.github.com/1233593">planned API</a>)</code></li>
</ul>
For complete documentation see the header file: <a href="https://github.com/joyent/libuv/blob/03d0c57ea216abd611286ff1e58d4e344a459f76/include/uv.h">include/uv.h</a>. There are a number of tests in <a href="https://github.com/joyent/libuv/tree/3ca382be741ec6ce6a001f0db04d6375af8cd642/test">the test directory</a> which demonstrate the API.

<code>libuv</code> supports Microsoft Windows operating systems since Windows XP SP2. It can be built with either Visual Studio or MinGW. Solaris 121 and later using GCC toolchain. Linux 2.6 or better using the GCC toolchain. Macinotsh Darwin using the GCC or XCode toolchain. It is known to work on the BSDs but we do not check the build regularly.

In addition to Node v0.5, a number of projects have begun to use <code>libuv</code>:
<ul>
	<li>Mozilla's <a href="https://github.com/graydon/rust">Rust</a></li>
	<li>Tim Caswell's <a href="https://github.com/creationix/luanode">LuaNode</a></li>
	<li>Ben Noordhuis and Bert Belder's <a href="https://github.com/bnoordhuis/phode">Phode</a> async PHP project</li>
	<li>Kerry Snyder's <a href="https://github.com/kersny/libuv-csharp">libuv-csharp</a></li>
	<li>Andrea Lattuada's <a href="https://gist.github.com/1195428">web server</a></li>
</ul>
We hope to see more people contributing and using <code>libuv</code> in the future!
