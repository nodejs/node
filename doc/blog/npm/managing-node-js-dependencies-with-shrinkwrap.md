title: Managing Node.js Dependencies with Shrinkwrap
author: Dave Pacheco
date: Mon Feb 27 2012 10:51:59 GMT-0800 (PST)
status: publish
category: npm
slug: managing-node-js-dependencies-with-shrinkwrap

<p style="float:right;text-align:center;margin:5px;"><a href="http://www.flickr.com/photos/luc_viatour/4247957432/"><img class="size-medium wp-image-652" style="border:1px #000000 solid;" title="Web" src="http://dtrace.org/blogs/dap/files/2012/02/web-300x300.jpg" alt="" width="250" height="250" /></a><br>
Photo by Luc Viatour (flickr)</p>

<p>Managing dependencies is a fundamental problem in building complex software. The terrific success of github and <a href="http://npmjs.org/">npm</a> have made code reuse especially easy in the Node world, where packages don&#039;t exist in isolation but rather as nodes in a large graph. The software is constantly changing (releasing new versions), and each package has its own constraints about what other packages it requires to run (dependencies). npm keeps track of these constraints, and authors express what kind of changes are compatible using <a href="http://npmjs.org/doc/semver.html">semantic versioning</a>, allowing authors to specify that their package will work with even future versions of its dependencies as long as the semantic versions are assigned properly.

</p>
<p>This does mean that when you &quot;npm install&quot; a package with dependencies, there&#039;s no guarantee that you&#039;ll get the same set of code now that you would have gotten an hour ago, or that you would get if you were to run it again an hour later. You may get a bunch of bug fixes now that weren&#039;t available an hour ago. This is great during development, where you want to keep up with changes upstream. It&#039;s not necessarily what you want for deployment, though, where you want to validate whatever bits you&#039;re actually shipping.

</p>
<p>Put differently, <strong>it&#039;s understood that all software changes incur some risk, and it&#039;s critical to be able to manage this risk on your own terms</strong>. Taking that risk in development is good because by definition that&#039;s when you&#039;re incorporating and testing software changes. On the other hand, if you&#039;re shipping production software, you probably don&#039;t want to take this risk when cutting a release candidate (i.e. build time) or when you actually ship (i.e. deploy time) because you want to validate whatever you ship.

</p>
<p>You can address a simple case of this problem by only depending on specific versions of packages, allowing no semver flexibility at all, but this falls apart when you depend on packages that don&#039;t also adopt the same principle. Many of us at Joyent started wondering: can we generalize this approach?

</p>
<h2>Shrinkwrapping packages</h2>
<p>That brings us to <a href="http://npmjs.org/doc/shrinkwrap.html">npm shrinkwrap</a><a href="#note1-note" name="note1-top">[1]</a>:

</p>
<pre><code>NAME
       npm-shrinkwrap -- Lock down dependency versions

SYNOPSIS
       npm shrinkwrap

DESCRIPTION
       This  command  locks down the versions of a package&#039;s dependencies so
       that you can control exactly which versions of each  dependency  will
       be used when your package is installed.</code></pre>
<p>Let&#039;s consider package A:

</p>
<pre><code>{
    &quot;name&quot;: &quot;A&quot;,
    &quot;version&quot;: &quot;0.1.0&quot;,
    &quot;dependencies&quot;: {
        &quot;B&quot;: &quot;&lt;0.1.0&quot;
    }
}</code></pre>
<p>package B:

</p>
<pre><code>{
    &quot;name&quot;: &quot;B&quot;,
    &quot;version&quot;: &quot;0.0.1&quot;,
    &quot;dependencies&quot;: {
        &quot;C&quot;: &quot;&lt;0.1.0&quot;
    }
}</code></pre>
<p>and package C:

</p>
<pre><code>{
    &quot;name&quot;: &quot;C,
    &quot;version&quot;: &quot;0.0.1&quot;
}</code></pre>
<p>If these are the only versions of A, B, and C available in the registry, then a normal &quot;npm install A&quot; will install:

</p>
<pre><code>A@0.1.0
└─┬ B@0.0.1
  └── C@0.0.1</code></pre>
<p>Then if B@0.0.2 is published, then a fresh &quot;npm install A&quot; will install:

</p>
<pre><code>A@0.1.0
└─┬ B@0.0.2
  └── C@0.0.1</code></pre>
<p>assuming the new version did not modify B&#039;s dependencies. Of course, the new version of B could include a new version of C and any number of new dependencies. As we said before, if A&#039;s author doesn&#039;t want that, she could specify a dependency on B@0.0.1. But if A&#039;s author and B&#039;s author are not the same person, there&#039;s no way for A&#039;s author to say that she does not want to pull in newly published versions of C when B hasn&#039;t changed at all.

</p>
<p>In this case, A&#039;s author can use

</p>
<pre><code># npm shrinkwrap</code></pre>
<p>This generates npm-shrinkwrap.json, which will look something like this:

