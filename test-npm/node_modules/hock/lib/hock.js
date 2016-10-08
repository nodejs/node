var http = require('http'),
    Request = require('./request'),
    deepEqual = require('deep-equal');

/**
 * Hock class
 *
 * @description This is the main class for Hock. It handles creation
 * of the underlying webserver, and enqueing all of the requests.
 *
 * @param {object}      [options]       options for your Hock server
 * @param {Number}      [options]       an optional port for your Hock server
 * @param {Number}      [options.port]  port number for your Hock server
 * @param {boolean}     [options.throwOnUnmatched]  Tell Hock to throw if
 *    receiving a request without a match (Default=true)
 *
 * @type {Function}
 */
var Hock = module.exports = function (options) {
  if (typeof options === 'number') {
    this.port = options;
    options = {};
  }
  else if (typeof options === 'object' && options.port) {
    this.port = options.port;
  }

  this._throwOnUnmatched = (typeof options.throwOnUnmatched === 'boolean' ? options.throwOnUnmatched : true);
  this._assertions = [];
  this._started = false;
};

/**
 * Hock.enqueue
 *
 * @description enqueue a request into the queue
 *
 * @param {object}    request     The request to enter in the hock queue
 * @param request
 */
Hock.prototype.enqueue = function (request) {
  if (this._requestFilter) {
    request._requestFilter = this._requestFilter;
  }

  if (this._defaultReplyHeaders) {
    request._defaultReplyHeaders = this._defaultReplyHeaders;
  }

  this._assertions.push(request);
};

/**
 * Hock.hasRoute
 *
 * @description test if there is a request on the assertions queue
 *
 * @param {String}    method      the method of the request to match
 * @param {String}    url         the route of the request to match
 * @param {String}    [body]      optionally - use if you set a body
 * @param {object}    [headers]   optionally - use if you set a header
 * @returns {Boolean}
 */
Hock.prototype.hasRoute = function (method, url, body, headers) {
  if (!body) {
    body = '';
  }

  if (!headers) {
    headers = {};
  }

  var found = this._assertions.filter(function(request) {
    return request.method === method
          && request.url === url
          && request.body === body
          && deepEqual(request.headers, headers);
  })
  return !!found.length;
};

/**
 * Hock.done
 *
 * @description Throw an error if there are unprocessed requests in the assertions queue.
 * If there are unfinsihed requests, i.e. min: 2, max 4 with a count of 2, that request will be
 * ignored for the purposes of throwing an error.
 *
 */
Hock.prototype.done = function (cb) {
  var err;

  if (this._assertions.length) {
    this._assertions = this._assertions.filter(function(request) {
      return request.isDone();
    });

    if (this._assertions.length) {
      err = new Error('Unprocessed Requests in Assertions Queue: \n' + JSON.stringify(this._assertions.map(function (item) {
        return item.method + ' ' + item.url;
      })));
    }
  }

  if (!err) {
    return cb && cb()
  }

  if (!cb) {
    throw err;
  }

  return cb(err);

};

/**
 * Hock.close
 *
 * @description stop listening and shutdown the Hock server
 *
 * @param callback
 */
Hock.prototype.close = function (callback) {
  this._server.close(callback);
};

/**
 * Hock.address
 *
 * @description retrieve the address for the hock server

 * @returns {*}
 */
Hock.prototype.address = function() {
  return this._server.address();
};

/**
 * Hock._initialize
 *
 * @description The internal helper function to start the Hock server
 *
 * @param {Function}    callback    This is fired when the server is listening
 * @private
 */
Hock.prototype._initialize = function (callback) {
  var self = this;

  if (self._started) {
    callback(new Error('Server is already listening'));
    return;
  }

  self._server = http.createServer(this._handleRequest());
  self._server.listen(self.port, function () {
    self._started = true;
    callback(null, self);
  });
};

/**
 * Hock.get
 *
 * @description enqueue a GET request into the assertion queue
 *
 * @param {String}    url         the route of the request to match
 * @param {object}    [headers]   optionally match the request headers
 * @returns {Request}
 */
Hock.prototype.get = function (url, headers) {
  return new Request(this, {
    method: 'GET',
    url: url,
    headers: headers || {}
  });
};

/**
 * Hock.head
 *
 * @description enqueue a HEAD request into the assertion queue
 *
 * @param {String}    url         the route of the request to match
 * @param {object}    [headers]   optionally match the request headers
 * @returns {Request}
 */
Hock.prototype.head = function (url, headers) {
  return new Request(this, {
    method: 'HEAD',
    url: url,
    headers: headers || {}
  });
};

/**
 * Hock.put
 *
 * @description enqueue a PUT request into the assertion queue
 *
 * @param {String}    url         the route of the request to match
 * @param {object}    [body]      the request body (if any) of the request to match
 * @param {String}    [body]      the request body (if any) of the request to match
 * @param {object}    [headers]   optionally match the request headers
 * @returns {Request}
 */
Hock.prototype.put = function (url, body, headers) {
  return new Request(this, {
    method: 'PUT',
    url: url,
    body: body || '',
    headers: headers || {}
  });
};

