# spdx-license-ids

A list of [SPDX license](https://spdx.org/licenses/) identifiers

[**Download JSON**](https://raw.githubusercontent.com/shinnn/spdx-license-ids/master/spdx-license-ids.json)

## Use as a JavaScript Library

[![NPM version](https://img.shields.io/npm/v/spdx-license-ids.svg)](https://www.npmjs.org/package/spdx-license-ids)
[![Bower version](https://img.shields.io/bower/v/spdx-license-ids.svg)](https://github.com/shinnn/spdx-license-ids/releases)
[![Build Status](https://travis-ci.org/shinnn/spdx-license-ids.svg?branch=master)](https://travis-ci.org/shinnn/spdx-license-ids)
[![Coverage Status](https://img.shields.io/coveralls/shinnn/spdx-license-ids.svg)](https://coveralls.io/r/shinnn/spdx-license-ids)
[![devDependency Status](https://david-dm.org/shinnn/spdx-license-ids/dev-status.svg)](https://david-dm.org/shinnn/spdx-license-ids#info=devDependencies)

### Installation

#### Package managers

##### [npm](https://www.npmjs.com/)

```sh
npm install spdx-license-ids
```

##### [bower](http://bower.io/)

```sh
bower install spdx-license-ids
```

##### [Duo](http://duojs.org/)

```javascript
const spdxLicenseIds = require('shinnn/spdx-license-ids');
```

#### Standalone

[Download the script file directly.](https://raw.githubusercontent.com/shinnn/spdx-license-ids/master/spdx-license-ids-browser.js)

### API

#### spdxLicenseIds

Type: `Array` of `String`

It returns an array of SPDX license identifiers.

```javascript
const spdxLicenseIds = require('spdx-license-ids'); //=> ['Glide', 'Abstyles', 'AFL-1.1', ... ]
```

## License

[The Unlicense](./LICENSE).
