'use strict';

const path = require('node:path');
const CLI = require('./_cli.js');
const { analyzeCompare } = require('./_node-bench-analysis.js');
const {
  csvEncode,
  parseInteger,
  parseNumber,
  runBenchmark,
} = require('./_node-bench.js');

const cli = new CLI(`usage: ./node compare-node-bench.js [options] [--] <file> ...
  Run explicit node:bench files repeatedly with two Node.js binaries. Each
  observation runs in a fresh process. Output is compatible with compare.R,
  or --analyze can summarize it directly.

  --new          binary     new Node.js binary (required)
  --old          binary     old Node.js binary (required)
  --runs         30         observations per binary
  --warmup       0          warmup samples before each observation
  --name-pattern pattern    only run matching benchmarks
  --node-arg     argument   pass an argument to both binaries (repeatable)
  --analyze                 analyze with Welch's t-test instead of writing CSV
  --scale        1000       rate multiplier used for histogram precision
  --max-regression N        fail if a family-wise significant regression's
                            95% confidence interval is entirely beyond N%
                            (implies --analyze)
`, { arrayArgs: ['node-arg'], boolArgs: ['analyze'] });

if (!cli.optional.new || !cli.optional.old || cli.items.length === 0) {
  cli.abort(cli.usage);
}

async function main() {
  const runs = parseInteger(cli.optional.runs, 30, '--runs', 1);
  const warmup = parseInteger(cli.optional.warmup, 0, '--warmup', 0);
  const scale = parseInteger(cli.optional.scale, 1000, '--scale', 1);
  const hasMaxRegression = cli.optional['max-regression'] !== undefined;
  const maxRegression = parseNumber(
    cli.optional['max-regression'], 0, '--max-regression', 0);
  const analyze = !!cli.optional.analyze || hasMaxRegression;
  const options = {
    namePattern: cli.optional['name-pattern'],
    nodeArgs: cli.optional['node-arg'],
    warmup,
  };
  const binaries = [
    { label: 'old', path: cli.optional.old },
    { label: 'new', path: cli.optional.new },
  ];
  const rows = [];
  const counts = new Map();
  const csvGroups = new Map();

  for (const file of cli.items) {
    const resolved = path.resolve(file);
    for (let run = 0; run < runs; run++) {
      const order = run % 2 === 0 ? binaries : [binaries[1], binaries[0]];
      for (const binary of order) {
        const samples = await runBenchmark(binary.path, resolved, options);
        for (const sample of samples) {
          const identity = JSON.stringify([resolved, sample.identity]);
          const csvGroup = JSON.stringify([
            sample.name,
            sample.configuration,
          ]);
          const groupedIdentity = csvGroups.get(csvGroup);
          if (groupedIdentity !== undefined && groupedIdentity !== identity) {
            throw new Error(
              `Distinct benchmarks would share the CSV group '${sample.name} ` +
              `${sample.configuration}'`,
            );
          }
          csvGroups.set(csvGroup, identity);
          let count = counts.get(identity);
          if (count === undefined) {
            count = { name: sample.name, new: 0, old: 0 };
            counts.set(identity, count);
          }
          count[binary.label]++;
          rows.push({ binary: binary.label, ...sample });
        }
      }
    }
  }

  if (rows.length === 0) {
    throw new Error('No benchmark samples were produced');
  }
  for (const count of counts.values()) {
    if (count.old !== runs || count.new !== runs) {
      throw new Error(
        `Benchmark '${count.name}' was not reported by both binaries in every run`,
      );
    }
  }

  if (analyze) {
    const result = analyzeCompare(
      rows, scale, hasMaxRegression ? maxRegression : undefined);
    process.stdout.write(result.output);
    if (result.failed) process.exitCode = 1;
    return;
  }

  const output = ['"binary","filename","configuration","rate","time"'];
  for (const row of rows) {
    output.push(`${csvEncode(row.binary)},${csvEncode(row.name)},` +
                `${csvEncode(row.configuration)},${row.rate},${row.duration}`);
  }
  process.stdout.write(`${output.join('\n')}\n`);
}

main().catch((error) => {
  console.error(error.stack);
  process.exitCode = 1;
});
