version: 0.7.10
title: Version 0.7.10 (unstable)
author: Isaac Schlueter
date: Mon Jun 11 2012 09:00:25 GMT-0700 (PDT)
status: publish
category: release
slug: version-0-7-10-unstable

<p>2012.06.11, Version 0.7.10 (unstable)

</p>
<p>This is the second-to-last release on the 0.7 branch.  Version 0.8.0
will be released some time next week.  As other even-numbered Node
releases before it, the v0.8.x releases will maintain API and binary
compatibility.

</p>
<p>The major changes are detailed in
<a href="https://github.com/joyent/node/wiki/API-changes-between-v0.6-and-v0.8">https://github.com/joyent/node/wiki/API-changes-between-v0.6-and-v0.8</a>

</p>
<p>Please try out this release.  There will be very few changes between
this and the v0.8.x release family.  This is the last chance to comment
on the API before it is locked down for stability.


</p>
<ul>
<li><p>Roll V8 back to 3.9.24.31</p>
</li>
<li><p>build: x64 target should always pass -m64 (Robert Mustacchi)</p>
</li>
<li><p>add NODE_EXTERN to node::Start (Joel Brandt)</p>
</li>
<li><p>repl: Warn about running npm commands (isaacs)</p>
</li>
<li><p>slab_allocator: fix crash in dtor if V8 is dead (Ben Noordhuis)</p>
</li>
<li><p>slab_allocator: fix leak of Persistent handles (Shigeki Ohtsu)</p>
</li>
<li><p>windows/msi: add node.js prompt to startmenu (Jeroen Janssen)</p>
</li>
<li><p>windows/msi: fix adding node to PATH (Jeroen Janssen)</p>
</li>
<li><p>windows/msi: add start menu links when installing (Jeroen Janssen)</p>
</li>
<li><p>windows: don&#39;t install x64 version into the &#39;program files (x86)&#39; folder (Matt Gollob)</p>
</li>
<li><p>domain: Fix #3379 domain.intercept no longer passes error arg to cb (Marc Harter)</p>
</li>
<li><p>fs: make callbacks run in global context (Ben Noordhuis)</p>
</li>
<li><p>fs: enable fs.realpath on windows (isaacs)</p>
</li>
<li><p>child_process: expose UV_PROCESS_DETACHED as options.detached (Charlie McConnell)</p>
</li>
<li><p>child_process: new stdio API for .spawn() method (Fedor Indutny)</p>
</li>
<li><p>child_process: spawn().ref() and spawn().unref() (Fedor Indutny)</p>
</li>
<li><p>Upgrade npm to 1.1.25</p>
</li>
<ul><li>Enable npm link on windows</li>
<li>Properly remove sh-shim on Windows</li>
<li>Abstract out registry client and logger</li></ul>
</ul>
<p>Source Code: <a href="http://nodejs.org/dist/v0.7.10/node-v0.7.10.tar.gz">http://nodejs.org/dist/v0.7.10/node-v0.7.10.tar.gz</a>

</p>
<p>Windows Installer: <a href="http://nodejs.org/dist/v0.7.10/node-v0.7.10.msi">http://nodejs.org/dist/v0.7.10/node-v0.7.10.msi</a>

</p>
<p>Windows x64 Files: <a href="http://nodejs.org/dist/v0.7.10/x64/">http://nodejs.org/dist/v0.7.10/x64/</a>

</p>
<p>Macintosh Installer (Universal): <a href="http://nodejs.org/dist/v0.7.10/node-v0.7.10.pkg">http://nodejs.org/dist/v0.7.10/node-v0.7.10.pkg</a>

</p>
<p>Other release files: <a href="http://nodejs.org/dist/v0.7.10/">http://nodejs.org/dist/v0.7.10/</a>

</p>
<p>Website: <a href="http://nodejs.org/docs/v0.7.10/">http://nodejs.org/docs/v0.7.10/</a>

</p>
<p>Documentation: <a href="http://nodejs.org/docs/v0.7.10/api/">http://nodejs.org/docs/v0.7.10/api/</a>
</p>
