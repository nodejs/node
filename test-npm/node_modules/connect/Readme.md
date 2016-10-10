# Connect

[![NPM Version][npm-image]][npm-url]
[![NPM Downloads][downloads-image]][downloads-url]
[![Build Status][travis-image]][travis-url]
[![Test Coverage][coveralls-image]][coveralls-url]
[![Gratipay][gratipay-image]][gratipay-url]

  Connect is an extensible HTTP server framework for [node](http://nodejs.org) using "plugins" known as _middleware_.

```js
var connect = require('connect');
var http = require('http');

var app = connect();

// gzip/deflate outgoing responses
var compression = require('compression');
app.use(compression());

// store session state in browser cookie
var cookieSession = require('cookie-session');
app.use(cookieSession({
    keys: ['secret1', 'secret2']
}));

// parse urlencoded request bodies into req.body
var bodyParser = require('body-parser');
app.use(bodyParser.urlencoded());

// respond to all requests
app.use(function(req, res){
  res.end('Hello from Connect!\n');
});

//create node.js http server and listen on port
http.createServer(app).listen(3000);
```

## Getting Started

Connect is a simple framework to glue together various "middleware" to handle requests.

### Install Connect

```sh
$ npm install connect
```

### Create an app

The main component is a Connect "app". This will store all the middleware
added and is, itself, a function.

```js
var app = connect();
```

### Use middleware

The core of Connect is "using" middleware. Middleware are added as a "stack"
where incoming requests will execute each middleware one-by-one until a middleware
does not call `next()` within it.

```js
app.use(function middleware1(req, res, next) {
  // middleware 1
  next();
});
app.use(function middleware2(req, res, next) {
  // middleware 2
  next();
});
```

### Mount middleware

The `.use()` method also takes an optional path string that is matched against
the beginning of the incoming request URL. This allows for basic routing.

```js
app.use('/foo', function fooMiddleware(req, res, next) {
  // req.url starts with "/foo"
  next();
});
app.use('/bar', function barMiddleware(req, res, next) {
  // req.url starts with "/bar"
  next();
});
```

### Error middleware

There are special cases of "error-handling" middleware. There are middleware
where the function takes exactly 4 arguments. When a middleware passes an error
to `next`, the app will proceed to look for the error middleware that was declared
after that middleware and invoke it, skipping any error middleware above that
middleware and any non-error middleware below.

```js
// regular middleware
app.use(function (req, res, next) {
  // i had an error
  next(new Error('boom!'));
});

// error middleware for errors that occurred in middleware
// declared before this
app.use(function onerror(err, req, res, next) {
  // an error occurred!
});
```

### Create a server from the app

The last step is to actually use the Connect app in a server. The `.listen()` method
is a convenience to start a HTTP server.

```js
var server = app.listen(port);
```

The app itself is really just a function with three arguments, so it can also be handed
to `.createServer()` in Node.js.

```js
var server = http.createServer(app);
```

## Middleware

These middleware and libraries are officially supported by the Connect/Express team:

  - [body-parser](https://www.npmjs.com/package/body-parser) - previous `bodyParser`, `json`, and `urlencoded`. You may also be interested in:
    - [body](https://www.npmjs.com/package/body)
    - [co-body](https://www.npmjs.com/package/co-body)
    - [raw-body](https://www.npmjs.com/package/raw-body)
  - [compression](https://www.npmjs.com/package/compression) - previously `compress`
  - [connect-timeout](https://www.npmjs.com/package/connect-timeout) - previously `timeout`
  - [cookie-parser](https://www.npmjs.com/package/cookie-parser) - previously `cookieParser`
  - [cookie-session](https://www.npmjs.com/package/cookie-session) - previously `cookieSession`
  - [csurf](https://www.npmjs.com/package/csurf) - previously `csrf`
  - [errorhandler](https://www.npmjs.com/package/errorhandler) - previously `error-handler`
  - [express-session](https://www.npmjs.com/package/express-session) - previously `session`
  - [method-override](https://www.npmjs.com/package/method-override) - previously `method-override`
  - [morgan](https://www.npmjs.com/package/morgan) - previously `logger`
  - [response-time](https://www.npmjs.com/package/response-time) - previously `response-time`
  - [serve-favicon](https://www.npmjs.com/package/serve-favicon) - previously `favicon`
  - [serve-index](https://www.npmjs.com/package/serve-index) - previously `directory`
  - [serve-static](https://www.npmjs.com/package/serve-static) - previously `static`
  - [vhost](https://www.npmjs.com/package/vhost) - previously `vhost`

Most of these are exact ports of their Connect 2.x equivalents. The primary exception is `cookie-session`.

Some middleware previously included with Connect are no longer supported by the Connect/Express team, are replaced by an alternative module, or should be superseded by a better module. Use one of these alternatives instead:

  - `cookieParser`
    - [cookies](https://www.npmjs.com/package/cookies) and [keygrip](https://www.npmjs.com/package/keygrip)
  - `limit`
    - [raw-body](https://www.npmjs.com/package/raw-body)
  - `multipart`
    - [connect-multiparty](https://www.npmjs.com/package/connect-multiparty)
    - [connect-busboy](https://www.npmjs.com/package/connect-busboy)
  - `query`
    - [qs](https://www.npmjs.com/package/qs)
  - `staticCache`
    - [st](https://www.npmjs.com/package/st)
    - [connect-static](https://www.npmjs.com/package/connect-static)

Checkout [http-framework](https://github.com/Raynos/http-framework/wiki/Modules) for many other compatible middleware! 

## Running Tests

```bash
npm install
npm test
```

## Contributors

 https://github.com/senchalabs/connect/graphs/contributors

## Node Compatibility

  - Connect `< 1.x` - node `0.2`
  - Connect `1.x` - node `0.4`
  - Connect `< 2.8` - node `0.6`
  - Connect `>= 2.8 < 3` - node `0.8`
  - Connect `>= 3` - node `0.10`, `0.12`; io.js `1.x`, `2.x`

## License

[MIT](LICENSE)

[npm-image]: https://img.shields.io/npm/v/connect.svg
[npm-url]: https://npmjs.org/package/connect
[travis-image]: https://img.shields.io/travis/senchalabs/connect/master.svg
[travis-url]: https://travis-ci.org/senchalabs/connect
[coveralls-image]: https://img.shields.io/coveralls/senchalabs/connect/master.svg
[coveralls-url]: https://coveralls.io/r/senchalabs/connect
[downloads-image]: https://img.shields.io/npm/dm/connect.svg
[downloads-url]: https://npmjs.org/package/connect
[gratipay-image]: https://img.shields.io/gratipay/dougwilson.svg
[gratipay-url]: https://www.gratipay.com/dougwilson/
