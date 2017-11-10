<!--This file is generated-->

# remark-lint-no-file-name-outer-dashes

Warn when file names contain initial or final dashes.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `readme.md`

###### Out

No messages.

##### `-readme.md`

###### Out

```text
1:1: Do not use initial or final dashes in a file name
```

##### `readme-.md`

###### Out

```text
1:1: Do not use initial or final dashes in a file name
```

## Install

```sh
npm install remark-lint-no-file-name-outer-dashes
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-file-name-outer-dashes",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-file-name-outer-dashes readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-file-name-outer-dashes'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
