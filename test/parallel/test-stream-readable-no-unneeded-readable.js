'use strict';
const common = require('../common');
const { Readable, PassThrough } = require('stream');

function test(r) {
  const wrapper = new Readable({
    read: () => {
      let data = r.read();

      if (data) {
        wrapper.push(data);
        return;
      }

      r.once('readable', function() {
        data = r.read();
        if (data) {
          wrapper.push(data);
        }
        // else the end event should fire
      });
    },
  });

  r.once('end', function() {
    wrapper.push(null);
  });

  wrapper.resume();
  wrapper.once('end', common.mustCall());
}

{
  const source = new Readable({
    read: () => {}
  });
  source.push('foo');
  source.push('bar');
  source.push(null);

  const pt = source.pipe(new PassThrough());
  test(pt);
}

{
  // This is the underlying cause of the above test case.
  const pushChunks = ['foo', 'bar'];
  const r = new Readable({
    read: () => {
      const chunk = pushChunks.shift();
      if (chunk) {
        // synchronous call
        r.push(chunk);
      } else {
        // asynchronous call
        process.nextTick(() => r.push(null));
      }
    },
  });

  test(r);
}
