# remark [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

The [**remark**][remark] processor is a markdown processor powered by
[plug-ins][plugins].

*   Interface by [**unified**][unified];
*   [**mdast**][mdast] syntax tree;
*   Parses markdown to the tree with [**remark-parse**][parse];
*   [Plug-ins][plugins] transform the tree;
*   Compiles the tree to markdown using [**remark-stringify**][stringify].

Don’t need the parser?  Or the compiler?  [That’s OK][unified-usage].

## Installation

[npm][]:

```bash
npm install remark
```

## Usage

```js
var remark = require('remark');
var lint = require('remark-lint');
var html = require('remark-html');
var report = require('vfile-reporter');

remark().use(lint).use(html).process('## Hello world!', function (err, file) {
    console.error(report(file));
    console.log(file.toString());
});
```

Yields:

```txt
<stdin>
        1:1  warning  Missing newline character at end of file  final-newline
   1:1-1:16  warning  First heading level should be `1`         first-heading-level
   1:1-1:16  warning  Don’t add a trailing `!` to headings      no-heading-punctuation

⚠ 3 warnings
<h2>Hello world!</h2>
```

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/remark.svg

[build-status]: https://travis-ci.org/wooorm/remark

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/remark.svg

[coverage-status]: https://codecov.io/github/wooorm/remark

[chat-badge]: https://img.shields.io/gitter/room/wooorm/remark.svg

[chat]: https://gitter.im/wooorm/remark

[license]: https://github.com/wooorm/remark/blob/master/LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[remark]: https://github.com/wooorm/remark

[unified]: https://github.com/wooorm/unified

[mdast]: https://github.com/wooorm/mdast

[parse]: https://github.com/wooorm/remark/blob/master/packages/remark-parse

[stringify]: https://github.com/wooorm/remark/blob/master/packages/remark-stringify

[plugins]: https://github.com/wooorm/remark/blob/master/doc/plugins.md

[unified-usage]: https://github.com/wooorm/unified#usage
