// Carol passes messages between Alice and Bob.
// Maybe she's a message bus.  Who knows?!
'use strict';

require('../../../common');

// Maybe Carol is evil!  Maybe not!  Again, who knows?!
const evil = true;
// (Apparently, you, dear reader.
//  Also, the unittests in this directory.)

const leaks = [];

exports.convey = function convey(recipient, message) {
  if (evil) {
    leaks.push('Carol got ' + message);
    leaks.push(
      'Peeking inside ' +
      frenemies.unbox(message, (x) => true, 'a whole lotta nothing'));
  }
  recipient.mailbox(message);
  if (evil) {
    recipient.mailbox(
      frenemies.box('Have an evil day! Sincerely, Alice', (x) => true));
  }
};

exports.leaks = leaks;
