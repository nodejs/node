title: Development Environment
author: ryandahl
date: Mon Apr 04 2011 20:16:27 GMT-0700 (PDT)
status: publish
category: Uncategorized
slug: development-environment

If you're compiling a software package because you need a particular version (e.g. the latest), then it requires a little bit more maintenance than using a package manager like <code>dpkg</code>. Software that you compile yourself should *not* go into <code>/usr</code>, it should go into your home directory. This is part of being a software developer. 

One way of doing this is to install everything into <code>$HOME/local/$PACKAGE</code>. Here is how I install node on my machine:<pre>./configure --prefix=$HOME/local/node-v0.4.5 &amp;&amp; make install</pre>

To have my paths automatically set I put this inside my <code>$HOME/.zshrc</code>:<pre>PATH="$HOME/local/bin:/opt/local/bin:/usr/bin:/sbin:/bin"
LD_LIBRARY_PATH="/opt/local/lib:/usr/local/lib:/usr/lib"
for i in $HOME/local/*; do
  [ -d $i/bin ] &amp;&amp; PATH="${i}/bin:${PATH}"
  [ -d $i/sbin ] &amp;&amp; PATH="${i}/sbin:${PATH}"
  [ -d $i/include ] &amp;&amp; CPATH="${i}/include:${CPATH}"
  [ -d $i/lib ] &amp;&amp; LD_LIBRARY_PATH="${i}/lib:${LD_LIBRARY_PATH}"
  [ -d $i/lib/pkgconfig ] &amp;&amp; PKG_CONFIG_PATH="${i}/lib/pkgconfig:${PKG_CONFIG_PATH}"
  [ -d $i/share/man ] &amp;&amp; MANPATH="${i}/share/man:${MANPATH}"
done</pre>

Node is under sufficiently rapid development that <i>everyone</i> should be compiling it themselves. A corollary of this is that <code>npm</code> (which should be installed alongside Node) does not require root to install packages.

CPAN and RubyGems have blurred the lines between development tools and system package managers. With <code>npm</code> we wish to draw a clear line: it is not a system package manager. It is not for installing firefox or ffmpeg or OpenSSL; it is for rapidly downloading, building, and setting up Node packages. <code>npm</code> is a <i>development</i> tool. When a program written in Node becomes sufficiently mature it should be distributed as a tarball, <code>.deb</code>, <code>.rpm</code>, or other package system. It should not be distributed to end users with <code>npm</code>.
