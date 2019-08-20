# is-cidr

[![](https://img.shields.io/npm/v/is-cidr.svg?style=flat)](https://www.npmjs.org/package/is-cidr) [![](https://img.shields.io/npm/dm/is-cidr.svg)](https://www.npmjs.org/package/is-cidr) [![](https://api.travis-ci.org/silverwind/is-cidr.svg?style=flat)](https://travis-ci.org/silverwind/is-cidr)

> Check if a string is an IP address in CIDR notation

## Install

```
npm i is-cidr
```


## Usage

```js
const isCidr = require('is-cidr');

isCidr('192.168.0.1/24'); //=> 4
isCidr('1:2:3:4:5:6:7:8/64'); //=> 6
isCidr('10.0.0.0'); //=> 0
isCidr.v6('10.0.0.0/24'); //=> false
```


## API

### isCidr(input)

Check if `input` is a IPv4 or IPv6 CIDR address. Returns either `4`, `6` (indicating the IP version) or `0` if the string is not a CIDR.

### isCidr.v4(input)

Check if `input` is a IPv4 CIDR address. Returns a boolean.

### isCidr.v6(input)

Check if `input` is a IPv6 CIDR address. Returns a boolean.


## Related

- [cidr-regex](https://github.com/silverwind/cidr-regex) - Regular expression for matching IP addresses in CIDR notation
- [is-ip](https://github.com/sindresorhus/is-ip) - Check if a string is an IP address
- [ip-regex](https://github.com/sindresorhus/ip-regex) - Regular expression for matching IP addresses

## License

© [silverwind](https://github.com/silverwind), distributed under BSD licence

Based on previous work by [Felipe Apostol](https://github.com/flipjs)
