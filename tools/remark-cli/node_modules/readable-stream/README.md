# readable-stream

***Node-core v6.3.1 streams for userland*** [![Build Status](https://travis-ci.org/nodejs/readable-stream.svg?branch=master)](https://travis-ci.org/nodejs/readable-stream)


[![NPM](https://nodei.co/npm/readable-stream.png?downloads=true&downloadRank=true)](https://nodei.co/npm/readable-stream/)
[![NPM](https://nodei.co/npm-dl/readable-stream.png?&months=6&height=3)](https://nodei.co/npm/readable-stream/)


[![Sauce Test Status](https://saucelabs.com/browser-matrix/readable-stream.svg)](https://saucelabs.com/u/readable-stream)

```bash
npm install --save readable-stream
```

***Node-core streams for userland***

This package is a mirror of the Streams2 and Streams3 implementations in
Node-core, including [documentation](doc/stream.md).

If you want to guarantee a stable streams base, regardless of what version of
Node you, or the users of your libraries are using, use **readable-stream** *only* and avoid the *"stream"* module in Node-core, for background see [this blogpost](http://r.va.gg/2014/06/why-i-dont-use-nodes-core-stream-module.html).

As of version 2.0.0 **readable-stream** uses semantic versioning.  

# Streams WG Team Members

* **Chris Dickinson** ([@chrisdickinson](https://github.com/chrisdickinson)) &lt;christopher.s.dickinson@gmail.com&gt;
  - Release GPG key: 9554F04D7259F04124DE6B476D5A82AC7E37093B
* **Calvin Metcalf** ([@calvinmetcalf](https://github.com/calvinmetcalf)) &lt;calvin.metcalf@gmail.com&gt;
  - Release GPG key: F3EF5F62A87FC27A22E643F714CE4FF5015AA242
* **Rod Vagg** ([@rvagg](https://github.com/rvagg)) &lt;rod@vagg.org&gt;
  - Release GPG key: DD8F2338BAE7501E3DD5AC78C273792F7D83545D
* **Sam Newman** ([@sonewman](https://github.com/sonewman)) &lt;newmansam@outlook.com&gt;
* **Mathias Buus** ([@mafintosh](https://github.com/mafintosh)) &lt;mathiasbuus@gmail.com&gt;
* **Domenic Denicola** ([@domenic](https://github.com/domenic)) &lt;d@domenic.me&gt;
