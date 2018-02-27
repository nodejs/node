// Bob gets a message from Alice and verifies that it comes from her.
'use strict';

require('../../../common');

function ifFrom(sender) {
  const alice = require('./alice');
  return sender === alice.publicKey && sender();
}

const messagesReceived = [];

// Carol puts messages in mailboxes.
exports.mailbox = function mailbox(box) {
  const value = frenemies.unbox(
    box, ifFrom, 'a message of questionable provenance!');
  messagesReceived.push(`Bob read: ${value}`);
};
exports.messagesReceived = messagesReceived;
