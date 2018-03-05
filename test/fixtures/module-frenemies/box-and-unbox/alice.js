// Alice sends a message to Bob for Bob's eyes only.
'use strict';

require('../../../common');
const bob = require('./bob');
const carol = require('./carol');

const mayOpen = (opener) => opener === bob.publicKey && opener();
const messageForBob = frenemies.box(
  'Have a nice day, Bob! Sincerely, Alice',
  mayOpen);

exports.send = () => carol.convey(bob, messageForBob);
