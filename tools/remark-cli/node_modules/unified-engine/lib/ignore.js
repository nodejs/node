'use strict';

var path = require('path');
var gitignore = require('ignore');
var FindUp = require('./find-up');

module.exports = Ignore;

Ignore.prototype.check = check;

var dirname = path.dirname;
var relative = path.relative;
var resolve = path.resolve;

function Ignore(options) {
  this.cwd = options.cwd;

  this.findUp = new FindUp({
    filePath: options.ignorePath,
    cwd: options.cwd,
    detect: options.detectIgnore,
    names: options.ignoreName ? [options.ignoreName] : [],
    create: create
  });
}

function check(filePath, callback) {
  var self = this;

  self.findUp.load(filePath, done);

  function done(err, ignore) {
    var normal;

    if (err) {
      callback(err);
    } else if (ignore) {
      normal = relative(ignore.filePath, resolve(self.cwd, filePath));
      callback(null, normal ? ignore.ignores(normal) : false);
    } else {
      callback(null, false);
    }
  }
}

function create(buf, filePath) {
  var ignore = gitignore().add(String(buf));
  ignore.filePath = dirname(filePath);
  return ignore;
}
