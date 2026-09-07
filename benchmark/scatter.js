'use strict';

const { spawn, fork } = require('node:child_process');
const { createHistogram } = require('node:perf_hooks');
const { inspect } = require('util');
const path = require('path');
const CLI = require('./_cli.js');
const BenchmarkProgress = require('./_benchmark_progress.js');

//
// Parse arguments
//
const cli = new CLI(`usage: ./node scatter.js [options] [--] <filename>
  Run the benchmark script <filename> many times and output the rate (ops/s)
  together with the benchmark variables as a csv, which can be processed using
  for example 'scatter.R'. Use --analyze to summarize the results directly
  without R.

  --runs 30              number of samples
  --set  variable=value  set benchmark variable (can be repeated)
  --no-progress          don't show benchmark progress indicator
  --analyze              print a statistical summary (mean rate and confidence
                         interval per group) instead of csv output
  --xaxis     variable   variable to group by when using --analyze (required
                         by --analyze)
  --category  variable   additional variable to group by when using --analyze
  --no-chart             don't print the bar chart when using --analyze

  Examples:
    --set CPUSET=0            Runs benchmarks on CPU core 0.
    --set CPUSET=0-2          Specifies that benchmarks should run on CPU cores 0 to 2.

  Note: The CPUSET format should match the specifications of the 'taskset' command
`, {
  arrayArgs: ['set'],
  boolArgs: ['no-progress', 'analyze', 'no-chart'],
});

if (cli.items.length !== 1) {
  cli.abort(cli.usage);
}

const filepath = path.resolve(cli.items[0]);
const name = path.relative(__dirname, filepath);
const runs = cli.optional.runs ? parseInt(cli.optional.runs, 10) : 30;
const analyze = !!cli.optional.analyze;
const showChart = !cli.optional['no-chart'];
const xAxis = cli.optional.xaxis;
const category = cli.optional.category;

// Grouping is what makes the summary meaningful, so --analyze needs to know
// which variable is the independent one. There is no sensible default: the
// answer depends entirely on what is being measured.
if (analyze && !xAxis) {
  cli.abort(
    `--analyze requires --xaxis <variable>

  --xaxis names the benchmark variable to summarize against; --category
  optionally breaks each point down by a second variable. Both must be
  configuration variables of this benchmark -- the keys passed to
  createBenchmark() at the top of the benchmark file. Nearly every
  benchmark defines 'n'; parameter sweeps add a size or mode variable
  such as 'len', 'size' or 'encoding'.

  Example:
    ./node benchmark/scatter.js --analyze --xaxis n ${name}
`,
  );
}

// When --analyze is set, collect results rather than streaming csv.
const samples = analyze ? [] : null;

let printHeader = true;

function csvEncodeValue(value) {
  // Benchmark configuration values are numbers, booleans or strings
  // (see the config parsing in common.js). Only strings need quoting,
  // but anything unexpected is stringified rather than crashing the run.
  if (typeof value === 'number' || typeof value === 'boolean') {
    return value.toString();
  }
  return `"${String(value).replace(/"/g, '""')}"`;
}

// Note: BenchmarkProgress reports progress per file; scatter.js only ever
// runs one file, so every queue entry shares the same filename.
const queue = [];
for (let iter = 0; iter < runs; iter++) {
  queue.push({ filename: name, iter });
}

const kStartOfQueue = 0;

const showProgress = !cli.optional['no-progress'];
let progress;
if (showProgress) {
  progress = new BenchmarkProgress(queue, [name], { analyze });
  progress.startQueue(kStartOfQueue);
}

