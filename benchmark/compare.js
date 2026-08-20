'use strict';

const { spawn, fork } = require('node:child_process');
const { inspect } = require('util');
const path = require('path');
const CLI = require('./_cli.js');
const BenchmarkProgress = require('./_benchmark_progress.js');

//
// Parse arguments
//
const cli = new CLI(`usage: ./node compare.js [options] [--] <category> ...
  Run each benchmark in the <category> directory many times using two different
  node versions. More than one <category> directory can be specified.
  The output is formatted as csv, which can be processed using for
  example 'compare.R'. Use --analyze to perform statistical analysis
  directly without R.

  --new      ./new-node-binary  new node binary (required)
  --old      ./old-node-binary  old node binary (required)
  --runs     30                 number of samples
  --filter   pattern            includes only benchmark scripts matching
                                <pattern> (can be repeated)
  --exclude  pattern            excludes scripts matching <pattern> (can be
                                repeated)
  --set      variable=value     set benchmark variable (can be repeated)
  --no-progress                 don't show benchmark progress indicator
  --analyze                     perform statistical analysis after benchmarks
                                complete (Welch's t-test, effect size) instead
                                of printing csv output
  --scale    1000               rate-to-integer multiplier for histogram
                                 precision when using --analyze (default: 1000)
  --max-regression  N           exit with code 1 if any statistically
                                 significant regression exceeds N% (implies
                                 --analyze)

  Examples:
    --set CPUSET=0            Runs benchmarks on CPU core 0.
    --set CPUSET=0-2          Specifies that benchmarks should run on CPU cores 0 to 2.

  Note: The CPUSET format should match the specifications of the 'taskset' command
`, { arrayArgs: ['set', 'filter', 'exclude'], boolArgs: ['no-progress', 'analyze'] });

if (!cli.optional.new || !cli.optional.old) {
  cli.abort(cli.usage);
}

const binaries = ['old', 'new'];
const runs = cli.optional.runs ? parseInt(cli.optional.runs, 10) : 30;
const maxRegression = cli.optional['max-regression'] ?
  parseFloat(cli.optional['max-regression']) :
  0;
const analyze = !!cli.optional.analyze || maxRegression > 0;
const scale = cli.optional.scale ? parseInt(cli.optional.scale, 10) : 1000;
const benchmarks = cli.benchmarks();

if (benchmarks.length === 0) {
  console.error('No benchmarks found');
  process.exitCode = 1;
  return;
}

// When --analyze is set, collect results for statistical analysis.
const results = analyze ? new Map() : null;

// Create queue from the benchmarks list such both node versions are tested
// `runs` amount of times each.
// Note: BenchmarkProgress relies on this order to estimate
// how much runs remaining for a file. All benchmarks generated from
// the same file must be run consecutively.
const queue = [];
for (const filename of benchmarks) {
  for (let iter = 0; iter < runs; iter++) {
    for (const binary of binaries) {
      queue.push({ binary, filename, iter });
    }
  }
}
// queue.length = binary.length * runs * benchmarks.length

// Print csv header (unless analyzing inline).
if (!analyze) {
  console.log('"binary","filename","configuration","rate","time"');
}

const kStartOfQueue = 0;

const showProgress = !cli.optional['no-progress'];
let progress;
if (showProgress) {
  progress = new BenchmarkProgress(queue, benchmarks, { analyze });
  progress.startQueue(kStartOfQueue);
}