/**
 * Hock.post
 *
 * @description enqueue a POST request into the assertion queue
 *
 * @param {String}    url         the route of the request to match
 * @param {object}    [body]      the request body (if any) of the request to match
 * @param {String}    [body]      the request body (if any) of the request to match
 * @param {object}    [headers]   optionally match the request headers
 * @returns {Request}
 */
Hock.prototype.post = function (url, body, headers) {
  return new Request(this, {
    method: 'POST',
    url: url,
    body: body || '',
    headers: headers || {}
  });
};

/**
 * Hock.delete
 *
 * @description enqueue a DELETE request into the assertion queue
 *
 * @param {String}    url         the route of the request to match
 * @param {object}    [body]      the request body (if any) of the request to match
 * @param {String}    [body]      the request body (if any) of the request to match
 * @param {object}    [headers]   optionally match the request headers
 * @returns {Request}
 */
Hock.prototype.delete = function (url, body, headers) {
  return new Request(this, {
    method: 'DELETE',
    url: url,
    body: body || '',
    headers: headers || {}
  });
};

/**
 * Hock.filteringRequestBody
 *
 * @description Provide a function to Hock to filter the request body
 *
 * @param {function}    filter    the function to filter on
 *
 * @returns {Hock}
 */
Hock.prototype.filteringRequestBody = function (filter) {
  this._requestFilter = filter;
  return this;
};

/**
 * Hock.filteringRequestBodyRegEx
 *
 * @description match incoming requests, and replace the body based on
 * a regular expression match
 *
 * @param {RegEx}       source    The source regular expression
 * @param {string}      replace   What to replace the source with
 *
 * @returns {Hock}
 */
Hock.prototype.filteringRequestBodyRegEx = function (source, replace) {
  this._requestFilter = function (path) {
    if (path) {
      path = path.replace(source, replace);
    }
    return path;
  };

  return this;
};

/**
 * Hock.filteringPath
 *
 * @description Provide a function to Hock to filter request path
 *
 * @param {function}    filter    the function to filter on
 *
 * @returns {Hock}
 */
Hock.prototype.filteringPath = function (filter) {
  this._pathFilter = filter;
  return this;
};

/**
 * Hock.filteringPathRegEx
 *
 * @description match incoming requests, and replace the path based on
 * a regular expression match
 *
 * @param {RegEx}       source    The source regular expression
 * @param {string}      replace   What to replace the source with
 *
 * @returns {Hock}
 */
Hock.prototype.filteringPathRegEx = function (source, replace) {
  this._pathFilter = function (path) {
    if (path) {
      path = path.replace(source, replace);
    }
    return path;
  };

  return this;
};

/**
 * Hock.clearBodyFilter
 *
 * @description clear the body request filter, if any
 *
 * @returns {Hock}
 */
Hock.prototype.clearBodyFilter = function () {
  delete this._requestFilter;
  return this;
}

/**
 * Hock.defaultReplyHeaders
 *
 * @description set standard headers for all responses
 *
 * @param {object}    headers   the list of headers to send by default
 *
 * @returns {Hock}
 */
Hock.prototype.defaultReplyHeaders = function (headers) {
  this._defaultReplyHeaders = headers;
  return this;
};

/**
 * Hock._handleReqeust
 *
 * @description internal helper function for handling requests
 *
 * @returns {Function}
 * @private
 */
Hock.prototype._handleRequest = function () {
  var self = this;

  return function (req, res) {
    var matchIndex = null;

    req.body = '';

    req.on('data', function (data) {
      req.body += data.toString();
    });

    req.on('end', function () {

      for (var i = 0; i < self._assertions.length; i++) {
        if (self._assertions[i].isMatch(req)) {
          matchIndex = i;
          break;
        }
      }

      if (matchIndex === null) {
        if (self._throwOnUnmatched) {
          throw new Error('No Match For: ' + req.method + ' ' + req.url);
        }

        console.error('No Match For: ' + req.method + ' ' + req.url);
        if (req.method === 'PUT' || req.method === 'POST') {
          console.error(req.body);
        }
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end('No Matching Response!\n');
      }
      else {
        if (self._assertions[matchIndex].sendResponse(res)) {
          self._assertions.splice(matchIndex, 1)[0];
        }
      }
    });
  }
};

/**
 * exports.createHock
 *
 * @description static method for creating your hock server
 *
 * @param {object}      [options]       options for your Hock server
 * @param {Number}      [options.port]  port number for your Hock server
 * @param {boolean}     [options.throwOnUnmatched]  Tell Hock to throw if
 *    receiving a request without a match (Default=true)
 *
 * @param {function}    callback        invoked when hock is ready and listening
 *
 * @returns {Hock}
 */
var createHock = function(options, callback) {
  // options is optional
  if (typeof options === 'function') {
    callback = options;
    options = {};
  }
  var hock = new Hock(options);
  hock._initialize(callback);
  return hock;
};

module.exports = createHock;
module.exports.createHock = createHock;
