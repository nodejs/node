<!--This file is generated-->

# remark-lint-no-inline-padding

Warn when inline nodes are padded with spaces between their markers and
content.

Warns for emphasis, strong, delete, images, and links.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |
| [`remark-preset-lint-recommended`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-recommended) |  |

## Example

##### `valid.md`

###### In

```markdown
Alpha, *bravo*, _charlie_, [delta](http://echo.fox/trot)
```

###### Out

No messages.

##### `invalid.md`

###### In

```markdown
Alpha, * bravo *, _ charlie _, [ delta ](http://echo.fox/trot)
```

###### Out

```text
1:8-1:17: Don’t pad `emphasis` with inner spaces
1:19-1:30: Don’t pad `emphasis` with inner spaces
1:32-1:63: Don’t pad `link` with inner spaces
```

## Install

```sh
npm install remark-lint-no-inline-padding
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-no-inline-padding",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-no-inline-padding readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-no-inline-padding'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
