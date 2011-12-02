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

Request is designed to be the simplest way possible to make http calls. It support HTTPS and follows redirects by default.

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
fs.readStream('file.json').pipe(request.put('http://mysite.com/obj.json'))
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
  // Assume by some stretch of magic you aquired the verifier
  var access_token = qs.parse(body)
    , oauth = 
      { consumer_key: CONSUMER_KEY
      , consumer_secret: CONSUMER_SECRET
      , token: access_token.oauth_token
      , verifier: VERIFIER
      , token_secret: access_token.oauth_token_secret
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
* `method` - http method, defaults to GET
* `headers` - http headers, defaults to {}
* `body` - entity body for POST and PUT requests. Must be buffer or string.
* `json` - sets `body` but to JSON representation of value and adds `Content-type: application/json` header.
* `multipart` - (experimental) array of objects which contains their own headers and `body` attribute. Sends `multipart/related` request. See example below.
* `followRedirect` - follow HTTP 3xx responses as redirects. defaults to true.
* `maxRedirects` - the maximum number of redirects to follow, defaults to 10.
* `onResponse` - If true the callback will be fired on the "response" event instead of "end". If a function it will be called on "response" and not effect the regular semantics of the main callback on "end".
* `encoding` - Encoding to be used on response.setEncoding when buffering the response data.
* `pool` - A hash object containing the agents for these requests. If omitted this request will use the global pool which is set to node's default maxSockets.
* `pool.maxSockets` - Integer containing the maximum amount of sockets in the pool.
* `timeout` - Integer containing the number of milliseconds to wait for a request to respond before aborting the request	
* `proxy` - An HTTP proxy to be used. Support proxy Auth with Basic Auth the same way it's supported with the `url` parameter by embedding the auth info in the uri.
* `oauth` - Options for OAuth HMAC-SHA1 signing, see documentation above.
* `strictSSL` - Set to `true` to require that SSL certificates be valid. Note: to use your own certificate authority, you need to specify an agent that was created with that ca as an option.
* `jar` - Set to `false` if you don't want cookies to be remembered for future use or define your custom cookie jar (see examples section)


The callback argument gets 3 arguments. The first is an error when applicable (usually from the http.Client option not the http.ClientRequest object). The second in an http.ClientResponse object. The third is the response body buffer.

## Convenience methods

There are also shorthand methods for different HTTP METHODs and some other conveniences.

### request.defaults(options)  
  
This method returns a wrapper around the normal request API that defaults to whatever options you pass in to it.

### request.put

Same as request() but defaults to `method: "PUT"`.

```javascript
request.put(url)
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
Cookies are enabled by default (so they can be used in subsequent requests). To disable cookies set jar to false (either in defaults or in the options sent).

```javascript
var request = request.defaults({jar: false})
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
