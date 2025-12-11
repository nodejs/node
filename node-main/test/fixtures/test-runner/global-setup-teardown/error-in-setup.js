'use strict';

async function globalSetup() {
  throw new Error('Deliberate error in global setup');
}

async function globalTeardown() {
  console.log('Teardown should not run if setup fails');
}

module.exports = { globalSetup, globalTeardown };
