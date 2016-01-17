'use strict';

/*!
 * ws: a node.js websocket client
 * Copyright(c) 2011 Einar Otto Stangvik <einaros@gmail.com>
 * MIT Licensed
 */

try {
  module.exports = require('bufferutil');
} catch (e) {
  module.exports = require('./BufferUtil.fallback');
}
