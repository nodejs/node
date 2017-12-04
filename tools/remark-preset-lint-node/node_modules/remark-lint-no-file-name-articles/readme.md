<!--This file is generated-->

# remark-lint-no-file-name-articles

Warn when file name start with an article.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `title.md`

###### Out

No messages.

##### `a-title.md`

###### Out

```text
1:1: Do not start file names with `a`
```

##### `the-title.md`

###### Out

```text
1:1: Do not start file names with `the`
```

##### `teh-title.md`

###### Out

```text
1:1: Do not start file names with `teh`
```

##### `an-article.md`

###### Out

```text
1:1: Do not start file names with `an`
```

## Install

```sh
npm install remark-lint-no-file-name-articles
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-file-name-articles",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-file-name-articles readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-file-name-articles'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
