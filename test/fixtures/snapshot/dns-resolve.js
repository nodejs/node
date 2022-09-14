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

function onResolve(addresses) {
  console.log(`addresses: ${JSON.stringify(addresses)}`);
}

function onReverse(hostnames) {
  console.log(`hostnames: ${JSON.stringify(hostnames)}`);
}

function query() {
  if (process.env.NODE_TEST_DNS) {
    dns.setServers([process.env.NODE_TEST_DNS])
  }

  const host = process.env.NODE_TEST_HOST;
  if (process.env.NODE_TEST_PROMISE === 'true') {
    dns.promises.resolve4(host).then(onResolve, onError);
  } else {
    dns.resolve4(host, (err, addresses) => {
      if (err) {
        onError(err);
      } else {
        onResolve(addresses);
      }
    });
  }

  const ip = process.env.NODE_TEST_IP;
  if (ip) {
    if (process.env.NODE_TEST_PROMISE === 'true') {
      dns.promises.reverse(ip).then(onReverse, onError);
    } else {
      dns.reverse(ip, (err, hostnames) => {
        if (err) {
          onError(err);
        } else {
          onReverse(hostnames);
        }
      });
    }
  }
}

query();

setDeserializeMainFunction(query);
