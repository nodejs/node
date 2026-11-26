// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs Speedometer in d8 with the mock DOM:
//
//   out/x64.release/d8 --experimental-mock-dom \
//       experimental/d8/speedometer-harness/run-speedometer.js \
//       -- [--version=2.1|3.1|main] [--iterations=N] [--tags=default]
//          [--verbose] [--fingerprint] [SuiteName...]
//
// This drives the unmodified Speedometer runner and suite definitions: the
// same steps, the same requestAnimationFrame-based measurement, a fresh page
// per suite per iteration and the same score formula as in a browser. The
// only substitute is the page loader (browser-shell.js).
//
// A score is only printed if nothing went wrong: every uncaught exception,
// console.error, failed load and failed check is collected and makes the run
// fail.
//
// Known not to work: Responsive-Design (--version=main --tags=all), which
// sizes its UI by text and grid layout that the mock DOM does not have.
//
// Scores are only comparable within one version: the pinned revision of
// Speedometer main, for one, reports the last step of a suite as the suite's
// total.

load('experimental/d8/speedometer-harness/browser-shell.js');

const HARNESS_DIR = 'experimental/d8/speedometer-harness/';

// Observable Plot names its stylesheet class after
// Math.random().toString(16), whose length varies, and repeats that name in
// a <style> element inside the chart. The text length therefore differs by
// a few characters between runs, in a browser too.
function plotTolerances(checkpoints) {
  const tolerances = {};
  for (const checkpoint of checkpoints) {
    tolerances[`Charts-observable-plot/${checkpoint}`] = {textLength: 12};
  }
  return tolerances;
}

function selectByName(Suites, names, setEnabled) {
  const unknown =
      names.filter((name) => !Suites.some((suite) => suite.name === name));
  if (unknown.length > 0) {
    const available = Suites.map((s) => s.name).join('\n  ');
    throw new Error(`Unknown suite(s) ${unknown.join(', ')}. ` +
                    `Available suites:\n  ${available}`);
  }
  for (const suite of Suites) setEnabled(suite, names.includes(suite.name));
}

// What differs between Speedometer versions. load() returns the runner
// class, the suites to construct it with, and the suites that will run.
const VERSIONS = {
  '2.1': {
    root: 'test/benchmarks/Speedometer2/',
    search: () => '',
    tolerances: {},
    ignoredErrors: [],
    async load(options) {
      // Classic scripts that define BenchmarkRunner and Suites as globals.
      load(this.root + 'resources/benchmark-runner.js');
      load(this.root + 'resources/tests.js');
      const {BenchmarkRunner, Suites} = globalThis;
      if (options.suites.length > 0) {
        selectByName(Suites, options.suites,
                     (suite, enabled) => { suite.disabled = !enabled; });
      } else if (options.tags.join() !== 'default') {
        throw new Error('Speedometer 2.1 has no tags');
      }
      const suites = Suites.filter((suite) => !suite.disabled);
      return {BenchmarkRunner, runnerSuites: suites, suites};
    },
    // The runner reports each step when it is done.
    expectedCheckpoints: (suite) => suite.tests.length,
  },
  '3.1': {
    root: 'test/benchmarks/Speedometer3/',
    // params.mjs reads the configuration from location.search when imported.
    search: (options) => `?iterationCount=${options.iterations}`,
    tolerances: plotTolerances(['Stacked by 6', 'Stacked by 20', 'Dotted']),
    ignoredErrors: [],
    todoMVCChecks: true,
    async load(options) {
      const base = '../../../test/benchmarks/Speedometer3/resources/';
      const {BenchmarkRunner} = await import(base + 'benchmark-runner.mjs');
      const {Suites} = await import(base + 'tests.mjs');
      // Suites that upstream marks `disabled` are not in the default set.
      if (options.suites.length > 0) {
        selectByName(Suites, options.suites,
                     (suite, enabled) => { suite.disabled = !enabled; });
      } else if (options.tags.join() !== 'default') {
        for (const suite of Suites) {
          suite.disabled =
              !options.tags.some((tag) => suite.tags.includes(tag));
        }
      }
      const suites = Suites.filter((suite) => !suite.disabled);
      return {BenchmarkRunner, runnerSuites: suites, suites};
    },
    expectedCheckpoints: (suite) => suite.tests.length,
  },
  'main': {
    root: 'test/benchmarks/SpeedometerMain/',
    // The parameters a browser run started from a URL has. (With only
    // default parameters the pinned revision builds a Perf-Dashboard URL
    // that the workload cannot parse.)
    search: (options) =>
        `?iterationCount=${options.iterations}&startAutomatically` +
        (options.suites.length ? `&suites=${options.suites.join(',')}` : ''),
    tolerances: plotTolerances(['before:Stacked by 20', 'before:Dotted',
                                'end']),
    // Speedometer's own parameter parser logs this for the IndexedDB
    // workloads' storageType parameter, in a browser too.
    ignoredErrors: ['Got unused search params storageType'],
    async load(options) {
      const base = '../../../test/benchmarks/SpeedometerMain/resources/';
      const {BenchmarkRunner} = await import(base + 'benchmark-runner.mjs');
      const {benchmarkConfigurator} =
          await import(base + 'benchmark-configurator.mjs');
      const Suites = benchmarkConfigurator.suites;
      if (options.suites.length > 0) {
        selectByName(Suites, options.suites, () => {});
      }
      benchmarkConfigurator.enableSuites(
          options.suites, options.suites.length ? [] : options.tags);
      const suites = Suites.filter((suite) => suite.enabled);
      // This runner takes all suites and skips the ones not enabled.
      return {BenchmarkRunner, runnerSuites: Suites, suites};
    },
    // The runner reports a step before it runs and a suite when it is done.
    // Remote suites run their steps inside the workload page and only
    // report the end.
    expectedCheckpoints: (suite) =>
        suite.type === 'remote' ? 1 : suite.tests.length + 1,
  },
};

