version: 0.5.6
title: Node v0.5.6 (unstable)
author: piscisaureus
date: Fri Sep 09 2011 16:30:39 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-5-6

2011.09.08, Version 0.5.6 (unstable)
<ul>
	<li>#345, #1635, #1648 Documentation improvements (Thomas Shinnick, Abimanyu Raja, AJ ONeal, Koichi Kobayashi, Michael Jackson, Logan Smyth, Ben Noordhuis)</li>
	<li>#650 Improve path parsing on windows (Bert Belder)</li>
	<li>#752 Remove headers sent check in OutgoingMessage.getHeader() (Peter Lyons)</li>
	<li>#1236, #1438, #1506, #1513, #1621, #1640, #1647 Libuv-related bugs fixed (Jorge Chamorro Bieling, Peter Bright, Luis Lavena, Igor Zinkovsky)</li>
	<li>#1296, #1612 crypto: Fix BIO's usage. (Koichi Kobayashi)</li>
	<li>#1345 Correctly set socket.remoteAddress with libuv backend (Bert Belder)</li>
	<li>#1429 Don't clobber quick edit mode on windows (Peter Bright)</li>
	<li>#1503 Make libuv backend default on unix, override with `node --use-legacy`</li>
	<li>#1565 Fix fs.stat for paths ending with \ on windows (Igor Zinkovsky)</li>
	<li>#1568 Fix x509 certificate subject parsing (Koichi Kobayashi)</li>
	<li>#1586 Make socket write encoding case-insensitive (Koichi Kobayashi)</li>
	<li>#1591, #1656, #1657 Implement fs in libuv, remove libeio and pthread-win32 dependency on windows (Igor Zinkovsky, Ben Noordhuis, Ryan Dahl, Isaac Schlueter)</li>
	<li>#1592 Don't load-time link against CreateSymbolicLink on windows (Peter Bright)</li>
	<li>#1601 Improve API consistency when dealing with the socket underlying a HTTP client request (Mikeal Rogers)</li>
	<li>#1610 Remove DigiNotar CA from trusted list (Isaac Schlueter)</li>
	<li>#1617 Added some win32 os functions (Karl Skomski)</li>
	<li>#1624 avoid buffer overrun with 'binary' encoding (Koichi Kobayashi)</li>
	<li>#1633 make Buffer.write() always set _charsWritten (Koichi Kobayashi)</li>
	<li>#1644 Windows: set executables to be console programs (Peter Bright)</li>
	<li>#1651 improve inspection for sparse array (Koichi Kobayashi)</li>
	<li>#1672 set .code='ECONNRESET' on socket hang up errors (Ben Noordhuis)</li>
	<li>Add test case for foaf+ssl client certificate (Niclas Hoyer)</li>
	<li>Added RPATH environment variable to override run-time library paths (Ashok Mudukutore)</li>
	<li>Added TLS client-side session resumption support (Sean Cunningham)</li>
	<li>Added additional properties to getPeerCertificate (Nathan Rixham, Niclas Hoyer)</li>
	<li>Don't eval repl command twice when an error is thrown (Nathan Rajlich)</li>
	<li>Improve util.isDate() (Nathan Rajlich)</li>
	<li>Improvements in libuv backend and bindings, upgrade libuv to bd6066cb349a9b3a1b0d87b146ddaee06db31d10</li>
	<li>Show warning when using lib/sys.js (Maciej Malecki)</li>
	<li>Support plus sign in url protocol (Maciej Malecki)</li>
	<li>Upgrade V8 to 3.6.2</li>
</ul>
Download: <a href="http://nodejs.org/dist/v0.5.6/node-v0.5.6.tar.gz">http://nodejs.org/dist/v0.5.6/node-v0.5.6.tar.gz</a>

Windows Executable: <a href="http://nodejs.org/dist/v0.5.6/node.exe">http://nodejs.org/dist/v0.5.6/node.exe</a>

Website: <a href="http://nodejs.org/docs/v0.5.6/">http://nodejs.org/docs/v0.5.6/</a>

Documentation: <a href="http://nodejs.org/docs/v0.5.6/api/">http://nodejs.org/docs/v0.5.6/api/</a>
