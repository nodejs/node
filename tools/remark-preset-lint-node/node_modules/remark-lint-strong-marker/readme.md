<!--This file is generated-->

# remark-lint-strong-marker

Warn for violating strong markers.

Options: `'consistent'`, `'*'`, or `'_'`, default: `'consistent'`.

`'consistent'` detects the first used strong style and warns when subsequent
strongs use different styles.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-consistent`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-consistent) |  |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

###### In

```markdown
**foo** and **bar**.
```

###### Out

No messages.

##### `also-valid.md`

###### In

```markdown
__foo__ and __bar__.
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
**foo** and __bar__.
```

###### Out

```text
1:13-1:20: Strong should use `*` as a marker
```

##### `valid.md`

When configured with `'*'`.

###### In

```markdown
**foo**.
```

###### Out

No messages.

##### `valid.md`

When configured with `'_'`.

###### In

```markdown
__foo__.
```

###### Out

No messages.

##### `invalid.md`

When configured with `'!'`.

###### Out

```text
1:1: Invalid strong marker `!`: use either `'consistent'`, `'*'`, or `'_'`
```

## Install

```sh
npm install remark-lint-strong-marker
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-strong-marker",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-strong-marker readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-strong-marker'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
