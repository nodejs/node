version: 0.6.7
title: Node v0.6.7
author: Isaac Schlueter
date: Fri Jan 06 2012 17:54:49 GMT-0800 (PST)
status: publish
category: release
slug: node-v0-6-7

<p>2012.01.06, Version 0.6.7 (stable)</p>

<ul>
<li><p>V8 hash collision fix (Breaks MIPS) (Bert Belder, Erik Corry)</p></li>
<li><p>Upgrade V8 to 3.6.6.15</p></li>
<li><p>Upgrade npm to 1.1.0-beta-10 (isaacs)</p></li>
<li><p>many doc updates (Ben Noordhuis, Jeremy Martin, koichik, Dave Irvine,
Seong-Rak Choi, Shannen, Adam Malcontenti-Wilson, koichik)</p></li>
<li><p>Fix segfault in <code>node_http_parser.cc</code></p></li>
<li><p>dgram, timers: fix memory leaks (Ben Noordhuis, Yoshihiro Kikuchi)</p></li>
<li><p>repl: fix repl.start not passing the <code>ignoreUndefined</code> arg (Damon Oehlman)</p></li>
<li><p>#1980: Socket.pause null reference when called on a closed Stream (koichik)</p></li>
<li><p>#2263: XMLHttpRequest piped in a writable file stream hang (koichik)</p></li>
<li><p>#2069: http resource leak (koichik)</p></li>
<li><p>buffer.readInt global pollution fix (Phil Sung)</p></li>
<li><p>timers: fix performance regression (Ben Noordhuis)</p></li>
<li><p>#2308, #2246: node swallows openssl error on request (koichik)</p></li>
<li><p>#2114: timers: remove _idleTimeout from item in .unenroll() (James Hartig)</p></li>
<li><p>#2379: debugger: Request backtrace w/o refs (Fedor Indutny)</p></li>
<li><p>simple DTrace ustack helper (Dave Pacheco)</p></li>
<li><p>crypto: rewrite HexDecode without snprintf (Roman Shtylman)</p></li>
<li><p>crypto: don&#8217;t ignore DH init errors (Ben Noordhuis)</p></li>
</ul>

<p>Source Code: <a href="http://nodejs.org/dist/v0.6.7/node-v0.6.7.tar.gz">http://nodejs.org/dist/v0.6.7/node-v0.6.7.tar.gz</a></p>

<p>Windows Installer: <a href="http://nodejs.org/dist/v0.6.7/node-v0.6.7.msi">http://nodejs.org/dist/v0.6.7/node-v0.6.7.msi</a></p>

<p>Macintosh Installer: <a href="http://nodejs.org/dist/v0.6.7/node-v0.6.7.pkg">http://nodejs.org/dist/v0.6.7/node-v0.6.7.pkg</a></p>

<p>Website: <a href="http://nodejs.org/docs/v0.6.7/">http://nodejs.org/docs/v0.6.7/</a></p>

<p>Documentation: <a href="http://nodejs.org/docs/v0.6.7/api/">http://nodejs.org/docs/v0.6.7/api/</a></p>
