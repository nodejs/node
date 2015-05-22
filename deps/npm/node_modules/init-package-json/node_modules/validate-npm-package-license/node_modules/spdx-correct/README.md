spdx-correct.js
===============

[![npm version](https://img.shields.io/npm/v/spdx-correct.svg)](https://www.npmjs.com/package/spdx-correct)
[![license](https://img.shields.io/badge/license-Apache--2.0-303284.svg)](http://www.apache.org/licenses/LICENSE-2.0)
[![build status](https://img.shields.io/travis/kemitchell/spdx-correct.js.svg)](http://travis-ci.org/kemitchell/spdx-correct.js)


Correct invalid SPDX identifiers.

<!--js
var correct = require('./');
-->

```js
correct('mit'); // => 'MIT'

correct('Apache 2'); // => 'Apache-2.0'

correct('No idea what license'); // => null
```
