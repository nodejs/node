version: 0.4.6
title: Node v0.4.6
author: ryandahl
date: Thu Apr 14 2011 05:00:30 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-4-6

2011.04.13, Version 0.4.6 (stable)
<ul><li> Don't error on ENOTCONN from shutdown() #670
<li> Auto completion of built-in debugger suggests prefix match rather than partial match. (koichik)
<li> circular reference in vm modules. #822 (Jakub Lekstan)
<li> http response.readable should be false after 'end' #867 (Abe Fettig)
<li> Implement os.cpus() and os.uptime() on Solaris (Scott McWhirter)
<li> fs.ReadStream: Allow omission of end option for range reads #801 (Felix Geisendörfer)
<li> Buffer.write() with UCS-2 should not be write partial char #916 (koichik)
<Li> Pass secureProtocol through on tls.Server creation (Theo Schlossnagle)
<li> TLS use RC4-SHA by default
<li> Don't strangely drop out of event loop on HTTPS client uploads #892
<li> Doc improvements
<li> Upgrade v8 to 3.1.8.10</ul>

Download: <a href="http://nodejs.org/dist/node-v0.4.6.tar.gz">http://nodejs.org/dist/node-v0.4.6.tar.gz</a>

Website: <a href="http://nodejs.org/docs/v0.4.6/">http://nodejs.org/docs/v0.4.6/</a>

Documentation: <a href="http://nodejs.org/docs/v0.4.6/api/">http://nodejs.org/docs/v0.4.6/api/</a>
