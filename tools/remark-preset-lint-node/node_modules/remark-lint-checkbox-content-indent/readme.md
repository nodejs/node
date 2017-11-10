<!--This file is generated-->

# remark-lint-checkbox-content-indent

Warn when list item checkboxes are followed by too much white-space.

## Presets

This rule is not included in any default preset

## Example

##### `valid.md`

###### In

```markdown
- [ ] List item
+  [x] List Item
*   [X] List item
-    [ ] List item
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
- [ ] List item
+ [x]  List item
* [X]   List item
- [ ]    List item
```

###### Out

```text
2:7-2:8: Checkboxes should be followed by a single character
3:7-3:9: Checkboxes should be followed by a single character
4:7-4:10: Checkboxes should be followed by a single character
```

## Install

```sh
npm install remark-lint-checkbox-content-indent
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-checkbox-content-indent",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-checkbox-content-indent readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-checkbox-content-indent'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
