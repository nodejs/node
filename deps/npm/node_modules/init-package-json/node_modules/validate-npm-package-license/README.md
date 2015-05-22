npm-validate-package-license
============================

Give me a string and I'll tell you if it's a valid npm package license.

*This package is not endorsed or approved by npm. It is part of a proposal to add license field validation to the npm command-line interface.*

<!-- js var valid = require('./'); -->

```js
var validResult = {
  validForNewPackages: true,
  validForOldPackages: true
};

valid('Apache-2.0'); // => validResult
valid('GPL-3.0 OR BSD-2-Clause'); // => validResult

var invalidResult = {
  validForOldPackages: false,
  validForNewPackages: false,
  warnings: [
    'license should be a valid SPDX license expression',
	'license is similar to the valid expression "Apache-2.0"'
  ]
};

valid('Apache 2.0'); // => invalidResult
```
