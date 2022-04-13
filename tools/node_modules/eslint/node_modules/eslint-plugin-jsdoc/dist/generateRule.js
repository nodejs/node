"use strict";

var _fs = require("fs");

var _promises = _interopRequireDefault(require("fs/promises"));

var _path = require("path");

var _camelcase = _interopRequireDefault(require("camelcase"));

var _openEditor = _interopRequireDefault(require("open-editor"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _getRequireWildcardCache(nodeInterop) { if (typeof WeakMap !== "function") return null; var cacheBabelInterop = new WeakMap(); var cacheNodeInterop = new WeakMap(); return (_getRequireWildcardCache = function (nodeInterop) { return nodeInterop ? cacheNodeInterop : cacheBabelInterop; })(nodeInterop); }

function _interopRequireWildcard(obj, nodeInterop) { if (!nodeInterop && obj && obj.__esModule) { return obj; } if (obj === null || typeof obj !== "object" && typeof obj !== "function") { return { default: obj }; } var cache = _getRequireWildcardCache(nodeInterop); if (cache && cache.has(obj)) { return cache.get(obj); } var newObj = {}; var hasPropertyDescriptor = Object.defineProperty && Object.getOwnPropertyDescriptor; for (var key in obj) { if (key !== "default" && Object.prototype.hasOwnProperty.call(obj, key)) { var desc = hasPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : null; if (desc && (desc.get || desc.set)) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } newObj.default = obj; if (cache) { cache.set(obj, newObj); } return newObj; }

// Todo: Would ideally have prompts, e.g., to ask for whether type was problem/layout, etc.
const [,, ruleName, ...options] = process.argv;
const recommended = options.includes('--recommended');

(async () => {
  if (!ruleName) {
    console.error('Please supply a rule name');
    return;
  }

  if (/[A-Z]/u.test(ruleName)) {
    console.error('Please ensure the rule has no capital letters');
    return;
  }

  const ruleNamesPath = './test/rules/ruleNames.json';
  const ruleNames = JSON.parse(await _promises.default.readFile(ruleNamesPath, 'utf8'));

  if (!ruleNames.includes(ruleName)) {
    ruleNames.push(ruleName);
    ruleNames.sort();
  }

  await _promises.default.writeFile(ruleNamesPath, JSON.stringify(ruleNames, null, 2) + '\n');
  console.log('ruleNames', ruleNames);
  const ruleTemplate = `import iterateJsdoc from '../iterateJsdoc';

export default iterateJsdoc(({
  context,
  utils,
}) => {
  // Rule here
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: '',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-${ruleName}',
    },
    schema: [
      {
        additionalProperies: false,
        properties: {
          // Option properties here (or remove the object)
        },
        type: 'object',
      },
    ],
    type: 'suggestion',
  },
});
`;
  const camelCasedRuleName = (0, _camelcase.default)(ruleName);
  const rulePath = `./src/rules/${camelCasedRuleName}.js`;

  if (!(0, _fs.existsSync)(rulePath)) {
    await _promises.default.writeFile(rulePath, ruleTemplate);
  }

  const ruleTestTemplate = `export default {
  invalid: [
    {
      code: \`
      \`,
      errors: [{
        line: '',
        message: '',
      }],
    },
  ],
  valid: [
    {
      code: \`
      \`,
    },
  ],
};
`;
  const ruleTestPath = `./test/rules/assertions/${camelCasedRuleName}.js`;

  if (!(0, _fs.existsSync)(ruleTestPath)) {
    await _promises.default.writeFile(ruleTestPath, ruleTestTemplate);
  }

  const ruleReadmeTemplate = `### \`${ruleName}\`

|||
|---|---|
|Context|everywhere|
|Tags|\`\`|
|Recommended|${recommended ? 'true' : 'false'}|
|Settings||
|Options||

<!-- assertions ${camelCasedRuleName} -->
`;
  const ruleReadmePath = `./.README/rules/${ruleName}.md`;

  if (!(0, _fs.existsSync)(ruleReadmePath)) {
    await _promises.default.writeFile(ruleReadmePath, ruleReadmeTemplate);
  }

  const replaceInOrder = async ({
    path,
    oldRegex,
    checkName,
    newLine,
    oldIsCamel
  }) => {
    const offsets = [];
    let readme = await _promises.default.readFile(path, 'utf8');
    readme.replace(oldRegex, (matchedLine, n1, offset, str, {
      oldRule
    }) => {
      offsets.push({
        matchedLine,
        offset,
        oldRule
      });
    });
    offsets.sort(({
      oldRule
    }, {
      oldRule: oldRuleB
    }) => {
      // eslint-disable-next-line no-extra-parens
      return oldRule < oldRuleB ? -1 : oldRule > oldRuleB ? 1 : 0;
    });
    let alreadyIncluded = false;
    const itemIndex = offsets.findIndex(({
      oldRule
    }) => {
      alreadyIncluded || (alreadyIncluded = oldIsCamel ? camelCasedRuleName === oldRule : ruleName === oldRule);
      return oldIsCamel ? camelCasedRuleName < oldRule : ruleName < oldRule;
    });
    let item = itemIndex !== undefined && offsets[itemIndex];

    if (item && itemIndex === 0 && // This property would not always be sufficient but in this case it is.
    oldIsCamel) {
      item.offset = 0;
    }

    if (!item) {
      item = offsets.pop();
      item.offset += item.matchedLine.length;
    }

    if (alreadyIncluded) {
      console.log(`Rule name is already present in ${checkName}.`);
    } else {
      readme = readme.slice(0, item.offset) + (item.offset ? '\n' : '') + newLine + (item.offset ? '' : '\n') + readme.slice(item.offset);
      await _promises.default.writeFile(path, readme);
    }
  };

  await replaceInOrder({
    checkName: 'README',
    newLine: `{"gitdown": "include", "file": "./rules/${ruleName}.md"}`,
    oldRegex: /\n\{"gitdown": "include", "file": ".\/rules\/(?<oldRule>[^.]*).md"\}/gu,
    path: './.README/README.md'
  });
  await replaceInOrder({
    checkName: 'index import',
    newLine: `import ${camelCasedRuleName} from './rules/${camelCasedRuleName}';`,
    oldIsCamel: true,
    oldRegex: /\nimport (?<oldRule>[^ ]*) from '.\/rules\/\1';/gu,
    path: './src/index.js'
  });
  await replaceInOrder({
    checkName: 'index recommended',
    newLine: `${' '.repeat(8)}'jsdoc/${ruleName}': '${recommended ? 'warn' : 'off'}',`,
    oldRegex: /\n\s{8}'jsdoc\/(?<oldRule>[^']*)': '[^']*',/gu,
    path: './src/index.js'
  });
  await replaceInOrder({
    checkName: 'index rules',
    newLine: `${' '.repeat(4)}'${ruleName}': ${camelCasedRuleName},`,
    oldRegex: /\n\s{4}'(?<oldRule>[^']*)': [^,]*,/gu,
    path: './src/index.js'
  });
  await Promise.resolve().then(() => _interopRequireWildcard(require('./generateReadme.js')));
  /*
  console.log('Paths to open for further editing\n');
  console.log(`open ${ruleReadmePath}`);
  console.log(`open ${rulePath}`);
  console.log(`open ${ruleTestPath}\n`);
  */
  // Set chdir as somehow still in operation from other test

  process.chdir((0, _path.resolve)(__dirname, '../../'));
  await (0, _openEditor.default)([// Could even add editor line column numbers like `${rulePath}:1:1`
  ruleReadmePath, ruleTestPath, rulePath]);
})();
//# sourceMappingURL=generateRule.js.map