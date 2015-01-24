# mime-db

[![NPM Version][npm-version-image]][npm-url]
[![NPM Downloads][npm-downloads-image]][npm-url]
[![Node.js Version][node-image]][node-url]
[![Build Status][travis-image]][travis-url]
[![Coverage Status][coveralls-image]][coveralls-url]

This is a database of all mime types.
It consistents of a single, public JSON file and does not include any logic,
allowing it to remain as unopinionated as possible with an API.
It aggregates data from the following sources:

- http://www.iana.org/assignments/media-types/media-types.xhtml
- http://svn.apache.org/repos/asf/httpd/httpd/trunk/docs/conf/mime.types

## Usage

```bash
npm i mime-db
```

```js
var db = require('mime-db');

// grab data on .js files
var data = db['application/javascript'];
```

If you're crazy enough to use this in the browser,
you can just grab the JSON file:

```
https://cdn.rawgit.com/jshttp/mime-db/master/db.json
```

## Data Structure

The JSON file is a map lookup for lowercased mime types.
Each mime type has the following properties:

- `.source` - where the mime type is defined.
    If not set, it's probably a custom media type.
    - `apache` - [Apache common media types](http://svn.apache.org/repos/asf/httpd/httpd/trunk/docs/conf/mime.types)
    - `iana` - [IANA-defined media types](http://www.iana.org/assignments/media-types/media-types.xhtml)
- `.extensions[]` - known extensions associated with this mime type.
- `.compressible` - whether a file of this type is can be gzipped.
- `.charset` - the default charset associated with this type, if any.

If unknown, every property could be `undefined`.

## Repository Structure

- `scripts` - these are scripts to run to build the database
- `src/` - this is a folder of files created from remote sources like Apache and IANA
- `lib/` - this is a folder of our own custom sources and db, which will be merged into `db.json`
- `db.json` - the final built JSON file for end-user usage

## Contributing

To edit the database, only make PRs against files in the `lib/` folder.
To update the build, run `npm run update`.

[npm-version-image]: https://img.shields.io/npm/v/mime-db.svg?style=flat
[npm-downloads-image]: https://img.shields.io/npm/dm/mime-db.svg?style=flat
[npm-url]: https://npmjs.org/package/mime-db
[travis-image]: https://img.shields.io/travis/jshttp/mime-db.svg?style=flat
[travis-url]: https://travis-ci.org/jshttp/mime-db
[coveralls-image]: https://img.shields.io/coveralls/jshttp/mime-db.svg?style=flat
[coveralls-url]: https://coveralls.io/r/jshttp/mime-db?branch=master
[node-image]: https://img.shields.io/node/v/mime-db.svg?style=flat
[node-url]: http://nodejs.org/download/
