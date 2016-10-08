# connect jsonp

## Installation

    $ npm install connect-jsonp

## Config

In order to facilitate caching as well as avoid downstream middleware conflicts the filter config option
exists and instructs this module to filter (or not) the callback param from the request url.  This defaults 
to false but can be specified passing either a boolean or the literal {filter: true} to your requires statement.  
In is important to note that you will want to filter the callback param if you are planning on using the 
connect cache middleware.  Furthermore you will also want to ensure that the cache module is after this module 
as the url, including the query string, is used as a key into the cache (thanks to <a href="https://github.com/jmarca">jmarca</a> 
for pointing this out).

## Testing

    git submodule update --init
    make test

## TODOs

Current this middleware proxies the res.writeHead method.  This is limiting and will will potentially raise exceptions 
for apps that simply call end.  

## License 

(The MIT License)

Copyright (c) 2010 Sean McDaniel

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