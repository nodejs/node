// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that constructor names are truncated and CustomPreviewBudget charges className before recursion.');

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Runtime.enable();
    await Protocol.Runtime.setCustomObjectFormatterEnabled({enabled: true});
    Protocol.Runtime.onConsoleAPICalled(({params: {type, args}}) => {
      InspectorTest.log(
          `console.${type}: ${args.map(({value}) => value).join(' ')}`);
    });
  },

  async function testPlainObject() {
    for (const generatePreview of [false, true]) {
      const expr = `({[Symbol.toStringTag]: 'x'.repeat(5000)})`;
      const {result: {result}} = await Protocol.Runtime.evaluate({
        expression: expr,
        generatePreview,
      });
      InspectorTest.log(
          `plainObject (generatePreview=${generatePreview}) className length: ${
              result.className ? result.className.length : 0}`);
      InspectorTest.log(`plainObject (generatePreview=${
          generatePreview}) ends with ellipsis: ${
          result.className && result.className.endsWith('\u2026')}`);
      InspectorTest.log(`plainObject (generatePreview=${
          generatePreview}) description length: ${
          result.description ? result.description.length : 0}`);
    }
  },

  async function testCollection() {
    for (const generatePreview of [false, true]) {
      const expr = `(() => {
        class LongMap extends Map {}
        Object.defineProperty(LongMap, 'name', {value: 'y'.repeat(5000)});
        return new LongMap();
      })()`;
      const {result: {result}} = await Protocol.Runtime.evaluate({
        expression: expr,
        generatePreview,
      });
      InspectorTest.log(
          `collection (generatePreview=${generatePreview}) className length: ${
              result.className ? result.className.length : 0}`);
      InspectorTest.log(`collection (generatePreview=${
          generatePreview}) className ends with ellipsis: ${
          result.className && result.className.endsWith('\u2026')}`);
      InspectorTest.log(`collection (generatePreview=${
          generatePreview}) description length: ${
          result.description ? result.description.length : 0}`);
      InspectorTest.log(`collection (generatePreview=${
          generatePreview}) description ends with (0): ${
          result.description && result.description.endsWith('(0)')}`);
    }
  },

  async function testCustomError() {
    const expr = `(() => {
      const Cls = eval('(class ' + 'E'.repeat(5000) + ' extends Error {})');
      return new Cls('boom');
    })()`;
    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: expr,
      generatePreview: false,
    });
    InspectorTest.log(`customError className length: ${
        result.className ? result.className.length : 0}`);
    InspectorTest.log(`customError className ends with ellipsis: ${
        result.className && result.className.endsWith('\u2026')}`);
    const prefix = 'E'.repeat(1000) + '\u2026: boom';
    InspectorTest.log(`customError description starts with prefix: ${
        result.description && result.description.startsWith(prefix)}`);
    InspectorTest.log(`customError description contains stack: ${
        result.description && result.description.includes('\n    at ')}`);
  },

  async function testFunction() {
    const expr = `(() => {
      const name = 'F'.repeat(5000);
      const fn = Object.create(Function.prototype);
      Object.defineProperty(fn, Symbol.toStringTag, {value: name});
      return fn;
    })()`;
    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: expr,
      generatePreview: false,
    });
    InspectorTest.log(`function className length: ${
        result.className ? result.className.length : 0}`);
    InspectorTest.log(`function className ends with ellipsis: ${
        result.className && result.className.endsWith('\u2026')}`);
  },

  async function testClassInstance() {
    const expr = `(() => {
      const name = 'C'.repeat(5000);
      const Cls = {[name]: class {}}[name];
      return new Cls();
    })()`;
    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: expr,
      generatePreview: false,
    });
    InspectorTest.log(`classInstance className length: ${
        result.className ? result.className.length : 0}`);
    InspectorTest.log(`classInstance className ends with ellipsis: ${
        result.className && result.className.endsWith('\u2026')}`);
    InspectorTest.log(`classInstance description length: ${
        result.description ? result.description.length : 0}`);
  },

  async function testBudgetClassNameCharge() {
    const setupScript = `(() => {
      globalThis.formatObjA = {
        kind: 'targetA',
      };
      const child = {
        [Symbol.toStringTag]: 't'.repeat(5000),
      };

      // Budget is 10,000,000.
      // Consuming 9,998,500 chars leaves ~1500 chars in budget.
      // For child: className = 1001, description = 1001.
      // If className is NOT charged: wrapperChars = 1001 <= 1500 (budget NOT exhausted -> customPreview exists).
      // If className IS charged: wrapperChars = 2002 > 1500 (budget EXHAUSTED -> customPreview is null).
      const filler = 'a'.repeat(9998500);
      globalThis.devtoolsFormatters = [{
        header(obj) {
          if (obj.kind === 'targetA') {
            return ['div', filler, ['object', {object: child}]];
          }
          return null;
        },
        hasBody() { return false; },
      }];
    })()`;
    await Protocol.Runtime.evaluate({expression: setupScript});
    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: 'globalThis.formatObjA',
      generatePreview: true,
    });
    InspectorTest.log(`testBudgetClassNameCharge customPreview present: ${
        Boolean(result.customPreview)}`);
  },

  async function testBudgetPreRecursionCharge() {
    const setupScript = `(() => {
      globalThis.childHeaderCalls = 0;
      const childWithFormatter = {
        [Symbol.toStringTag]: 'c'.repeat(5000),
        isChild: true,
      };

      // Consuming 9,998,500 chars leaves ~1500 chars in budget.
      // For childWithFormatter: className + description = 2002 > 1500.
      // Before fix: childWithFormatter is recursed into FIRST (header() runs -> childHeaderCalls = 1),
      // then afterwards wrapperChars was charged.
      // After fix: child's RemoteObject charges budget BEFORE recursing, so childHeaderCalls = 0.
      const filler = 'b'.repeat(9998500);
      globalThis.parentObj = {
        isParent: true,
      };
      globalThis.devtoolsFormatters = [
        {
          header(obj, config) {
            if (obj.isParent) {
              return ['div', filler, ['object', {object: childWithFormatter, config: {}}]];
            }
            if (obj.isChild) {
              globalThis.childHeaderCalls++;
              return ['span', 'child'];
            }
            return null;
          },
          hasBody() { return false; },
        },
      ];
    })()`;
    await Protocol.Runtime.evaluate({expression: setupScript});
    await Protocol.Runtime.evaluate({
      expression: 'globalThis.parentObj',
      generatePreview: true,
    });
    const res = await Protocol.Runtime.evaluate({
      expression: 'globalThis.childHeaderCalls',
    });
    InspectorTest.log(`childHeaderCalls: ${res.result.result.value}`);
  },

  async function testSelfRecursiveFormatter() {
    const setupScript = `
      const selfObj = {
        [Symbol.toStringTag]: 'z'.repeat(10 * 1024 * 1024),
        isSelf: true,
      };
      globalThis.selfObj = selfObj;
      globalThis.devtoolsFormatters = [{
        header(obj) {
          if (obj.isSelf) {
            return ['div', ['object', {object: obj}]];
          }
          return null;
        },
        hasBody() { return false; },
      }];
    `;
    await Protocol.Runtime.evaluate({expression: setupScript});
    const {result: {result}} = await Protocol.Runtime.evaluate({
      expression: 'globalThis.selfObj',
      generatePreview: true,
    });
    InspectorTest.log(
        `selfObj formatted safely without OOM: ${result.type === 'object'}`);
  },
]);
