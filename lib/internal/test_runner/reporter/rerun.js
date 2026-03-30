'use strict';

const {
  ArrayPrototypeMap,
  ArrayPrototypePush,
  JSONStringify,
} = primordials;
const { relative } = require('path');
const { writeFileSync } = require('fs');

function reportReruns(previousRuns, globalOptions) {
  return async function reporter(source) {
    const obj = { __proto__: null };
    const disambiguator = { __proto__: null };
    let currentSuite = null;
    const roots = [];

    function getTestId(data) {
      return `${relative(globalOptions.cwd, data.file)}:${data.line}:${data.column}`;
    }

    function startTest(data) {
      const originalSuite = currentSuite;
      currentSuite = { __proto__: null, data, parent: currentSuite, children: [] };
      if (originalSuite?.children) {
        ArrayPrototypePush(originalSuite.children, currentSuite);
      }
      if (!currentSuite.parent) {
        ArrayPrototypePush(roots, currentSuite);
      }
    }

    for await (const { type, data } of source) {
      let currentTest;
      if (type === 'test:start') {
        startTest(data);
      } else if (type === 'test:fail' || type === 'test:pass') {
        if (!currentSuite) {
          startTest({ __proto__: null, name: 'root', nesting: 0 });
        }
        if (currentSuite.data.name !== data.name || currentSuite.data.nesting !== data.nesting) {
          startTest(data);
        }
        currentTest = currentSuite;
        if (currentSuite?.data.nesting === data.nesting) {
          currentSuite = currentSuite.parent;
        }
      }


      if (type === 'test:pass') {
        let identifier = getTestId(data);
        if (disambiguator[identifier] !== undefined) {
          identifier += `:(${disambiguator[identifier]})`;
          disambiguator[identifier] += 1;
        } else {
          disambiguator[identifier] = 1;
        }
        const children = ArrayPrototypeMap(currentTest.children, (child) => child.data);
        obj[identifier] = {
          __proto__: null,
          name: data.name,
          children,
          passed_on_attempt: data.details.passed_on_attempt ?? data.details.attempt,
        };
      }
    }

    ArrayPrototypePush(previousRuns, obj);
    writeFileSync(globalOptions.rerunFailuresFilePath, JSONStringify(previousRuns, null, 2), 'utf8');
  };
};

module.exports = {
  __proto__: null,
  reportReruns,
};
