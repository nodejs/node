version: 0.5.0
title: Node v0.5.0 (Unstable)
author: ryandahl
date: Wed Jul 06 2011 02:23:17 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-5-0-unstable

2011.07.05, Version 0.5.0 (unstable)

<li> New non-default libuv backend to support IOCP on Windows. Use <code>--use-uv</code> to enable.
<li> deprecate http.cat
<li> docs improved.
<li> add child_process.fork
<li> add fs.utimes() and fs.futimes() support (Ben Noordhuis)
<li> add process.uptime() (Tom Huges)
<li> add path.relative (Tony Huang)
<li> add os.getNetworkInterfaces()
<li> add remoteAddress and remotePort for client TCP connections (Brian White)
<li> add secureOptions flag, setting ciphers, SSL_OP_CRYPTOPRO_TLSEXT_BUG to TLS (Theo Schlossnagle)
<li> add process.arch (Nathan Rajlich)
<li> add reading/writing of floats and doubles from/to buffers (Brian White)
<li> Allow script to be read from stdin
<li> #477 add Buffer::fill method to do memset (Konstantin Käfer)
<li> #573 Diffie-Hellman support to crypto module (Håvard Stranden)
<li> #695 add 'hex' encoding to buffer (isaacs)
<li> #851 Update how REPLServer uses contexts (Ben Weaver)
<li> #853 add fs.lchow, fs.lchmod, fs.fchmod, fs.fchown (isaacs)
<li> #889 Allow to remove all EventEmitter listeners at once (Felix Geisendörfer)
<li> #926 OpenSSL NPN support (Fedor Indutny)
<li> #955 Change ^C handling in REPL (isaacs)
<li> #979 add support for Unix Domain Sockets to HTTP (Mark Cavage)
<li> #1173 #1170 add AMD, asynchronous module definition (isaacs)
<li> DTrace probes: support X-Forwarded-For (Dave Pacheco) </ul>
Download: <a href="http://nodejs.org/dist/node-v0.5.0.tar.gz">http://nodejs.org/dist/node-v0.5.0.tar.gz</a>

Website: <a href="http://nodejs.org/docs/v0.5.0/">http://nodejs.org/docs/v0.5.0/</a>

Documentation: <a href="http://nodejs.org/docs/v0.5.0/api/">http://nodejs.org/docs/v0.5.0/api/</a>
