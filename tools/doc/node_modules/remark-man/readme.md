# remark-man [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status] [![Chat][chat-badge]][chat]

Compile markdown to man pages with [**remark**][remark].  Great unicode
support; name, section, and description detection; nested block quotes
and lists; tables; and much more.

## Installation

[npm][]:

```bash
npm install remark-man
```

## Usage

Say we have the following file, `example.md`:

```markdown
# ls(1) -- list directory contents

## SYNOPSIS

`ls` [`-ABCFGHLOPRSTUW@abcdefghiklmnopqrstuwx1`] \[_file_ _..._]
```

And our script, `example.js`, looks as follows:

```javascript
var vfile = require('to-vfile');
var unified = require('unified');
var markdown = require('remark-parse');
var man = require('remark-man');

unified()
  .use(markdown)
  .use(man)
  .process(vfile.readSync('example.md'), function (err, file) {
    if (err) throw err;
    file.extname = '.1';
    vfile.writeSync(file);
  });
```

Now, running `node example` and `cat example.1` yields:

```roff
.TH "LS" "1" "June 2015" "" ""
.SH "NAME"
\fBls\fR - list directory contents
.SH "SYNOPSIS"
.P
\fBls\fR \fB\fB-ABCFGHLOPRSTUW@abcdefghiklmnopqrstuwx1\fR\fR \[lB]\fIfile\fR \fI...\fR\[rB]
```

Now, that looks horrible, but that’s how roff/groff/troff are :wink:.

To properly view that man page, use something like this: `man ./example.1`.

### `remark.use(man[, options])`

Compile markdown to a man page.

###### `options`

*   `name` (`string`, optional)
*   `section` (`number` or `string`, optional)
*   `description` (`string`, optional)
*   `date` (given to `new Date()`, optional)
*   `version` (`string`, optional)
*   `manual` (`string`, optional)

`name` and `section` can also be inferred from the file’s name:
`hello-world.1.md` will set `name` to `'hello-world'` and `section` to `'1'`.

In addition, you can also provide inline configuration with a main heading.
The following file:

```md
# hello-world(7) -- Two common words
```

...will set `name` to `'hello-world'`, `section` to `'7'`, and `description` to
`'Two common words'`.

The main heading has precedence over the file name, and the file name
over the plugin settings.

## Related

*   [`remark-react`](https://github.com/mapbox/remark-react)
    — Compile to React
*   [`remark-vdom`](https://github.com/wooorm/remark-vdom)
    — Compile to VDOM
*   [`remark-html`](https://github.com/wooorm/remark-html)
    — Compile to HTML
*   [`remark-rehype`](https://github.com/wooorm/remark-rehype)
    — Properly transform to HTML

## License

[MIT][license] © [Titus Wormer][author]

<!-- Definitions -->

[build-badge]: https://img.shields.io/travis/wooorm/remark-man.svg

[build-status]: https://travis-ci.org/wooorm/remark-man

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/remark-man.svg

[coverage-status]: https://codecov.io/github/wooorm/remark-man

[chat-badge]: https://img.shields.io/gitter/room/wooorm/remark.svg

[chat]: https://gitter.im/wooorm/remark

[license]: LICENSE

[author]: http://wooorm.com

[npm]: https://docs.npmjs.com/cli/install

[remark]: https://github.com/wooorm/remark
