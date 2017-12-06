<!--This file is generated-->

# remark-lint-no-heading-indent

Warn when a heading is indented.

## Presets

This rule is not included in any default preset

## Example

##### `valid.md`

###### In

Note: `·` represents a space.

```markdown
#·Hello world

Foo
-----

#·Hello world·#

Bar
=====
```

###### Out

No messages.

##### `invalid.md`

###### In

Note: `·` represents a space.

```markdown
···# Hello world

·Foo
-----

·# Hello world #

···Bar
=====
```

###### Out

```text
1:4: Remove 3 spaces before this heading
3:2: Remove 1 space before this heading
6:2: Remove 1 space before this heading
8:4: Remove 3 spaces before this heading
```

## Install

```sh
npm install remark-lint-no-heading-indent
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-heading-indent",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-heading-indent readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-heading-indent'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
