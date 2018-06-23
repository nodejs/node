# rehype-raw [![Build Status][travis-badge]][travis] [![Coverage Status][codecov-badge]][codecov]

Reparse a [HAST][] tree, with support for embedded `raw`
nodes.  Keeping positional info OK.  🙌

## Installation

[npm][]:

```bash
npm install rehype-raw
```

## Usage

Say we have the following markdown file, `example.md`:

```markdown
<div class="note">

A mix of *markdown* and <em>HTML</em>.

</div>
```

And our script, `example.js`, looks as follows:

```javascript
var vfile = require('to-vfile');
var report = require('vfile-reporter');
var unified = require('unified');
var markdown = require('remark-parse');
var remark2rehype = require('remark-rehype');
var raw = require('rehype-raw');
var doc = require('rehype-document');
var format = require('rehype-format');
var stringify = require('rehype-stringify');

unified()
  .use(markdown)
  .use(remark2rehype, {allowDangerousHTML: true})
  .use(raw)
  .use(doc, {title: '🙌'})
  .use(format)
  .use(stringify)
  .process(vfile.readSync('example.md'), function (err, file) {
    console.error(report(err || file));
    console.log(String(file));
  });
```

Now, running `node example` yields:

```html
example.md: no issues found
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <title>🙌</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
  </head>
  <body>
    <div class="note">
      <p>A mix of <em>markdown</em> and <em>HTML</em>.</p>
    </div>
  </body>
</html>
```

## API

### `rehype().use(raw)`

Parse the tree again, also parsing “raw” nodes (as exposed by remark).

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[travis-badge]: https://img.shields.io/travis/wooorm/rehype-raw.svg

[travis]: https://travis-ci.org/wooorm/rehype-raw

[codecov-badge]: https://img.shields.io/codecov/c/github/wooorm/rehype-raw.svg

[codecov]: https://codecov.io/github/wooorm/rehype-raw

[npm]: https://docs.npmjs.com/cli/install

[license]: LICENSE

[author]: http://wooorm.com

[hast]: https://github.com/wooorm/hast
