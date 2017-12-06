<!--This file is generated-->

# remark-lint-hard-break-spaces

Warn when too many spaces are used to create a hard break.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |
| [`remark-preset-lint-recommended`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-recommended) |  |

## Example

##### `valid.md`

###### In

Note: `·` represents a space.

```markdown
Lorem ipsum··
dolor sit amet
```

###### Out

No messages.

##### `invalid.md`

###### In

Note: `·` represents a space.

```markdown
Lorem ipsum···
dolor sit amet.
```

###### Out

```text
1:12-2:1: Use two spaces for hard line breaks
```

## Install

```sh
npm install remark-lint-hard-break-spaces
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-hard-break-spaces",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-hard-break-spaces readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-hard-break-spaces'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
