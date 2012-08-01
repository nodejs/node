version: 0.5.3
title: Node v0.5.3
author: ryandahl
date: Tue Aug 02 2011 08:03:06 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-5-3

2011.08.01, Version 0.5.3 (unstable)

<ul><li>Fix crypto encryption/decryption with Base64. (SAWADA Tadashi)

<li>#243 Add an optional length argument to Buffer.write() (koichik)

<li>#657 convert nonbuffer data to string in fs.writeFile/Sync (Daniel Pihlström)

<li>Add process.features, remove process.useUV (Ben Noordhuis)

<li>#324 Fix crypto hmac to accept binary keys + add test cases from rfc 2202 and 4231 (Stefan Bühler)

<li>Add Socket::bytesRead, Socket::bytesWritten (Alexander Uvarov)

<li>#572 Don't print result of --eval in CLI (Ben Noordhuis)

<li>#1223 Fix http.ClientRequest crashes if end() was called twice (koichik)

<li>#1383 Emit 'close' after all connections have closed (Felix Geisendörfer)

<li>Add sprintf-like util.format() function (Ben Noordhuis)

<li>Add support for TLS SNI (Fedor Indutny)

<li>New http agent implementation. Off by default the command line flag <code>--use-http2</code> will enable it. <code>make test-http2</code> will run the tests for the new implementation. (Mikeal Rogers)

<li>Revert AMD compatibility. (isaacs)

<li>Windows: improvements, child_process support.

<li>Remove pkg-config file.

<li>Fix startup time regressions.

<li>doc improvements</ul>



Download: <a href="http://nodejs.org/dist/v0.5.3/node-v0.5.3.tar.gz">http://nodejs.org/dist/v0.5.3/node-v0.5.3.tar.gz</a>

Windows Executable: <a href="http://nodejs.org/dist/v0.5.3/node.exe">http://nodejs.org/dist/v0.5.3/node.exe</a>

Website: <a href="http://nodejs.org/dist/v0.5.3/docs">http://nodejs.org/dist/v0.5.3/docs</a>

Documentation: <a href="http://nodejs.org/dist/v0.5.3/docs/api">http://nodejs.org/dist/v0.5.3/docs/api</a>
