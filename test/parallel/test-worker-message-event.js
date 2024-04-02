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
    name: 'TypeError',
    message: /Expected 1 to be an instance of MessagePort/,
  });
  assert.throws(() => {
    new MessageEvent('message', { source: {} });
  }, {
    name: 'TypeError',
    message: /Expected {} to be an instance of MessagePort/,
  });
  assert.throws(() => {
    new MessageEvent('message', { ports: 0 });
  }, {
    message: /Sequence: Value of type Number is not an Object/,
  });
  assert.throws(() => {
    new MessageEvent('message', { ports: [ null ] });
  }, {
    name: 'TypeError',
    message: /Expected null to be an instance of MessagePort/,
  });
  assert.throws(() => {
    new MessageEvent('message', { ports: [ {} ] });
  }, {
    name: 'TypeError',
    message: /Expected {} to be an instance of MessagePort/,
  });
}

{
  assert(new MessageEvent('message') instanceof Event);
}
