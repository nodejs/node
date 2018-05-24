# cidr-regex
[![](https://img.shields.io/npm/v/cidr-regex.svg?style=flat)](https://www.npmjs.org/package/cidr-regex) [![](https://img.shields.io/npm/dm/cidr-regex.svg)](https://www.npmjs.org/package/cidr-regex) [![](https://api.travis-ci.org/silverwind/cidr-regex.svg?style=flat)](https://travis-ci.org/silverwind/cidr-regex)

> Regular expression for matching IP addresses in CIDR notation

## Install

```sh
$ npm install --save cidr-regex
```

## Usage

```js
const cidrRegex = require('cidr-regex');

// Contains a CIDR IP address?
cidrRegex().test('foo 192.168.0.1/24');
//=> true

// Is a CIDR IP address?
cidrRegex({exact: true}).test('foo 192.168.0.1/24');
//=> false

cidrRegex.v6({exact: true}).test('1:2:3:4:5:6:7:8/64');
//=> true

'foo 192.168.0.1/24 bar 1:2:3:4:5:6:7:8/64 baz'.match(cidrRegex());
//=> ['192.168.0.1/24', '1:2:3:4:5:6:7:8/64']
```

## API

### cidrRegex([options])

Returns a regex for matching both IPv4 and IPv6 CIDR IP addresses.

### cidrRegex.v4([options])

Returns a regex for matching IPv4 CIDR IP addresses.

### cidrRegex.v6([options])

Returns a regex for matching IPv6 CIDR IP addresses.

#### options.exact

Type: `boolean`<br>
Default: `false` *(Matches any CIDR IP address in a string)*

Only match an exact string. Useful with `RegExp#test()` to check if a string is a CIDR IP address.


## Related

- [is-cidr](https://github.com/silverwind/is-cidr) - Check if a string is an IP address in CIDR notation
- [is-ip](https://github.com/sindresorhus/is-ip) - Check if a string is an IP address
- [ip-regex](https://github.com/sindresorhus/ip-regex) - Regular expression for matching IP addresses

## License

© [silverwind](https://github.com/silverwind), distributed under BSD licence

Based on previous work by [Felipe Apostol](https://github.com/flipjs)
