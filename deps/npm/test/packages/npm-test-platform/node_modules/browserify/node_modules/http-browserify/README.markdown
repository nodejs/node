http-browserify
===============

The
[http](http://nodejs.org/docs/v0.4.10/api/all.html#hTTP) module from node.js,
but for browsers.

When you `require('http')` in
[browserify](http://github.com/substack/node-browserify),
this module will be loaded.

example
=======

````javascript
var http = require('http');

http.get({ path : '/beep' }, function (res) {
    var div = document.getElementById('result');
    div.innerHTML += 'GET /beep<br>';
    
    res.on('data', function (buf) {
        div.innerHTML += buf;
    });
    
    res.on('end', function () {
        div.innerHTML += '<br>__END__';
    });
});
````

http methods
============

var http = require('http');

var req = http.request(options, cb)
-----------------------------------

`options` can have:

* method
* path
* headers={}, as an object mapping key names to string or Array values
* host=window.location.host
* port=window.location.port

The callback will be called with the response object.

var req = http.get(options, cb)
-------------------------------

A shortcut for

````javascript
options.method = 'GET';
var req = http.request(options, cb);
req.end();
````

request methods
===============

req.setHeader(key, value)
-------------------------

Set an http header.

req.write(data)
---------------

Write some data to the request body.

req.end(data)
-------------

Close and send the request body, optionally with additional `data` to append.

response methods
================

res.getHeader(key)
------------------

Return an http header, if set. `key` is case-insensitive.

response attributes
===================

* res.statusCode, the numeric http response code
* res.headers, an object with all lowercase keys

response events
---------------

* data
* end
* error

compatibility
=============

This module has been tested and works with:

* Internet Explorer 5.5, 6, 7, 8, 9
* Firefox 3.5
* Chrome 7.0
* Opera 10.6
* Safari 5.0

Multipart streaming responses are buffered in all versions of Internet Explorer
and are somewhat buffered in Opera. In all the other browsers you get a nice
unbuffered stream of `"data"` events when you send down a content-type of
`multipart/octet-stream` or similar.

protip
======

You can do:

````javascript
var bundle = browserify({
    require : { http : 'http-browserify' }
});
````

in order to map "http-browserify" over `require('http')` in your browserified
source.
