title: npm 1.0: link
author: Isaac Schlueter
date: Wed Apr 06 2011 17:40:33 GMT-0700 (PDT)
status: publish
category: npm
slug: npm-1-0-link

<p><i>npm 1.0 is in release candidate mode. <a href="http://groups.google.com/group/npm-/browse_thread/thread/43d3e76d71d1f141">Go get it!</a></i></p>

<p>In npm 0.x, there was a command called <code>link</code>. With it, you could &#8220;link-install&#8221; a package so that changes would be reflected in real-time. This is especially handy when you&#8217;re actually building something. You could make a few changes, run the command again, and voila, your new code would be run without having to re-install every time.</p>

<p>Of course, compiled modules still have to be rebuilt. That&#8217;s not ideal, but it&#8217;s a problem that will take more powerful magic to solve.</p>

<p>In npm 0.x, this was a pretty awful kludge. Back then, every package existed in some folder like:</p>

<pre><code>prefix/lib/node/.npm/my-package/1.3.6/package
</code></pre>

<p>and the package&#8217;s version and name could be inferred from the path. Then, symbolic links were set up that looked like:</p>

<pre><code>prefix/lib/node/my-package@1.3.6 -&gt; ./.npm/my-package/1.3.6/package
</code></pre>

<p>It was easy enough to point that symlink to a different location. However, since the <em>package.json file could change</em>, that meant that the connection between the version and the folder was not reliable.</p>

<p>At first, this was just sort of something that we dealt with by saying, &#8220;Relink if you change the version.&#8221; However, as more and more edge cases arose, eventually the solution was to give link packages this fakey version of &#8220;9999.0.0-LINK-hash&#8221; so that npm knew it was an impostor. Sometimes the package was treated as if it had the 9999.0.0 version, and other times it was treated as if it had the version specified in the package.json.</p>

<h2 id="a_better_way">A better way</h2>

<p>For npm 1.0, we backed up and looked at what the actual use cases were. Most of the time when you link something you want one of the following:</p>

<ol>
<li>globally install this package I&#8217;m working on so that I can run the command it creates and test its stuff as I work on it.</li>
<li>locally install my thing into some <em>other</em> thing that depends on it, so that the other thing can <code>require()</code> it.</li>
</ol>

<p>And, in both cases, changes should be immediately apparent and not require any re-linking.</p>

<p><em>Also</em>, there&#8217;s a third use case that I didn&#8217;t really appreciate until I started writing more programs that had more dependencies:</p>

<ol start="3"> <li><p>Globally install something, and use it in development in a bunch of projects, and then update them all at once so that they all use the latest version. </ol>

<p>Really, the second case above is a special-case of this third case.</p>

<h2 id="link_devel_global">Link devel &rarr; global</h2>

<p>The first step is to link your local project into the global install space. (See <a href="http://blog.nodejs.org/2011/03/23/npm-1-0-global-vs-local-installation/">global vs local installation</a> for more on this global/local business.)</p>

<p>I do this as I&#8217;m developing node projects (including npm itself).</p>

<pre><code>cd ~/dev/js/node-tap  # go into the project dir
npm link              # create symlinks into {prefix}
</code></pre>

<p>Because of how I have my computer set up, with <code>/usr/local</code> as my install prefix, I end up with a symlink from <code>/usr/local/lib/node_modules/tap</code> pointing to <code>~/dev/js/node-tap</code>, and the executable linked to <code>/usr/local/bin/tap</code>.</p>

<p>Of course, if you <a href="http://blog.nodejs.org/2011/04/04/development-environment/">set your paths differently</a>, then you&#8217;ll have different results. (That&#8217;s why I tend to talk in terms of <code>prefix</code> rather than <code>/usr/local</code>.)</p>

<h2 id="link_global_local">Link global &rarr; local</h2>

<p>When you want to link the globally-installed package into your local development folder, you run <code>npm link pkg</code> where <code>pkg</code> is the name of the package that you want to install.</p>

<p>For example, let&#8217;s say that I wanted to write some tap tests for my node-glob package. I&#8217;d <em>first</em> do the steps above to link tap into the global install space, and <em>then</em> I&#8217;d do this:</p>

<pre><code>cd ~/dev/js/node-glob  # go to the project that uses the thing.
npm link tap           # link the global thing into my project.
</code></pre>

<p>Now when I make changes in <code>~/dev/js/node-tap</code>, they&#8217;ll be immediately reflected in <code>~/dev/js/node-glob/node_modules/tap</code>.</p>

<h2 id="link_to_stuff_you_don8217t_build">Link to stuff you <em>don&#8217;t</em> build</h2>

<p>Let&#8217;s say I have 15 sites that all use express. I want the benefits of local development, but I also want to be able to update all my dev folders at once. You can globally install express, and then link it into your local development folder.</p>

<pre><code>npm install express -g  # install express globally
cd ~/dev/js/my-blog     # development folder one
npm link express        # link the global express into ./node_modules
cd ~/dev/js/photo-site  # other project folder
npm link express        # link express into here, as well

                        # time passes
                        # TJ releases some new stuff.
                        # you want this new stuff.

npm update express -g   # update the global install.
                        # this also updates my project folders.
</code></pre>

<h2 id="caveat_not_for_real_servers">Caveat: Not For Real Servers</h2>

<p>npm link is a development tool. It&#8217;s <em>awesome</em> for managing packages on your local development box. But deploying with npm link is basically asking for problems, since it makes it super easy to update things without realizing it.</p>

<h2 id="caveat_2_sorry_windows">Caveat 2: Sorry, Windows!</h2>

<p>I highly doubt that a native Windows node will ever have comparable symbolic link support to what Unix systems provide.  I know that there are junctions and such, and I've heard legends about symbolic links on Windows 7.</p>

<p>When there is a native windows port of Node, if that native windows port has `fs.symlink` and `fs.readlink` support that is exactly identical to the way that they work on Unix, then this should work fine.</p>

<p>But I wouldn't hold my breath.  Any bugs about this not working on a native Windows system (ie, not Cygwin) will most likely be closed with <code>wontfix</code>.</p>


<h2 id="aside_credit_where_credit8217s_due">Aside: Credit where Credit&#8217;s Due</h2>

<p>Back before the Great Package Management Wars of Node 0.1, before npm or kiwi or mode or seed.js could do much of anything, and certainly before any of them had more than 2 users, Mikeal Rogers invited me to the Couch.io offices for lunch to talk about this npm registry thingie I&#8217;d mentioned wanting to build. (That is, to convince me to use CouchDB for it.)</p>

<p>Since he was volunteering to build the first version of it, and since couch is pretty much the ideal candidate for this use-case, it was an easy sell.</p>

<p>While I was there, he said, &#8220;Look. You need to be able to link a project directory as if it was installed as a package, and then have it all Just Work. Can you do that?&#8221;</p>

<p>I was like, &#8220;Well, I don&#8217;t know&#8230; I mean, there&#8217;s these edge cases, and it doesn&#8217;t really fit with the existing folder structure very well&#8230;&#8221;</p>

<p>&#8220;Dude. Either you do it, or I&#8217;m going to have to do it, and then there&#8217;ll be <em>another</em> package manager in node, instead of writing a registry for npm, and it won&#8217;t be as good anyway. Don&#8217;t be python.&#8221;</p>

<p>The rest is history.</p>
