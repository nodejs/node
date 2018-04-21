# is-cidr

[![](https://img.shields.io/npm/v/is-cidr.svg?style=flat)](https://www.npmjs.org/package/is-cidr) [![](https://img.shields.io/npm/dm/is-cidr.svg)](https://www.npmjs.org/package/is-cidr) [![](https://api.travis-ci.org/silverwind/is-cidr.svg?style=flat)](https://travis-ci.org/silverwind/is-cidr)

> Check if a string is an IP address in CIDR notation

## Install

```
$ npm install --save is-cidr
```


## Usage

```js
const isCidr = require('is-cidr');

isCidr('192.168.0.1/24');
//=> true

isCidr('1:2:3:4:5:6:7:8/64');
//=> true

isCidr.v4('1:2:3:4:5:6:7:8/64');
//=> false
```


## API

### isCidr(input)

Check if `input` is a IPv4 or IPv6 CIDR address.

### isCidr.v4(input)

Check if `input` is IPv4 CIDR address.

### isCidr.v6(input)

Check if `input` is IPv6 CIDR address.


## Related

- [cidr-regex](https://github.com/silverwind/cidr-regex) - Regular expression for matching IP addresses in CIDR notation
- [is-ip](https://github.com/sindresorhus/is-ip) - Check if a string is an IP address
- [ip-regex](https://github.com/sindresorhus/ip-regex) - Regular expression for matching IP addresses

## License

© [silverwind](https://github.com/silverwind), distributed under BSD licence

Based on previous work by [Felipe Apostol](https://github.com/flipjs)
