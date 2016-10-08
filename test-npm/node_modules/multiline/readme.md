# multiline [![Build Status](https://travis-ci.org/sindresorhus/multiline.svg?branch=master)](https://travis-ci.org/sindresorhus/multiline)

> Multiline strings in JavaScript

No more string concatenation or array join!

*Note that ES6 will have [template string](http://www.2ality.com/2011/09/quasi-literals.html) which can be multiline, but time...*

#### Before

```js
var str = '' +
'<!doctype html>' +
'<html>' +
'	<body>' +
'		<h1>❤ unicorns</h1>' +
'	</body>' +
'</html>' +
'';
```

#### After

```js
var str = multiline(function(){/*
<!doctype html>
<html>
	<body>
		<h1>❤ unicorns</h1>
	</body>
</html>
*/});
```


## How

It works by wrapping the text in a block comment, anonymous function, and a function call. The anonymous function is passed into the function call and the contents of the comment extracted.

Even though it's [slower than string concat](http://jsperf.com/multiline), that shouldn't realistically matter as you can still do 2 million of those a second. Convenience over micro performance always.


## Install

```sh
$ npm install --save multiline
```


## Usage

Everything after the first newline and before the last will be returned as seen below:

```js
var str = multiline(function(){/*
<!doctype html>
<html>
	<body>
		<h1>❤ unicorns</h1>
	</body>
</html>
*/});
```

Which outputs:

```
<!doctype html>
<html>
	<body>
		<h1>❤ unicorns</h1>
	</body>
</html>
```

### Strip indent

You can use `multiline.stripIndent()` to be able to indent your multiline string without preserving the redundant leading whitespace.

```js
	var str = multiline.stripIndent(function(){/*
			<!doctype html>
			<html>
				<body>
					<h1>❤ unicorns</h1>
				</body>
			</html>
	*/});
```

Which outputs:

```
<!doctype html>
<html>
	<body>
		<h1>❤ unicorns</h1>
	</body>
</html>
```


### String substitution

`console.log()` supports [string substitution](http://nodejs.org/docs/latest/api/console.html#console_console_log_data):

```js
var str = 'unicorns';

console.log(multiline(function(){/*
  I love %s
*/}), str);

//=> I love unicorns
```


## Use cases

- [CLI help output](https://github.com/sindresorhus/pageres/blob/cb85922dec2b962c7b45484023c9ba43a9abf6bd/cli.js#L14-L33)
- [Test fixtures](https://twitter.com/TooTallNate/status/465392558000984064)
- [Queries](https://github.com/freethejazz/twitter-to-neo4j/blob/a41b6c2e8480d4b9943640a8aa4b6976f07083bf/cypher/queries.js#L15-L22) - *here an example in Cypher, the query language for Neo4j*
- [CLI welcome message](https://github.com/yeoman/generator-jquery/blob/4b532843663e4b5ce7d433d351e0a78dcf2b1e20/app/index.js#L28-L40) - *here in a Yeoman generator*

Have one? [Let me know.](https://github.com/sindresorhus/multiline/issues/new)


## Experiment

I've also done an [experiment](experiment.js) where you don't need the anonymous function. It's too fragile and slow to be practical though.

It generates a callstack and extracts the contents of the comment in the function call.

```js
var str = multiline(/*
<!doctype html>
<html>
	<body>
		<h1>❤ unicorns</h1>
	</body>
</html>
*/);
```


## FAQ

### But JS already has multiline strings with `\`?

```js
var str = 'foo\
bar';
```

This is not a multiline string. It's line-continuation. It doesn't preserve newlines, which is the main reason for wanting multiline strings.

You would need to do the following:

```js
var str = 'foo\n\
bar';
```

But then you could just as well concatenate:

```js
var str = 'foo\n' +
'bar';
```

*Note that ES6 will have real [multiline strings](https://github.com/lukehoban/es6features#template-strings).*


## Browser

While it does work fine in the browser, it's mainly intended for use in Node.js. Use at your own risk.

### Install

```sh
$ npm install --save multiline
```

*(with [Browserify](http://browserify.org))*

```sh
$ bower install --save multiline
```


### Compatibility

- Latest Chrome
- Firefox >=17
- Safari >=4
- Opera >=9
- Internet Explorer >=6

### Minification

Even though minifiers strip comments by default there are ways to preserve them:

- Uglify: Use `/*@preserve` instead of `/*` and enable the `comments` option
- Closure Compiler: Use `/*@preserve` instead of `/*`
- YUI Compressor: Use `/*!` instead of `/*`

You also need to add `console.log` after the comment so it's not removed as dead-code.

The final result would be:

```js
var str = multiline(function(){/*!@preserve
<!doctype html>
<html>
	<body>
		<h1>❤ unicorns</h1>
	</body>
</html>
*/console.log});
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
