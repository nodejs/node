'use strict';
const dns = require('dns');
const assert = require('assert');

assert(process.env.NODE_TEST_HOST);

const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;

function onError(err) {
  console.error('error:', err);
}

function onLookup(address, family) {
  console.log(`address: ${JSON.stringify(address)}`);
  console.log(`family: ${JSON.stringify(family)}`);
}

function query() {
  const host = process.env.NODE_TEST_HOST;
  if (process.env.NODE_TEST_PROMISE === 'true') {
    dns.promises.lookup(host, { family: 4 }).then(
      ({address, family}) => onLookup(address, family),
      onError);
  } else {
    dns.lookup(host, { family: 4 }, (err, address, family) => {
      if (err) {
        onError(err);
      } else {
        onLookup(address, family);
      }
    });
  }
}

query();

setDeserializeMainFunction(query);
