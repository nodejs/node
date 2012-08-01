title: npm 1.0: The New "ls"
author: Isaac Schlueter
date: Thu Mar 17 2011 23:22:17 GMT-0700 (PDT)
status: publish
category: npm
slug: npm-1-0-the-new-ls

<p><em>This is the first in a series of hopefully more than 1 posts, each detailing some aspect of npm 1.0.</em></p>

<p>In npm 0.x, the <code>ls</code> command was a combination of both searching the registry as well as reporting on what you have installed.</p>

<p>As the registry has grown in size, this has gotten unwieldy.  Also, since npm 1.0 manages dependencies differently, nesting them in <code>node_modules</code> folder and installing locally by default, there are different things that you want to view.</p>

<p>The functionality of the <code>ls</code> command was split into two different parts.  <code>search</code> is now the way to find things on the registry (and it only reports one line per package, instead of one line per version), and <code>ls</code> shows a tree view of the packages that are installed locally.</p>

<p>Here&#8217;s an example of the output:</p>

<pre style="background:#333;color:#ccc;overflow:auto;padding:2px;"><code>$ npm ls
npm@1.0.0 /Users/isaacs/dev-src/js/npm
├── semver@1.0.1 
├─┬ ronn@0.3.5 
│ └── opts@1.2.1 
└─┬ express@2.0.0rc3 <span style="background:#000;color:#0f0;">extraneous</span> 
  ├─┬ connect@1.1.0 
  │ ├── qs@0.0.7 
  │ └── mime@1.2.1 
  ├── mime@1.2.1 
  └── qs@0.0.7
</code></pre>

<p>This is after I&#8217;ve done <code>npm install semver ronn express</code> in the npm source directory.  Since express isn&#8217;t actually a dependency of npm, it shows up with that &#8220;extraneous&#8221; marker.</p>

<p>Let&#8217;s see what happens when we create a broken situation:</p>

<pre style="background:#333;color:#ccc;overflow:auto;padding:2px;"><code>$ rm -rf ./node_modules/express/node_modules/connect
$ npm ls
npm@1.0.0 /Users/isaacs/dev-src/js/npm
├── semver@1.0.1 
├─┬ ronn@0.3.5 
│ └── opts@1.2.1 
└─┬ express@2.0.0rc3 <span style="background:#000;color:#0f0;">extraneous</span> 
  ├── <span style="background:#000;color:#f00;">UNMET DEPENDENCY</span> connect &gt;= 1.1.0 &lt; 2.0.0
  ├── mime@1.2.1 
  └── qs@0.0.7
</code></pre>

<p>Tree views are great for human readability, but some times you want to pipe that stuff to another program.  For that output, I took the same datastructure, but instead of building up a treeview string for each line, it spits out just the folders like this:</p>

<pre style="background:#333;color:#ccc;overflow:auto;padding:2px;"><code>$ npm ls -p
/Users/isaacs/dev-src/js/npm
/Users/isaacs/dev-src/js/npm/node_modules/semver
/Users/isaacs/dev-src/js/npm/node_modules/ronn
/Users/isaacs/dev-src/js/npm/node_modules/ronn/node_modules/opts
/Users/isaacs/dev-src/js/npm/node_modules/express
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/connect
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/connect/node_modules/qs
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/connect/node_modules/mime
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/mime
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/qs
</code></pre>

<p>Since you sometimes want a bigger view, I added the <code>--long</code> option to (shorthand: <code>-l</code>) to spit out more info:</p>

