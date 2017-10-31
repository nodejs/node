<!--This file is generated-->

# remark-lint-definition-spacing

Warn when consecutive white space is used in a definition.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

###### In

```markdown
[example domain]: http://example.com "Example Domain"
```

###### Out

No messages.

##### `invalid.md`

###### In

Note: `·` represents a space.

```markdown
[example····domain]: http://example.com "Example Domain"
```

###### Out

```text
1:1-1:57: Do not use consecutive white-space in definition labels
```

## Install

```sh
npm install remark-lint-definition-spacing
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-definition-spacing",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-definition-spacing readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-definition-spacing'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
