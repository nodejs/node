'use strict';

const {
  ArrayPrototypePush,
  JSONStringify,
} = primordials;
const { relative } = require('path');
const { writeFileSync } = require('fs');

function reportReruns(previousRuns, globalOptions) {
  return async function reporter(source) {
    const obj = { __proto__: null };
    const disambiguator = { __proto__: null };

    for await (const { type, data } of source) {
      if (type === 'test:pass') {
        let identifier = `${relative(globalOptions.cwd, data.file)}:${data.line}:${data.column}`;
        if (disambiguator[identifier] !== undefined) {
          identifier += `:(${disambiguator[identifier]})`;
          disambiguator[identifier] += 1;
        } else {
          disambiguator[identifier] = 1;
        }
        obj[identifier] = {
          __proto__: null,
          name: data.name,
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
