'use strict';

const { spawn } = require('node:child_process');
const path = require('node:path');
const { inspect } = require('node:util');

function parseInteger(value, defaultValue, name, minimum) {
  if (value === undefined) return defaultValue;
  if (!/^(?:0|[1-9]\d*)$/.test(value)) {
    throw new TypeError(`${name} must be an integer`);
  }
  const number = Number(value);
  if (!Number.isSafeInteger(number) || number < minimum) {
    throw new RangeError(`${name} must be at least ${minimum}`);
  }
  return number;
}

function parseNumber(value, defaultValue, name, minimum) {
  if (value === undefined) return defaultValue;
  if (value.trim() === '') throw new TypeError(`${name} must be a number`);
  const number = Number(value);
  if (!Number.isFinite(number)) throw new TypeError(`${name} must be a number`);
  if (number < minimum) {
    throw new RangeError(`${name} must be at least ${minimum}`);
  }
  return number;
}

function csvEncode(value) {
  if (typeof value === 'number' || typeof value === 'boolean') {
    return String(value);
  }
  const string = String(value);
  return `"${string.replace(/"/g, '""')}"`;
}

function formatConfiguration(params) {
  return Object.keys(params)
    .map((key) => `${key}=${inspect(params[key])}`)
    .join(' ');
}

function durationToSeconds(duration) {
  if (!/^\d+$/.test(duration)) {
    throw new TypeError(`Invalid benchmark duration '${duration}'`);
  }
  const padded = duration.padStart(10, '0');
  return `${padded.slice(0, -9)}.${padded.slice(-9)}`;
}

function runBenchmark(binary, file, options) {
  const args = [
    ...options.nodeArgs,
    '--no-warnings',
    '--bench',
    '--bench-reporter=json',
    '--bench-samples=1',
    `--bench-warmup=${options.warmup}`,
  ];
  if (options.namePattern !== undefined) {
    args.push(`--bench-name-pattern=${options.namePattern}`);
  }
  args.push('--', path.resolve(file));

  return new Promise((resolve, reject) => {
    const child = spawn(binary, args, {
      env: process.env,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');

    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (data) => { stdout += data; });
    child.stderr.on('data', (data) => { stderr += data; });
    child.once('error', reject);
    child.once('close', (code, signal) => {
      let records;
      try {
        records = stdout.trim().split('\n')
          .filter((line) => line.length > 0)
          .map((line) => JSON.parse(line));
      } catch (error) {
        reject(new Error(
          `Could not parse benchmark output from '${binary}': ${error.message}`,
          { cause: error },
        ));
        return;
      }

      const summary = records.findLast(
        ({ type }) => type === 'bench:summary')?.data;
      if (code !== 0 || signal !== null || summary?.success !== true) {
        const diagnostics = records
          .filter(({ type }) => type === 'bench:diagnostic')
          .map(({ data }) => data.message)
          .join('\n');
        const status = signal === null ? `exit code ${code}` : `signal ${signal}`;
        const details = stderr || diagnostics;
        reject(new Error(
          `Benchmark '${file}' failed with ${status}` +
          (details ? `:\n${details}` : ''),
        ));
        return;
      }

      const samples = [];
      for (const record of records) {
        if (record.type !== 'bench:complete' ||
            record.data.skip !== undefined) {
          continue;
        }
        if (record.data.error !== undefined) {
          reject(new Error(
            `Benchmark '${record.data.name}' failed: ` +
            record.data.error.message,
          ));
          return;
        }
        if (record.data.samples.length !== 1) {
          reject(new Error(
            `Benchmark '${record.data.name}' did not produce exactly one sample`,
          ));
          return;
        }
        const sample = record.data.samples[0];
        if (!Number.isFinite(sample.rate)) {
          reject(new Error(
            `Benchmark '${record.data.name}' produced a non-finite rate`,
          ));
          return;
        }
        samples.push({
          configuration: formatConfiguration(record.data.params),
          duration: durationToSeconds(sample.duration_ns),
          identity: record.data.benchId,
          logicalIdentity: JSON.stringify([
            record.data.file,
            record.data.parentId,
            record.data.name,
          ]),
          name: record.data.name,
          params: record.data.params,
          rate: sample.rate,
        });
      }
      resolve(samples);
    });
  });
}

module.exports = {
  csvEncode,
  parseInteger,
  parseNumber,
  runBenchmark,
};
