// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Debugger.stepOver with enterRanges.');

const source = [
  'function outlinedPart() {',            // Entered when in enterRanges.
  '  let x = 1;',
  '  return notEntered();',
  '}',
  'function notEntered() {',              // Never in enterRanges.
  '  return 2;',
  '}',
  'function callCallback(cb) {',
  '  return cb();',
  '}',
  'function main() {',
  '  debugger;',
  '  outlinedPart();',
  '  notEntered();',
  '  callCallback(outlinedPart);',
  '  [1, 2].forEach(outlinedPart);',
  '  outlinedPartInOtherScript();',
  '  return 0;',
  '}',
  'function recursive(n) {',
  '  if (n == 0) return 0;',
  '  debugger;',
  '  callRecursive(n);',
  '  return n;',
  '}',
  'function callRecursive(n) {',
  '  return recursive(n - 1);',
  '}',
];

const otherSource = [
  'function outlinedPartInOtherScript() {',
  '  return 3;',
  '}',
];

contextGroup.addScript(source.join('\n'));
contextGroup.addScript(otherSource.join('\n'));

const linesByScriptId = new Map();
const rangeNames = new Map();

// Returns the range [start of `function name(`, start of the line after its
// closing brace).
function functionRange(scriptId, name) {
  const lines = linesByScriptId.get(scriptId);
  const start = lines.findIndex(l => l.startsWith(`function ${name}(`));
  const end = lines.findIndex((l, i) => i > start && l.startsWith('}')) + 1;
  return namedRange(name, scriptId, start, end);
}

// Returns the range covering the line containing {text}.
function lineRange(scriptId, text) {
  const line = linesByScriptId.get(scriptId).findIndex(l => l.includes(text));
  return namedRange(`'${text}'`, scriptId, line, line + 1);
}

function namedRange(name, scriptId, startLine, endLine) {
  const range = {
    scriptId,
    start: {lineNumber: startLine, columnNumber: 0},
    end: {lineNumber: endLine, columnNumber: 0},
  };
  rangeNames.set(range, name);
  return range;
}

function describeStepArgs(args) {
  const parts = Object.entries(args).map(
      ([key, ranges]) =>
          `${key}: [${ranges.map(r => rangeNames.get(r)).join(', ')}]`);
  return parts.length ? parts.join(', ') : 'no arguments';
}

// Logs the pause as "function: source line", with # marking the position.
function logPause({params: {callFrames}}) {
  const {functionName, location: {scriptId, lineNumber, columnNumber}} =
      callFrames[0];
  const lines = linesByScriptId.get(scriptId);
  if (!lines) {
    InspectorTest.log('  (top-level)');
    return functionName;
  }
  const line = lines[lineNumber];
  const marked = line.slice(0, columnNumber) + '#' + line.slice(columnNumber);
  InspectorTest.log(`  ${functionName}: ${marked.trim()}`);
  return functionName;
}

async function finishEvaluation(evaluation) {
  await Protocol.Debugger.setSkipAllPauses({skip: true});
  await Protocol.Debugger.resume();
  await evaluation;
  await Protocol.Debugger.setSkipAllPauses({skip: false});
}

// Evaluates {expression} until the first debugger statement, then steps over
// with {args} until the top-level frame is reached.
async function stepThrough(expression, args) {
  InspectorTest.log(`Step over through ${expression} with ${
      describeStepArgs(args)}:`);
  const evaluation = Protocol.Runtime.evaluate({expression});
  let functionName = logPause(await Protocol.Debugger.oncePaused());
  while (functionName) {
    const response = await Protocol.Debugger.stepOver(args);
    if (response.error) {
      InspectorTest.log(`  Error: ${response.error.message}`);
      break;
    }
    functionName = logPause(await Protocol.Debugger.oncePaused());
  }
  await finishEvaluation(evaluation);
}

(async function() {
  Protocol.Debugger.enable();
  const [{params: {scriptId}}, {params: {scriptId: otherScriptId}}] =
      await Protocol.Debugger.onceScriptParsed(2);
  linesByScriptId.set(scriptId, source);
  linesByScriptId.set(otherScriptId, otherSource);
  const outlinedPart = functionRange(scriptId, 'outlinedPart');
  const outlinedPartInOtherScript =
      functionRange(otherScriptId, 'outlinedPartInOtherScript');

  InspectorTest.log('\nTest: without enterRanges, no function is entered');
  await stepThrough('main()', {});

  InspectorTest.log(
      '\nTest: functions in enterRanges are entered, also from another ' +
      'script and when called by a non-entered function or a builtin');
  InspectorTest.log(
      'Note: when an entered function returns into a non-entered function ' +
      '(callCallback), the step pauses there, like a regular step out.');
  await stepThrough(
      'main()', {enterRanges: [outlinedPart, outlinedPartInOtherScript]});

  InspectorTest.log(
      '\nTest: functions in enterRanges are entered through a blackboxed ' +
      'function, which itself is not paused in');
  const callCallback = functionRange(scriptId, 'callCallback');
  await Protocol.Debugger.setBlackboxedRanges({
    scriptId,
    positions: [callCallback.start, callCallback.end],
  });
  await stepThrough('main()', {enterRanges: [outlinedPart]});
  await Protocol.Debugger.setBlackboxedRanges({scriptId, positions: []});

  InspectorTest.log(
      '\nTest: skipList also applies inside entered functions');
  await stepThrough('main()', {
    enterRanges: [outlinedPart],
    skipList: [lineRange(scriptId, 'let x = 1;')],
  });

  InspectorTest.log(
      '\nTest: recursive calls of the stepped over function are not entered');
  await stepThrough('recursive(1)', {enterRanges: [outlinedPart]});

  InspectorTest.log(
      '\nTest: enterRanges only apply to the step they are passed to');
  const evaluation = Protocol.Runtime.evaluate({expression: 'main()'});
  logPause(await Protocol.Debugger.oncePaused());
  InspectorTest.log(`Step over with ${
      describeStepArgs({enterRanges: [outlinedPart]})}:`);
  Protocol.Debugger.stepOver({enterRanges: [outlinedPart]});
  logPause(await Protocol.Debugger.oncePaused());
  InspectorTest.log('Step over with no arguments steps over outlinedPart:');
  Protocol.Debugger.stepOver({});
  logPause(await Protocol.Debugger.oncePaused());
  await finishEvaluation(evaluation);

  InspectorTest.log('\nTest: enterRanges with an unknown script are rejected');
  await stepThrough(
      'main()', {enterRanges: [namedRange('unknown script', '-1', 0, 1)]});

  InspectorTest.completeTest();
})();
