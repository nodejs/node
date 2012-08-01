version: 0.4.10
title: Node v0.4.10
author: ryandahl
date: Wed Jul 20 2011 07:36:38 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-4-10

2011.07.19, Version 0.4.10 (stable)
<ul><li>#394 Fix Buffer drops last null character in UTF-8
<li>#829 Backport r8577 from V8 (Ben Noordhuis)
<li>#877 Don't wait for HTTP Agent socket pool to establish connections.
<li>#915 Find kqueue on FreeBSD correctly (Brett Kiefer) 
<li>#1085 HTTP: Fix race in abort/dispatch code (Stefan Rusu)
<li>#1274 debugger improvement (Yoshihiro Kikuchi)
<li>#1291 Properly respond to HEAD during end(body) hot path (Reid Burke)
<li>#1304 TLS: Fix race in abort/connection code (Stefan Rusu)
<li>#1360 Allow _ in url hostnames.
<li>Revert 37d529f8 - unbreaks debugger command parsing.
<li>Bring back global execScript
<li>Doc improvements</ul>

Download: <a href="http://nodejs.org/dist/node-v0.4.10.tar.gz">http://nodejs.org/dist/node-v0.4.10.tar.gz</a>
Website: <a href="http://nodejs.org/docs/v0.4.10">http://nodejs.org/docs/v0.4.10</a>
Documentation: <a href="http://nodejs.org/docs/v0.4.10/api">http://nodejs.org/docs/v0.4.10/api</a>
