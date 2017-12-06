<!--This file is generated-->

# remark-lint-no-multiple-toplevel-headings

Warn when multiple top-level headings are used.

Options: `number`, default: `1`.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

When configured with `1`.

###### In

```markdown
# Foo

## Bar
```

###### Out

No messages.

##### `invalid.md`

When configured with `1`.

###### In

```markdown
# Foo

# Bar
```

###### Out

```text
3:1-3:6: Don’t use multiple top level headings (3:1)
```

## Install

```sh
npm install remark-lint-no-multiple-toplevel-headings
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-multiple-toplevel-headings",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-multiple-toplevel-headings readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-multiple-toplevel-headings'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
