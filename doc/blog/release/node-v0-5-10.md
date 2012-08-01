version: 0.5.10
title: Node v0.5.10
author: ryandahl
date: Fri Oct 21 2011 19:12:31 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-5-10

2011.10.21, Version 0.5.10 (unstable)
<ul><li>Remove cmake build system, support for Cygwin, legacy code base, process.ENV, process.ARGV, process.memoryUsage().vsize, os.openOSHandle</li>
<li>Documentation improvments (Igor Zinkovsky, Bert Belder, Ilya Dmitrichenko, koichik, Maciej Małecki, Guglielmo Ferri, isaacs)</li>
<li>Performance improvements (Daniel Ennis, Bert Belder, Ben Noordhuis) </li>
<li>Long process.title support (Ben Noordhuis)</li>
<li>net: register net.Server callback only once (Simen Brekken)</li>
<li>net: fix connect queue bugs (Ben Noordhuis)</li>
<li>debugger: fix backtrace err handling (Fedor Indutny)</li>
<li>Use getaddrinfo instead of c-ares for dns.lookup</li>
<li>Emit 'end' from crypto streams on close</li>
<li>repl: print out `undefined` (Nathan Rajlich)</li>
<li>#1902 buffer: use NO_NULL_TERMINATION flag (koichik)</li>
<li>#1907 http: Added support for HTTP PATCH verb (Thomas Parslow)</li>
<li>#1644 add GetCPUInfo on windows (Karl Skomski)</li>
<li>#1484, #1834, #1482, #771 Don't use a separate context for the repl.  (isaacs)</li>
<li>#1882 zlib Update 'availOutBefore' value, and test (isaacs)</li>
<li>#1888 child_process.fork: don't modify args (koichik)</li>
<li>#1516 tls: requestCert unusable with Firefox and Chrome (koichik)</li>
<li>#1467 tls: The TLS API is inconsistent with the TCP API (koichik)</li>
<li>#1894 net: fix error handling in listen() (koichik)</li>
<li>#1860 console.error now goes through uv_tty_t</li>
<li>Upgrade V8 to 3.7.0</li>
<li>Upgrade GYP to r1081</li></ul>



Download: <a href="http://nodejs.org/dist/v0.5.10/node-v0.5.10.tar.gz">http://nodejs.org/dist/v0.5.10/node-v0.5.10.tar.gz</a>

Windows Executable: <a href="http://nodejs.org/dist/v0.5.10/node.exe">http://nodejs.org/dist/v0.5.10/node.exe</a>

Website: <a href="http://nodejs.org/docs/v0.5.10/">http://nodejs.org/docs/v0.5.10/</a>

Documentation: <a href="http://nodejs.org/docs/v0.5.10/api/">http://nodejs.org/docs/v0.5.10/api/</a>
