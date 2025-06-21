'use strict';

require('../common');
const { test, assert_equals, assert_unreached } =
  require('../common/wpt').harness;

// Manually ported from: https://github.com/web-platform-tests/wpt/blob/6cef1d2087d6a07d7cc6cee8cf207eec92e27c5f/dom/events/EventTarget-this-of-listener.html

// Mock document
const document = {
  createElement: () => new EventTarget(),
  createTextNode: () => new EventTarget(),
  createDocumentFragment: () => new EventTarget(),
  createComment: () => new EventTarget(),
  createProcessingInstruction: () => new EventTarget(),
};

test(() => {
  const nodes = [
    document.createElement('p'),
    document.createTextNode('some text'),
    document.createDocumentFragment(),
    document.createComment('a comment'),
    document.createProcessingInstruction('target', 'data'),
  ];

  let callCount = 0;
  for (const node of nodes) {
    node.addEventListener('someevent', function() {
      ++callCount;
      assert_equals(this, node);
    });

    node.dispatchEvent(new Event('someevent'));
  }

  assert_equals(callCount, nodes.length);
}, 'the this value inside the event listener callback should be the node');

test(() => {
  const nodes = [
    document.createElement('p'),
    document.createTextNode('some text'),
    document.createDocumentFragment(),
    document.createComment('a comment'),
    document.createProcessingInstruction('target', 'data'),
  ];

  let callCount = 0;
  for (const node of nodes) {
    const handler = {};

    node.addEventListener('someevent', handler);
    handler.handleEvent = function() {
      ++callCount;
      assert_equals(this, handler);
    };

    node.dispatchEvent(new Event('someevent'));
  }

  assert_equals(callCount, nodes.length);
}, 'addEventListener should not require handleEvent to be defined on object listeners');

test(() => {
  const nodes = [
    document.createElement('p'),
    document.createTextNode('some text'),
    document.createDocumentFragment(),
    document.createComment('a comment'),
    document.createProcessingInstruction('target', 'data'),
  ];

  let callCount = 0;
  for (const node of nodes) {
    function handler() {
      ++callCount;
      assert_equals(this, node);
    }

    handler.handleEvent = () => {
      assert_unreached('should not call the handleEvent method on a function');
    };

    node.addEventListener('someevent', handler);

    node.dispatchEvent(new Event('someevent'));
  }

  assert_equals(callCount, nodes.length);
}, 'handleEvent properties added to a function before addEventListener are not reached');

test(() => {
  const nodes = [
    document.createElement('p'),
    document.createTextNode('some text'),
    document.createDocumentFragment(),
    document.createComment('a comment'),
    document.createProcessingInstruction('target', 'data'),
  ];

  let callCount = 0;
  for (const node of nodes) {
    function handler() {
      ++callCount;
      assert_equals(this, node);
    }

    node.addEventListener('someevent', handler);

    handler.handleEvent = () => {
      assert_unreached('should not call the handleEvent method on a function');
    };

    node.dispatchEvent(new Event('someevent'));
  }

  assert_equals(callCount, nodes.length);
}, 'handleEvent properties added to a function after addEventListener are not reached');
