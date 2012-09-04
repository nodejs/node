version: 0.6.11
title: Version 0.6.11 (stable)
author: Isaac Schlueter
date: Fri Feb 17 2012 13:32:55 GMT-0800 (PST)
status: publish
category: release
slug: version-0-6-11-stable

<p>2012.02.17 Version 0.6.11 (stable)

</p>
<ul>
<li><p>http: allow multiple WebSocket RFC6455 headers (Einar Otto Stangvik)</p>
</li>
<li><p>http: allow multiple WWW-Authenticate headers (Ben Noordhuis)</p>
</li>
<li><p>windows: support unicode argv and environment variables (Bert Belder)</p>
</li>
<li><p>tls: mitigate session renegotiation attacks (Ben Noordhuis)</p>
</li>
<li><p>tcp, pipe: don&#39;t assert on uv_accept() errors (Ben Noordhuis)</p>
</li>
<li><p>tls: Allow establishing secure connection on the existing socket (koichik)</p>
</li>
<li><p>dgram: handle close of dgram socket before DNS lookup completes (Seth Fitzsimmons)</p>
</li>
<li><p>windows: Support half-duplex pipes (Igor Zinkovsky)</p>
</li>
<li><p>build: disable omit-frame-pointer on solaris systems (Dave Pacheco)</p>
</li>
<li><p>debugger: fix --debug-brk (Ben Noordhuis)</p>
</li>
<li><p>net: fix large file downloads failing (koichik)</p>
</li>
<li><p>fs: fix ReadStream failure to read from existing fd (Christopher Jeffrey)</p>
</li>
<li><p>net: destroy socket on DNS error (Stefan Rusu)</p>
</li>
<li><p>dtrace: add missing translator (Dave Pacheco)</p>
</li>
<li><p>unix: don&#39;t flush tty on switch to raw mode (Ben Noordhuis)</p>
</li>
<li><p>windows: reset brightness when reverting to default text color (Bert Belder)</p>
</li>
<li><p>npm: update to 1.1.1</p>

<p>- Update which, fstream, mkdirp, request, and rimraf<br>- Fix #2123 Set path properly for lifecycle scripts on windows<br>- Mark the root as seen, so we don&#39;t recurse into it. Fixes #1838. (Martin Cooper)</p>

</li>
</ul>
<p>Source Code: <a href="http://nodejs.org/dist/v0.6.11/node-v0.6.11.tar.gz">http://nodejs.org/dist/v0.6.11/node-v0.6.11.tar.gz</a>

</p>
<p>Windows Installer: <a href="http://nodejs.org/dist/v0.6.11/node-v0.6.11.msi">http://nodejs.org/dist/v0.6.11/node-v0.6.11.msi</a>

</p>
<p>Macintosh Installer: <a href="http://nodejs.org/dist/v0.6.11/node-v0.6.11.pkg">http://nodejs.org/dist/v0.6.11/node-v0.6.11.pkg</a>

</p>
<p>Website: <a href="http://nodejs.org/docs/v0.6.11/">http://nodejs.org/docs/v0.6.11/</a>

</p>
<p>Documentation: <a href="http://nodejs.org/docs/v0.6.11/api/">http://nodejs.org/docs/v0.6.11/api/</a>
</p>
