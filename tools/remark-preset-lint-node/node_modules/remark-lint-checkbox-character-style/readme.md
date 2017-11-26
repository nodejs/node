<!--This file is generated-->

# remark-lint-checkbox-character-style

Warn when list item checkboxes violate a given style.

Options: `Object` or `'consistent'`, default: `'consistent'`.

`'consistent'` detects the first used checked and unchecked checkbox
styles and warns when subsequent checkboxes use different styles.

Styles can also be passed in like so:

```js
{ checked: 'x', unchecked: ' ' }
```

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-consistent`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-consistent) |  |

## Example

##### `valid.md`

When configured with `{ checked: 'x' }`.

###### In

```markdown
- [x] List item
- [x] List item
```

###### Out

No messages.

##### `valid.md`

When configured with `{ checked: 'X' }`.

###### In

```markdown
- [X] List item
- [X] List item
```

###### Out

No messages.

##### `valid.md`

When configured with `{ unchecked: ' ' }`.

###### In

Note: `·` represents a space.

```markdown
- [ ] List item
- [ ] List item
- [ ]··
- [ ]
```

###### Out

No messages.

##### `valid.md`

When configured with `{ unchecked: '\t' }`.

###### In

Note: `»` represents a tab.

```markdown
- [»] List item
- [»] List item
```

###### Out

No messages.

##### `invalid.md`

###### In

Note: `»` represents a tab.

```markdown
- [x] List item
- [X] List item
- [ ] List item
- [»] List item
```

###### Out

```text
2:4-2:5: Checked checkboxes should use `x` as a marker
4:4-4:5: Unchecked checkboxes should use ` ` as a marker
```

##### `invalid.md`

When configured with `{ unchecked: '!' }`.

###### Out

```text
1:1: Invalid unchecked checkbox marker `!`: use either `'\t'`, or `' '`
```

##### `invalid.md`

When configured with `{ checked: '!' }`.

###### Out

```text
1:1: Invalid checked checkbox marker `!`: use either `'x'`, or `'X'`
```

## Install

```sh
npm install remark-lint-checkbox-character-style
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-checkbox-character-style",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-checkbox-character-style readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-checkbox-character-style'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