function parseArguments(argv) {
  const options = {version: '3.1', iterations: 10, tags: ['default'],
                   suites: [], verbose: false, fingerprint: false};
  for (const arg of argv) {
    if (arg.startsWith('--version=')) {
      options.version = arg.slice('--version='.length);
      if (!VERSIONS[options.version]) {
        throw new Error(`Unknown version ${options.version}`);
      }
    } else if (arg.startsWith('--iterations=')) {
      options.iterations = parseInt(arg.slice('--iterations='.length));
    } else if (arg.startsWith('--tags=')) {
      options.tags = arg.slice('--tags='.length).split(',');
    } else if (arg === '--verbose') {
      options.verbose = true;
    } else if (arg === '--fingerprint') {
      // Print a DOM summary after every step, for comparison with a browser.
      options.fingerprint = true;
    } else if (arg.startsWith('--')) {
      throw new Error(`Unknown option ${arg}`);
    } else {
      options.suites.push(arg);
    }
  }
  return options;
}

// --- Post-conditions ---------------------------------------------------------
//
// The real steps already throw if an element they need is missing. These
// checks cover the other way a workload can silently do nothing: every
// element is there, but the app ignored the input.

function expectEqual(actual, expected, what) {
  if (actual !== expected) {
    throw new Error(`post-condition failed: ${what}: expected ${expected}, ` +
                    `got ${actual}`);
  }
}

const TODO_COUNT = 100;

// The checkbox of a todo item. The web components app names it differently.
function toggleSelector(suite) {
  return suite.name.includes('WebComponents') ? '.toggle-todo-input' :
                                                '.toggle';
}

const todoMVCChecks = {
  // Called before each step with the name of the step about to run, and once
  // more with null after the last step.
  before(suite, stepName, doc) {
    const toggles = shell.deepQuerySelectorAll(doc, toggleSelector(suite));
    const checked = toggles.filter((toggle) => toggle.checked).length;
    if (stepName === 'CompletingAllItems') {
      expectEqual(toggles.length, TODO_COUNT, 'todo items after adding');
      expectEqual(checked, 0, 'completed items before completing');
    } else if (stepName === 'DeletingAllItems') {
      expectEqual(toggles.length, TODO_COUNT, 'todo items before deleting');
      expectEqual(checked, TODO_COUNT, 'completed items after completing');
    } else if (stepName === null) {
      expectEqual(toggles.length, 0, 'todo items after deleting');
    }
  },
};

