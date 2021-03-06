# HAR Validator

[![license][license-img]][license-url]
[![version][npm-img]][npm-url]
[![super linter][super-linter-img]][super-linter-url]
[![test][test-img]][test-url]
[![release][release-img]][release-url]

[license-url]: LICENSE
[license-img]: https://badgen.net/github/license/ahmadnassri/node-har-validator

[npm-url]: https://www.npmjs.com/package/har-validator
[npm-img]: https://badgen.net/npm/v/har-validator

[super-linter-url]: https://github.com/ahmadnassri/node-har-validator/actions?query=workflow%3Asuper-linter
[super-linter-img]: https://github.com/ahmadnassri/node-har-validator/workflows/super-linter/badge.svg

[test-url]: https://github.com/ahmadnassri/node-har-validator/actions?query=workflow%3Atest
[test-img]: https://github.com/ahmadnassri/node-har-validator/workflows/test/badge.svg

[release-url]: https://github.com/ahmadnassri/node-har-validator/actions?query=workflow%3Arelease
[release-img]: https://github.com/ahmadnassri/node-har-validator/workflows/release/badge.svg

> Extremely fast HTTP Archive ([HAR](https://github.com/ahmadnassri/har-spec/blob/master/versions/1.2.md)) validator using JSON Schema.

## Install

```bash
npm install har-validator
```

## CLI Usage

Please refer to [`har-cli`](https://github.com/ahmadnassri/har-cli) for more info.

## API

**Note**: as of [`v2.0.0`](https://github.com/ahmadnassri/node-har-validator/releases/tag/v2.0.0) this module defaults to Promise based API.
_For backward compatibility with `v1.x` an [async/callback API](docs/async.md) is also provided_

- [async API](docs/async.md)
- [callback API](docs/async.md)
- [Promise API](docs/promise.md) _(default)_
