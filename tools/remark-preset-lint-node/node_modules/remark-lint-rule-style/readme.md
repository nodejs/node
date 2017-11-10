<!--This file is generated-->

# remark-lint-rule-style

Warn when the horizontal rules violate a given or detected style.

Options: `string`, either a valid markdown rule, or `'consistent'`,
default: `'consistent'`.

`'consistent'` detects the first used rule style and warns when subsequent
rules use different styles.

Note: horizontal rules are also called “thematic break”.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-consistent`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-consistent) |  |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

When configured with `'* * *'`.

###### In

```markdown
* * *

* * *
```

###### Out

No messages.

##### `valid.md`

When configured with `'_______'`.

###### In

```markdown
_______

_______
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
***

* * *
```

###### Out

```text
3:1-3:6: Rules should use `***`
```

##### `invalid.md`

When configured with `'!!!'`.

###### Out

```text
1:1: Invalid preferred rule-style: provide a valid markdown rule, or `'consistent'`
```

## Install

```sh
npm install remark-lint-rule-style
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-rule-style",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-rule-style readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-rule-style'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
