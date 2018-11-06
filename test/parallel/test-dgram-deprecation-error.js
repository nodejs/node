'use strict';

const assert = require('assert');
const common = require('../common');
const dgram = require('dgram');
const fork = require('child_process').fork;

const sock = dgram.createSocket('udp4');

const testNumber = parseInt(process.argv[2], 10);

const propertiesToTest = [
  '_handle',
  '_receiving',
  '_bindState',
  '_queue',
  '_reuseAddr'
];

const methodsToTest = [
  '_healthCheck',
  '_stopReceiving'
];

const propertyCases = propertiesToTest.map((propName) => {
  return [
    () => {
      // Test property getter
      common.expectWarning(
        'DeprecationWarning',
        `Socket.prototype.${propName} is deprecated`,
        'DEP0112'
      );
      sock[propName];
    },
    () => {
      // Test property setter
      common.expectWarning(
        'DeprecationWarning',
        `Socket.prototype.${propName} is deprecated`,
        'DEP0112'
      );
      sock[propName] = null;
    }
  ];
});

const methodCases = methodsToTest.map((propName) => {
  return () => {
    common.expectWarning(
      'DeprecationWarning',
      `Socket.prototype.${propName}() is deprecated`,
      'DEP0112'
    );
    sock[propName]();
  };
});

const cases = [].concat(
  ...propertyCases,
  ...methodCases
);

// If we weren't passed a test ID then we need to spawn all of the cases.
// We run the cases in child processes since deprecations print once.
if (Number.isNaN(testNumber)) {
  const children = cases.map((_case, i) =>
    fork(process.argv[1], [ String(i) ]));

  children.forEach((child) => {
    child.on('close', (code) => {
      // Pass on child exit code
      if (code > 0) {
        process.exit(code);
      }
    });
  });

  return;
}

// We were passed a test ID - run the test case
assert.ok(cases[testNumber]);
cases[testNumber]();
