# package-json [![Build Status](https://travis-ci.org/sindresorhus/package-json.svg?branch=master)](https://travis-ci.org/sindresorhus/package-json)

> Get the package.json of a package from the npm registry


## Install

```
$ npm install --save package-json
```


## Usage

```js
const packageJson = require('package-json');

packageJson('pageres', 'latest').then(json => {
	console.log(json);
	//=> {name: 'pageres', ...}
});

// Also works with scoped packages
packageJson('@company/package', 'latest').then(json => {
	console.log(json);
	//=> {name: 'package', ...}
});
```


## API

### packageJson(name, [version])

You can optionally specify a version (e.g. `1.0.0`) or a [dist tag](https://docs.npmjs.com/cli/dist-tag) such as `latest`. If you don't specify a version you'll get the [main entry](http://registry.npmjs.org/pageres/) containing all versions.

The version can also be in any format supported by the [semver](https://www.npmjs.com/package/semver) module. For example:

- `1` - get the latest `1.x.x`
- `1.2` - get the latest `1.2.x`
- `^1.2.3` - get the latest `1.x.x` but at least `1.2.3`
- `~1.2.3` - get the latest `1.2.x` but at least `1.2.3`


## Authentication

Both public and private registries are supported, for both scoped and unscoped packages, as long as the registry uses either bearer tokens or basic authentication.


## Related

- [package-json-cli](https://github.com/sindresorhus/package-json-cli) - CLI for this module
- [latest-version](https://github.com/sindresorhus/latest-version) - Get the latest version of an npm package
- [pkg-versions](https://github.com/sindresorhus/pkg-versions) - Get the version numbers of a package from the npm registry
- [npm-keyword](https://github.com/sindresorhus/npm-keyword) - Get a list of npm packages with a certain keyword
- [npm-user](https://github.com/sindresorhus/npm-user) - Get user info of an npm user
- [npm-email](https://github.com/sindresorhus/npm-email) - Get the email of an npm user


## License

MIT © [Sindre Sorhus](https://sindresorhus.com)
