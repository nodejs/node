<!--This file is generated-->

# remark-lint-final-definition

Warn when definitions are not placed at the end of the file.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

###### In

```markdown
Paragraph.

[example]: http://example.com "Example Domain"
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
Paragraph.

[example]: http://example.com "Example Domain"

Another paragraph.
```

###### Out

```text
3:1-3:47: Move definitions to the end of the file (after the node at line `5`)
```

## Install

```sh
npm install remark-lint-final-definition
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-final-definition",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-final-definition readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-final-definition'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