// At every checkpoint (after each step, or before each step and at the end
// of the suite, depending on what the runner reports), the DOM the workload
// built is compared with what Chrome built at the same point
// (expected-dom-*.json, recorded with dom-fingerprint.js). This is what
// catches a workload that runs without errors but does less, or different,
// work than it does in a browser.
function compareFingerprint(expectedDom, tolerances, key, actual) {
  const expected = expectedDom[key];
  if (!expected) throw new Error(`no expected DOM recorded for ${key}`);
  // {field: maximum difference} for fields that vary from run to run.
  const tolerance = tolerances[key] ?? {};
  const differences = [];
  for (const field of ['elements', 'textNodes', 'textLength', 'attributes']) {
    const difference = Math.abs(actual[field] - expected[field]);
    if (difference > (tolerance[field] ?? 0)) {
      differences.push(`${field} ${actual[field]} (browser: ` +
                       `${expected[field]})`);
    }
  }
  const tags = new Set(
      [...Object.keys(actual.tags), ...Object.keys(expected.tags)]);
  for (const tag of tags) {
    const a = actual.tags[tag] ?? 0, e = expected.tags[tag] ?? 0;
    if (a !== e) differences.push(`<${tag}> ${a} (browser: ${e})`);
  }
  if (differences.length > 0) {
    throw new Error(`DOM differs from the browser's at ${key}: ` +
                    differences.slice(0, 6).join(', '));
  }
}

// --- Main --------------------------------------------------------------------

// d8 exposes the command line after `--` as the script-level `arguments`.
const commandLine =
    typeof arguments !== 'undefined' ? Array.from(arguments) : [];

