title: Profiling Node.js
author: Dave Pacheco
date: Wed Apr 25 2012 13:48:58 GMT-0700 (PDT)
status: publish
category: Uncategorized
slug: profiling-node-js

It's incredibly easy to visualize where your Node program spends its time using DTrace and <a href="http://github.com/davepacheco/node-stackvis">node-stackvis</a> (a Node port of Brendan Gregg's <a href="http://github.com/brendangregg/FlameGraph/">FlameGraph</a> tool):

<ol>
    <li>Run your Node.js program as usual.</li>
    <li>In another terminal, run:
        <pre>
$ dtrace -n 'profile-97/execname == "node" &amp;&amp; arg1/{
    @[jstack(150, 8000)] = count(); } tick-60s { exit(0); }' &gt; stacks.out</pre>
        This will sample about 100 times per second for 60 seconds and emit results to stacks.out. <strong>Note that this will sample all running programs called "node".  If you want a specific process, replace <code>execname == "node"</code> with <code>pid == 12345</code> (the process id).</strong>
    </li>
    <li>Use the "stackvis" tool to transform this directly into a flame graph. First, install it:
        <pre>$ npm install -g stackvis</pre>
        then use <code>stackvis</code> to convert the DTrace output to a flamegraph:
        <pre>$ stackvis dtrace flamegraph-svg &lt; stacks.out &gt; stacks.svg</pre>
    </li>
    <li>Open stacks.svg in your favorite browser.</li>
</ol>

You'll be looking at something like this:

<a href="http://www.cs.brown.edu/~dap/helloworld.svg"><img src="http://dtrace.org/blogs/dap/files/2012/04/helloworld-flamegraph-550x366.png" alt="" title="helloworld-flamegraph" width="550" height="366" class="aligncenter size-large wp-image-1047" /></a>

This is a visualization of all of the profiled call stacks. This example is from the "hello world" HTTP server on the <a href="http://nodejs.org">Node.js</a> home page under load. Start at the bottom, where you have "main", which is present in most Node stacks because Node spends most on-CPU time in the main thread. Above each row, you have the functions called by the frame beneath it. As you move up, you'll see actual JavaScript function names. The boxes in each row are not in chronological order, but their width indicates how much time was spent there. When you hover over each box, you can see exactly what percentage of time is spent in each function. This lets you see at a glance where your program spends its time.

That's the summary. There are a few prerequisites:

<ul>
    <li>You must gather data on a system that supports DTrace with the Node.js ustack helper. For now, this pretty much means <a href="http://illumos.org/">illumos</a>-based systems like <a href="http://smartos.org/">SmartOS</a>, including the Joyent Cloud. <strong>MacOS users:</strong> OS X supports DTrace, but not ustack helpers. The way to get this changed is to contact your Apple developer liaison (if you're lucky enough to have one) or <strong>file a bug report at bugreport.apple.com</strong>. I'd suggest referencing existing bugs 5273057 and 11206497. More bugs filed (even if closed as dups) show more interest and make it more likely Apple will choose to fix this.</li>
    <li>You must be on 32-bit Node.js 0.6.7 or later, built <code>--with-dtrace</code>. The helper doesn't work with 64-bit Node yet. On illumos (including SmartOS), development releases (the 0.7.x train) include DTrace support by default.</li>
</ul>

There are a few other notes:

<ul>
    <li>You can absolutely profile apps <strong>in production</strong>, not just development, since compiling with DTrace support has very minimal overhead. You can start and stop profiling without restarting your program.</li>
    <li>You may want to run the stacks.out output through <code>c++filt</code> to demangle C++ symbols. Be sure to use the <code>c++filt</code> that came with the compiler you used to build Node. For example:
    <pre>c++filt &lt; stacks.out &gt; demangled.out</pre>
    then you can use demangled.out to create the flamegraph.
    </li>
    <li>If you want, you can filter stacks containing a particular function.  The best way to do this is to first collapse the original DTrace output, then grep out what you want:
        <pre>
$ stackvis dtrace collapsed &lt; stacks.out | grep SomeFunction &gt; collapsed.out
$ stackvis collapsed flamegraph-svg &lt; collapsed.out &gt; stacks.svg</pre>
    </li>
    <li>If you've used Brendan's FlameGraph tools, you'll notice the coloring is a little different in the above flamegraph. I ported his tools to Node first so I could incorporate it more easily into other Node programs, but I've also been playing with different coloring options. The current default uses hue to denote stack depth and saturation to indicate time spent. (These are also indicated by position and size.) Other ideas include coloring by module (so V8, JavaScript, libc, etc. show up as different colors.)
    </li>
</ul>

For more on the underlying pieces, see my <a href="http://dtrace.org/blogs/dap/2012/01/05/where-does-your-node-program-spend-its-time/">previous post on Node.js profiling</a> and <a href="http://dtrace.org/blogs/brendan/2011/12/16/flame-graphs/">Brendan's post on Flame Graphs</a>.

<hr />

Dave Pacheco blogs at <a href="http://dtrace.org/blogs/dap">dtrace.org</a>
