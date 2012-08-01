version: 0.4.3
title: Node v0.4.3
author: ryandahl
date: Fri Mar 18 2011 22:17:59 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-4-3

2011.03.18, Version 0.4.3 (stable)
<ul>
<li> Don't decrease server connection counter again if destroy() is called more  than once GH-431 (Andreas Reich, Anders Conbere)
<li> Documentation improvements (koichik)
<li> Fix bug with setMaxListeners GH-682
<li> Start up memory footprint improvement. (Tom Hughes)
<li> Solaris improvements.
<li> Buffer::Length(Buffer*) should not invoke itself recursively GH-759 (Ben Noordhuis)
<li> TLS: Advertise support for client certs GH-774 (Theo Schlossnagle)
<li> HTTP Agent bugs: GH-787, GH-784, GH-803.
<li> Don't call GetMemoryUsage every 5 seconds.
<li> Upgrade V8 to 3.1.8.3
</ul>



Download: http://nodejs.org/dist/node-v0.4.3.tar.gz

Website: http://nodejs.org/docs/v0.4.3/

Documentation: http://nodejs.org/docs/v0.4.3/api

<a href="https://groups.google.com/d/topic/nodejs/JrYQCQtf6lM/discussion">Announcement</a>

<a href="https://github.com/joyent/node/tree/v0.4.3">commit</a>