(function recursive(i) {
  const cpuCore = cli.getCpuCoreSetting();
  let child;
  if (cpuCore !== null) {
    const spawnArgs = ['-c', cpuCore, process.execPath, filepath, ...cli.optional.set];
    child = spawn('taskset', spawnArgs, {
      env: process.env,
      stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
    });
  } else {
    child = fork(filepath, cli.optional.set);
  }

  child.on('message', (data) => {
    if (data.type === 'config') {
      if (showProgress) {
        progress.startSubqueue(data, i);
      }
      return;
    }

    if (data.type !== 'report') {
      return;
    }

    if (analyze) {
      // Validate the grouping variables against the first result rather than
      // at the end. A typo in --xaxis is otherwise not reported until the
      // whole run has finished, which can be many minutes of wasted work.
      if (samples.length === 0) {
        const confKeys = Object.keys(data.conf);
        for (const key of [xAxis, category]) {
          if (key !== undefined && !confKeys.includes(key)) {
            child.kill();
            cli.abort(
              `The variable "${key}" is not a configuration of ${name}.\n` +
              `Available variables: ${confKeys.join(', ')}`,
            );
          }
        }
      }
      samples.push(data);
    } else {
      // print csv header
      if (printHeader) {
        const confHeader = Object.keys(data.conf)
          .map(csvEncodeValue)
          .join(',');
        console.log(`"filename",${confHeader},"rate","time"`);
        printHeader = false;
      }

      // print data row
      const confData = Object.keys(data.conf)
        .map((key) => csvEncodeValue(data.conf[key]))
        .join(',');

      console.log(`"${name}",${confData},${data.rate},${data.time}`);
    }

    if (showProgress) {
      progress.completeConfig(data);
    }
  });

  child.once('close', (code) => {
    if (code) {
      process.exit(code);
      return;
    }
    if (showProgress) {
      progress.completeRun(queue[i]);
    }

    // If there are more benchmarks execute the next
    if (i + 1 < runs) {
      recursive(i + 1);
    } else if (analyze) {
      printAnalysis(samples, xAxis, category);
    }
  });
})(kStartOfQueue);

//
// Statistics
//
// scatter.R obtains the t quantile from R's qt(); without R it has to be
// computed here. The Student's t tail probability is an incomplete beta
// function, which is evaluated directly, and the quantile is recovered by
// bisecting it. Bisection rather than an inverse-beta routine because the
// cost is irrelevant at this scale and the error bound is explicit.
//

// Lanczos approximation, g=7, n=9.
function logGamma(x) {
  const g = [
    676.5203681218851, -1259.1392167224028, 771.32342877765313,
    -176.61502916214059, 12.507343278686905, -0.13857109526572012,
    9.9843695780195716e-6, 1.5056327351493116e-7,
  ];
  if (x < 0.5) {
    // Reflection formula.
    return Math.log(Math.PI / Math.sin(Math.PI * x)) - logGamma(1 - x);
  }
  x -= 1;
  let a = 0.99999999999980993;
  const t = x + 7.5;
  for (let i = 0; i < g.length; i++) {
    a += g[i] / (x + i + 1);
  }
  return 0.5 * Math.log(2 * Math.PI) + (x + 0.5) * Math.log(t) - t + Math.log(a);
}

// Continued fraction for the incomplete beta function (Lentz's algorithm).
function betaContinuedFraction(x, a, b) {
  const tiny = 1e-30;
  const qab = a + b;
  const qap = a + 1;
  const qam = a - 1;
  let c = 1;
  let d = 1 - qab * x / qap;
  if (Math.abs(d) < tiny) d = tiny;
  d = 1 / d;
  let h = d;
  for (let m = 1; m <= 300; m++) {
    const m2 = 2 * m;
    let aa = m * (b - m) * x / ((qam + m2) * (a + m2));
    d = 1 + aa * d;
    if (Math.abs(d) < tiny) d = tiny;
    c = 1 + aa / c;
    if (Math.abs(c) < tiny) c = tiny;
    d = 1 / d;
    h *= d * c;
    aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
    d = 1 + aa * d;
    if (Math.abs(d) < tiny) d = tiny;
    c = 1 + aa / c;
    if (Math.abs(c) < tiny) c = tiny;
    d = 1 / d;
    const del = d * c;
    h *= del;
    if (Math.abs(del - 1) < 1e-14) break;
  }
  return h;
}

