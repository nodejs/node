'use strict';

var rc = require('rc'),
    untildify = require('untildify');

var path = require('path');


module.exports = function () {
  var rcPrefix = rc('npm', null, []).prefix;

  if (rcPrefix) {
    return untildify(rcPrefix);
  }
  else if (process.platform == 'win32') {
    return path.dirname(process.execPath);
  }
  else {
    return path.resolve(process.execPath, '../..');
  }
};
