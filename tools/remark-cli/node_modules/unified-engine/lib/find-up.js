'use strict';

var fs = require('fs');
var path = require('path');
var fault = require('fault');
var debug = require('debug')('unified-engine:find-up');
var func = require('x-is-function');
var object = require('is-object');

module.exports = FindUp;

var read = fs.readFile;
var resolve = path.resolve;
var relative = path.relative;
var join = path.join;
var dirname = path.dirname;

FindUp.prototype.load = load;

function FindUp(options) {
  var self = this;
  var fp = options.filePath;

  self.cache = {};
  self.cwd = options.cwd;
  self.detect = options.detect;
  self.names = options.names;
  self.create = options.create;

  if (fp) {
    self.givenFilePath = resolve(options.cwd, fp);
  }
}

function load(filePath, callback) {
  var self = this;
  var cache = self.cache;
  var givenFilePath = self.givenFilePath;
  var givenFile = self.givenFile;
  var names = self.names;
  var create = self.create;
  var cwd = self.cwd;
  var parent;

  if (givenFilePath) {
    if (givenFile) {
      apply(callback, givenFile);
    } else {
      givenFile = [callback];
      self.givenFile = givenFile;
      debug('Checking given file `%s`', givenFilePath);
      read(givenFilePath, loadGiven);
    }

    return;
  }

  if (!self.detect) {
    return callback();
  }

  filePath = resolve(cwd, filePath);
  parent = dirname(filePath);

  if (parent in cache) {
    apply(callback, cache[parent]);
  } else {
    cache[parent] = [callback];
    find(parent);
  }

  function loadGiven(err, buf) {
    var cbs = self.givenFile;
    var result;

    if (err) {
      result = fault('Cannot read given file `%s`\n%s', relative(cwd, givenFilePath), err.stack);
      result.code = 'ENOENT';
      result.path = err.path;
      result.syscall = err.syscall;
    } else {
      try {
        result = create(buf, givenFilePath);
        debug('Read given file `%s`', givenFilePath);
      } catch (err) {
        result = fault('Cannot parse given file `%s`\n%s', relative(cwd, givenFilePath), err.stack);
        debug(err.message);
      }
    }

    givenFile = result;
    self.givenFile = result;
    applyAll(cbs, result);
  }

  function find(directory) {
    var index = -1;
    var length = names.length;

    next();

    function next() {
      var parent;

      /* Try to read the next file. We don’t use `readdir` because on
       * huge directories, that could be *very* slow. */
      if (++index < length) {
        read(join(directory, names[index]), done);
      } else {
        parent = dirname(directory);

        if (directory === parent) {
          debug('No files found for `%s`', filePath);
          found();
        } else if (parent in cache) {
          apply(found, cache[parent]);
        } else {
          cache[parent] = [found];
          find(parent);
        }
      }
    }

    function done(err, buf) {
      var name = names[index];
      var fp = join(directory, name);
      var contents;

      /* istanbul ignore if - Hard to test. */
      if (err) {
        if (err.code === 'ENOENT') {
          return next();
        }

        err = fault('Cannot read file `%s`\n%s', relative(cwd, fp), err.message);
        debug(err.message);
        return found(err);
      }

      try {
        contents = create(buf, fp);
      } catch (err) {
        return found(fault('Cannot parse file `%s`\n%s', relative(cwd, fp), err.message));
      }

      /* istanbul ignore else - maybe used in the future. */
      if (contents) {
        debug('Read file `%s`', fp);
        found(null, contents);
      } else {
        next();
      }
    }

    function found(err, result) {
      var cbs = cache[directory];
      cache[directory] = err || result;
      applyAll(cbs, err || result);
    }
  }

  function applyAll(cbs, result) {
    var index = cbs.length;

    while (index--) {
      apply(cbs[index], result);
    }
  }

  function apply(cb, result) {
    if (object(result) && func(result[0])) {
      result.push(cb);
    } else if (result instanceof Error) {
      cb(result);
    } else {
      cb(null, result);
    }
  }
}