async function main() {
  const options = parseArguments(commandLine);
  const version = VERSIONS[options.version];
  const expectedDom = JSON.parse(
      read(`${HARNESS_DIR}expected-dom-${options.version}.json`)).steps;

  shell.install({
    root: version.root,
    url: `${shell.ORIGIN}/index.html${version.search(options)}`,
    verbose: options.verbose,
  });
  const {BenchmarkRunner, runnerSuites, suites} = await version.load(options);

  const failures = [];
  let runner;
  let iteration = 0;
  // Checkpoints whose DOM was compared. If a runner stops reporting them,
  // nothing would be verified.
  let checkpoints = 0;
  // Per suite, the total of each iteration, and the score of each iteration.
  const totals = new Map();
  const scores = [];

  function recordErrors(where) {
    for (const error of shell.errors) {
      if (version.ignoredErrors.some((text) => error.includes(text))) continue;
      failures.push(where ? `${where}: ${error}` : error);
    }
    shell.clearErrors();
  }

  function where(suite) {
    return `${suite.name} (iteration ${iteration})`;
  }

  function checkpoint(suite, name) {
    const fingerprint = d8.mockDom.fingerprint(runner._frame.contentWindow);
    if (options.fingerprint && iteration === 0) {
      console.log(`FP ${JSON.stringify([suite.name, name, fingerprint])}`);
    }
    checkpoints++;
    try {
      compareFingerprint(expectedDom, version.tolerances,
                         `${suite.name}/${name}`, fingerprint);
    } catch (e) {
      failures.push(`${where(suite)}: ${e.message}`);
    }
  }

  function checkTodoMVC(suite, stepName) {
    if (!version.todoMVCChecks || !suite.name.startsWith('TodoMVC-')) return;
    try {
      todoMVCChecks.before(suite, stepName, runner._frame.contentDocument);
    } catch (e) {
      failures.push(`${where(suite)}: ${e.message}`);
    }
  }

  // A page that never becomes ready would otherwise spin in
  // waitForElement() forever.
  const SUITE_TIMEOUT_MS = 30000;
  let watchdog = 0;
  function armWatchdog(what) {
    clearTimeout(watchdog);
    watchdog = setTimeout(() => {
      failures.push(`${what}: no progress for ${SUITE_TIMEOUT_MS} ms`);
      report(null);
    }, SUITE_TIMEOUT_MS);
  }

  // Which of these callbacks a runner calls depends on its version; see
  // expectedCheckpoints in VERSIONS.
  const client = {
    willStartFirstIteration() { armWatchdog('first suite'); },
    willAddTestFrame() {},
    willRunTest(suite, test) {
      if (test === suite.tests[0] && options.verbose) {
        console.log(`[run] ${suite.name} iteration ${iteration}`);
      }
      // 2.1 and 3.1 report every finished step, see didRunTest.
      if (options.version === 'main') checkpoint(suite, `before:${test.name}`);
      checkTodoMVC(suite, test.name);
      armWatchdog(`${suite.name} / ${test.name}`);
    },
    didRunTest(suite, test) {
      checkpoint(suite, test.name);
      armWatchdog(`after ${suite.name} / ${test.name}`);
      if (test !== suite.tests[suite.tests.length - 1]) return;
      checkTodoMVC(suite, null);
      recordErrors(where(suite));
    },
    didFinishSuite(suite) {
      checkpoint(suite, 'end');
      recordErrors(where(suite));
      armWatchdog(`after ${suite.name}`);
    },
    didRunSuites(measuredValues) {
      for (const [name, result] of Object.entries(measuredValues.tests)) {
        if (!totals.has(name)) totals.set(name, []);
        totals.get(name).push(result.total);
        if (options.verbose) {
          console.log(`  ${name}: ${result.total.toFixed(2)} ms`);
        }
      }
      scores.push(measuredValues.score);
      console.log(`Iteration ${iteration}: geomean ` +
                  `${measuredValues.geomean.toFixed(3)} ms, score ` +
                  `${measuredValues.score.toFixed(2)}`);
      iteration++;
    },
    handleError(error) {
      failures.push(`runner error: ${error && error.stack || error}`);
      report(null);
    },
    // 3.1 and main pass their metrics; 2.1 passes nothing.
    didFinishLastIteration(metrics) {
      clearTimeout(watchdog);
      report(metrics ?? {});
    },
  };

  function mean(values) {
    return values.reduce((sum, value) => sum + value, 0) / values.length;
  }

  // |metrics| is null if the run did not finish.
  function report(metrics) {
    recordErrors('');
    if (metrics) {
      const expected = options.iterations * suites.reduce(
          (sum, suite) => sum + version.expectedCheckpoints(suite), 0);
      if (checkpoints !== expected) {
        failures.push(`${checkpoints} checkpoints were compared, expected ` +
                      `${expected}`);
      }
    }
    const uncaught = d8.mockDom.uncaughtErrorCount();
    console.log('');
    if (failures.length > 0 || uncaught > 0) {
      console.log(`FAILED: ${failures.length} failure(s), ${uncaught} ` +
                  `uncaught error(s). No score is reported.`);
      const shown = failures.slice(0, 40);
      for (const failure of shown) console.log(`  - ${failure}`);
      if (failures.length > shown.length) {
        console.log(`  ... and ${failures.length - shown.length} more`);
      }
      quit(1);
    }
    console.log('Suite totals (ms, mean of iterations):');
    for (const suite of suites) {
      const metric = metrics[suite.name];
      const suiteMean = metric ? metric.mean : mean(totals.get(suite.name));
      console.log(`  ${suite.name.padEnd(44)} ${suiteMean.toFixed(2)}` +
                  (metric ? ` ± ${metric.delta.toFixed(2)}` : ''));
      if (!options.verbose || !metric) continue;
      for (const child of metric.children) {
        const sync = metrics[`${child.name}/Sync`];
        const async = metrics[`${child.name}/Async`];
        const stepName = child.name.slice(suite.name.length + 1);
        console.log(
            `      ${stepName.padEnd(28)} ${child.mean.toFixed(2)}` +
            (sync && async ?
                 `  (sync ${sync.mean.toFixed(2)}, async ` +
                     `${async.mean.toFixed(2)})` :
                 ''));
      }
    }
    console.log('');
    if (metrics.Geomean) {
      console.log(`Geomean: ${metrics.Geomean.mean.toFixed(3)} ms`);
    }
    console.log(metrics.Score ?
        `Score:   ${metrics.Score.mean.toFixed(2)} ± ` +
            `${metrics.Score.delta.toFixed(2)}` :
        `Score:   ${mean(scores).toFixed(2)}`);
  }

  runner = new BenchmarkRunner(runnerSuites, client);
  await runner.runMultipleIterations(options.iterations);
}

main().catch((e) => {
  console.log(`FAILED: ${e && e.stack || e}`);
  quit(1);
});
