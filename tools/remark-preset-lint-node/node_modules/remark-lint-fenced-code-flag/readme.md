<!--This file is generated-->

# remark-lint-fenced-code-flag

Check fenced code-block flags.

Options: `Array.<string>` or `Object`, optional.

Providing an array is as passing `{flags: Array}`.

The object can have an array of `'flags'` which are deemed valid.
In addition it can have the property `allowEmpty` (`boolean`, default:
`false`) which signifies whether or not to warn for fenced code-blocks
without language flags.

## Presets

This rule is included in the following presets:

| Preset | Setting |
| ------ | ------- |
| [`remark-preset-lint-markdown-style-guide`](https://github.com/wooorm/remark-lint/tree/master/packages/remark-preset-lint-markdown-style-guide) |  |

## Example

##### `valid.md`

###### In

````markdown
        ```alpha
        bravo();
        ```
````

###### Out

No messages.

##### `invalid.md`

###### In

````markdown
        ```
        alpha();
        ```
````

###### Out

```text
1:1-3:4: Missing code-language flag
```

##### `valid.md`

When configured with `{ allowEmpty: true }`.

###### In

````markdown
        ```
        alpha();
        ```
````

###### Out

No messages.

##### `invalid.md`

When configured with `{ allowEmpty: false }`.

###### In

````markdown
        ```
        alpha();
        ```
````

###### Out

```text
1:1-3:4: Missing code-language flag
```

##### `valid.md`

When configured with `[ 'alpha' ]`.

###### In

````markdown
        ```alpha
        bravo();
        ```
````

###### Out

No messages.

##### `invalid.md`

When configured with `[ 'charlie' ]`.

###### In

````markdown
        ```alpha
        bravo();
        ```
````

###### Out

```text
1:1-3:4: Invalid code-language flag
```

## Install

```sh
npm install remark-lint-fenced-code-flag
```

## Usage

You probably want to use it on the CLI through a config file:

```diff
 ...
 "remarkConfig": {
   "plugins": [
     ...
     "lint",
+    "lint-fenced-code-flag",
     ...
   ]
 }
 ...
```

Or use it on the CLI directly

```sh
remark -u lint -u lint-fenced-code-flag readme.md
```

Or use this on the API:

```diff
 var remark = require('remark');
 var report = require('vfile-reporter');

 remark()
   .use(require('remark-lint'))
+  .use(require('remark-lint-fenced-code-flag'))
   .process('_Emphasis_ and **importance**', function (err, file) {
     console.error(report(err || file));
   });
```

## License

[MIT](https://github.com/wooorm/remark-lint/blob/master/LICENSE) © [Titus Wormer](http://wooorm.com)
