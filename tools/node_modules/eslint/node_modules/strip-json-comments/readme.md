# strip-json-comments [![Build Status](https://travis-ci.org/sindresorhus/strip-json-comments.svg?branch=master)](https://travis-ci.org/sindresorhus/strip-json-comments)

> Strip comments from JSON. Lets you use comments in your JSON files!

This is now possible:

```js
{
	// Rainbows
	"unicorn": /* ❤ */ "cake"
}
```

It will replace single-line comments `//` and multi-line comments `/**/` with whitespace. This allows JSON error positions to remain as close as possible to the original source.

Also available as a [Gulp](https://github.com/sindresorhus/gulp-strip-json-comments)/[Grunt](https://github.com/sindresorhus/grunt-strip-json-comments)/[Broccoli](https://github.com/sindresorhus/broccoli-strip-json-comments) plugin.


## Install

```
$ npm install strip-json-comments
```


## Usage

```js
const json = `{
	// Rainbows
	"unicorn": /* ❤ */ "cake"
}`;

JSON.parse(stripJsonComments(json));
//=> {unicorn: 'cake'}
```


## API

### stripJsonComments(jsonString, [options])

#### jsonString

Type: `string`

Accepts a string with JSON and returns a string without comments.

#### options

Type: `object`

##### whitespace

Type: `boolean`<br>
Default: `true`

Replace comments with whitespace instead of stripping them entirely.


## Benchmark

```
$ npm run bench
```


## Related

- [strip-json-comments-cli](https://github.com/sindresorhus/strip-json-comments-cli) - CLI for this module
- [strip-css-comments](https://github.com/sindresorhus/strip-css-comments) - Strip comments from CSS


## License

MIT © [Sindre Sorhus](https://sindresorhus.com)
