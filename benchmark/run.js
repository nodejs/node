'use strict';

const path = require('node:path');
const { spawn, fork } = require('node:child_process');
const fs = require('node:fs');
const CLI = require('./_cli.js');

const cli = new CLI(`usage: ./node run.js [options] [--] <category> ...
  Run each benchmark in the <category> directory a single time, more than one
  <category> directory can be specified.

  --filter   pattern        includes only benchmark scripts matching <pattern>
                            (can be repeated)
  --exclude  pattern        excludes scripts matching <pattern> (can be
                            repeated)
  --set    variable=value   set benchmark variable (can be repeated)
  --format [simple|csv]     optional value that specifies the output format
  test                      only run a single configuration from the options
                            matrix
  coverage                  generate a coverage report for the nodejs
                            benchmark suite
  all                       each benchmark category is run one after the other

  Examples:
    --set CPUSET=0            Runs benchmarks on CPU core 0.
    --set CPUSET=0-2          Specifies that benchmarks should run on CPU cores 0 to 2.

  Note: The CPUSET format should match the specifications of the 'taskset' command on your system.
`, { arrayArgs: ['set', 'filter', 'exclude'] });

const benchmarks = cli.benchmarks();

if (benchmarks.length === 0) {
  console.error('No benchmarks found');
  process.exitCode = 1;
  return;
}

const validFormats = ['csv', 'simple'];
const format = cli.optional.format || 'simple';
if (!validFormats.includes(format)) {
  console.error('Invalid format detected');
  process.exitCode = 1;
  return;
}

if (format === 'csv' && !cli.coverage) {
  console.log('"filename", "configuration", "rate", "time"');
}

function fetchModules() {
  const dir = fs.readdirSync(path.join(__dirname, '../lib'));
  const allModuleExports = {};
  for (const f of dir) {
    if (f.endsWith('.js') && !f.startsWith('_')) {
      const moduleName = `node:${f.slice(0, f.length - 3)}`;
      const exports = require(moduleName);
      allModuleExports[moduleName] = {};
      for (const fnKey of Object.keys(exports)) {
        if (typeof exports[fnKey] === 'function' && !fnKey.startsWith('_')) {
          allModuleExports[moduleName] = {
            ...allModuleExports[moduleName],
            [fnKey]: 0,
          };
        }
      }
    }
  }
  return allModuleExports;
}

let allModuleExports = {};
if (cli.coverage) {
  allModuleExports = fetchModules();
}

(function recursive(i) {
  const filename = benchmarks[i];
  const scriptPath = path.resolve(__dirname, filename);

  const args = cli.test ? ['--test'] : [...cli.optional.set];

  let execArgv = [];
  if (cli.coverage) {
    execArgv = ['-r', path.join(__dirname, './coverage.js'), '--experimental-sqlite', '--no-warnings'];
  }

  const cpuCore = cli.getCpuCoreSetting();
  let child;
  if (cpuCore !== null) {
    child = spawn('taskset', ['-c', cpuCore, 'node', scriptPath, ...args], {
      stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
    });
  } else {
    child = fork(
      scriptPath,
      args,
      { execArgv },
    );
  }

  if (format !== 'csv') {
    console.log();
    console.log(filename);
  }

  child.on('message', (data) => {
    if (cli.coverage) {
      if (data.type === 'coverage') {
        if (allModuleExports[data.module][data.fn] !== undefined) {
          delete allModuleExports[data.module][data.fn];
        }
      }
      return;
    }

    if (data.type !== 'report') {
      return;
    }
    // Construct configuration string, " A=a, B=b, ..."
    let conf = '';
    for (const key of Object.keys(data.conf)) {
      if (conf !== '')
        conf += ' ';
      conf += `${key}=${JSON.stringify(data.conf[key])}`;
    }
    if (format === 'csv') {
      // Escape quotes (") for correct csv formatting
      conf = conf.replace(/"/g, '""');
      console.log(`"${data.name}", "${conf}", ${data.rate}, ${data.time}`);
    } else {
      let rate = data.rate.toString().split('.');
      rate[0] = rate[0].replace(/(\d)(?=(?:\d\d\d)+(?!\d))/g, '$1,');
      rate = (rate[1] ? rate.join('.') : rate[0]);
      console.log(`${data.name} ${conf}: ${rate}`);
    }
  });

  child.once('close', (code) => {
    if (code) {
      process.exit(code);
    }

    // If there are more benchmarks execute the next
    if (i + 1 < benchmarks.length) {
      recursive(i + 1);
    }
  });
})(0);

const skippedFunctionClasses = [
  'EventEmitter',
  'Worker',
  'ClientRequest',
  'Readable',
  'StringDecoder',
  'TLSSocket',
  'MessageChannel',
];

const skippedModules = [
  'node:cluster',
  'node:trace_events',
  'node:stream/promises',
];

if (cli.coverage) {
  process.on('beforeExit', () => {
    for (const key in allModuleExports) {
      if (skippedModules.includes(key)) continue;
      const tableData = [];
      for (const innerKey in allModuleExports[key]) {
        if (
          allModuleExports[key][innerKey].toString().match(/^class/) ||
            skippedFunctionClasses.includes(innerKey)
        ) {
          continue;
        }

        tableData.push({
          [key]: innerKey,
          Values: allModuleExports[key][innerKey],
        });
      }
      if (tableData.length) {
        console.table(tableData);
      }
    }
  });
}
