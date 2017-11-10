<!--This file is generated-->

# remark-lint-file-extension

Warn when the file extension differ from the preferred extension.

Does not warn when given documents have no file extensions (such as
`AUTHORS` or `LICENSE`).

Options: `string`, default: `'md'` — Expected file extension.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `readme.md`

###### Out

No messages.

##### `readme`

###### Out

No messages.

##### `readme.mkd`

###### Out

```text
1:1: Invalid extension: use `md`
```

##### `readme.mkd`

When configured with `'mkd'`.

###### Out

No messages.

## Install

```sh
npm install remark-lint-file-extension
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-file-extension",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-file-extension readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-file-extension'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
