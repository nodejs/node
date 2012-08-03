version: 0.5.9
title: Node v0.5.9
author: ryandahl
date: Mon Oct 10 2011 19:06:21 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-5-9

2011.10.10, Version 0.5.9 (unstable)
<ul><li>fs.watch interface backed by kqueue, inotify, and ReadDirectoryChangesW (Igor Zinkovsky, Ben Noordhuis)</li>
<li>add dns.resolveTxt (Christian Tellnes)</li>
<li>Remove legacy http library (Ben Noordhuis)</li>
<li>child_process.fork returns and works on Windows. Allows passing handles.  (Igor Zinkovsky, Bert Belder)</li>
<li>#1774 Lint and clean up for --harmony_block_scoping (Tyler Larson, Colton Baker)</li>
<li>#1813 Fix ctrl+c on Windows (Bert Belder)</li>
<li>#1844 unbreak --use-legacy (Ben Noordhuis)</li>
<li>process.stderr now goes through libuv. Both process.stdout and process.stderr are blocking when referencing a TTY.</li>
<li>net_uv performance improvements (Ben Noordhuis, Bert Belder)</li></ul>


Download: <a href="http://nodejs.org/dist/v0.5.9/node-v0.5.9.tar.gz">http://nodejs.org/dist/v0.5.9/node-v0.5.9.tar.gz</a>

Windows Executable: <a href="http://nodejs.org/dist/v0.5.9/node.exe">http://nodejs.org/dist/v0.5.9/node.exe</a>

Website: <a href="http://nodejs.org/docs/v0.5.9/">http://nodejs.org/docs/v0.5.9/</a>

Documentation: <a href="http://nodejs.org/docs/v0.5.9/api/">http://nodejs.org/docs/v0.5.9/api/</a>
