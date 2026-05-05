'use strict';

const {
  ArrayIsArray,
  ObjectFreeze,
  ObjectKeys,
  SafeWeakSet,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_VALIDATOR_INVALID_SCHEMA,
  },
} = require('internal/errors');

const { inspect } = require('internal/util/inspect');

const { compileSchema } = require('internal/validator/compile');
const { validateValue } = require('internal/validator/validate');
const { applyDefaults } = require('internal/validator/defaults');

const kCompiledSchema = Symbol('kCompiledSchema');
const kDefinition = Symbol('kDefinition');

function deepFreezeDefinition(value, seen) {
  if (value === null || typeof value !== 'object') return value;
  if (seen.has(value)) {
    throw new ERR_VALIDATOR_INVALID_SCHEMA(
      '', 'schema definition contains a circular reference');
  }
  seen.add(value);

  let out;
  if (ArrayIsArray(value)) {
    out = [];
    for (let i = 0; i < value.length; i++) {
      out[i] = deepFreezeDefinition(value[i], seen);
    }
  } else {
    out = {};
    const keys = ObjectKeys(value);
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      // `default` is typed {any}; keep the user's reference verbatim so
      // Dates, Maps, class instances, etc. survive toJSON() round-trips.
      out[key] = key === 'default' ?
        value[key] : deepFreezeDefinition(value[key], seen);
    }
  }

  seen.delete(value);
  return ObjectFreeze(out);
}

class Schema {
  constructor(definition) {
    if (typeof definition !== 'object' || definition === null ||
        ArrayIsArray(definition)) {
      throw new ERR_INVALID_ARG_TYPE('definition', 'a plain object', definition);
    }

    // Snapshot first: this deep-freezes the user-supplied definition and
    // rejects circular references with ERR_VALIDATOR_INVALID_SCHEMA instead
    // of letting `compileSchema` recurse until it blows the stack.
    this[kDefinition] = deepFreezeDefinition(definition, new SafeWeakSet());
    this[kCompiledSchema] = compileSchema(definition);
  }

  validate(data) {
    const errors = [];
    validateValue(data, this[kCompiledSchema], '', errors);
    return ObjectFreeze({
      __proto__: null,
      valid: errors.length === 0,
      errors: ObjectFreeze(errors),
    });
  }

  applyDefaults(data) {
    return applyDefaults(data, this[kCompiledSchema]);
  }

  toJSON() {
    return this[kDefinition];
  }

  get [SymbolToStringTag]() {
    return 'Schema';
  }

  [inspect.custom](depth, options) {
    if (depth < 0) return 'Schema [Object]';
    return `Schema ${inspect(this[kDefinition], {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    })}`;
  }

  static validate(definition, data) {
    const schema = new Schema(definition);
    return schema.validate(data);
  }
}

module.exports = { Schema };
