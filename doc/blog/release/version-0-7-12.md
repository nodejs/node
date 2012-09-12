version: 0.7.12
title: Version 0.7.12
author: Isaac Schlueter
date: Tue Jun 19 2012 16:31:09 GMT-0700 (PDT)
status: publish
category: release
slug: version-0-7-12

<p>2012.06.19, Version 0.7.12 (unstable)  </p>
<p>This is the last release on the 0.7 branch.  Version 0.8.0 will be released some time later this week, barring any major problems.  </p>
<p>As with other even-numbered Node releases before it, the v0.8.x releases will maintain API and binary compatibility.  </p>
<p>The major changes between v0.6 and v0.8 are detailed in <a href="https://github.com/joyent/node/wiki/API-changes-between-v0.6-and-v0.8">https://github.com/joyent/node/wiki/API-changes-between-v0.6-and-v0.8</a>  </p>
<p>Please try out this release.  There will be very virtually no changes between this and the v0.8.x release family.  This is the last chance to comment before it is locked down for stability.  The API is effectively frozen now.  </p>
<p>This version adds backwards-compatible shims for binary addons that use libeio and libev directly.  If you find that binary modules that could compile on v0.6 can not compile on this version, please let us know. Note that libev is officially deprecated in v0.8, and will be removed in v0.9.  You should be porting your modules to use libuv as soon as possible.  </p>
<p>V8 is on 3.11.10 currently, and will remain on the V8 3.11.x branch for the duration of Node v0.8.x.   </p>
<ul>   <li><p>npm: Upgrade to 1.1.30<br> - Improved &#39;npm init&#39;<br> - Fix the &#39;cb never called&#39; error from &#39;oudated&#39; and &#39;update&#39;<br> - Add --save-bundle|-B config<br> - Fix isaacs/npm#2465: Make npm script and windows shims cygwin-aware<br> - Fix isaacs/npm#2452 Use --save(-dev|-optional) in npm rm<br> - <code>logstream</code> option to replace removed <code>logfd</code> (Rod Vagg)<br> - Read default descriptions from README.md files </p>
  </li> <li><p>Shims to support deprecated <code>ev_*</code> and <code>eio_*</code> methods (Ben Noordhuis)</p>
  </li> <li><p>#3118 net.Socket: Delay pause/resume until after connect (isaacs)</p>
  </li> <li><p>#3465 Add ./configure --no-ifaddrs flag (isaacs)</p>
  </li> <li><p>child_process: add .stdin stream to forks (Fedor Indutny)</p>
  </li> <li><p>build: fix <code>make install DESTDIR=/path</code> (Ben Noordhuis)</p>
  </li> <li><p>tls: fix off-by-one error in renegotiation check (Ben Noordhuis)</p>
  </li> <li><p>crypto: Fix diffie-hellman key generation UTF-8 errors (Fedor Indutny)</p>
  </li> <li><p>node: change the constructor name of process from EventEmitter to process (Andreas Madsen)</p>
  </li> <li><p>net: Prevent property access throws during close (Reid Burke)</p>
  </li> <li><p>querystring: improved speed and code cleanup (Felix Böhm)</p>
  </li> <li><p>sunos: fix assertion errors breaking fs.watch() (Fedor Indutny)</p>
  </li> <li><p>unix: stat: detect sub-second changes (Ben Noordhuis)</p>
  </li> <li><p>add stat() based file watcher (Ben Noordhuis)</p>
  </li> </ul> <p>Source Code: <a href="http://nodejs.org/dist/v0.7.12/node-v0.7.12.tar.gz">http://nodejs.org/dist/v0.7.12/node-v0.7.12.tar.gz</a>  </p>
  <p>Macintosh Installer (Universal): <a href="http://nodejs.org/dist/v0.7.12/node-v0.7.12.pkg">http://nodejs.org/dist/v0.7.12/node-v0.7.12.pkg</a>  </p>
  <p>Windows Installer: <a href="http://nodejs.org/dist/v0.7.12/node-v0.7.12-x86.msi">http://nodejs.org/dist/v0.7.12/node-v0.7.12-x86.msi</a>  </p>
  <p>Windows x64 Installer: <a href="http://nodejs.org/dist/v0.7.12/x64/node-v0.7.12-x64.msi">http://nodejs.org/dist/v0.7.12/x64/node-v0.7.12-x64.msi</a>  </p>
  <p>Windows x64 Files: <a href="http://nodejs.org/dist/v0.7.12/x64/">http://nodejs.org/dist/v0.7.12/x64/</a>  </p>
  <p>Other release files: <a href="http://nodejs.org/dist/v0.7.12/">http://nodejs.org/dist/v0.7.12/</a>  </p>
  <p>Website: <a href="http://nodejs.org/docs/v0.7.12/">http://nodejs.org/docs/v0.7.12/</a>  </p>
  <p>Documentation: <a href="http://nodejs.org/docs/v0.7.12/api/">http://nodejs.org/docs/v0.7.12/api/</a> </p>

<h2>Shasums</h2>

<pre>ded6a2197b1149b594eb45fea921e8538ba442aa  blog.html
dfabff0923d4b4f1d53bd9831514c1ac8c4b1876  email.md
be313d35511e6d7e43cae5fa2b18f3e0d2275ba1  node-v0.7.12-x86.msi
8f7ba0c8283e3863de32fd5c034f5b599c78f830  node-v0.7.12.pkg
cb570abacbf4eb7e23c3d2620d00dd3080d9c19d  node-v0.7.12.tar.gz
e13a6edfcba1c67ffe794982dd20a222ce8ce40f  node.exe
20906ad76a43eca52795b2a089654a105e11c1e6  node.exp
acbcbb87b6f7f2af34a3ed0abe6131d9e7a237af  node.lib
4013d5b25fe36ae4245433b972818686cd9a2205  node.pdb
6c0a7a2e8ee360e2800156293fb2f6a5c1a57382  npm-1.1.30.tgz
9d23e42e8260fa20001d5618d2583a62792bf63f  npm-1.1.30.zip
840157b2d6f7389ba70b07fc9ddc048b92c501cc  x64/node-v0.7.12-x64.msi
862d42706c21ea83bf73cd827101f0fe598b0cf7  x64/node.exe
de0af95fac083762f99c875f91bab830dc030f71  x64/node.exp
3122bd886dfb96f3b41846cef0bdd7e97326044a  x64/node.lib
e0fa4e42cd19cadf8179e492ca606b7232bbc018  x64/node.pdb</pre>