(function recursive(i) {
  const job = queue[i];
  const resolvedPath = path.resolve(__dirname, job.filename);

  const cpuCore = cli.getCpuCoreSetting();
  let child;
  if (cpuCore !== null) {
    const spawnArgs = ['-c', cpuCore, cli.optional[job.binary], resolvedPath, ...cli.optional.set];
    child = spawn('taskset', spawnArgs, {
      env: process.env,
      stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
    });
  } else {
    child = fork(resolvedPath, cli.optional.set, {
      execPath: cli.optional[job.binary],
    });
  }

  child.on('message', (data) => {
    if (data.type === 'report') {
      // Construct configuration string, " A=a, B=b, ..."
      let conf = '';
      for (const key of Object.keys(data.conf)) {
        conf += ` ${key}=${inspect(data.conf[key])}`;
      }
      conf = conf.slice(1);

      if (analyze) {
        // Collect results for post-run analysis.
        const name = `${job.filename} ${conf}`;
        if (!results.has(name)) {
          results.set(name, { old: [], new: [] });
        }
        results.get(name)[job.binary].push(data.rate);
      } else {
        // Escape quotes (") for correct csv formatting
        conf = conf.replace(/"/g, '""');
        console.log(`"${job.binary}","${job.filename}","${conf}",` +
                    `${data.rate},${data.time}`);
      }
      if (showProgress) {
        // One item in the subqueue has been completed.
        progress.completeConfig(data);
      }
    } else if (showProgress && data.type === 'config') {
      // The child has computed the configurations, ready to run subqueue.
      progress.startSubqueue(data, i);
    }
  });

  child.once('close', (code) => {
    if (code) {
      process.exit(code);
    }
    if (showProgress) {
      progress.completeRun(job);
    }

    // If there are more benchmarks execute the next
    if (i + 1 < queue.length) {
      recursive(i + 1);
    } else if (analyze) {
      printAnalysis(results, scale, maxRegression);
    }
  });
})(kStartOfQueue);

function printAnalysis(results, scale, maxRegression) {
  const { createHistogram } = require('node:perf_hooks');

  // Build per-benchmark histograms and run statistical tests.
  const rows = [];
  let maxNameLen = 0;

  let skipped = 0;

  for (const [name, { old: oldRates, new: newRates }] of results) {
    if (oldRates.length < 2 || newRates.length < 2) {
      skipped++;
      continue;
    }

    const hOld = createHistogram({ figures: 3 });
    const hNew = createHistogram({ figures: 3 });

    for (const r of oldRates) hOld.record(Math.max(1, Math.round(r * scale)));
    for (const r of newRates) hNew.record(Math.max(1, Math.round(r * scale)));

    const oldMean = oldRates.reduce((a, b) => a + b, 0) / oldRates.length;
    const newMean = newRates.reduce((a, b) => a + b, 0) / newRates.length;
    const improvement = ((newMean - oldMean) / oldMean) * 100;

    // Query the three confidence levels. The p-value and t-statistic
    // are the same regardless of the confidence level, so we extract
    // them from the first result.
    const w95 = hOld.welchTest(hNew, { confidence: 0.95 });
    const w99 = hOld.welchTest(hNew, { confidence: 0.99 });
    const w999 = hOld.welchTest(hNew, { confidence: 0.999 });

    // Significance stars matching compare.R convention.
    let stars = '';
    if (w95.pValue < 0.001) stars = '***';
    else if (w95.pValue < 0.01) stars = ' **';
    else if (w95.pValue < 0.05) stars = '  *';

    // Confidence intervals expressed as percentage of the old mean.
    const ciPct = (w) => {
      const half =
        (w.confidenceInterval.upper - w.confidenceInterval.lower) / 2;
      return (half / (oldMean * scale)) * 100;
    };

    rows.push({
      name,
      stars,
      improvement,
      ci95: ciPct(w95),
      ci99: ciPct(w99),
      ci999: ciPct(w999),
      pValue: w95.pValue,
    });

    if (name.length > maxNameLen) maxNameLen = name.length;
  }

  // Print header.
  const pad = (s, n) => s + ' '.repeat(Math.max(0, n - s.length));
  const rpad = (s, n) => ' '.repeat(Math.max(0, n - s.length)) + s;

  console.log(`${pad('', maxNameLen)}  confidence` +
              `   improvement   accuracy (*)    (**)   (***)`);

  for (const row of rows) {
    const imp = `${row.improvement >= 0 ? '+' : ''}${row.improvement.toFixed(2)} %`;
    console.log(
      `${pad(row.name, maxNameLen)}  ${pad(row.stars, 10)}` +
      `  ${rpad(imp, 11)}` +
      `   ±${row.ci95.toFixed(2)}%` +
      `  ±${row.ci99.toFixed(2)}%` +
      `  ±${row.ci999.toFixed(2)}%`,
    );
  }

  if (skipped > 0) {
    console.log('');
    console.log(
      `Note: ${skipped} configuration${skipped === 1 ? ' was' : 's were'}` +
      ` skipped because Welch's t-test requires at least 2 samples per` +
      ` binary. Use --runs 2 or higher.`,
    );
  }

  // --- Bar chart visualization ---
  printChart(rows, maxNameLen);

  console.log('');
  console.log(
    `Rates were scaled by ${scale}x into HdrHistogram (3 significant figures).\n` +
    `Use --scale to adjust precision if needed.\n`,
  );
  console.log(
    `Be aware that when doing many comparisons the risk of a false-positive\n` +
    `result increases. In this case, there are ${rows.length} comparisons, ` +
    `you can thus\nexpect the following amount of false-positive results:\n` +
    `  ${(rows.length * 0.05).toFixed(2)} false positives, when considering ` +
    `a   5% risk acceptance (*, **, ***),\n` +
    `  ${(rows.length * 0.01).toFixed(2)} false positives, when considering ` +
    `a   1% risk acceptance (**, ***),\n` +
    `  ${(rows.length * 0.001).toFixed(2)} false positives, when considering ` +
    `a 0.1% risk acceptance (***)`,
  );

  // Gate: exit with error if any significant regression exceeds the limit.
  if (maxRegression > 0) {
    const failures = rows.filter(
      (r) => r.stars.trim() !== '' && r.improvement < -maxRegression,
    );
    if (failures.length > 0) {
      console.log('');
      console.log(
        `FAIL: ${failures.length} benchmark${failures.length === 1 ? '' : 's'}` +
        ` showed a statistically significant regression exceeding` +
        ` ${maxRegression}%:`,
      );
      for (const f of failures) {
        console.log(`  ${f.name}  ${f.improvement.toFixed(2)}%`);
      }
      process.exitCode = 1;
    }
  }
}

function printChart(rows, maxNameLen) {
  if (rows.length === 0) return;

  // Determine the chart scale from the data. The bar region covers
  // the range [-maxAbs, +maxAbs] so the zero line sits in the center.
  const barWidth = 40;
  const halfWidth = barWidth / 2;
  let maxAbs = 0;
  for (const row of rows) {
    const extent = Math.abs(row.improvement) + row.ci95;
    if (extent > maxAbs) maxAbs = extent;
  }
  if (maxAbs === 0) maxAbs = 1;

  const pad = (s, n) => s + ' '.repeat(Math.max(0, n - s.length));

  // Scale axis labels.
  const axisLeft = `-${maxAbs.toFixed(1)}%`;
  const axisRight = `+${maxAbs.toFixed(1)}%`;
  const axisCenter = '0%';

  // Print axis header.
  const labelPad = maxNameLen + 5;
  const leftLabel = ' '.repeat(labelPad) +
    axisLeft +
    ' '.repeat(Math.max(0, halfWidth - axisLeft.length - Math.floor(axisCenter.length / 2))) +
    axisCenter +
    ' '.repeat(Math.max(0, halfWidth - Math.ceil(axisCenter.length / 2) - axisRight.length)) +
    axisRight;
  console.log('');
  console.log(leftLabel);

  for (const row of rows) {
    const imp = row.improvement;
    const ci = row.ci95;

    // Position of the improvement value in the bar region [0, barWidth].
    const center = halfWidth;
    const impPos = center + (imp / maxAbs) * halfWidth;

    // CI extent in bar positions.
    const ciLeft = center + ((imp - ci) / maxAbs) * halfWidth;
    const ciRight = center + ((imp + ci) / maxAbs) * halfWidth;

    // Build the bar character by character.
    const chars = [];
    for (let x = 0; x < barWidth; x++) {
      const pos = x + 0.5; // Center of this character cell.
      if (x === Math.floor(center)) {
        chars.push('|');
      } else if ((imp >= 0 && pos > center && pos <= impPos) ||
                 (imp < 0 && pos < center && pos >= impPos)) {
        chars.push(row.stars ? '\u2588' : '\u2593'); // solid or dark shade
      } else if (pos >= ciLeft && pos <= ciRight) {
        chars.push('\u2591'); // Light shade for CI region
      } else {
        chars.push(' ');
      }
    }

    const label = `${row.improvement >= 0 ? '+' : ''}${row.improvement.toFixed(2)}%`;
    const sig = row.stars.trim();
    console.log(`${pad(row.name, maxNameLen)}  ${chars.join('')}  ${label} ${sig}`);
  }
}
