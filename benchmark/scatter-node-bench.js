'use strict';

const CLI = require('./_cli.js');
const {
  analyzeScatter,
  validateScatterParameters,
} = require('./_node-bench-analysis.js');
const {
  csvEncode,
  parseInteger,
  runBenchmark,
} = require('./_node-bench.js');

const cli = new CLI(`usage: ./node scatter-node-bench.js [options] [--] <file>
  Run an explicit node:bench file repeatedly and output each independent
  observation with its benchmark parameters as CSV, or summarize the results
  directly with --analyze.

  --node         ./node     Node.js binary
  --runs         30         number of observations
  --warmup       0          warmup samples before each observation
  --name-pattern pattern    only run matching benchmarks
  --node-arg     argument   pass an argument to the binary (repeatable)
  --analyze                 print a statistical summary instead of CSV
  --xaxis        parameter  parameter to group by with --analyze (required)
  --category     parameter  optional second grouping parameter
  --no-chart                omit the analysis bar chart
`, {
  arrayArgs: ['node-arg'],
  boolArgs: ['analyze', 'no-chart'],
});

if (cli.items.length !== 1) cli.abort(cli.usage);
if (cli.optional.analyze && cli.optional.xaxis === undefined) {
  cli.abort('--analyze requires --xaxis <parameter>');
}

async function main() {
  const runs = parseInteger(cli.optional.runs, 30, '--runs', 1);
  const warmup = parseInteger(cli.optional.warmup, 0, '--warmup', 0);
  const options = {
    namePattern: cli.optional['name-pattern'],
    nodeArgs: cli.optional['node-arg'],
    warmup,
  };
  const binary = cli.optional.node || process.execPath;
  const rows = [];
  const paramNames = new Set();
  const csvGroups = new Map();
  let expectedIdentities;
  let logicalIdentity;

  for (let run = 0; run < runs; run++) {
    const samples = await runBenchmark(binary, cli.items[0], options);
    if (run === 0 && cli.optional.analyze) {
      validateScatterParameters(
        samples, cli.optional.xaxis, cli.optional.category);
    }
    const identities = new Set();
    for (const sample of samples) {
      if (logicalIdentity !== undefined &&
          logicalIdentity !== sample.logicalIdentity) {
        throw new Error(
          'scatter-node-bench.js requires one logical benchmark name per file',
        );
      }
      logicalIdentity = sample.logicalIdentity;
      if (identities.has(sample.identity)) {
        throw new Error(`Benchmark '${sample.name}' was reported more than once`);
      }
      identities.add(sample.identity);
      const csvGroup = JSON.stringify([sample.name, sample.params]);
      const groupedIdentity = csvGroups.get(csvGroup);
      if (groupedIdentity !== undefined &&
          groupedIdentity !== sample.identity) {
        throw new Error(
          `Distinct benchmarks would share the CSV group '${sample.name}'`,
        );
      }
      csvGroups.set(csvGroup, sample.identity);
      rows.push({ ...sample, observation: run });
      for (const name of Object.keys(sample.params)) paramNames.add(name);
    }
    if (expectedIdentities === undefined) {
      expectedIdentities = identities;
    } else if (identities.size !== expectedIdentities.size ||
               ![...identities].every((id) => expectedIdentities.has(id))) {
      throw new Error('The set of reported benchmarks changed between runs');
    }
  }
  if (rows.length === 0) {
    throw new Error('No benchmark samples were produced');
  }

  const params = [...paramNames].sort();
  if (cli.optional.analyze) {
    const output = analyzeScatter(
      rows,
      cli.optional.xaxis,
      cli.optional.category,
      !cli.optional['no-chart'],
    );
    process.stdout.write(output);
    return;
  }

  for (const name of params) {
    if (name === 'filename' || name === 'rate' || name === 'time') {
      throw new Error(`Benchmark parameter '${name}' is reserved in scatter CSV`);
    }
  }
  const header = ['filename', ...params, 'rate', 'time']
    .map(csvEncode)
    .join(',');
  const output = [header];
  for (const row of rows) {
    const values = [
      csvEncode(row.name),
      ...params.map((name) => csvEncode(row.params[name] ?? '')),
      row.rate,
      row.duration,
    ];
    output.push(values.join(','));
  }
  process.stdout.write(`${output.join('\n')}\n`);
}

main().catch((error) => {
  console.error(error.stack);
  process.exitCode = 1;
});
