'use strict';
const {
  ArrayPrototypePush,
  ArrayPrototypePop,
  ArrayPrototypeShift,
  ArrayPrototypeSplice,
  Number,
  ObjectAssign,
  StringPrototypeReplace,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  StringPrototypeTrim,
  RegExp,
  RegExpPrototypeExec,
} = primordials;

const Transform = require('internal/streams/transform');
const { kDefaultIndent } = require('internal/test_runner/tap_stream');
const { kEmptyObject } = require('internal/util');

const kNestingIndentFactor = kDefaultIndent.length;
const kSubtestRegExp = /^(\s*)#\sSubtest:\s*(.*)$/;
const kTestRegexp = /^(\s*)(not\s)?ok\s(\d+)\s-\s([^#]+)\s*(?:#\s+(TODO|SKIP)(.*))?$/;
const kPlanRexExp = /^(\s*)1\.\.(\d+)$/;
const kDetailsStartRegExp = /^(\s*)\s{2}---$/;
const kDetailsEndRegExp = /^(\s*)\s{2}\.\.\.$/;
const kDiagnosticRegExp = /^(\s*)# (.*)$/;


// This is a simple TAP parser that is used to parse the output of the test runner
// It is not a full TAP parser and does not support all TAP features,
// The supported features are those used by TapStream
// unrecognized output is reported as 'unknown'
class TapParser extends Transform {
  #lines = [];
  #lastLine = '';
  #state = {
    harness: false,
    testStack: [],
  };

  constructor() {
    super({ __proto__: null, readableObjectMode: true });
  }

  get #currentTest() {
    return this.#state.testStack[this.#state.testStack.length - 1];
  }

  #popTest() {
    if (this.#state.testStack.length === 0) {
      return;
    }
    const { indent, testNumber, ok, name, yamlLines, directive } = ArrayPrototypePop(this.#state.testStack);
    const nesting = indent.length / kNestingIndentFactor;
    this.push({
      __proto__: null,
      type: ok ? 'ok' : 'not ok',
      nesting, testNumber, name, yamlLines, directive,
    });
  }

  #processLines() {
    while (this.#lines.length > 0) {
      const line = ArrayPrototypeShift(this.#lines);
      let regexpExecResult;

      // Once tap 13 header is detected - start parsing
      if (line === 'TAP version 13') {
        this.#state.harness = true;
        this.push({ __proto__: null, type: 'version', version: 13 });
        continue;
      }

      // If didn't see a valid tap 13 header, treat all lines as unknown
      if (!this.#state.harness) {
        this.push({ __proto__: null, type: 'unknown', line });
        continue;
      }

      // Inside a test, detect yaml section: ---
      if (this.#currentTest && (regexpExecResult = RegExpPrototypeExec(kDetailsStartRegExp, line)) !== null) {
        const { 1: indent } = regexpExecResult;
        this.#currentTest.inYaml = { __proto__: null, indent, trimmer: new RegExp(`^\\s{${indent.length}}\\s{2}?`) };
        continue;
      }

      // Inside a test, detect end of yaml section: ...
      if (this.#currentTest?.inYaml &&
        this.#currentTest?.inYaml.indent === RegExpPrototypeExec(kDetailsEndRegExp, line)?.[1]) {
        this.#currentTest.inYaml = false;
        this.#popTest();
        continue;
      }

      // Inside a yaml section, collect lines
      if (this.#currentTest?.inYaml) {
        ArrayPrototypePush(this.#currentTest.yamlLines,
                           StringPrototypeReplace(line, this.#currentTest.inYaml.trimmer, ''));
        continue;
      }

      // Detect subtest line: # Subtest: name
      if ((regexpExecResult = RegExpPrototypeExec(kSubtestRegExp, line)) !== null) {
        const { 1: indent, 2: name } = regexpExecResult;
        if (this.#currentTest && this.#currentTest.indent >= indent) {
          this.#popTest();
        }
        ArrayPrototypePush(this.#state.testStack, { __proto__: null, name, indent, yamlLines: [], subtestPending: true });
        const nesting = indent.length / kNestingIndentFactor;
        this.push({ __proto__: null, type: 'subtest', nesting, name });
        continue;
      }

      // Detect test line: (not?) ok 1 - name # TODO|SKIP
      if ((regexpExecResult = RegExpPrototypeExec(kTestRegexp, line)) !== null) {
        const { 1: indent, 2: not, 3: testNumber, 4: name, 5: directive, 6: comment } = regexpExecResult;

        if (this.#currentTest && this.#currentTest.indent.length >= indent.length &&
          !(this.#currentTest.subtestPending && this.#currentTest.indent === indent && this.#currentTest.name === StringPrototypeTrim(name))) {
          this.#popTest();
        }

        if (!this.#currentTest || this.#currentTest.indent.length < indent.length) {
          ArrayPrototypePush(this.#state.testStack, { __proto__: null, yamlLines: [] });
        }

        ObjectAssign(this.#currentTest, {
          __proto__: null,
          subtestPending: false,
          indent,
          ok: !not,
          testNumber,
          name: StringPrototypeTrim(name),
          directive: directive ?
            { __proto__: null, [StringPrototypeToLowerCase(directive)]: StringPrototypeTrim(comment) } : kEmptyObject,
        });
        continue;
      }

      // Detect plan line: 1..n
      if ((regexpExecResult = RegExpPrototypeExec(kPlanRexExp, line)) !== null) {
        const { 1: indent, 2: count } = regexpExecResult;
        if (Number(count) !== 1) {
          const nesting = indent.length / kNestingIndentFactor;
          this.push({ __proto__: null, type: 'plan', nesting, count });
        }
        continue;
      }

      // Detect diagnostic line: # message
      if ((regexpExecResult = RegExpPrototypeExec(kDiagnosticRegExp, line)) !== null) {
        const { 1: indent, 2: message } = regexpExecResult;
        const nesting = indent.length / kNestingIndentFactor;
        this.push({ __proto__: null, type: 'diagnostic', nesting, message });
        continue;
      }

      // Consider any unrecognized line as a unknown
      this.push({ __proto__: null, type: 'unknown', line });
    }
  }

  _transform(chunk, encoding, callback) {
    const str = this.#lastLine + chunk.toString('utf8');
    const lines = StringPrototypeSplit(str, '\n');
    this.#lastLine = ArrayPrototypeSplice(lines, lines.length - 1, 1)[0];
    ArrayPrototypePush(this.#lines, ...lines);
    this.#processLines();

    callback();
  }
  _flush(callback) {
    if (this.#lastLine) {
      ArrayPrototypePush(this.#lines, this.#lastLine);
      this.#lastLine = '';
    }
    this.#processLines();
    while (this.#state.testStack.length > 0) {
      this.#popTest();
    }
    callback();
  }
}

module.exports = { TapParser };
