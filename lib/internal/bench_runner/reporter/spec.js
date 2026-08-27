'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  JSONStringify,
  MathAbs,
  NumberIsFinite,
  NumberPrototypeToFixed,
  NumberPrototypeToPrecision,
  ObjectEntries,
  String,
  StringPrototypeReplaceAll,
} = primordials;
const Transform = require('internal/streams/transform');

const kHeader =
  'benchmark | samples | mean rate | 95% CI | median rate | warning\n';

function escapeCell(value) {
  let result = String(value);
  result = StringPrototypeReplaceAll(result, '\r', '\\r');
  result = StringPrototypeReplaceAll(result, '\n', '\\n');
  return StringPrototypeReplaceAll(result, '|', '\\|');
}

function formatName(data) {
  const name = escapeCell(data.name);
  const entries = ObjectEntries(data.params);
  if (entries.length === 0) return name;

  const params = [];
  for (let i = 0; i < entries.length; i++) {
    const { 0: key, 1: value } = entries[i];
    ArrayPrototypePush(
      params, `${escapeCell(key)}=${escapeCell(JSONStringify(value))}`);
  }
  return `${name} [${ArrayPrototypeJoin(params, ', ')}]`;
}

function formatRate(rate) {
  if (!NumberIsFinite(rate)) return '-';

  const absolute = MathAbs(rate);
  let divisor = 1;
  let prefix = '';
  if (absolute >= 1e9) {
    divisor = 1e9;
    prefix = 'G';
  } else if (absolute >= 1e6) {
    divisor = 1e6;
    prefix = 'M';
  } else if (absolute >= 1e3) {
    divisor = 1e3;
    prefix = 'k';
  }

  const scaled = rate / divisor;
  const formatted = MathAbs(scaled) > 0 && MathAbs(scaled) < 1 ?
    NumberPrototypeToPrecision(scaled, 3) :
    NumberPrototypeToFixed(scaled, 2);
  return `${formatted}${prefix} ops/s`;
}

function formatWarning(summary) {
  const warnings = [];
  if (NumberIsFinite(summary.coefficientOfVariation) &&
      summary.coefficientOfVariation > 0.05) {
    ArrayPrototypePush(warnings, 'noisy');
  }
  if (NumberIsFinite(summary.skewness) && MathAbs(summary.skewness) > 1) {
    ArrayPrototypePush(warnings, 'skewed');
  }
  return ArrayPrototypeJoin(warnings, ', ');
}

function formatResult(data) {
  const name = formatName(data);
  if (data.skip !== undefined) {
    const reason = typeof data.skip === 'string' && data.skip.length > 0 ?
      `: ${escapeCell(data.skip)}` : '';
    return `${name} | 0 | - | - | - | skipped${reason}\n`;
  }
  if (data.error !== undefined) {
    const message = data.error?.message ?? data.error;
    return `${name} | ${data.samples.length} | - | - | - | ` +
      `error: ${escapeCell(message)}\n`;
  }

  const { confidenceInterval, median } = data.summary;
  return `${name} | ${data.samples.length} | ` +
    `${formatRate(data.summary.mean)} | ` +
    `[${formatRate(confidenceInterval.lower)}, ` +
    `${formatRate(confidenceInterval.upper)}] | ` +
    `${formatRate(median)} | ${formatWarning(data.summary)}\n`;
}

class SpecReporter extends Transform {
  #diagnostics = [];
  #reported = false;
  #results = [];

  constructor() {
    super({ __proto__: null, writableObjectMode: true });
  }

  #format(summary = undefined) {
    let output = kHeader;
    for (let i = 0; i < this.#results.length; i++) {
      output += formatResult(this.#results[i]);
    }
    for (let i = 0; i < this.#diagnostics.length; i++) {
      output += `diagnostic: ${escapeCell(this.#diagnostics[i].message)}\n`;
    }
    if (summary !== undefined) {
      const { completed, failed, skipped } = summary.counts;
      output += `\n${completed} completed, ${failed} failed, ` +
        `${skipped} skipped\n`;
    }
    return output;
  }

  _transform({ type, data }, _encoding, callback) {
    switch (type) {
      case 'bench:complete':
        ArrayPrototypePush(this.#results, data);
        break;
      case 'bench:diagnostic':
        ArrayPrototypePush(this.#diagnostics, data);
        break;
      case 'bench:summary':
        this.#reported = true;
        callback(null, this.#format(data));
        return;
    }
    callback();
  }

  _flush(callback) {
    if (this.#reported ||
        (this.#results.length === 0 && this.#diagnostics.length === 0)) {
      callback();
      return;
    }
    callback(null, this.#format());
  }
}

module.exports = SpecReporter;
