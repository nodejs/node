'use strict';

const common = require('../common');
const assert = require('assert');
const { ReadableStream, WritableStream } = require('stream/web');
const { finished } = require('stream');

{
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue('asd');
      controller.close();
    },
  });
  finished(rs, common.mustSucceed());
  async function test() {
    const values = [];
    for await (const chunk of rs) {
      values.push(chunk);
    }
    assert.deepStrictEqual(values, ['asd']);
  }
  test();
}

{
  let str = '';
  const ws = new WritableStream({
    write(chunk) {
      console.log(chunk);
      str += chunk;
    }
  });
  finished(ws, common.mustSucceed(() => {
    assert.strictEqual(str, 'asd');
  }));
  const writer = ws.getWriter();
  writer.write('asd');
  writer.close();
}
