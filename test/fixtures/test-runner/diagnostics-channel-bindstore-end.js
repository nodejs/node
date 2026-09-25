'use strict';
const dc = require('node:diagnostics_channel');
const { AsyncLocalStorage } = require('node:async_hooks');
const { test } = require('node:test');

const als = new AsyncLocalStorage();
const ch = dc.tracingChannel('node.test');

ch.start.bindStore(als, (data) => data.name);

const storeAtEnd = {};
const storeAtError = {};

ch.end.subscribe((data) => {
  storeAtEnd[data.name] = als.getStore();
});

ch.error.subscribe((data) => {
  storeAtError[data.name] = als.getStore();
});

test('passing test', () => {});
test('failing test', () => { throw new Error('boom'); });

process.on('exit', () => {
  console.log(JSON.stringify({ storeAtEnd, storeAtError }));
});
