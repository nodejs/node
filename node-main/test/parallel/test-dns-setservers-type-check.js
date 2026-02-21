'use strict';
const common = require('../common');
const { addresses } = require('../common/internet');
const assert = require('assert');
const dns = require('dns');
const resolver = new dns.promises.Resolver();
const dnsPromises = dns.promises;
const promiseResolver = new dns.promises.Resolver();

{
  [
    null,
    undefined,
    Number(addresses.DNS4_SERVER),
    addresses.DNS4_SERVER,
    {
      address: addresses.DNS4_SERVER
    },
  ].forEach((val) => {
    const errObj = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "servers" argument must be an instance of Array.' +
               common.invalidArgTypeHelper(val)
    };
    assert.throws(
      () => {
        dns.setServers(val);
      }, errObj
    );
    assert.throws(
      () => {
        resolver.setServers(val);
      }, errObj
    );
    assert.throws(
      () => {
        dnsPromises.setServers(val);
      }, errObj
    );
    assert.throws(
      () => {
        promiseResolver.setServers(val);
      }, errObj
    );
  });
}

{
  [
    [null],
    [undefined],
    [Number(addresses.DNS4_SERVER)],
    [
      {
        address: addresses.DNS4_SERVER
      },
    ],
  ].forEach((val) => {
    const errObj = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "servers[0]" argument must be of type string.' +
               common.invalidArgTypeHelper(val[0])
    };
    assert.throws(
      () => {
        dns.setServers(val);
      }, errObj
    );
    assert.throws(
      () => {
        resolver.setServers(val);
      }, errObj
    );
    assert.throws(
      () => {
        dnsPromises.setServers(val);
      }, errObj
    );
    assert.throws(
      () => {
        promiseResolver.setServers(val);
      }, errObj
    );
  });
}

// This test for 'dns/promises'
{
  const {
    setServers
  } = require('dns/promises');

  // This should not throw any error.
  (async () => {
    setServers([ '127.0.0.1' ]);
  })().then(common.mustCall());

  [
    [null],
    [undefined],
    [Number(addresses.DNS4_SERVER)],
    [
      {
        address: addresses.DNS4_SERVER
      },
    ],
  ].forEach((val) => {
    const errObj = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "servers[0]" argument must be of type string.' +
              common.invalidArgTypeHelper(val[0])
    };
    assert.throws(() => {
      setServers(val);
    }, errObj);
  });
}
