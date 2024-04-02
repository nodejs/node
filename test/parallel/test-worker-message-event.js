'use strict';
require('../common');
const assert = require('assert');

const dummyPort = new MessageChannel().port1;

{
  for (const [ args, expected ] of [
    [
      ['message'],
      {
        type: 'message', data: null, origin: '',
        lastEventId: '', source: null, ports: []
      },
    ],
    [
      ['message', { data: undefined, origin: 'foo' }],
      {
        type: 'message', data: null, origin: 'foo',
        lastEventId: '', source: null, ports: []
      },
    ],
    [
      ['message', { data: 2, origin: 1, lastEventId: 0 }],
      {
        type: 'message', data: 2, origin: '1',
        lastEventId: '0', source: null, ports: []
      },
    ],
    [
      ['message', { lastEventId: 'foo' }],
      {
        type: 'message', data: null, origin: '',
        lastEventId: 'foo', source: null, ports: []
      },
    ],
    [
      ['messageerror', { lastEventId: 'foo', source: dummyPort }],
      {
        type: 'messageerror', data: null, origin: '',
        lastEventId: 'foo', source: dummyPort, ports: []
      },
    ],
    [
      ['message', { ports: [dummyPort], source: null }],
      {
        type: 'message', data: null, origin: '',
        lastEventId: '', source: null, ports: [dummyPort]
      },
    ],
  ]) {
    const ev = new MessageEvent(...args);
    const { type, data, origin, lastEventId, source, ports } = ev;
    assert.deepStrictEqual(expected, {
      type, data, origin, lastEventId, source, ports
    });
  }
}

{
  assert.throws(() => {
    new MessageEvent('message', { source: 1 });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "init\.source" property must be an instance of MessagePort/,
  });
  assert.throws(() => {
    new MessageEvent('message', { source: {} });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "init\.source" property must be an instance of MessagePort/,
  });
  assert.throws(() => {
    new MessageEvent('message', { ports: 0 });
  }, {
    message: /ports is not iterable/,
  });
  assert.throws(() => {
    new MessageEvent('message', { ports: [ null ] });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "init\.ports\[0\]" property must be an instance of MessagePort/,
  });
  assert.throws(() => {
    new MessageEvent('message', { ports: [ {} ] });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "init\.ports\[0\]" property must be an instance of MessagePort/,
  });
}

{
  assert(new MessageEvent('message') instanceof Event);
}
