# Request -- Simplified HTTP request method

## Install

<pre>
  npm install request
</pre>

Or from source:

<pre>
  git clone git://github.com/mikeal/request.git 
  cd request
  npm link
</pre>

## Super simple to use

Request is designed to be the simplest way possible to make http calls. It supports HTTPS and follows redirects by default.

```javascript
var request = require('request');
request('http://www.google.com', function (error, response, body) {
  if (!error && response.statusCode == 200) {
    console.log(body) // Print the google web page.
  }
})
```

## Streaming

You can stream any response to a file stream.

```javascript
request('http://google.com/doodle.png').pipe(fs.createWriteStream('doodle.png'))
```

You can also stream a file to a PUT or POST request. This method will also check the file extension against a mapping of file extensions to content-types, in this case `application/json`, and use the proper content-type in the PUT request if one is not already provided in the headers.

```javascript
fs.createReadStream('file.json').pipe(request.put('http://mysite.com/obj.json'))
```

Request can also pipe to itself. When doing so the content-type and content-length will be preserved in the PUT headers.

```javascript
request.get('http://google.com/img.png').pipe(request.put('http://mysite.com/img.png'))
```

Now let's get fancy.

```javascript
http.createServer(function (req, resp) {
  if (req.url === '/doodle.png') {
    if (req.method === 'PUT') {
      req.pipe(request.put('http://mysite.com/doodle.png'))
    } else if (req.method === 'GET' || req.method === 'HEAD') {
      request.get('http://mysite.com/doodle.png').pipe(resp)
    } 
  }
})
```

You can also pipe() from a http.ServerRequest instance and to a http.ServerResponse instance. The HTTP method and headers will be sent as well as the entity-body data. Which means that, if you don't really care about security, you can do:

```javascript
http.createServer(function (req, resp) {
  if (req.url === '/doodle.png') {
    var x = request('http://mysite.com/doodle.png')
    req.pipe(x)
    x.pipe(resp)
  }
})
```

And since pipe() returns the destination stream in node 0.5.x you can do one line proxying :)

```javascript
req.pipe(request('http://mysite.com/doodle.png')).pipe(resp)
```

Also, none of this new functionality conflicts with requests previous features, it just expands them.

```javascript
var r = request.defaults({'proxy':'http://localproxy.com'})

http.createServer(function (req, resp) {
  if (req.url === '/doodle.png') {
    r.get('http://google.com/doodle.png').pipe(resp)
  }
})
```
You can still use intermediate proxies, the requests will still follow HTTP forwards, etc.

## Forms

`request` supports `application/x-www-form-urlencoded` and `multipart/form-data` form uploads. For `multipart/related` refer to the `multipart` API.

Url encoded forms are simple

```javascript
request.post('http://service.com/upload', {form:{key:'value'}})
// or
request.post('http://service.com/upload').form({key:'value'})
```