<pre style="background:#333;color:#ccc;overflow:auto;padding:2px;"><code>$ npm ls -l
npm@1.0.0 
│ /Users/isaacs/dev-src/js/npm
│ A package manager for node
│ git://github.com/isaacs/npm.git
│ http://npmjs.org/
├── semver@1.0.1 
│   ./node_modules/semver
│   The semantic version parser used by npm.
│   git://github.com/isaacs/node-semver.git
├─┬ ronn@0.3.5 
│ │ ./node_modules/ronn
│ │ markdown to roff and html converter
│ └── opts@1.2.1 
│     ./node_modules/ronn/node_modules/opts
│     Command line argument parser written in the style of commonjs. To be used with node.js
└─┬ express@2.0.0rc3 <span style="background:#000;color:#0f0;">extraneous</span> 
  │ ./node_modules/express
  │ Sinatra inspired web development framework
  ├─┬ connect@1.1.0 
  │ │ ./node_modules/express/node_modules/connect
  │ │ High performance middleware framework
  │ │ git://github.com/senchalabs/connect.git
  │ ├── qs@0.0.7 
  │ │   ./node_modules/express/node_modules/connect/node_modules/qs
  │ │   querystring parser
  │ └── mime@1.2.1 
  │     ./node_modules/express/node_modules/connect/node_modules/mime
  │     A comprehensive library for mime-type mapping
  ├── mime@1.2.1 
  │   ./node_modules/express/node_modules/mime
  │   A comprehensive library for mime-type mapping
  └── qs@0.0.7 
      ./node_modules/express/node_modules/qs
      querystring parser

$ npm ls -lp
/Users/isaacs/dev-src/js/npm:npm@1.0.0::::
/Users/isaacs/dev-src/js/npm/node_modules/semver:semver@1.0.1::::
/Users/isaacs/dev-src/js/npm/node_modules/ronn:ronn@0.3.5::::
/Users/isaacs/dev-src/js/npm/node_modules/ronn/node_modules/opts:opts@1.2.1::::
/Users/isaacs/dev-src/js/npm/node_modules/express:express@2.0.0rc3:EXTRANEOUS:::
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/connect:connect@1.1.0::::
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/connect/node_modules/qs:qs@0.0.7::::
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/connect/node_modules/mime:mime@1.2.1::::
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/mime:mime@1.2.1::::
/Users/isaacs/dev-src/js/npm/node_modules/express/node_modules/qs:qs@0.0.7::::
</code></pre>

<p>And, if you want to get at the globally-installed modules, you can use ls with the global flag:</p>

<pre style="background:#333;color:#ccc;overflow:auto;padding:2px;"><code>$ npm ls -g
/usr/local
├─┬ A@1.2.3 -&gt; /Users/isaacs/dev-src/js/A
│ ├── B@1.2.3 -&gt; /Users/isaacs/dev-src/js/B
│ └─┬ npm@0.3.15 
│   └── semver@1.0.1 
├─┬ B@1.2.3 -&gt; /Users/isaacs/dev-src/js/B
│ └── A@1.2.3 -&gt; /Users/isaacs/dev-src/js/A
├── glob@2.0.5 
├─┬ npm@1.0.0 -&gt; /Users/isaacs/dev-src/js/npm
│ ├── semver@1.0.1 
│ └─┬ ronn@0.3.5 
│   └── opts@1.2.1 
└── supervisor@0.1.2 -&gt; /Users/isaacs/dev-src/js/node-supervisor

$ npm ls -gpl
/usr/local:::::
/usr/local/lib/node_modules/A:A@1.2.3::::/Users/isaacs/dev-src/js/A
/usr/local/lib/node_modules/A/node_modules/npm:npm@0.3.15::::/Users/isaacs/dev-src/js/A/node_modules/npm
/usr/local/lib/node_modules/A/node_modules/npm/node_modules/semver:semver@1.0.1::::/Users/isaacs/dev-src/js/A/node_modules/npm/node_modules/semver
/usr/local/lib/node_modules/B:B@1.2.3::::/Users/isaacs/dev-src/js/B
/usr/local/lib/node_modules/glob:glob@2.0.5::::
/usr/local/lib/node_modules/npm:npm@1.0.0::::/Users/isaacs/dev-src/js/npm
/usr/local/lib/node_modules/npm/node_modules/semver:semver@1.0.1::::/Users/isaacs/dev-src/js/npm/node_modules/semver
/usr/local/lib/node_modules/npm/node_modules/ronn:ronn@0.3.5::::/Users/isaacs/dev-src/js/npm/node_modules/ronn
/usr/local/lib/node_modules/npm/node_modules/ronn/node_modules/opts:opts@1.2.1::::/Users/isaacs/dev-src/js/npm/node_modules/ronn/node_modules/opts
/usr/local/lib/node_modules/supervisor:supervisor@0.1.2::::/Users/isaacs/dev-src/js/node-supervisor
</code></pre>

<p>Those <code>-&gt;</code> flags are indications that the package is link-installed, which will be covered in the next installment.</p>
