# is-builtin-module

> Check if a string matches the name of a Node.js builtin module


## Install

```
$ npm install is-builtin-module
```


## Usage

```js
const isBuiltinModule = require('is-builtin-module');

isBuiltinModule('fs');
//=> true

isBuiltinModule('fs/promises');
//=> true

isBuiltinModule('node:fs/promises');
//=> true

isBuiltinModule('unicorn');
//=> false
```


## Related

- [builtin-modules](https://github.com/sindresorhus/builtin-modules) - List of the Node.js builtin modules


---

<div align="center">
	<b>
		<a href="https://tidelift.com/subscription/pkg/npm-is-builtin-module?utm_source=npm-is-builtin-module&utm_medium=referral&utm_campaign=readme">Get professional support for this package with a Tidelift subscription</a>
	</b>
	<br>
	<sub>
		Tidelift helps make open source sustainable for maintainers while giving companies<br>assurances about security, maintenance, and licensing for their dependencies.
	</sub>
</div>
