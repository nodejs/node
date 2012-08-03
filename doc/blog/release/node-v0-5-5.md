version: 0.5.5
title: Node v0.5.5
author: bennoordhuis
date: Fri Aug 26 2011 23:20:10 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-5-5

<p>2011.08.26, Version 0.5.5 (unstable)</p>
<ul>
<li>typed arrays, implementation from Plesk
<li>fix IP multicast on SunOS
<li>fix DNS lookup order: IPv4 first, IPv6 second (--use-uv only)
<li>remove support for UNIX datagram sockets (--use-uv only)
<li>UDP support for Windows (Bert Belder)
<li>#1572 improve tab completion for objects in the REPL (Nathan Rajlich)
<li>#1563 fix buffer overflow in child_process module (reported by Dean McNamee)
<li>#1546 fix performance regression in http module (reported by Brian Geffon)
<li>#1491 add PBKDF2 crypto support (Glen Low)
<li>#1447 remove deprecated http.cat() function (Mikeal Rogers)
<li>#1140 fix incorrect dispatch of vm.runInContext's filename argument<br />
  (Antranig Basman)</p>
<li>#1140 document vm.runInContext() and vm.createContext() (Antranig Basman)
<li>#1428 fix os.freemem() on 64 bits freebsd (Artem Zaytsev)
<li>#1164 make all DNS lookups async, fixes uncatchable exceptions<br />
  (Koichi Kobayashi)</p>
<li>fix incorrect ssl shutdown check (Tom Hughes)
<li>various cmake fixes (Tom Hughes)
<li>improved documentation (Koichi Kobayashi, Logan Smyth, Fedor Indutny,<br />
  Mikeal Rogers, Maciej Małecki, Antranig Basman, Mickaël Delahaye)</p>
<li>upgrade libuv to commit 835782a
<li>upgrade V8 to 3.5.8
</ul>
<p>Download: <a href="http://nodejs.org/dist/node-v0.5.5.tar.gz">http://nodejs.org/dist/node-v0.5.5.tar.gz</a></p>
<p>Windows Executable: <a href="http://nodejs.org/dist/v0.5.5/node.exe">http://nodejs.org/dist/v0.5.5/node.exe</a></p>
<p>Website: <a href="http://nodejs.org/docs/v0.5.5/">http://nodejs.org/docs/v0.5.5/</a></p>
<p>Documentation: <a href="http://nodejs.org/docs/v0.5.5/api/">http://nodejs.org/docs/v0.5.5/api/</a></p>
<br /><br />

<b>Update:</b> The <code>.exe</code> has a bug that results in incompatibility with Windows XP and Server 2003. This has been reported in <a href="https://github.com/joyent/node/issues/1592">issue #1592</a> and fixed. A new binary was made that is compatibile with the older Windows: <a href="http://nodejs.org/dist/v0.5.5/node-186364e.exe">http://nodejs.org/dist/v0.5.5/node-186364e.exe</a>.
