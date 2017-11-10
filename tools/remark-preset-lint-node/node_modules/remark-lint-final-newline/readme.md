<!--This file is generated-->

# remark-lint-final-newline

Warn when a newline at the end of a file is missing. Empty files are allowed.

See [StackExchange](http://unix.stackexchange.com/questions/18743) for why.

## Example

##### `valid.md`

###### In

Note: `␊` represents LF.

```markdown
Alpha␊
```

###### Out

No messages.

##### `invalid.md`

###### In

Note: The below file does not have a final newline.

```markdown
Bravo
```

###### Out

```text
1:1: Missing newline character at end of file
```

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-recommended`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-recommended) |  |

## Install

```sh
npm install remark-lint-final-newline
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-final-newline",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-final-newline readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-final-newline'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
