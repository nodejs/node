
/*!
 * Connect - vhost
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Setup vhost for the given `hostname` and `server`.
 *
 * Examples:
 *
 *     connect(
 *       connect.vhost('foo.com',
 *         connect.createServer(...middleware...)
 *       ),
 *       connect.vhost('bar.com',
 *         connect.createServer(...middleware...)
 *       )
 *     );
 *
 * @param {String} hostname
 * @param {Server} server
 * @return {Function}
 * @api public
 */

module.exports = function vhost(hostname, server){
  if (!hostname) throw new Error('vhost hostname required');
  if (!server) throw new Error('vhost server required');
  var regexp = new RegExp('^' + hostname.replace(/[*]/g, '(.*?)') + '$');
  if (server.onvhost) server.onvhost(hostname);
  return function vhost(req, res, next){
    if (!req.headers.host) return next();
    var host = req.headers.host.split(':')[0];
    if (req.subdomains = regexp.exec(host)) {
      req.subdomains = req.subdomains[0].split('.').slice(0, -1);
      server.emit("request", req, res, next);
    } else {
      next();
    }
  };
};
