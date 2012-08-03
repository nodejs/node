title: npm 1.0: Global vs Local installation
author: Isaac Schlueter
date: Wed Mar 23 2011 23:07:13 GMT-0700 (PDT)
status: publish
category: npm
slug: npm-1-0-global-vs-local-installation

<p><i>npm 1.0 is in release candidate mode.  <a href="http://groups.google.com/group/npm-/browse_thread/thread/43d3e76d71d1f141">Go get it!</a></i></p>

<p>More than anything else, the driving force behind the npm 1.0 rearchitecture was the desire to simplify what a package installation directory structure looks like.</p>

<p>In npm 0.x, there was a command called <code>bundle</code> that a lot of people liked. <code>bundle</code> let you install your dependencies locally in your project, but even still, it was basically a hack that never really worked very reliably.</p>

<p>Also, there was that activation/deactivation thing.  That&#8217;s confusing.</p>

<h2>Two paths</h2>

<p>In npm 1.0, there are two ways to install things:</p>

<ol> <li>globally &#8212;- This drops modules in <code>{prefix}/lib/node_modules</code>, and puts executable files in <code>{prefix}/bin</code>, where <code>{prefix}</code> is usually something like <code>/usr/local</code>. It also installs man pages in <code>{prefix}/share/man</code>, if they&#8217;re supplied.</li> <li>locally &#8212;- This installs your package in the current working directory. Node modules go in <code>./node_modules</code>, executables go in <code>./node_modules/.bin/</code>, and man pages aren&#8217;t installed at all.</li> </ol>

<h2>Which to choose</h2>

<p>Whether to install a package globally or locally depends on the <code>global</code> config, which is aliased to the <code>-g</code> command line switch.</p>

<p>Just like how global variables are kind of gross, but also necessary in some cases, global packages are important, but best avoided if not needed.</p>

<p>In general, the rule of thumb is:</p>

<ol> <li>If you&#8217;re installing something that you want to use <em>in</em> your program, using <code>require('whatever')</code>, then install it locally, at the root of your project.</li> <li>If you&#8217;re installing something that you want to use in your <em>shell</em>, on the command line or something, install it globally, so that its binaries end up in your <code>PATH</code> environment variable.</li> </ol>

<h2>When you can't choose</h2>

<p>Of course, there are some cases where you want to do both. <a href="http://coffeescript.org/">Coffee-script</a> and <a href="http://expressjs.com/">Express</a> both are good examples of apps that have a command line interface, as well as a library. In those cases, you can do one of the following:</p>

<ol> <li>Install it in both places. Seriously, are you that short on disk space? It&#8217;s fine, really. They&#8217;re tiny JavaScript programs.</li> <li>Install it globally, and then <code>npm link coffee-script</code> or <code>npm link express</code> (if you&#8217;re on a platform that supports symbolic links.) Then you only need to update the global copy to update all the symlinks as well.</li> </ol>

<p>The first option is the best in my opinion.  Simple, clear, explicit.  The second is really handy if you are going to re-use the same library in a bunch of different projects.  (More on <code>npm link</code> in a future installment.)</p>

<p>You can probably think of other ways to do it by messing with environment variables. But I don&#8217;t recommend those ways. Go with the grain.</p>

<h2 id="slight_exception_it8217s_not_always_the_cwd">Slight exception: It&#8217;s not always the cwd.</h2>

<p>Let&#8217;s say you do something like this:</p>

<pre style="background:#333!important;color:#ccc!important;overflow:auto!important;padding:2px!important;"><code>cd ~/projects/foo     # go into my project
npm install express   # ./node_modules/express
cd lib/utils          # move around in there
vim some-thing.js     # edit some stuff, work work work
npm install redis     # ./lib/utils/node_modules/redis!? ew.</code></pre>

<p>In this case, npm will install <code>redis</code> into <code>~/projects/foo/node_modules/redis</code>. Sort of like how git will work anywhere within a git repository, npm will work anywhere within a package, defined by having a <code>node_modules</code> folder.</p>

<h2>Test runners and stuff</h2>

<p>If your package's <code>scripts.test</code> command uses a command-line program installed by one of your dependencies, not to worry.  npm makes <code>./node_modules/.bin</code> the first entry in the <code>PATH</code> environment variable when running any lifecycle scripts, so this will work fine, even if your program is not globally installed:

<pre style="background:#333!important;color:#ccc!important;overflow:auto!important;padding:2px!important;"><code>{ "name" : "my-program"
, "version" : "1.2.3"
, "dependencies": { "express": "*", "coffee-script": "*" }
, "devDependencies": { "vows": "*" }
, "scripts":
  { "test": "vows test/*.js"
  , "preinstall": "cake build" } }</code></pre>
