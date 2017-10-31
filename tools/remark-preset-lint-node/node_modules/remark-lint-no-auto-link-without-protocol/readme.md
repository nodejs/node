<!--This file is generated-->

# remark-lint-no-auto-link-without-protocol

Warn for angle-bracketed links without protocol.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |
| [`remark-preset-lint-recommended`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-recommended) |  |

## Example

##### `valid.md`

###### In

```markdown
<http://www.example.com>
<mailto:foo@bar.com>
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
<www.example.com>
<foo@bar.com>
```

###### Out

```text
2:1-2:14: All automatic links must start with a protocol
```

## Install

```sh
npm install remark-lint-no-auto-link-without-protocol
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-auto-link-without-protocol",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-auto-link-without-protocol readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-auto-link-without-protocol'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
