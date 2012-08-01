title: An Easy Way to Build Scalable Network Programs
author: ryandahl
date: Tue Oct 04 2011 15:39:56 GMT-0700 (PDT)
status: publish
category: Uncategorized
slug: an-easy-way-to-build-scalable-network-programs

Suppose you're writing a web server which does video encoding on each file upload. Video encoding is very much compute bound. Some recent blog posts suggest that Node.js would fail miserably at this.

Using Node does not mean that you have to write a video encoding algorithm in JavaScript (a language without even 64 bit integers) and crunch away in the main server event loop. The suggested approach is to separate the I/O bound task of receiving uploads and serving downloads from the compute bound task of video encoding. In the case of video encoding this is accomplished by forking out to ffmpeg. Node provides advanced means of asynchronously controlling subprocesses for work like this.

It has also been suggested that Node does not take advantage of multicore machines. Node has long supported load-balancing connections over multiple processes in just a few lines of code - in this way a Node server will use the available cores. In coming releases we'll make it even easier: just pass <code>--balance</code> on the command line and Node will manage the cluster of processes.

Node has a clear purpose: provide an easy way to build scalable network programs. It is not a tool for every problem. Do not write a ray tracer with Node. Do not write a web browser with Node. Do however reach for Node if tasked with writing a DNS server, DHCP server, or even a video encoding server.

By relying on the kernel to schedule and preempt computationally expensive tasks and to load balance incoming connections, Node appears less magical than server platforms that employ userland scheduling. So far, our focus on simplicity and transparency has paid off: <a href="http://joyeur.com/2011/08/11/node-js-meetup-distributed-web-architectures/">the</a> <a href="http://venturebeat.com/2011/08/16/linkedin-node/">number</a> <a href="http://corp.klout.com/blog/2011/10/the-tech-behind-klout-com/">of</a> <a href="http://www.joelonsoftware.com/items/2011/09/13.html">success</a> <a href="http://pow.cx/">stories</a> from developers and corporations who are adopting the technology continues to grow.
