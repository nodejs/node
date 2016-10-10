/*!
 * Connect - errorHandler
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var utils = require('../utils')
  , url = require('url')
  , fs = require('fs');

/**
 * Flexible error handler, providing (_optional_) stack traces
 * and error message responses for requests accepting text, html,
 * or json.
 *
 * Options:
 *
 *   - `showStack`, `stack` respond with both the error message and stack trace. Defaults to `false`
 *   - `showMessage`, `message`, respond with the exception message only. Defaults to `false`
 *   - `dumpExceptions`, `dump`, dump exceptions to stderr (without terminating the process). Defaults to `false`
 *
 * Text:
 *
 *   By default, and when _text/plain_ is accepted a simple stack trace
 *   or error message will be returned.
 *
 * JSON:
 *
 *   When _application/json_ is accepted, connect will respond with
 *   an object in the form of `{ "error": error }`. 
 *
 * HTML:
 *
 *   When accepted connect will output a nice html stack trace.
 *
 * @param {Object} options
 * @return {Function}
 * @api public
 */

exports = module.exports = function errorHandler(options){
  options = options || {};

  // defaults
  var showStack = options.showStack || options.stack
    , showMessage = options.showMessage || options.message
    , dumpExceptions = options.dumpExceptions || options.dump
    , formatUrl = options.formatUrl;

  return function errorHandler(err, req, res, next){
    res.statusCode = 500;
    if (dumpExceptions) console.error(err.stack);
    if (showStack) {
      var accept = req.headers.accept || '';
      // html
      if (~accept.indexOf('html')) {
        fs.readFile(__dirname + '/../public/style.css', 'utf8', function(e, style){
          fs.readFile(__dirname + '/../public/error.html', 'utf8', function(e, html){
            var stack = (err.stack || '')
              .split('\n').slice(1)
              .map(function(v){ return '<li>' + v + '</li>'; }).join('');
              html = html
                .replace('{style}', style)
                .replace('{stack}', stack)
                .replace('{title}', exports.title)
                .replace(/\{error\}/g, utils.escape(err.toString()));
              res.setHeader('Content-Type', 'text/html');
              res.end(html);
          });
        });
      // json
      } else if (~accept.indexOf('json')) {
        var json = JSON.stringify({ error: err });
        res.setHeader('Content-Type', 'application/json');
        res.end(json);
      // plain text
      } else {
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end(err.stack);
      }
    } else {
      var body = showMessage
        ? err.toString()
        : 'Internal Server Error';
      res.setHeader('Content-Type', 'text/plain');
      res.end(body);
    }
  };
};

/**
 * Template title.
 */

exports.title = 'Connect';