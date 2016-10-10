#connect-restreamer

connect's bodyParser has a problem when using it with a proxy. It gobbles up all the
body events, so that the proxy doesn't see anything!

`connect-restreamer` comes to the rescue by re-emitting the body to the proxy.

it has defaults that make it suitable for use with connect's `bodyParser` but can be customized.

## usage

just include `connect-restreamer` in you're middleware chain after the bodyParser

    var bodyParser = require('connect/lib/middleware/bodyParser')
      , restreamer = require('connect-restreamer')

    //don't worry about incoming contont type
    //bodyParser.parse[''] = JSON.parse

    require('http-proxy').createServer(
      //refactor the body parser and re-streamer into a separate package
      bodyParser(),
      //body parser absorbs the data and end events before passing control to the next
      // middleware. if we want to proxy it, we'll need to re-emit these events after 
      //passing control to the middleware.
      require('connect-restreamer')(),
      function (req,res, proxy) {
        //custom proxy logic
        //... see https://github.com/nodejitsu/node-http-proxy
      }
    ).listen(80)


## customization

restreamer takes 3 options:

    var options = {
      modify: function(body) {
        //a function that may modify the buffered property
        return body
      },
      property: 'body', //name of the buffered property
      stringify:JSON.stringify //function to turn the buffered object back into a string
    }

   require('connect-restreamer')(options)
