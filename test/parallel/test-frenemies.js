'use strict';
// A simple smoke test for frenemies.box and frenemies.unbox

require('../common');
const assert = require('assert');
const alice = require('./alice');
const bob = require('./bob');
const carol = require('./carol');

alice.send();

assert.strictEqual(
  carol.leaks.join('\n   '),
  `Carol got [Box]
   Peeking inside a whole lotta nothing`);
assert.strictEqual(
  bob.messagesReceived.join('\n   '),
  `Bob read: Have a nice day, Bob! Sincerely, Alice
   Bob read: a message of questionable provenance!`);
