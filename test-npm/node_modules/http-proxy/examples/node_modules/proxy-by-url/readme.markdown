#proxy by url

this is a simple example of a node-http-middleware that will proxy based on the incoming url.
say you want to proxy every request thing under /database to localhost:5984 (couchbd)
(and remove the /database prefix)

this is how:

    require('http-proxy').createServer(
      require('proxy-by-url')({
        '/database': { port: 5984, host: 'localhost' },
      })
    ).listen(8000)
