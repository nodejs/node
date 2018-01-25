'use strict';
const common = require('../common');
const { Readable, PassThrough } = require('stream');

const source = new Readable({
  read: () => {}
});

source.push('foo');
source.push('bar');
source.push(null);

const pt = source.pipe(new PassThrough());

const wrapper = new Readable({
  read: () => {
    let data = pt.read();

    if (data) {
      wrapper.push(data);
      return;
    }

    pt.once('readable', function() {
      data = pt.read();
      if (data) {
        wrapper.push(data);
      }
      // else the end event should fire
    });
  }
});

pt.once('end', function() {
  wrapper.push(null);
});

wrapper.resume();
wrapper.once('end', common.mustCall());
