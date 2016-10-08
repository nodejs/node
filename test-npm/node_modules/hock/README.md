# hock [![Build Status](https://secure.travis-ci.org/mmalecki/hock.png?branch=master)](http://travis-ci.org/mmalecki/hock)

An HTTP mocking server based on [Nock](https://github.com/flatiron/nock).

## Overview

Hock is an HTTP mocking server with an API designed to closely match that of Nock. The key difference between Nock and Hock is that nock works by overriding `http.clientRequest`, allowing requests to be intercepted before they go over the wire.

Hock is designed as a fully functioning HTTP service. You enqueue requests and responses in a similar fashion to Nock:

```Javascript

    var hock = require('hock'),
        request = require('request');

    hock.createHock(function(err, hockServer) {
        var port = hockServer.address().port;

        hockServer
            .get('/some/url')
            .reply(200, 'Hello!');

        request('http://localhost:' + port + '/some/url', function(err, res, body) {
           console.log(body);
        });
    });

```

A port can be optionally specified when creating the server:

```Javascript
    hock.createHock(12345, function(err, hockServer) {
        ....
    });
```

Unlike Nock, you create a `Hock` server with a callback based factory method. Behind the scenes, this spins up the new HTTP service, and begins listening to requests.

## HTTP Methods

Hock supports the 5 primary HTTP methods at this time:

* GET
* POST
* PUT
* DELETE
* HEAD

```Javascript
    // Returns a hock Request object
    var req = hockServer.get(url, requestHeaders);
```

```Javascript
    // Returns a hock Request object
    var req = hockServer.delete(url, requestHeaders);
```

```Javascript
    // Returns a hock Request object
    var req = hockServer.post(url, body, requestHeaders);
```

```Javascript
    // Returns a hock Request object
    var req = hockServer.put(url, body, requestHeaders);
```

```Javascript
    // Returns a hock Request object
    var req = hockServer.head(url, requestHeaders);
```

## Request Object

All of these methods return an instance of a `Request`, a hock object which contains all of the state for a mocked request. To define the response and enqueue into the `hockServer`, call either `reply` or `replyWithFile` on the `Request` object:

```Javascript
    // returns the current hockServer instance
    req.reply(statusCode, body, responseHeaders);
```

```Javascript
    // returns the current hockServer instance
    req.replyWithFile(statusCode, filePath, responseHeaders);
```

## Multiple matching requests

You can optionally tell hock to match multiple requests for the same route:

```Javascript
    hockServer.put('/path/one', {
        foo: 1,
        bar: {
            baz: true
            biz: 'asdf1234'
        }
    })
    .min(4)
    .max(10)
    .reply(202, {
        status: 'OK'
    })
```

Call `many` if you need to handle at least one, possibly
many requests:

```Javascript
    hockServer.put('/path/one', {
        foo: 1,
        bar: {
            baz: true
            biz: 'asdf1234'
        }
    })
    .many() // min 1, max Unlimited
    .reply(202, {
        status: 'OK'
    })
```

Provide custom min and max options to `many`:

```Javascript
    hockServer.put('/path/one', {
        foo: 1,
        bar: {
            baz: true
            biz: 'asdf1234'
        }
    })
    .many({
        min: 4,
        max: 10
    })
    .reply(202, {
        status: 'OK'
    })
```

Set infinite number of requests with `max(Infinity)`:

```Javascript
    hockServer.put('/path/one', {
        foo: 1,
        bar: {
            baz: true
            biz: 'asdf1234'
        }
    })
    .max(Infinity)
    .reply(202, {
        status: 'OK'
    })
```

If you don't care how many or how few requests are served, you can use `any`:

```Javascript
    hockServer.put('/path/one', {
        foo: 1,
        bar: {
            baz: true
            biz: 'asdf1234'
        }
    })
    .any() // equivalent to min(0), max(Infinity)
    .reply(202, {
        status: 'OK'
    })
```
### hockServer.done() with many

`hockServer.done()` will verify the number of requests fits within the
minimum and maximum constraints specified by `min`, `max`, `many` or `any`:

```js
hockServer.get('/').min(2)
request.get('/', function() {
  hockServer.done(function(err) {
    console.error(err) // error, only made one request
  })
})
```

If the number of requests doesn't verify and you don't supply a callback
to `hockServer.done()` it will throw!

## Chaining requests

As the `reply` and `replyWithFile` methods return the current hockServer, you can chain them together:

```Javascript

    hockServer.put('/path/one', {
        foo: 1,
        bar: {
            baz: true
            biz: 'asdf1234'
        }
    })
    .reply(202, {
        status: 'OK'
    })
    .get('/my/file/should/be/here')
    .replyWithFile(200, __dirname + '/foo.jpg');

```

## Matching requests

When a request comes in, hock iterates through the queue in a First-in-first-out approach, so long as the request matches. The criteria for matching is based on the method and the url, and additionally the request body if the request is a `PUT` or `POST`. If you specify request headers, they will also be matched against before sending the response.

## Path filtering

You can filter paths using regex or a custom function, this is useful for things like timestamps that get appended to urls from clients.

```Javascript

    hockServer
        .filteringPathRegEx(/timestamp=[^&]*/g, 'timestamp=123')
        .get('/url?timestamp=123')
        .reply(200, 'Hi!');

```

```Javascript

    hockServer
        .filteringPath(function (p) {
            return '/url?timestamp=XXX';
        })
        .get('/url?timestamp=XXX')
        .reply(200, 'Hi!');

```