For `multipart/form-data` we use the [form-data](https://github.com/felixge/node-form-data) library by [@felixge](https://github.com/felixge). You don't need to worry about piping the form object or setting the headers, `request` will handle that for you.

```javascript
var r = request.post('http://service.com/upload')
var form = r.form()
form.append('my_field', 'my_value')
form.append('my_buffer', new Buffer([1, 2, 3]))
form.append('my_file', fs.createReadStream(path.join(__dirname, 'doodle.png'))
form.append('remote_file', request('http://google.com/doodle.png'))
```

## HTTP Authentication

```javascript
request.get('http://some.server.com/').auth('username', 'password', false);
// or
request.get('http://some.server.com/', {
  'auth': {
    'user': 'username',
    'pass': 'password',
    'sendImmediately': false
  }
});
```

If passed as an option, `auth` should be a hash containing values `user` || `username`, `password` || `pass`, and `sendImmediately` (optional).  The method form takes parameters `auth(username, password, sendImmediately)`.

`sendImmediately` defaults to true, which will cause a basic authentication header to be sent.  If `sendImmediately` is `false`, then `request` will retry with a proper authentication header after receiving a 401 response from the server (which must contain a `WWW-Authenticate` header indicating the required authentication method).

Digest authentication is supported, but it only works with `sendImmediately` set to `false` (otherwise `request` will send basic authentication on the initial request, which will probably cause the request to fail).

## OAuth Signing

```javascript
// Twitter OAuth
var qs = require('querystring')
  , oauth =
    { callback: 'http://mysite.com/callback/'
    , consumer_key: CONSUMER_KEY
    , consumer_secret: CONSUMER_SECRET
    }
  , url = 'https://api.twitter.com/oauth/request_token'
  ;
request.post({url:url, oauth:oauth}, function (e, r, body) {
  // Ideally, you would take the body in the response
  // and construct a URL that a user clicks on (like a sign in button).
  // The verifier is only available in the response after a user has 
  // verified with twitter that they are authorizing your app.
  var access_token = qs.parse(body)
    , oauth = 
      { consumer_key: CONSUMER_KEY
      , consumer_secret: CONSUMER_SECRET
      , token: access_token.oauth_token
      , verifier: access_token.oauth_verifier
      }
    , url = 'https://api.twitter.com/oauth/access_token'
    ;
  request.post({url:url, oauth:oauth}, function (e, r, body) {
    var perm_token = qs.parse(body)
      , oauth = 
        { consumer_key: CONSUMER_KEY
        , consumer_secret: CONSUMER_SECRET
        , token: perm_token.oauth_token
        , token_secret: perm_token.oauth_token_secret
        }
      , url = 'https://api.twitter.com/1/users/show.json?'
      , params = 
        { screen_name: perm_token.screen_name
        , user_id: perm_token.user_id
        }
      ;
    url += qs.stringify(params)
    request.get({url:url, oauth:oauth, json:true}, function (e, r, user) {
      console.log(user)
    })
  })
})
```



### request(options, callback)

The first argument can be either a url or an options object. The only required option is uri, all others are optional.

* `uri` || `url` - fully qualified uri or a parsed url object from url.parse()
* `qs` - object containing querystring values to be appended to the uri
* `method` - http method, defaults to GET
* `headers` - http headers, defaults to {}
* `body` - entity body for PATCH, POST and PUT requests. Must be buffer or string.
* `form` - when passed an object this will set `body` but to a querystring representation of value and adds `Content-type: application/x-www-form-urlencoded; charset=utf-8` header. When passed no option a FormData instance is returned that will be piped to request.
* `auth` - A hash containing values `user` || `username`, `password` || `pass`, and `sendImmediately` (optional).  See documentation above.
* `json` - sets `body` but to JSON representation of value and adds `Content-type: application/json` header.  Additionally, parses the response body as json.
* `multipart` - (experimental) array of objects which contains their own headers and `body` attribute. Sends `multipart/related` request. See example below.
* `followRedirect` - follow HTTP 3xx responses as redirects. defaults to true.
* `followAllRedirects` - follow non-GET HTTP 3xx responses as redirects. defaults to false.
* `maxRedirects` - the maximum number of redirects to follow, defaults to 10.
* `encoding` - Encoding to be used on `setEncoding` of response data. If set to `null`, the body is returned as a Buffer.
* `pool` - A hash object containing the agents for these requests. If omitted this request will use the global pool which is set to node's default maxSockets.
* `pool.maxSockets` - Integer containing the maximum amount of sockets in the pool.
* `timeout` - Integer containing the number of milliseconds to wait for a request to respond before aborting the request	
* `proxy` - An HTTP proxy to be used. Support proxy Auth with Basic Auth the same way it's supported with the `url` parameter by embedding the auth info in the uri.
* `oauth` - Options for OAuth HMAC-SHA1 signing, see documentation above.
* `hawk` - Options for [Hawk signing](https://github.com/hueniverse/hawk). The `credentials` key must contain the necessary signing info, [see hawk docs for details](https://github.com/hueniverse/hawk#usage-example).
* `strictSSL` - Set to `true` to require that SSL certificates be valid. Note: to use your own certificate authority, you need to specify an agent that was created with that ca as an option.
* `jar` - Set to `true` if you want cookies to be remembered for future use, or define your custom cookie jar (see examples section)
* `aws` - object containing aws signing information, should have the properties `key` and `secret` as well as `bucket` unless you're specifying your bucket as part of the path, or you are making a request that doesn't use a bucket (i.e. GET Services)
* `httpSignature` - Options for the [HTTP Signature Scheme](https://github.com/joyent/node-http-signature/blob/master/http_signing.md) using [Joyent's library](https://github.com/joyent/node-http-signature). The `keyId` and `key` properties must be specified. See the docs for other options.
* `localAddress` - Local interface to bind for network connections.


The callback argument gets 3 arguments. The first is an error when applicable (usually from the http.Client option not the http.ClientRequest object). The second is an http.ClientResponse object. The third is the response body String or Buffer.

## Convenience methods

There are also shorthand methods for different HTTP METHODs and some other conveniences.

### request.defaults(options)  
  
This method returns a wrapper around the normal request API that defaults to whatever options you pass in to it.

### request.put

Same as request() but defaults to `method: "PUT"`.

```javascript
request.put(url)
```

### request.patch

Same as request() but defaults to `method: "PATCH"`.

```javascript
request.patch(url)
```

### request.post

Same as request() but defaults to `method: "POST"`.

```javascript
request.post(url)
```

### request.head

Same as request() but defaults to `method: "HEAD"`.

```javascript
request.head(url)
```

### request.del

Same as request() but defaults to `method: "DELETE"`.

```javascript
request.del(url)
```

### request.get

Alias to normal request method for uniformity.

```javascript
request.get(url)
```
### request.cookie

Function that creates a new cookie.

```javascript
request.cookie('cookie_string_here')
```
### request.jar

Function that creates a new cookie jar.

```javascript
request.jar()
```


## Examples:

```javascript
  var request = require('request')
    , rand = Math.floor(Math.random()*100000000).toString()
    ;
  request(
    { method: 'PUT'
    , uri: 'http://mikeal.iriscouch.com/testjs/' + rand
    , multipart: 
      [ { 'content-type': 'application/json'
        ,  body: JSON.stringify({foo: 'bar', _attachments: {'message.txt': {follows: true, length: 18, 'content_type': 'text/plain' }}})
        }
      , { body: 'I am an attachment' }
      ] 
    }
  , function (error, response, body) {
      if(response.statusCode == 201){
        console.log('document saved as: http://mikeal.iriscouch.com/testjs/'+ rand)
      } else {
        console.log('error: '+ response.statusCode)
        console.log(body)
      }
    }
  )
```
Cookies are disabled by default (else, they would be used in subsequent requests). To enable cookies set jar to true (either in defaults or in the options sent).

```javascript
var request = request.defaults({jar: true})
request('http://www.google.com', function () {
  request('http://images.google.com')
})
```

If you to use a custom cookie jar (instead of letting request use its own global cookie jar) you do so by setting the jar default or by specifying it as an option:

```javascript
var j = request.jar()
var request = request.defaults({jar:j})
request('http://www.google.com', function () {
  request('http://images.google.com')
})
```
OR

```javascript
var j = request.jar()
var cookie = request.cookie('your_cookie_here')
j.add(cookie)
request({url: 'http://www.google.com', jar: j}, function () {
  request('http://images.google.com')
})
```
