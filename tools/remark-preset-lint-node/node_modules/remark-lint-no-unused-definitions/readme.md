<!--This file is generated-->

# remark-lint-no-unused-definitions

Warn when unused definitions are found.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-recommended`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-recommended) |  |

## Example

##### `valid.md`

###### In

```markdown
[foo][]

[foo]: https://example.com
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
[bar]: https://example.com
```

###### Out

```text
1:1-1:27: Found unused definition
```

## Install

```sh
npm install remark-lint-no-unused-definitions
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-unused-definitions",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-unused-definitions readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-unused-definitions'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
