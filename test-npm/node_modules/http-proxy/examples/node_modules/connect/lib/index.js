
/**
 * # Connect
 *
 * Connect is a middleware framework for node,
 * shipping with over 11 bundled middleware and a rich choice of
 * [3rd-party middleware](https://github.com/senchalabs/connect/wiki).
 *
 * Installation:
 * 
 *     $ npm install connect
 * 
 * API:
 *
 *  - [connect](connect.html) general
 *  - [http](http.html) http server
 *  - [https](https.html) https server
 *
 * Middleware:
 *
 *  - [logger](middleware-logger.html) request logger with custom format support
 *  - [csrf](middleware-csrf.html) Cross-site request forgery protection
 *  - [basicAuth](middleware-basicAuth.html) basic http authentication
 *  - [bodyParser](middleware-bodyParser.html) extensible request body parser
 *  - [cookieParser](middleware-cookieParser.html) cookie parser
 *  - [session](middleware-session.html) session management support with bundled [MemoryStore](middleware-session-memory.html)
 *  - [compiler](middleware-compiler.html) static asset compiler (sass, less, coffee-script, etc)
 *  - [methodOverride](middleware-methodOverride.html) faux HTTP method support
 *  - [responseTime](middleware-responseTime.html) calculates response-time and exposes via X-Response-Time
 *  - [router](middleware-router.html) provides rich Sinatra / Express-like routing
 *  - [static](middleware-static.html) streaming static file server supporting `Range` and more
 *  - [directory](middleware-directory.html) directory listing middleware
 *  - [vhost](middleware-vhost.html) virtual host sub-domain mapping middleware
 *  - [favicon](middleware-favicon.html) efficient favicon server (with default icon)
 *  - [limit](middleware-limit.html) limit the bytesize of request bodies
 *  - [profiler](middleware-profiler.html) request profiler reporting response-time, memory usage, etc
 *  - [query](middleware-query.html) automatic querystring parser, populating `req.query`
 *  - [errorHandler](middleware-errorHandler.html) flexible error handler
 *
 * Internals:
 *
 *  - connect [utilities](utils.html)
 *  - node monkey [patches](patch.html)
 *
 */