</p>
<pre><code>{
    &quot;name&quot;: &quot;A&quot;,
    &quot;dependencies&quot;: {
        &quot;B&quot;: {
            &quot;version&quot;: &quot;0.0.1&quot;,
            &quot;dependencies&quot;: {
                &quot;C&quot;: {  &quot;version&quot;: &quot;0.1.0&quot; }
            }
        }
    }
}</code></pre>
<p>The shrinkwrap command has locked down the dependencies based on what&#039;s currently installed in node_modules.  <strong>When &quot;npm install&quot; installs a package with a npm-shrinkwrap.json file in the package root, the shrinkwrap file (rather than package.json files) completely drives the installation of that package and all of its dependencies (recursively).</strong>  So now the author publishes A@0.1.0, and subsequent installs of this package will use B@0.0.1 and C@0.1.0, regardless the dependencies and versions listed in A&#039;s, B&#039;s, and C&#039;s package.json files.  If the authors of B and C publish new versions, they won&#039;t be used to install A because the shrinkwrap refers to older versions.  Even if you generate a new shrinkwrap, it will still reference the older versions, since &quot;npm shrinkwrap&quot; uses what&#039;s installed locally rather than what&#039;s available in the registry. 

</p>
<h4>Using shrinkwrapped packages</h4>
<p>Using a shrinkwrapped package is no different than using any other package: you can &quot;npm install&quot; it by hand, or add a dependency to your package.json file and &quot;npm install&quot; it.

</p>
<h4>Building shrinkwrapped packages</h4>
<p>To shrinkwrap an existing package:

</p>
<ol>
<li>Run &quot;npm install&quot; in the package root to install the current versions of all dependencies.</li>
<li>Validate that the package works as expected with these versions.</li>
<li>Run &quot;npm shrinkwrap&quot;, add npm-shrinkwrap.json to git, and publish your package.</li>
</ol>
<p>To add or update a dependency in a shrinkwrapped package:

</p>
<ol>
<li>Run &quot;npm install&quot; in the package root to install the current versions of all dependencies.</li>
<li>Add or update dependencies. &quot;npm install&quot; each new or updated package individually and then update package.json.</li>
<li>Validate that the package works as expected with the new dependencies.</li>
<li>Run &quot;npm shrinkwrap&quot;, commit the new npm-shrinkwrap.json, and publish your package.</li>
</ol>
<p>You can still use <a href="http://npmjs.org/doc/outdated.html">npm outdated(1)</a> to view which dependencies have newer versions available.

</p>
<p>For more details, check out the full docs on <a href="http://npmjs.org/doc/shrinkwrap.html">npm shrinkwrap</a>, from which much of the above is taken.

</p>
<h2>Why not just check <code>node_modules</code> into git?</h2>
<p>One previously <a href="http://www.mikealrogers.com/posts/nodemodules-in-git.html">proposed solution</a> is to &quot;npm install&quot; your dependencies during development and commit the results into source control. Then you deploy your app from a specific git SHA knowing you&#039;ve got exactly the same bits that you tested in development. This does address the problem, but it has its own issues: for one, binaries are tricky because you need to &quot;npm install&quot; them to get their sources, but this builds the [system-dependent] binary too. You can avoid checking in the binaries and use &quot;npm rebuild&quot; at build time, but we&#039;ve had a lot of difficulty trying to do this.<a href="#note2-note" name="note2-top">[2]</a> At best, this is second-class treatment for binary modules, which are critical for many important types of Node applications.<a href="#note3-note" name="note3-top">[3]</a>

</p>
<p>Besides the issues with binary modules, this approach just felt wrong to many of us. There&#039;s a reason we don&#039;t check binaries into source control, and it&#039;s not just because they&#039;re platform-dependent. (After all, we could build and check in binaries for all supported platforms and operating systems.) It&#039;s because that approach is error-prone and redundant: error-prone because it introduces a new human failure mode where someone checks in a source change but doesn&#039;t regenerate all the binaries, and redundant because the binaries can always be built from the sources alone. An important principle of software version control is that you don&#039;t check in files derived directly from other files by a simple transformation.<a href="#note4-note" name="note4-top">[4]</a> Instead, you check in the original sources and automate the transformations via the build process.

</p>
<p>Dependencies are just like binaries in this regard: they&#039;re files derived from a simple transformation of something else that is (or could easily be) already available: the name and version of the dependency. Checking them in has all the same problems as checking in binaries: people could update package.json without updating the checked-in module (or vice versa). Besides that, adding new dependencies has to be done by hand, introducing more opportunities for error (checking in the wrong files, not checking in certain files, inadvertently changing files, and so on). Our feeling was: why check in this whole dependency tree (and create a mess for binary add-ons) when we could just check in the package name and version and have the build process do the rest?

</p>
<p>Finally, the approach of checking in node_modules doesn&#039;t really scale for us. We&#039;ve got at least a dozen repos that will use restify, and it doesn&#039;t make sense to check that in everywhere when we could instead just specify which version each one is using. There&#039;s another principle at work here, which is <strong>separation of concerns</strong>: each repo specifies <em>what</em> it needs, while the build process figures out <em>where to get it</em>.

