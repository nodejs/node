import { createRequire } from 'node:module';

export { default as globals } from 'globals';

export const importEslintTool = (specifier) => import(specifier);

const localRequire = createRequire(new URL(import.meta.url));
export const resolveEslintTool = (request) => localRequire.resolve(request);

export const noRestrictedSyntaxCommonAll = [
  {
    selector: "CallExpression[callee.name='setInterval'][arguments.length<2]",
    message: '`setInterval()` must be invoked with at least two arguments.',
  },
  {
    selector: 'ThrowStatement > CallExpression[callee.name=/Error$/]',
    message: 'Use `new` keyword when throwing an `Error`.',
  },
  {
    selector: "CallExpression[callee.property.name='substr']",
    message: 'Use String.prototype.slice() or String.prototype.substring() instead of String.prototype.substr()',
  },
  {
    selector: `CallExpression:matches(${[
      '[callee.name="assert"]',
      '[callee.object.name="assert"][callee.property.name="ok"]',
    ]})[arguments.0.type="Literal"]`,
    message: 'Do not use a literal for the first argument of assert(), use assert.fail() instead or remove the call',
  },
];

export const noRestrictedSyntaxCommonLib = [
  {
    selector: "CallExpression[callee.name='setTimeout'][arguments.length<2]",
    message: '`setTimeout()` must be invoked with at least two arguments.',
  },
];
