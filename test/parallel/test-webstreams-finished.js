'use strict';

const common = require('../common');
const assert = require('assert');
const { ReadableStream, WritableStream } = require('stream/web');
const { finished } = require('stream');

{
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue('asd');
    },
  });
  finished(rs, common.mustCall(() => {
    
  }));
}
