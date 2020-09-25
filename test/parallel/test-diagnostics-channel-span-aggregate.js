'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const inputs = [
  { test: 'start' },
  { test: 'annotation' },
  { test: 'end' }
];

const channel = dc.channel('test');

const receivedSpans = [];

const map = common.mustCall((message) => {
  message.ts = process.hrtime.bigint();
  return message;
}, 7);

const onComplete = common.mustCall((span) => {
  receivedSpans.push(span);
}, 3);

dc.Span.aggregate(channel, map, onComplete);

let expectedMessage = inputs.shift();
const spanA = channel.span(expectedMessage);
const spanB = channel.span(expectedMessage);
const spanC = channel.span(expectedMessage);

expectedMessage = inputs.shift();
spanA.annotate(expectedMessage);

expectedMessage = inputs.shift();
spanB.end(expectedMessage);
spanC.end(expectedMessage);
spanA.end(expectedMessage);

const [ receivedB, receivedC, receivedA ] = receivedSpans;

// Spans received in order of completion
for (const span of receivedB) {
  assert.strictEqual(span.id, 2);
}
for (const span of receivedC) {
  assert.strictEqual(span.id, 3);
}
for (const span of receivedA) {
  assert.strictEqual(span.id, 1);
}

// A starts before B which starts before C
assert.ok(receivedA[0].ts < receivedB[0].ts);
assert.ok(receivedB[0].ts < receivedC[0].ts);

// B ends before C which ends before A
assert.ok(receivedA[2].ts > receivedC[1].ts);
assert.ok(receivedC[1].ts > receivedB[1].ts);

// Middle event of last span is an annotation
const [, annotation] = receivedA;
assert.strictEqual(annotation.action, 'annotation');

for (const spans of receivedSpans) {
  const [ start, ...rest ] = spans;
  const end = rest.pop();

  // First and last messages are start and end
  assert.strictEqual(start.action, 'start');
  assert.strictEqual(end.action, 'end');

  // Annotation time occurs between all starts and ends
  assert.ok(annotation.ts > start.ts);
  assert.ok(annotation.ts < end.ts);
}
