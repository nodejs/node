//
// This is an example of a url-routing middleware.
// This is not intended for production use, but rather as
// an example of how to write a middleware.
//

function matcher (url, dest) {
  // First, turn the URL into a regex.
  // NOTE: Turning user input directly into a Regular Expression is NOT SAFE.
  var r = new RegExp(url.replace(/\//, '\\/'));
  // This next block of code may look a little confusing.
  // It returns a closure (anonymous function) for each URL to be matched,
  // storing them in an array - on each request, if the URL matches one that has
  // a function stored for it, the function will be called.
  return function (url) {
    var m = r.exec(url);
    if (!m) {
      return;
    }
    var path = url.slice(m[0].length);
    console.log('proxy:', url, '->', dest);
    return {url: path, dest: dest};
  }
}

module.exports = function (urls) {
  // This is the entry point for our middleware.
  // 'matchers' is the array of URL matchers, as mentioned above.
  var matchers = [];
  for (var url in urls) {
    // Call the 'matcher' function above, and store the resulting closure.
    matchers.push(matcher(url, urls[url]));
  }

  // This closure is returned as the request handler.
  return function (req, res, next) {
    //
    // in node-http-proxy middlewares, `proxy` is the prototype of `next`
    // (this means node-http-proxy middlewares support both the connect API (req, res, next)
    // and the node-http-proxy API (req, res, proxy)
    //
    var proxy = next;
    for (var k in matchers) {
      // for each URL matcher, try the request's URL.
      var m = matchers[k](req.url);
      // If it's a match:
      if (m) {
        // Replace the local URL with the destination URL.
        req.url = m.url;
        // If routing to a server on another domain, the hostname in the request must be changed.
        req.headers.host = m.host;
        // Once any changes are taken care of, this line makes the magic happen.
        return proxy.proxyRequest(req, res, m.dest);
      }
    }
    next() //if there wasno matching rule, fall back to next middleware.
  }
}
