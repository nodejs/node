'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const binding = process.binding('http_parser');
const HTTPParser = binding.HTTPParser;

const CRLF = '\r\n';
const REQUEST = HTTPParser.REQUEST;

const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;

const hooks = initHooks();

hooks.enable();

const request = Buffer.from(
  'GET /hello HTTP/1.1' + CRLF + CRLF
);

const parser = new HTTPParser(REQUEST);
const as = hooks.activitiesOfTypes('HTTPPARSER');
const httpparser = as[0];

assert.strictEqual(as.length, 1);
assert.strictEqual(typeof httpparser.uid, 'number');
assert.strictEqual(typeof httpparser.triggerAsyncId, 'number');
checkInvocations(httpparser, { init: 1 }, 'when created new Httphttpparser');

parser[kOnHeadersComplete] = common.mustCall(onheadersComplete);
parser.execute(request, 0, request.length);

function onheadersComplete() {
  checkInvocations(httpparser, { init: 1, before: 1 },
                   'when onheadersComplete called');
  tick(1, common.mustCall(tick1));
}

function tick1() {
  parser.close();
  tick(1);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('HTTPPARSER');
  checkInvocations(httpparser, { init: 1, before: 1, after: 1, destroy: 1 },
                   'when process exits');
}