</p>
<h2>What if an author republishes an existing version of a package?</h2>
<p>We&#039;re not suggesting deploying a shrinkwrapped package directly and running &quot;npm install&quot; to install from shrinkwrap in production. We already have a build process to deal with binary modules and other automateable tasks. That&#039;s where we do the &quot;npm install&quot;. We tar up the result and distribute the tarball. Since we test each build before shipping, we won&#039;t deploy something we didn&#039;t test.

</p>
<p>It&#039;s still possible to pick up newly published versions of existing packages at build time. We assume force publish is not that common in the first place, let alone force publish that breaks compatibility. If you&#039;re worried about this, you can use git SHAs in the shrinkwrap or even consider maintaining a mirror of the part of the npm registry that you use and require human confirmation before mirroring unpublishes.

</p>
<h2>Final thoughts</h2>
<p>Of course, the details of each use case matter a lot, and the world doesn&#039;t have to pick just one solution. If you like checking in node_modules, you should keep doing that. We&#039;ve chosen the shrinkwrap route because that works better for us.

</p>
<p>It&#039;s not exactly news that Joyent is heavy on Node. Node is the heart of our SmartDataCenter (SDC) product, whose public-facing web portal, public API, Cloud Analytics, provisioning, billing, heartbeating, and other services are all implemented in Node. That&#039;s why it&#039;s so important to us to have robust components (like <a href="https://github.com/trentm/node-bunyan">logging</a> and <a href="http://mcavage.github.com/node-restify/">REST</a>) and tools for <a href="http://dtrace.org/blogs/dap/2012/01/13/playing-with-nodev8-postmortem-debugging/">understanding production failures postmortem</a>, <a href="http://dtrace.org/blogs/dap/2012/01/05/where-does-your-node-program-spend-its-time/">profile Node apps in production</a>, and now managing Node dependencies. Again, we&#039;re interested to hear feedback from others using these tools.

</p>
<hr />
Dave Pacheco blogs at <a href="http://dtrace.org/blogs/dap/">dtrace.org</a>.

<p><a href="#note1-top" name="note1-note">[1]</a> Much of this section is taken directly from the &quot;npm shrinkwrap&quot; documentation.

</p>
<p><a href="#note2-top" name="note2-note">[2]</a> We&#039;ve had a lot of trouble with checking in node_modules with binary dependencies. The first problem is figuring out exactly which files <em>not</em> to check in (<em>.o, </em>.node, <em>.dynlib, </em>.so, *.a, ...). When <a href="https://twitter.com/#!/mcavage">Mark</a> went to apply this to one of our internal services, the &quot;npm rebuild&quot; step blew away half of the dependency tree because it ran &quot;make clean&quot;, which in dependency <a href="http://ldapjs.org/">ldapjs</a> brings the repo to a clean slate by blowing away its dependencies. Later, a new (but highly experienced) engineer on our team was tasked with fixing a bug in our Node-based DHCP server. To fix the bug, we went with a new dependency. He tried checking in node_modules, which added 190,000 lines of code (to this repo that was previously a few hundred LOC). And despite doing everything he could think of to do this correctly and test it properly, the change broke the build because of the binary modules. So having tried this approach a few times now, it appears quite difficult to get right, and as I pointed out above, the lack of actual documentation and real world examples suggests others either aren&#039;t using binary modules (which we know isn&#039;t true) or haven&#039;t had much better luck with this approach.

</p>
<p><a href="#note3-top" name="note3-note">[3]</a> Like a good Node-based distributed system, our architecture uses lots of small HTTP servers. Each of these serves a REST API using <a href="http://mcavage.github.com/node-restify/">restify</a>. restify uses the binary module <a href="https://github.com/chrisa/node-dtrace-provider">node-dtrace-provider</a>, which gives each of our services <a href="http://mcavage.github.com/node-restify/#DTrace">deep DTrace-based observability for free</a>. So literally almost all of our components are or will soon be depending on a binary add-on. Additionally, the foundation of <a href="http://dtrace.org/blogs/dap/2011/03/01/welcome-to-cloud-analytics/">Cloud Analytics</a> are a pair of binary modules that extract data from <a href="https://github.com/bcantrill/node-libdtrace">DTrace</a> and <a href="https://github.com/bcantrill/node-kstat">kstat</a>. So this isn&#039;t a corner case for us, and we don&#039;t believe we&#039;re exceptional in this regard. The popular <a href="https://github.com/pietern/hiredis-node">hiredis</a> package for interfacing with redis from Node is also a binary module.

</p>
<p><a href="#note4-top" name="note4-note">[4]</a> Note that I said this is an important principle for <em>software version control</em>, not using git in general. People use git for lots of things where checking in binaries and other derived files is probably fine. Also, I&#039;m not interested in proselytizing; if you want to do this for software version control too, go ahead. But don&#039;t do it out of ignorance of existing successful software engineering practices.</p>
