version: 0.5.7
title: Node v0.5.7 (unstable)
author: ryandahl
date: Fri Sep 16 2011 18:57:03 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-5-7-unstable

2011.09.16, Version 0.5.7 (unstable)
<ul>
<li>Upgrade V8 to 3.6.4
<li>Improve Windows compatibility
<li>Documentation improvements
<li>Debugger and REPL improvements (Fedor Indutny)
<li>Add legacy API support: net.Stream(fd), process.stdout.writable, process.stdout.fd
<li>Fix mkdir EEXIST handling (isaacs)
<li>Use net_uv instead of net_legacy for stdio
<li>Do not load readline from util.inspect
<li>#1673 Fix bug related to V8 context with accessors (Fedor Indutny)
<li>#1634 util: Fix inspection for Error (koichik)
<li>#1645 fs: Add positioned file writing feature to fs.WriteStream (Thomas Shinnick)
<li>#1637 fs: Unguarded fs.watchFile cache statWatchers checking fixed (Thomas Shinnick)
<li>#1695 Forward customFds to ChildProcess.spawn
<li>#1707 Fix hasOwnProperty security problem in querystring (isaacs)
<li>#1719 Drain OpenSSL error queue</ul>



Download: <a href="http://nodejs.org/dist/v0.5.7/node-v0.5.7.tar.gz">http://nodejs.org/dist/v0.5.7/node-v0.5.7.tar.gz</a>

Windows Executable: <a href="http://nodejs.org/dist/v0.5.7/node.exe">http://nodejs.org/dist/v0.5.7/node.exe</a>

Website: <a href="http://nodejs.org/docs/v0.5.7/">http://nodejs.org/docs/v0.5.7/</a>

Documentation: <a href="http://nodejs.org/docs/v0.5.7/api/">http://nodejs.org/docs/v0.5.7/api/</a>
