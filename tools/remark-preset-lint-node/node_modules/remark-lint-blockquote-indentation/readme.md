<!--This file is generated-->

# remark-lint-blockquote-indentation

Warn when blockquotes are indented too much or too little.

Options: `number` or `'consistent'`, default: `'consistent'`.

`'consistent'` detects the first used indentation and will warn when
other blockquotes use a different indentation.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-consistent`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-consistent) |  |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

When configured with `2`.

###### In

```markdown
> Hello

Paragraph.

> World
```

###### Out

No messages.

##### `valid.md`

When configured with `4`.

###### In

```markdown
>   Hello

Paragraph.

>   World
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
>  Hello

Paragraph.

>   World

Paragraph.

> World
```

###### Out

```text
5:3: Remove 1 space between blockquote and content
9:3: Add 1 space between blockquote and content
```

## Install

```sh
npm install remark-lint-blockquote-indentation
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-blockquote-indentation",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-blockquote-indentation readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-blockquote-indentation'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
