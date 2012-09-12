version: 0.7.9
title: Node Version 0.7.9 (unstable)
author: Isaac Schlueter
date: Tue May 29 2012 10:11:45 GMT-0700 (PDT)
status: publish
category: release
slug: node-version-0-7-9-unstable

<p>2012.05.28, Version 0.7.9 (unstable)

</p>
<ul>
<li><p>Upgrade V8 to 3.11.1</p>
</li>
<li><p>Upgrade npm to 1.1.23</p>
</li>
<li><p>uv: rework reference counting scheme (Ben Noordhuis)</p>
</li>
<li><p>uv: add interface for joining external event loops (Bert Belder)</p>
</li>
<li><p>repl, readline: Handle Ctrl+Z and SIGCONT better (Nathan Rajlich)</p>
</li>
<li><p>fs: 64bit offsets for fs calls (Igor Zinkovsky)</p>
</li>
<li><p>fs: add sync open flags &#39;rs&#39; and &#39;rs+&#39; (Kevin Bowman)</p>
</li>
<li><p>windows: enable creating directory junctions with fs.symlink (Igor Zinkovsky, Bert Belder)</p>
</li>
<li><p>windows: fix fs.lstat to properly detect symlinks. (Igor Zinkovsky)</p>
</li>
<li><p>Fix #3270 Escape url.parse delims (isaacs)</p>
</li>
<li><p>http: make http.get() accept a URL (Adam Malcontenti-Wilson)</p>
</li>
<li><p>Cleanup vm module memory leakage (Marcel Laverdet)</p>
</li>
<li><p>Optimize writing strings with Socket.write (Bert Belder)</p>
</li>
<li><p>add support for CESU-8 and UTF-16LE encodings (koichik)</p>
</li>
<li><p>path: add path.sep to get the path separator. (Yi, EungJun)</p>
</li>
<li><p>net, http: add backlog parameter to .listen() (Erik Dubbelboer)</p>
</li>
<li><p>debugger: support mirroring Date objects (Fedor Indutny)</p>
</li>
<li><p>addon: add AtExit() function (Ben Noordhuis)</p>
</li>
<li><p>net: signal localAddress bind failure in connect (Brian Schroeder)</p>
</li>
<li><p>util: handle non-string return value in .inspect() (Alex Kocharin)</p>
</li>
</ul>
<p>Source Code: <a href="http://nodejs.org/dist/v0.7.9/node-v0.7.9.tar.gz">http://nodejs.org/dist/v0.7.9/node-v0.7.9.tar.gz</a>

</p>
<p>Windows Installer: <a href="http://nodejs.org/dist/v0.7.9/node-v0.7.9.msi">http://nodejs.org/dist/v0.7.9/node-v0.7.9.msi</a>

</p>
<p>Windows x64 Files: <a href="http://nodejs.org/dist/v0.7.9/x64/">http://nodejs.org/dist/v0.7.9/x64/</a>

</p>
<p>Macintosh Installer (Universal): <a href="http://nodejs.org/dist/v0.7.9/node-v0.7.9.pkg">http://nodejs.org/dist/v0.7.9/node-v0.7.9.pkg</a>

</p>
<p>Other release files: <a href="http://nodejs.org/dist/v0.7.9/">http://nodejs.org/dist/v0.7.9/</a>

</p>
<p>Website: <a href="http://nodejs.org/docs/v0.7.9/">http://nodejs.org/docs/v0.7.9/</a>

</p>
<p>Documentation: <a href="http://nodejs.org/docs/v0.7.9/api/">http://nodejs.org/docs/v0.7.9/api/</a>
</p>
