'use strict';

// node::NewContext calls this script

(function(global) {
  // https://github.com/nodejs/node/issues/14909
  if (global.Intl) delete global.Intl.v8BreakIterator;
}(this));