// Regularized incomplete beta function I_x(a, b).
function incompleteBeta(x, a, b) {
  if (x <= 0) return 0;
  if (x >= 1) return 1;
  const lbeta = logGamma(a + b) - logGamma(a) - logGamma(b);
  // Use the continued fraction on whichever side converges quickly.
  if (x < (a + 1) / (a + b + 2)) {
    return Math.exp(lbeta + a * Math.log(x) + b * Math.log(1 - x)) *
           betaContinuedFraction(x, a, b) / a;
  }
  return 1 - Math.exp(lbeta + b * Math.log(1 - x) + a * Math.log(x)) *
             betaContinuedFraction(1 - x, b, a) / b;
}

// Two-tailed probability P(|T| > t) for Student's t with v degrees of freedom.
function tTailProbability(t, v) {
  return incompleteBeta(v / (v + t * t), v / 2, 0.5);
}

// Quantile of Student's t: the value q with P(T <= q) = p, for p > 0.5.
// Equivalent to R's qt(p, v). Accurate to ~1e-6 over the range used here.
function tQuantile(p, v) {
  const alpha = 2 * (1 - p);
  let lo = 0;
  let hi = 1e3;
  for (let i = 0; i < 200; i++) {
    const mid = (lo + hi) / 2;
    if (tTailProbability(mid, v) > alpha) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return (lo + hi) / 2;
}

function printAnalysis(samples, xAxis, category) {
  if (samples.length === 0) {
    console.error('No benchmark results were reported.');
    process.exitCode = 1;
    return;
  }

  // The grouping variables were already validated against the first result.
  const confKeys = Object.keys(samples[0].conf);

  // Every variable that is neither a grouping variable nor constant across
  // the run is averaged over. Report them: an aggregated variable can hide a
  // real effect, and the reader has no other way to know it happened.
  const aggregated = confKeys.filter((key) => {
    if (key === xAxis || key === category) return false;
    const first = samples[0].conf[key];
    return samples.some((s) => s.conf[key] !== first);
  });

  // Group by the grouping variables. The full sample is retained, not just
  // the rate, so the aggregated variables can be accounted for below.
  const groups = new Map();
  for (const sample of samples) {
    const xValue = sample.conf[xAxis];
    const catValue = category === undefined ? undefined : sample.conf[category];
    const key = `${String(xValue)}\u0000${String(catValue)}`;
    let group = groups.get(key);
    if (group === undefined) {
      group = { xValue, catValue, rates: [], members: [] };
      groups.set(key, group);
    }
    group.rates.push(sample.rate);
    group.members.push(sample);
  }

  // Naming an aggregated variable is not enough: if it drives most of the
  // spread within a group, every interval below is reporting that variable
  // rather than the benchmark's noise, and no number of --runs will shrink
  // it. Quantify the share so the reader can tell those cases apart.
  const contamination = new Map();
  for (const variable of aggregated) {
    contamination.set(variable, varianceShare([...groups.values()], variable));
  }

  for (const variable of aggregated) {
    const share = contamination.get(variable);
    // Rounding 99.7% up to "100%" would claim the residual noise is zero.
    const percent = share === 1 ?
      '100' :
      (share >= 0.995 ? '>99' : (share * 100).toFixed(0));
    const suffix = Number.isNaN(share) ?
      '' :
      ` (explains ${percent}% of within-group variance)`;
    console.log(`aggregating variable: ${variable}${suffix}`);
  }

  // Aggregation that dominates the spread is the single most common reason
  // for uselessly wide intervals, and the usual reaction -- raising --runs --
  // cannot help, because the spread is a real effect of a variable that has
  // been averaged over rather than sampling noise.
  const dominant = aggregated.filter((v) => contamination.get(v) > 0.5);
  if (dominant.length > 0) {
    console.log('');
    const one = dominant.length === 1;
    printWrapped(
      `Note: ${dominant.join(', ')} ${one ? 'explains' : 'explain'} most of ` +
      `the spread within each group, so the intervals below describe ` +
      `${one ? 'that variable' : 'those variables'} rather than the ` +
      `benchmark's own noise, and more --runs will not narrow them. Pin ` +
      `${one ? 'it' : 'them'} with --set ${dominant[0]}=<value>, or pass as ` +
      `--category, for intervals that can be acted on.`,
    );
  }

  if (aggregated.length > 0) {
    console.log('');
  }

  const compare = (a, b) => {
    if (typeof a === 'number' && typeof b === 'number') return a - b;
    return String(a).localeCompare(String(b));
  };

  const scale = histogramScale(samples.map((s) => s.rate));

  const rows = [...groups.values()]
    .sort((a, b) => compare(a.xValue, b.xValue) ||
                    compare(a.catValue, b.catValue))
    .map((group) => {
      const rates = group.rates;
      const n = rates.length;
      const mean = rates.reduce((a, b) => a + b, 0) / n;

      // Confidence interval of the mean, matching scatter.R: the sample
      // standard error scaled by the t quantile at 97.5%. Computed from the
      // raw samples rather than the histogram, which stores bucketed values.
      // Undefined for a single sample, where there is no spread to estimate.
      let ci = NaN;
      if (n > 1) {
        const variance =
          rates.reduce((sum, r) => sum + (r - mean) ** 2, 0) / (n - 1);
        ci = Math.sqrt(variance / n) * tQuantile(0.975, n - 1);
      }

      // The histogram supplies the statistics that have no closed form here:
      // an exact-binomial interval on the median, and the shape measures used
      // to decide whether the mean is worth trusting.
      const histogram = createHistogram({ figures: 5 });
      for (const rate of rates) {
        histogram.record(Math.max(1, Math.round(rate * scale)));
      }

      const medianCI = histogram.percentileCI(50);
      const median = medianCI.value / scale;
      const skewness = n > 1 ? histogram.skewness : NaN;

      // A summary is suspect when the two estimates of the centre disagree by
      // more than the uncertainty claimed for one of them, or when the sample
      // is badly asymmetric. Either way the mean is being moved by the tail
      // rather than describing the bulk of the runs.
      const skewed = n > 1 &&
        (Math.abs(skewness) > 1 || Math.abs(median - mean) > ci);

      return {
        ...group,
        n,
        mean,
        ci,
        histogram,
        median,
        medianLower: medianCI.lower / scale,
        medianUpper: medianCI.upper / scale,
        skewness,
        skewed,
      };
    });

  // Values are rendered through short labels so that long or non-printable
  // configuration values cannot break the layout.
  const legend = assignLabels(rows, 'xValue', 'xLabel');
  if (category !== undefined) {
    legend.push(...assignLabels(rows, 'catValue', 'catLabel'));
  }

  printTable(rows, xAxis, category);

  if (showChart) {
    printChart(rows, xAxis, category);
  }

  printComparisons(rows, xAxis, category);
  printLegend(legend);

  const singleSample = rows.filter((r) => r.n < 2).length;
  if (singleSample > 0) {
    console.log('');
    printWrapped(
      `Note: ${singleSample} group${singleSample === 1 ? ' has' : 's have'} ` +
      `only one sample, so no confidence interval could be estimated. ` +
      `Use --runs 2 or higher.`,
    );
  }

  if (rows.some((r) => r.skewed)) {
    console.log('');
    printWrapped(
      `(!) marks groups where the median falls outside the mean's confidence ` +
      `interval, or the sample is strongly skewed (|skewness| > 1). For those ` +
      `rows the mean is being pulled by a few slow or fast runs, so the ` +
      `median and its interval describe the typical run better. This is ` +
      `usually GC or JIT tiering; more --runs will not necessarily make it ` +
      `go away.`,
    );
  }
}

// HdrHistogram records positive integers with a fixed number of significant
// figures, so precision is relative and large rates need no scaling at all.
// Small rates do: without scaling, anything below 1 op/s rounds to the same
// bucket and the group collapses to a single value. Scale up until the
// smallest rate carries enough digits for rounding to be irrelevant.
function histogramScale(rates) {
  let min = Infinity;
  let max = 0;
  for (const rate of rates) {
    if (rate > 0 && rate < min) min = rate;
    if (rate > max) max = rate;
  }
  if (!Number.isFinite(min) || max === 0) return 1;

  let scale = 1;
  while (min * scale < 1e6 && max * scale < 1e15) {
    scale *= 10;
  }
  return scale;
}

// Share of the within-group variance attributable to one aggregated
// variable, pooled over groups. This is a one-way eta squared: the spread
// between that variable's levels divided by the total spread inside the
// group. Computed per variable and independently, so when two aggregated
// variables are correlated their shares overlap and do not sum to one.
function varianceShare(groups, variable) {
  let between = 0;
  let total = 0;

  for (const group of groups) {
    const members = group.members;
    if (members.length < 2) continue;

    const groupMean =
      members.reduce((sum, s) => sum + s.rate, 0) / members.length;

    // Partition the group by the level of this variable.
    const levels = new Map();
    for (const sample of members) {
      const key = String(sample.conf[variable]);
      let level = levels.get(key);
      if (level === undefined) {
        level = { sum: 0, count: 0 };
        levels.set(key, level);
      }
      level.sum += sample.rate;
      level.count++;
    }

    // A variable with a single level inside this group explains nothing
    // here, but the group still contributes to the total spread.
    for (const level of levels.values()) {
      const levelMean = level.sum / level.count;
      between += level.count * (levelMean - groupMean) ** 2;
    }
    for (const sample of members) {
      total += (sample.rate - groupMean) ** 2;
    }
  }

  if (total === 0) return NaN;
  return Math.min(1, between / total);
}

// The smallest p-value the Mann-Whitney implementation can return for these
// sample sizes, found by giving it perfectly separated groups. Below a
// certain size that floor sits above 0.05, meaning no effect of any
// magnitude can be reported as significant; a non-significant result then
// says nothing at all. Derived from the implementation rather than the exact
// combinatorial bound because it uses a normal approximation.
const mannWhitneyFloors = new Map();
function mannWhitneyFloor(nA, nB) {
  const key = `${nA},${nB}`;
  let floor = mannWhitneyFloors.get(key);
  if (floor === undefined) {
    // Values must stay distinct after the histogram's 5-significant-figure
    // rounding, or they collapse into ties and the tie correction reports a
    // floor lower than the test can actually reach. The 10000 range is exact
    // at that precision, and the two runs cannot overlap.
    const low = createHistogram({ figures: 5 });
    const high = createHistogram({ figures: 5 });
    for (let i = 0; i < nA; i++) low.record(10000 + i);
    for (let i = 0; i < nB; i++) high.record(10000 + nA + i);
    floor = high.mannWhitneyTest(low).pValue;
    mannWhitneyFloors.set(key, floor);
  }
  return floor;
}

// Cliff's delta magnitude thresholds (Romano et al.), the conventional
// reading of the statistic.
function effectSizeLabel(delta) {
  const d = Math.abs(delta);
  if (d < 0.147) return 'negligible';
  if (d < 0.33) return 'small';
  if (d < 0.474) return 'medium';
  return 'large';
}

// The question scatter.js exists to answer is whether a parameter changes the
// rate, which the per-group table only hints at through overlapping intervals.
// Compare consecutive x-axis values directly, holding the category fixed.
// Mann-Whitney rather than a t-test because a parameter sweep routinely
// changes the shape and spread of the distribution, not just its centre.
function printComparisons(rows, xAxis, category) {
  const usable = rows.filter((row) => row.n > 1);
  if (usable.length < 2) return;

  // Partition by category so each series is a sweep over the x-axis alone.
  const series = new Map();
  for (const row of usable) {
    const key = String(row.catValue);
    if (!series.has(key)) series.set(key, []);
    series.get(key).push(row);
  }

  const lines = [];
  let floor = 0;
  for (const group of series.values()) {
    if (group.length < 2) continue;

    const entries = [];
    for (let i = 1; i < group.length; i++) {
      const previous = group[i - 1];
      const current = group[i];
      const { pValue } = current.histogram.mannWhitneyTest(previous.histogram);
      const delta = current.histogram.cliffsD(previous.histogram);
      const change = ((current.mean - previous.mean) / previous.mean) * 100;

      floor = Math.max(floor, mannWhitneyFloor(previous.n, current.n));

      // How the rate scales with the parameter over this step, which is the
      // complexity question these sweeps are usually run to answer. Reported
      // per step rather than as one fit across the whole sweep: a single
      // exponent spanning a curve that changes regime describes neither.
      let ratio = '';
      let exponent = '';
      if (typeof previous.xValue === 'number' &&
          typeof current.xValue === 'number' &&
          previous.xValue > 0 && current.xValue > 0 &&
          previous.xValue !== current.xValue &&
          previous.mean > 0 && current.mean > 0) {
        const xRatio = current.xValue / previous.xValue;
        const value = Math.log(current.mean / previous.mean) /
                      Math.log(xRatio);
        ratio = `${xRatio.toFixed(1)}x`;
        exponent = `${value >= 0 ? '+' : ''}${value.toFixed(2)}`;
      }

      entries.push({
        transition: `${previous.xLabel} -> ${current.xLabel}`,
        change: `${change >= 0 ? '+' : ''}${change.toFixed(2)}%`,
        ratio,
        exponent,
        pValue: pValue < 1e-4 ? pValue.toExponential(1) : pValue.toFixed(4),
        delta: `${delta >= 0 ? '+' : ''}${delta.toFixed(3)}`,
        label: effectSizeLabel(delta),
      });
    }

    if (entries.length > 0) {
      lines.push({
        heading: category === undefined ?
          null :
          `${category}=${group[0].catLabel}`,
        entries,
      });
    }
  }

  if (lines.length === 0) return;

  const all = lines.flatMap((l) => l.entries);
  const widths = {
    transition: Math.max(...all.map((e) => displayWidth(e.transition))),
    change: Math.max(...all.map((e) => displayWidth(e.change))),
    ratio: Math.max(...all.map((e) => displayWidth(e.ratio))),
    exponent: Math.max(...all.map((e) => displayWidth(e.exponent))),
    pValue: Math.max(...all.map((e) => displayWidth(e.pValue))),
    delta: Math.max(...all.map((e) => displayWidth(e.delta))),
  };
  const rpad = (s, n) => padTo(s, n, true);
  const pad = (s, n) => padTo(s, n, false);

  console.log('');
  console.log(`Change between consecutive ${xAxis} values ` +
              `(Mann-Whitney U, Cliff's delta):`);

  for (const { heading, entries } of lines) {
    console.log('');
    if (heading !== null) {
      console.log(`  ${heading}`);
    }
    for (const entry of entries) {
      console.log(
        `    ${pad(entry.transition, widths.transition)}` +
        `  ${rpad(entry.change, widths.change)}` +
        (widths.exponent > 0 ?
          `  ${rpad(entry.ratio, widths.ratio)}` +
          `  exponent=${rpad(entry.exponent, widths.exponent)}` :
          '') +
        `  p=${rpad(entry.pValue, widths.pValue)}` +
        `  delta=${rpad(entry.delta, widths.delta)} (${entry.label})`,
      );
    }
  }

  // A p-value has to be read against what the test could have produced. Below
  // a certain sample size the floor sits above the threshold, and "not
  // significant" then carries no information whatsoever.
  if (floor >= 0.05) {
    console.log('');
    printWrapped(
      `Warning: at this sample size the smallest p-value this test can ` +
      `produce is ${floor.toFixed(4)}, so no comparison above can reach ` +
      `significance however large the real effect is. Treat every p-value ` +
      `here as uninformative and raise --runs.`,
    );
  } else if (floor >= 0.005) {
    console.log('');
    printWrapped(
      `Note: at this sample size the smallest p-value this test can produce ` +
      `is ${floor.toFixed(4)}. A non-significant result above is therefore ` +
      `weak evidence of no change rather than evidence of none; raise ` +
      `--runs to strengthen it.`,
    );
  }
}

function formatRate(rate) {
  return rate.toLocaleString('en-US', {
    minimumFractionDigits: 1,
    maximumFractionDigits: 1,
  });
}

// Widest a single variable value may be before it is abbreviated. Benchmark
// values are usually short (`16`, `'ascii'`), but some are long enough to
// destroy the layout on their own, so there has to be a ceiling.
const kMaxValueWidth = 24;

// Widest a composed chart label may be, since it can hold two values.
const kMaxChartLabelWidth = 44;

// Widths are measured in code points, not UTF-16 code units: several
// benchmarks use emoji and CJK in their configurations, and counting those
// as two would misalign every column to their right.
function displayWidth(text) {
  return [...text].length;
}

function padTo(text, width, alignRight) {
  const padding = ' '.repeat(Math.max(0, width - displayWidth(text)));
  return alignRight ? padding + text : text + padding;
}

// Drop out the middle rather than the tail: benchmark values that share a
// long prefix are common, and a head-only truncation would render them
// identical. Slicing by code point rather than by index so that an astral
// character cannot be cut in half into a lone surrogate.
function truncateMiddle(text, maxWidth) {
  const chars = [...text];
  if (chars.length <= maxWidth) return text;
  const keep = maxWidth - 3;
  const head = Math.ceil(keep / 2);
  const tail = Math.floor(keep / 2);
  return `${chars.slice(0, head).join('')}...` +
         `${chars.slice(chars.length - tail).join('')}`;
}

// Configuration values are not guaranteed to be printable: strings may carry
// newlines or tabs (see benchmark/mime/mimetype-instantiation.js), which
// would otherwise split a table row across several lines. inspect() escapes
// them and quotes strings, matching how compare.js renders configurations.
function displayValue(value) {
  const text = typeof value === 'string' ? inspect(value) : String(value);
  return { text: truncateMiddle(text, kMaxValueWidth), full: text };
}

// Assigns a short, unique, printable label to every distinct value of one
// grouping variable, and collects the abbreviated ones for a legend so the
// full value is still recoverable from the output.
function assignLabels(rows, valueField, labelField) {
  const assigned = new Map();
  const used = new Map();
  const legend = [];

  for (const row of rows) {
    const key = String(row[valueField]);
    let label = assigned.get(key);

    if (label === undefined) {
      const { text, full } = displayValue(row[valueField]);
      // Two different values can abbreviate to the same text; keep them
      // distinguishable so the table cannot show one group twice.
      const collisions = used.get(text) ?? 0;
      used.set(text, collisions + 1);
      label = collisions === 0 ? text : `${text}~${collisions + 1}`;
      assigned.set(key, label);
      if (text !== full) legend.push({ label, full });
    }

    row[labelField] = label;
  }

  return legend;
}

// Notes interpolate variable names and sample sizes, so their length is not
// known when they are written. Wrapping here rather than hand-placing
// newlines, which overflow as soon as an interpolated value is longer than
// the author guessed.
function printWrapped(text, width = 76) {
  let line = '';
  for (const word of text.split(/\s+/)) {
    if (line === '') {
      line = word;
    } else if (displayWidth(line) + 1 + displayWidth(word) <= width) {
      line += ` ${word}`;
    } else {
      console.log(line);
      line = word;
    }
  }
  if (line !== '') console.log(line);
}

function printLegend(legend) {
  if (legend.length === 0) return;
  console.log('');
  console.log('Abbreviated values:');
  for (const { label, full } of legend) {
    console.log(`  ${label}`);
    console.log(`    = ${full}`);
  }
}

function printTable(rows, xAxis, category) {
  const header = [xAxis];
  if (category !== undefined) header.push(category);
  header.push('samples', 'rate', 'confidence.interval',
              'median', 'median.interval', '');

  // Numbers line up on the right, text reads better on the left. The grouping
  // columns can be either, so they follow the type of the underlying value.
  const alignRight = [typeof rows[0].xValue === 'number'];
  if (category !== undefined) {
    alignRight.push(typeof rows[0].catValue === 'number');
  }
  alignRight.push(true, true, true, true, true, false);

  const body = rows.map((row) => {
    const cells = [row.xLabel];
    if (category !== undefined) cells.push(row.catLabel);

    // The median interval is asymmetric, so it is shown as a relative range
    // rather than a single half-width.
    const medianInterval = row.n > 1 ?
      `[${(((row.medianLower - row.median) / row.median) * 100).toFixed(2)}%, ` +
      `+${(((row.medianUpper - row.median) / row.median) * 100).toFixed(2)}%]` :
      'NA';

    cells.push(
      String(row.n),
      formatRate(row.mean),
      // Absolute for parity with scatter.R, relative because that is what
      // tells you whether the rate beside it is worth reading. Kept in one
      // column so every column has a header of its own.
      Number.isNaN(row.ci) ?
        'NA' :
        `${formatRate(row.ci)} (±${((row.ci / row.mean) * 100).toFixed(2)}%)`,
      formatRate(row.median),
      medianInterval,
      row.skewed ? '(!)' : '',
    );
    return cells;
  });

  const widths = header.map((_, col) => Math.max(
    displayWidth(header[col]),
    ...body.map((cells) => displayWidth(cells[col])),
  ));

  const line = (cells) => cells
    .map((cell, col) => padTo(cell, widths[col], alignRight[col]))
    .join('  ')
    .trimEnd();

  console.log(line(header));
  for (const cells of body) {
    console.log(line(cells));
  }
}

function printChart(rows, xAxis, category) {
  if (rows.length === 0) return;

  const barWidth = 40;
  // Bars are drawn from zero so that the visual length is proportional to the
  // rate. A truncated axis would exaggerate small differences.
  let maxRate = 0;
  for (const row of rows) {
    const extent = row.mean + (Number.isNaN(row.ci) ? 0 : row.ci);
    if (extent > maxRate) maxRate = extent;
  }
  if (maxRate === 0) return;

  const labels = rows.map((row) => {
    const parts = [`${xAxis}=${row.xLabel}`];
    if (category !== undefined) parts.push(`${category}=${row.catLabel}`);
    return truncateMiddle(parts.join(' '), kMaxChartLabelWidth);
  });
  const labelWidth = Math.max(...labels.map(displayWidth));
  const rateWidth = Math.max(
    ...rows.map((r) => displayWidth(formatRate(r.mean))));

  const pad = (s, n) => padTo(s, n, false);
  const rpad = (s, n) => padTo(s, n, true);

  const axisRight = formatRate(maxRate);
  const indent = ' '.repeat(labelWidth + 2);

  console.log('');
  console.log('Rate in operations/second; longer is faster. \u2502 marks the ' +
              'mean and the');
  console.log('shaded band (\u2591) is its 95% confidence interval, so bars ' +
              'whose bands');
  console.log('overlap are not clearly different.');
  console.log('');
  console.log(
    `${indent}0` +
    `${' '.repeat(Math.max(1, barWidth - 1 - displayWidth(axisRight)))}` +
    `${axisRight}`,
  );
  console.log(`${indent}+${'-'.repeat(Math.max(0, barWidth - 2))}+`);

  let previousX;
  for (let i = 0; i < rows.length; i++) {
    const row = rows[i];

    // A blank line between x-axis values, so the categories being compared at
    // each point stay visually grouped.
    if (previousX !== undefined && row.xValue !== previousX) {
      console.log('');
    }
    previousX = row.xValue;

    const barEnd = (row.mean / maxRate) * barWidth;
    const ci = Number.isNaN(row.ci) ? 0 : row.ci;
    const ciLeft = ((row.mean - ci) / maxRate) * barWidth;
    const ciRight = ((row.mean + ci) / maxRate) * barWidth;

    // Without an explicit marker the mean is invisible, because the shaded
    // interval is drawn over the solid bar and straddles it.
    const meanCell = Math.min(barWidth - 1, Math.floor(barEnd));

    let bar = '';
    for (let x = 0; x < barWidth; x++) {
      const pos = x + 0.5; // Center of this character cell.
      if (x === meanCell) {
        bar += '\u2502'; // The mean itself.
      } else if (pos >= ciLeft && pos <= ciRight) {
        bar += '\u2591'; // Light shade marks the confidence interval.
      } else if (pos <= barEnd) {
        bar += '\u2588';
      } else {
        bar += ' ';
      }
    }

    console.log(
      `${pad(labels[i], labelWidth)}  ${bar}  ` +
      `${rpad(formatRate(row.mean), rateWidth)}`,
    );
  }
}
