version: 0.6.13
title: Version 0.6.13 (stable)
author: Isaac Schlueter
date: Thu Mar 15 2012 10:37:02 GMT-0700 (PDT)
status: publish
category: release
slug: version-0-6-13-stable

<p>2012.03.15 Version 0.6.13 (stable)

</p>
<ul>
<li><p>Windows: Many libuv test fixes (Bert Belder)</p>
</li>
<li><p>Windows: avoid uv_guess_handle crash in when fd &lt; 0 (Bert Belder)</p>
</li>
<li><p>Map EBUSY and ENOTEMPTY errors (Bert Belder)</p>
</li>
<li><p>Windows: include syscall in fs errors (Bert Belder)</p>
</li>
<li><p>Fix fs.watch ENOSYS on Linux kernel version mismatch (Ben Noordhuis)</p>
</li>
<li><p>Update npm to 1.1.9</p>
<p>
- upgrade node-gyp to 0.3.5 (Nathan Rajlich)<br>
- Fix isaacs/npm#2249 Add cache-max and cache-min configs<br>
- Properly redirect across https/http registry requests<br>
- log config usage if undefined key in set function (Kris Windham)<br>
- Add support for os/cpu fields in package.json (Adam Blackburn)<br>
- Automatically node-gyp packages containing a binding.gyp<br>
- Fix failures unpacking in UNC shares<br>
- Never create un-listable directories<br>
- Handle cases where an optionalDependency fails to build
</p>
</li>
</ul>
<p>Source Code: <a href="http://nodejs.org/dist/v0.6.13/node-v0.6.13.tar.gz">http://nodejs.org/dist/v0.6.13/node-v0.6.13.tar.gz</a>

</p>
<p>Windows Installer: <a href="http://nodejs.org/dist/v0.6.13/node-v0.6.13.msi">http://nodejs.org/dist/v0.6.13/node-v0.6.13.msi</a>

</p>
<p>Macintosh Installer: <a href="http://nodejs.org/dist/v0.6.13/node-v0.6.13.pkg">http://nodejs.org/dist/v0.6.13/node-v0.6.13.pkg</a>

</p>
<p>Website: <a href="http://nodejs.org/docs/v0.6.13/">http://nodejs.org/docs/v0.6.13/</a>

</p>
<p>Documentation: <a href="http://nodejs.org/docs/v0.6.13/api/">http://nodejs.org/docs/v0.6.13/api/</a>
</p>
