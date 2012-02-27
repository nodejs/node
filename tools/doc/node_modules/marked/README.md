# marked

A full-featured markdown parser and compiler.
Built for speed.

## Benchmarks

node v0.4.x

``` bash
$ node test --bench
marked completed in 12071ms.
showdown (reuse converter) completed in 27387ms.
showdown (new converter) completed in 75617ms.
markdown-js completed in 70069ms.
```

node v0.6.x

``` bash
$ node test --bench
marked completed in 6485ms.
marked (with gfm) completed in 7466ms.
discount completed in 7169ms.
showdown (reuse converter) completed in 15937ms.
showdown (new converter) completed in 18279ms.
markdown-js completed in 23572ms.
```

__Marked is now faster than Discount, which is written in C.__

For those feeling skeptical: These benchmarks run the entire markdown test suite
1000 times. The test suite tests every feature. It doesn't cater to specific
aspects.

Benchmarks for other engines to come (?).

## Install

``` bash
$ npm install marked
```

## Another javascript markdown parser

The point of marked was to create a markdown compiler where it was possible to
frequently parse huge chunks of markdown without having to worry about
caching the compiled output somehow...or blocking for an unnecesarily long time.

marked is very concise and still implements all markdown features. It is also
now fully compatible with the client-side.

marked more or less passes the official markdown test suite in its
entirety. This is important because a surprising number of markdown compilers
cannot pass more than a few tests. It was very difficult to get marked as
compliant as it is. It could have cut corners in several areas for the sake
of performance, but did not in order to be exactly what you expect in terms
of a markdown rendering. In fact, this is why marked could be considered at a
disadvantage in the benchmarks above.

Along with implementing every markdown feature, marked also implements
[GFM features](http://github.github.com/github-flavored-markdown/).

## Usage

``` js
var marked = require('marked');
console.log(marked('i am using __markdown__.'));
```

You also have direct access to the lexer and parser if you so desire.

``` js
var tokens = marked.lexer(str);
console.log(marked.parser(tokens));
```

``` bash
$ node
> require('marked').lexer('> i am using marked.')
[ { type: 'blockquote_start' },
  { type: 'text', text: ' i am using marked.' },
  { type: 'blockquote_end' },
  links: {} ]
```

## CLI

``` bash
$ marked -o hello.html
hello world
^D
$ cat hello.html
<p>hello world</p>
```

## Syntax Highlighting

Marked has an interface that allows for a syntax highlighter to highlight code
blocks before they're output.

Example implementation:

``` js
var highlight = require('my-syntax-highlighter')
  , marked_ = require('marked');

var marked = function(text) {
  var tokens = marked_.lexer(text)
    , l = tokens.length
    , i = 0
    , token;

  for (; i < l; i++) {
    token = tokens[i];
    if (token.type === 'code') {
      token.text = highlight(token.text, token.lang);
      // marked should not escape this
      token.escaped = true;
    }
  }

  text = marked_.parser(tokens);

  return text;
};

module.exports = marked;
```

## License

Copyright (c) 2011-2012, Christopher Jeffrey. (MIT License)

See LICENSE for more info.
