// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('../common/tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();
hooks.enable();

const { HTTPParser } = require('_http_common');

const REQUEST = HTTPParser.REQUEST;

const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;

const request = Buffer.from(
  'GET /hello HTTP/1.1\r\n\r\n'
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
