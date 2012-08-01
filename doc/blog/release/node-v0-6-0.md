version: 0.6.0
title: Node v0.6.0
author: ryandahl
date: Sat Nov 05 2011 02:07:10 GMT-0700 (PDT)
status: publish
category: release
slug: node-v0-6-0

We are happy to announce the third stable branch of Node v0.6. We will be freezing JavaScript, C++, and binary interfaces for all v0.6 releases.

The major differences between v0.4 and v0.6 are<ul>
<li>Native Windows support using I/O Completion Ports for sockets.
<li>Integrated load balancing over multiple processes. <a href="http://nodejs.org/docs/v0.6.0/api/cluster.html">docs</a>
<li>Better support for IPC between Node instances <a href="http://nodejs.org/docs/v0.6.0/api/child_processes.html#child_process.fork">docs</a>
<li>Improved command line debugger <a href="http://nodejs.org/docs/v0.6.0/api/debugger.html">docs</a>
<li>Built-in binding to zlib for compression <a href="http://nodejs.org/docs/v0.6.0/api/zlib.html">docs</a>
<li>Upgrade v8 from 3.1 to 3.6</ul>

In order to support Windows we reworked much of the core architecture. There was some fear that our work would degrade performance on UNIX systems but this was not the case. Here is a Linux system we benched for demonstration:

<table><tr> <th></th> <th>v0.4.12 (linux)</th><th>v0.6.0 (linux)</th></tr>
<tr> <td>http_simple.js /bytes/1024</td> <td>5461 r/s</td> <td>6263 r/s</td> </tr>
<tr> <td>io.js read </td> <td>19.75 mB/s</td> <td>26.63 mB/s</td> </tr>
<tr> <td>io.js write </td> <td>21.60 mB/s</td> <td>17.40 mB/s</td> </tr>
<tr> <td>startup.js </td> <td>74.7 ms</td> <td>49.6 ms</td> </tr></table>

Bigger is better in http and io benchmarks, smaller is better in startup. The http benchmark was done with 600 clients on a 10GE network served from three load generation machines.

In the last version of Node, v0.4, we could only run Node on Windows with Cygwin. Therefore we've gotten massive improvements by targeting the native APIs. Benchmarks on the same machine:

<table><tr><th></th><th>v0.4.12 (windows)</th><th>v0.6.0 (windows)</th></tr>
<tr> <td>http_simple.js /bytes/1024</td> <td>3858 r/s</td> <td>5823 r/s</td> </tr>
<tr> <td>io.js read </td> <td>12.41 mB/s</td> <td>26.51 mB/s</td> </tr>
<tr> <td>io.js write </td> <td>12.61 mB/s</td> <td>33.58 mB/s</td> </tr>
<tr> <td>startup.js </td> <td>152.81 ms</td> <td>52.04 ms</td> </tr></table>

We consider this a good intermediate stage for the Windows port. There is still work to be done. For example, we are not yet providing users with a blessed path for building addon modules in MS Visual Studio.  Work will continue in later releases.

For users upgrading code bases from v0.4 to v0.6 <a href="https://github.com/joyent/node/wiki/API-changes-between-v0.4-and-v0.6">we've documented</a> most of the issues that you will run into. Most people find the change painless. Despite the long list of changes most core APIs remain untouched. 

Our release cycle will be tightened dramatically now. Expect to see a new stable branch in January. We wish to eventually have our releases in sync with Chrome and V8's 6 week cycle.

Thank you to everyone who contributed code, tests, docs, or sent in bug reports.

Here are the changes between v0.5.12 and v0.6.0:

2011.11.04, Version 0.6.0 (stable)
<ul><li>print undefined on undefined values in REPL (Nathan Rajlich)</li>
<li>doc improvements (koichik, seebees, bnoordhuis, Maciej Małecki, Jacob Kragh)</li>
<li>support native addon loading in windows (Bert Belder)</li>
<li>rename getNetworkInterfaces() to networkInterfaces() (bnoordhuis)</li>
<li>add pending accepts knob for windows (igorzi)</li>
<li>http.request(url.parse(x)) (seebees)</li>
<li>#1929 zlib Respond to 'resume' events properly (isaacs)</li>
<li>stream.pipe: Remove resume and pause events</li>
<li>test fixes for windows (igorzi)</li>
<li>build system improvements (bnoordhuis)</li>
<li>#1936 tls: does not emit 'end' from EncryptedStream (koichik)</li>
<li>#758 tls: add address(), remoteAddress/remotePort</li>
<li>#1399 http: emit Error object after .abort() (bnoordhuis)</li>
<li>#1999 fs: make mkdir() default to 0777 permissions (bnoordhuis)</li>
<li>#2001 fix pipe error codes</li>
<li>#2002 Socket.write should reset timeout timer</li>
<li>stdout and stderr are blocking when associated with file too.</li>
<li>remote debugger support on windows (Bert Belder)</li>
<li>convenience methods for zlib (Matt Robenolt)</li>
<li>process.kill support on windows (igorzi)</li>
<li>process.uptime() support on windows (igorzi)</li>
<li>Return IPv4 addresses before IPv6 addresses from getaddrinfo</li>
<li>util.inspect improvements (Nathan Rajlich)</li>
<li>cluster module api changes</li>
<li>Downgrade V8 to 3.6.6.6</li></ul>

Download: <a href="http://nodejs.org/dist/v0.6.0/node-v0.6.0.tar.gz">http://nodejs.org/dist/v0.6.0/node-v0.6.0.tar.gz</a>

Windows Executable: <a href="http://nodejs.org/dist/v0.6.0/node.exe">http://nodejs.org/dist/v0.6.0/node.exe</a>

Website: <a href="http://nodejs.org/docs/v0.6.0/">http://nodejs.org/docs/v0.6.0/</a>

Documentation: <a href="http://nodejs.org/docs/v0.6.0/api/">http://nodejs.org/docs/v0.6.0/api/</a>
