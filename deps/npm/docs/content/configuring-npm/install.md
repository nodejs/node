---
title: install
section: 5
description: Download and install node and npm
---

### Description

To publish and install packages to and from the public npm registry, you
must install Node.js and the npm command line interface using either a Node
version manager or a Node installer. **We strongly recommend using a Node
version manager to install Node.js and npm.** We do not recommend using a
Node installer, since the Node installation process installs npm in a
directory with local permissions and can cause permissions errors when you
run npm packages globally.

### Overview

- [Checking your version of npm and
  Node.js](#checking-your-version-of-npm-and-nodejs)
- [Using a Node version manager to install Node.js and
  npm](#using-a-node-version-manager-to-install-nodejs-and-npm)
- [Using a Node installer to install Node.js and
  npm](#using-a-node-installer-to-install-nodejs-and-npm)

### Checking your version of npm and Node.js

To see if you already have Node.js and npm installed and check the
installed version, run the following commands:

```
node -v
npm -v
```

### Using a Node version manager to install Node.js and npm

Node version managers allow you to install and switch between multiple
versions of Node.js and npm on your system so you can test your
applications on multiple versions of npm to ensure they work for users on
different versions.

#### OSX or Linux Node version managers

* [nvm](https://github.com/creationix/nvm)
* [n](https://github.com/tj/n)

#### Windows Node version managers

* [nodist](https://github.com/marcelklehr/nodist)
* [nvm-windows](https://github.com/coreybutler/nvm-windows)

### Using a Node installer to install Node.js and npm

If you are unable to use a Node version manager, you can use a Node
installer to install both Node.js and npm on your system.

* [Node.js installer](https://nodejs.org/en/download/)
* [NodeSource installer](https://github.com/nodesource/distributions). If
  you use Linux, we recommend that you use a NodeSource installer.

#### OS X or Windows Node installers

If you're using OS X or Windows, use one of the installers from the
[Node.js download page](https://nodejs.org/en/download/). Be sure to
install the version labeled **LTS**. Other versions have not yet been
tested with npm.

#### Linux or other operating systems Node installers

If you're using Linux or another operating system, use one of the following
installers:

- [NodeSource installer](https://github.com/nodesource/distributions)
  (recommended)
- One of the installers on the [Node.js download
  page](https://nodejs.org/en/download/)

Or see [this page](https://nodejs.org/en/download/package-manager/) to
install npm for Linux in the way many Linux developers prefer.

#### Less-common operating systems

For more information on installing Node.js on a variety of operating
systems, see [this page][pkg-mgr].

[pkg-mgr]: https://nodejs.org/en/download/package-manager/
