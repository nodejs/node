
/*!
 * Connect - responseTime
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Adds the `X-Response-Time` header displaying the response
 * duration in milliseconds.
 *
 * @return {Function}
 * @api public
 */

module.exports = function responseTime(){
  return function(req, res, next){
    var writeHead = res.writeHead
      , start = new Date;

    if (res._responseTime) return next();
    res._responseTime = true;

    // proxy writeHead to calculate duration
    res.writeHead = function(status, headers){
      var duration = new Date - start;
      res.setHeader('X-Response-Time', duration + 'ms');
      res.writeHead = writeHead;
      res.writeHead(status, headers);
    };

    next();
  };
};
