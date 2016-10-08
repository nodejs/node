# connect-gzip

Gzip middleware for [Connect](http://senchalabs.github.com/connect/) on [Node.js](http://nodejs.org). Uses Unix [`gzip`](http://www.freebsd.org/cgi/man.cgi?query=gzip) command to perform compression of dynamic requests or static files. Originally based on implementation included with Connect before version 1.0.


## Installation

Install via npm:

    $ npm install connect-gzip


## Usage

### gzip.gzip([options])

Include this middleware to dynamically gzip data sent via `res.write` or `res.end` based on the Content-Type header.

    var connect = require('connect'),
        gzip = require('connect-gzip');
    
    connect.createServer(
      gzip.gzip(),
      function(req, res) {
        res.setHeader('Content-Type', 'text/html');
        res.end('<p>Some gzipped HTML!</p>');
      }
    ).listen(3000);
    
    
    // Only gzip css files:
    gzip.gzip({ matchType: /css/ })
    
    // Use maximum compression:
    gzip.gzip({ flags: '--best' })

Options:

- `matchType` - A regular expression tested against the Content-Type header to determine whether the response should be gzipped or not. The default value is `/text|javascript|json/`.
- `bin` - Command executed to perform gzipping. Defaults to `'gzip'`.
- `flags` - Command flags passed to the gzip binary. Nothing by default for dynamic gzipping, so gzip will typically default to a compression level of 6.


### gzip.staticGzip(root, [options])

Gzips files in a root directory, and then serves them using the default [`connect.static`](http://senchalabs.github.com/connect/middleware-static.html) middleware. Note that options get passed through as well, so the `maxAge` and other options supported by `connect.static` also work.

If a file under the root path (such as an image) does not have an appropriate MIME type for compression, it will still be passed through to `connect.static` and served uncompressed. Thus, you can simply use `gzip.staticGzip` in place of `connect.static`.

    var connect = require('connect'),
        gzip = require('connect-gzip');
    
    connect.createServer(
      gzip.staticGzip(__dirname + '/public')
    ).listen(3000);
    
    
    // Only gzip javascript files:
    gzip.staticGzip(__dirname + '/public', { matchType: /javascript/ })

    // Set a maxAge in milliseconds for browsers to cache files
    var oneDay = 86400000;
    gzip.staticGzip(__dirname + '/public', { maxAge: oneDay })

Options:

- `matchType` - A regular expression tested against the file MIME type to determine whether the response should be gzipped or not. As in `connect.static`, MIME types are determined based on file extensions using [node-mime](https://github.com/bentomas/node-mime). The default value is `/text|javascript|json/`.
- `bin` - Command executed to perform gzipping. Defaults to `'gzip'`.
- `flags` - Command flags passed to the gzip binary. Defaults to `'--best'` for staticGzip.


## Tests

Run the tests with

    expresso test


## License

(The MIT License)

Copyright (c) 2011 Nate Smith &lt;nate@nateps.com&gt;

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.