<a href="https://marked.js.org">
  <img width="60px" height="60px" src="https://marked.js.org/img/logo-black.svg" align="right" />
</a>

# Marked

[![npm](https://img.shields.io/npm/v/marked.svg)](https://www.npmjs.com/package/marked)
[![gzip size](http://img.badgesize.io/https://cdn.jsdelivr.net/npm/marked@0.3.19/marked.min.js?compression=gzip)](https://cdn.jsdelivr.net/npm/marked@0.3.19/marked.min.js)
[![install size](https://packagephobia.now.sh/badge?p=marked@0.3.19)](https://packagephobia.now.sh/result?p=marked@0.3.19)
[![downloads](https://img.shields.io/npm/dt/marked.svg)](https://www.npmjs.com/package/marked)
[![travis](https://travis-ci.org/markedjs/marked.svg?branch=master)](https://travis-ci.org/markedjs/marked)

- ⚡ built for speed
- ⬇️ low-level compiler for parsing markdown without caching or blocking for long periods of time
- ⚖️ light-weight while implementing all markdown features from the supported flavors & specifications
- 🌐 works in a browser, on a server, or from a command line interface (CLI)

## Demo

Checkout the [demo page](https://marked.js.org/demo/) to see marked in action ⛹️

## Docs

Our [documentation pages](https://marked.js.org) are also rendered using marked 💯

## Installation

**CLI:** `npm install -g marked`

**In-browser:** `npm install marked --save`

## Usage 

**CLI**

``` bash
$ marked -o hello.html
hello world
^D
$ cat hello.html
<p>hello world</p>
```

**Browser**

```html
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <title>Marked in the browser</title>
</head>
<body>
  <div id="content"></div>
  <script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>
  <script>
    document.getElementById('content').innerHTML =
      marked('# Marked in the browser\n\nRendered by **marked**.');
  </script>
</body>
</html>
```

## License

Copyright (c) 2011-2018, Christopher Jeffrey. (MIT License)

