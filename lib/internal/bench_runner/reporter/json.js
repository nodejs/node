'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  JSONStringify,
  ObjectKeys,
  SafeWeakSet,
  String,
} = primordials;
const { isError } = require('internal/util');

function toJSONValue(value, seen) {
  if (typeof value === 'bigint') return String(value);
  if (value === null || typeof value !== 'object') return value;
  if (seen.has(value)) return '[Circular]';
  seen.add(value);

  if (isError(value)) {
    const result = {
      __proto__: null,
      name: value.name,
      message: value.message,
      stack: value.stack,
      code: value.code,
    };
    if (value.cause !== undefined) {
      result.cause = toJSONValue(value.cause, seen);
    }
    if (value.errors !== undefined) {
      result.errors = toJSONValue(value.errors, seen);
    }
    return result;
  }

  if (ArrayIsArray(value)) {
    const result = [];
    for (let i = 0; i < value.length; i++) {
      ArrayPrototypePush(result, toJSONValue(value[i], seen));
    }
    return result;
  }

  const result = { __proto__: null };
  const keys = ObjectKeys(value);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    result[key] = toJSONValue(value[key], seen);
  }
  return result;
}

function stringify(record) {
  return JSONStringify(record === undefined ? null :
    toJSONValue(record, new SafeWeakSet()));
}

async function* jsonReporter(source) {
  for await (const record of source) {
    yield `${stringify(record)}\n`;
  }
}

module.exports = jsonReporter;
