'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('../common/tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();

hooks.enable();

const { HTTPParser } = require('_http_common');

const RESPONSE = HTTPParser.RESPONSE;
const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
const kOnBody = HTTPParser.kOnBody | 0;

const request = Buffer.from(
  'HTTP/1.1 200 OK\r\n' +
  'Content-Type: text/plain\r\n' +
  'Content-Length: 4\r\n' +
  '\r\n' +
  'pong',
);

const parser = new HTTPParser();
parser.initialize(RESPONSE, {});
const as = hooks.activitiesOfTypes('HTTPCLIENTREQUEST');
const httpparser = as[0];

assert.strictEqual(as.length, 1);
assert.strictEqual(typeof httpparser.uid, 'number');
assert.strictEqual(typeof httpparser.triggerAsyncId, 'number');
checkInvocations(httpparser, { init: 1 }, 'when created new Httphttpparser');

parser[kOnHeadersComplete] = common.mustCall(onheadersComplete);
parser[kOnBody] = common.mustCall(onbody);
parser.execute(request, 0, request.length);

function onheadersComplete() {
  checkInvocations(httpparser, { init: 1, before: 1 },
                   'when onheadersComplete called');
}

function onbody() {
  checkInvocations(httpparser, { init: 1, before: 2, after: 1 },
                   'when onbody called');
  tick(1, common.mustCall(tick1));
}

function tick1() {
  parser.close();
  tick(1);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('HTTPCLIENTREQUEST');
  checkInvocations(httpparser, { init: 1, before: 2, after: 2, destroy: 1 },
                   'when process exits');
}
