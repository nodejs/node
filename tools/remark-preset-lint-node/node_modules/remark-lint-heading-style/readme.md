<!--This file is generated-->

# remark-lint-heading-style

Warn when a heading does not conform to a given style.

Options: `'consistent'`, `'atx'`, `'atx-closed'`, or `'setext'`,
default: `'consistent'`.

`'consistent'` detects the first used heading style and warns when
subsequent headings use different styles.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-consistent`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-consistent) |  |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

When configured with `'atx'`.

###### In

```markdown
# Alpha

## Bravo

### Charlie
```

###### Out

No messages.

##### `valid.md`

When configured with `'atx-closed'`.

###### In

```markdown
# Delta ##

## Echo ##

### Foxtrot ###
```

###### Out

No messages.

##### `valid.md`

When configured with `'setext'`.

###### In

```markdown
Golf
====

Hotel
-----

### India
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
Juliett
=======

## Kilo

### Lima ###
```

###### Out

```text
4:1-4:8: Headings should use setext
6:1-6:13: Headings should use setext
```

## Install

```sh
npm install remark-lint-heading-style
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-heading-style",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-heading-style readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-heading-style'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
