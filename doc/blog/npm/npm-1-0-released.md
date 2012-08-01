title: npm 1.0: Released
author: Isaac Schlueter
date: Sun May 01 2011 08:09:45 GMT-0700 (PDT)
status: publish
category: npm
slug: npm-1-0-released

<p>npm 1.0 has been released. Here are the highlights:</p>

<ul> <li><a href="http://blog.nodejs.org/2011/03/23/npm-1-0-global-vs-local-installation/">Global vs local installation</a></li> <li><a href="http://blog.nodejs.org/2011/03/17/npm-1-0-the-new-ls/">ls displays a tree</a>, instead of being a remote search</li> <li>No more &#8220;activation&#8221; concept - dependencies are nested</li> <li><a href="http://blog.nodejs.org/2011/04/06/npm-1-0-link/">Updates to link command</a></li> <li>Install script cleans up any 0.x cruft it finds. (That is, it removes old packages, so that they can be installed properly.)</li> <li>Simplified &#8220;search&#8221; command. One line per package, rather than one line per version.</li> <li>Renovated &#8220;completion&#8221; approach</li> <li>More help topics</li> <li>Simplified folder structure</li> </ul>

<p>The focus is on npm being a development tool, rather than an apt-wannabe.</p>

<h2 id="installing_it">Installing it</h2>

<p>To get the new version, run this command:</p>

<pre style="background:#333;color:#ccc;overflow:auto;padding:2px;"><code>curl http://npmjs.org/install.sh | sh </code></pre>

<p>This will prompt to ask you if it&#8217;s ok to remove all the old 0.x cruft. If you want to not be asked, then do this:</p>

<pre style="background:#333;color:#ccc;overflow:auto;padding:2px;"><code>curl http://npmjs.org/install.sh | clean=yes sh </code></pre>

<p>Or, if you want to not do the cleanup, and leave the old stuff behind, then do this:</p>

<pre style="background:#333;color:#ccc;overflow:auto;padding:2px;"><code>curl http://npmjs.org/install.sh | clean=no sh </code></pre>

<p>A lot of people in the node community were brave testers and helped make this release a lot better (and swifter) than it would have otherwise been. Thanks :)</p>

<h2 id="code_freeze">Code Freeze</h2>

<p>npm will not have any major feature enhancements or architectural changes <span style="border-bottom:1px dotted;cursor:default;" title="That is, the freeze ends no sooner than November 1, 2011">for at least 6 months</span>. There are interesting developments planned that leverage npm in some ways, but it&#8217;s time to let the client itself settle. Also, I want to focus attention on some other problems for a little while.</p>

<p>Of course, <a href="https://github.com/isaacs/npm/issues">bug reports</a> are always welcome.</p>

<p>See you at NodeConf!</p>
