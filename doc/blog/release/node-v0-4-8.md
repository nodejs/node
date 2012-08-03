version: 0.4.8
title: Node v0.4.8
author: ryandahl
date: Sat May 21 2011 07:06:00 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-4-8

2011.05.20, Version 0.4.8 (stable)

* #974 Properly report traceless errors (isaacs)

* #983 Better JSON.parse error detection in REPL (isaacs)

* #836 Agent socket errors bubble up to req only if req exists

* #1041 Fix event listener leak check timing (koichik)

* #1038 Fix dns.resolve() with 'PTR' throws Error: Unknown type "PTR"
  (koichik)

* #1073 Share SSL context between server connections (Fedor Indutny)

* Disable compression with OpenSSL. Improves memory perf.

* Implement os.totalmem() and os.freemem() for SunOS (Alexandre Marangone)

* Fix a special characters in URL regression (isaacs)

* Fix idle timeouts in HTTPS (Felix Geisendörfer)

* SlowBuffer.write() with 'ucs2' throws ReferenceError. (koichik)

* http.ServerRequest 'close' sometimes gets an error argument
  (Felix Geisendörfer)

* Doc improvements

* cleartextstream.destroy() should close(2) the socket. Previously was being
  mapped to a shutdown(2) syscall.

* No longer compile out asserts and debug statements in normal build.

* Debugger improvements.

* Upgrade V8 to 3.1.8.16.




Website: <a href="http://nodejs.org/docs/v0.4.8/">http://nodejs.org/docs/v0.4.8/</a>

Download: <a href="http://nodejs.org/dist/node-v0.4.8.tar.gz">http://nodejs.org/dist/node-v0.4.8.tar.gz</a>

Documentation: <a href="http://nodejs.org/docs/v0.4.8/api/">http://nodejs.org/docs/v0.4.8/api/</a>
