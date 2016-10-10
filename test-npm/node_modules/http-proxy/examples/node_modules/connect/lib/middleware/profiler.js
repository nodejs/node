
/*!
 * Connect - profiler
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Profile the duration of a request.
 *
 * Typically this middleware should be utilized
 * _above_ all others, as it proxies the `res.end()`
 * method, being first allows it to encapsulate all
 * other middleware.
 *
 * Example Output:
 *
 *      GET /
 *      response time 2ms
 *      memory rss 52.00kb
 *      memory vsize 2.07mb
 *      heap before 3.76mb / 8.15mb
 *      heap after 3.80mb / 8.15mb
 *
 * @api public
 */

module.exports = function profiler(){
  return function(req, res, next){
    var end = res.end
      , start = snapshot();

    // state snapshot
    function snapshot() {
      return {
          mem: process.memoryUsage()
        , time: new Date
      };
    }

    // proxy res.end()
    res.end = function(data, encoding){
      res.end = end;
      res.end(data, encoding);
      compare(req, start, snapshot())
    };

    next();
  }
};

/**
 * Compare `start` / `end` snapshots.
 *
 * @param {IncomingRequest} req
 * @param {Object} start
 * @param {Object} end
 * @api private
 */

function compare(req, start, end) {
  console.log();
  row(req.method, req.url);
  row('response time:', (end.time - start.time) + 'ms');
  row('memory rss:', formatBytes(end.mem.rss - start.mem.rss));
  row('memory vsize:', formatBytes(end.mem.vsize - start.mem.vsize));
  row('heap before:', formatBytes(start.mem.heapUsed) + ' / ' + formatBytes(start.mem.heapTotal));
  row('heap after:', formatBytes(end.mem.heapUsed) + ' / ' + formatBytes(end.mem.heapTotal));
  console.log();
}

/**
 * Row helper
 *
 * @param {String} key
 * @param {String} val
 * @api private
 */

function row(key, val) {
  console.log('  \033[90m%s\033[0m \033[36m%s\033[0m', key, val);
}

/**
 * Format byte-size.
 *
 * @param {Number} bytes
 * @return {String}
 * @api private
 */

function formatBytes(bytes) {
  var kb = 1024
    , mb = 1024 * kb
    , gb = 1024 * mb;
  if (bytes < kb) return bytes + 'b';
  if (bytes < mb) return (bytes / kb).toFixed(2) + 'kb';
  if (bytes < gb) return (bytes / mb).toFixed(2) + 'mb';
  return (bytes / gb).toFixed(2) + 'gb';
};
