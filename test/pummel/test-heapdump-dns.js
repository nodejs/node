'use strict';
// This tests heap snapshot integration of dns ChannelWrap.

require('../common');
const { validateByRetainingPath } = require('../common/heap');
const assert = require('assert');

// Before dns is loaded, no ChannelWrap should be created.
{
  const nodes = validateByRetainingPath('Node / ChannelWrap', []);
  assert.strictEqual(nodes.length, 0);
}

const dns = require('dns');

// Right after dns is loaded, a ChannelWrap should be created for the default
// resolver, but it has no task list.
{
  validateByRetainingPath('Node / ChannelWrap', [
    { node_name: 'ChannelWrap', edge_name: 'native_to_javascript' },
  ]);
}

dns.resolve('localhost', () => {});

// After dns resolution, the ChannelWrap of the default resolver has the task list.
{
  validateByRetainingPath('Node / ChannelWrap', [
    { node_name: 'Node / NodeAresTask::List', edge_name: 'task_list' },
  ]);
}
