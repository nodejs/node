<!--This file is generated-->

# remark-lint-no-heading-content-indent

Warn when a heading’s content is indented.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-recommended`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-recommended) |  |

## Example

##### `valid.md`

###### In

Note: `·` represents a space.

```markdown
#·Foo

## Bar·##

  ##·Baz

Setext headings are not affected.

Baz
===
```

###### Out

No messages.

##### `invalid.md`

###### In

Note: `·` represents a space.

```markdown
#··Foo

## Bar··##

  ##··Baz
```

###### Out

```text
1:4: Remove 1 space before this heading’s content
3:7: Remove 1 space after this heading’s content
5:7: Remove 1 space before this heading’s content
```

##### `empty-heading.md`

###### In

Note: `·` represents a space.

```markdown
#··
```

###### Out

No messages.

##### `tight.md`

###### In

Note: `·` represents a space.

```markdown
In pedantic mode, headings without spacing can also be detected:

##No spacing left, too much right··##
```

###### Out

```text
3:3: Add 1 space before this heading’s content
3:34: Remove 1 space after this heading’s content
```

## Install

```sh
npm install remark-lint-no-heading-content-indent
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-heading-content-indent",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-heading-content-indent readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-heading-content-indent'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
