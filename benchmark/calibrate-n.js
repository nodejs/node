'use strict';

const path = require('node:path');
const { fork } = require('node:child_process');
const fs = require('node:fs');
const { styleText } = require('node:util');

const DEFAULT_RUNS = 30;        // Number of runs for each n value
const CV_THRESHOLD = 0.05;      // 5% coefficient of variation threshold
const MAX_N_INCREASE = 6;       // Maximum number of times to increase n (10**6)
const INCREASE_FACTOR = 10;     // Factor by which to increase n

const args = process.argv.slice(2);
if (args.length === 0) {
  console.log(`
Usage: node calibrate-n.js [options] <benchmark_path>

Options:
  --runs=N           Number of runs for each n value (default: ${DEFAULT_RUNS})
  --cv-threshold=N   Target coefficient of variation threshold (default: ${CV_THRESHOLD})
  --max-increases=N  Maximum number of n increases to try (default: ${MAX_N_INCREASE})
  --start-n=N        Initial n value to start with (default: autodetect)
  --increase=N       Factor by which to increase n (default: ${INCREASE_FACTOR})

Example:
  node calibrate-n.js buffers/buffer-compare.js
  node calibrate-n.js --runs=10 --cv-threshold=0.02 buffers/buffer-compare.js
  `);
  process.exit(1);
}

// Extract options
let benchmarkPath;
let runs = DEFAULT_RUNS;
let cvThreshold = CV_THRESHOLD;
let maxIncreases = MAX_N_INCREASE;
let startN = 10;
let increaseFactor = INCREASE_FACTOR;

for (const arg of args) {
  if (arg.startsWith('--runs=')) {
    runs = parseInt(arg.substring(7), 10);
  } else if (arg.startsWith('--cv-threshold=')) {
    cvThreshold = parseFloat(arg.substring(14));
  } else if (arg.startsWith('--max-increases=')) {
    maxIncreases = parseInt(arg.substring(15), 10);
    if (isNaN(maxIncreases)) {
      console.error(`Error: Invalid value for --max-increases. Using default: ${MAX_N_INCREASE}`);
      maxIncreases = MAX_N_INCREASE;
    }
  } else if (arg.startsWith('--start-n=')) {
    startN = parseInt(arg.substring(10), 10);
    if (isNaN(startN)) {
      console.error(`Error: Invalid value for --start-n. Using default: 10`);
      startN = 10;
    }
  } else if (arg.startsWith('--increase=')) {
    increaseFactor = parseInt(arg.substring(11), 10);
    if (isNaN(increaseFactor)) {
      console.error(`Error: Invalid value for --increase. Using default: ${INCREASE_FACTOR}`);
      increaseFactor = INCREASE_FACTOR;
    }
  } else {
    benchmarkPath = arg;
  }
}

if (!benchmarkPath) {
  console.error('Error: No benchmark path specified');
  process.exit(1);
}

const fullBenchmarkPath = path.resolve(benchmarkPath);
if (!fs.existsSync(fullBenchmarkPath)) {
  console.error(`Error: Benchmark file not found: ${fullBenchmarkPath}`);
  process.exit(1);
}

function calculateStats(values) {
  const mean = values.reduce((sum, val) => sum + val, 0) / values.length;

  const squaredDiffs = values.map((val) => {
    const diff = val - mean;
    const squared = diff ** 2;
    return squared;
  });

  const variance = squaredDiffs.reduce((sum, val) => sum + val, 0) / values.length;
  const stdDev = Math.sqrt(variance);
  const cv = stdDev / mean;

  return { mean, stdDev, cv, variance };
}

function runBenchmark(n) {
  return new Promise((resolve, reject) => {
    const child = fork(
      fullBenchmarkPath,
      [`n=${n}`],
      { stdio: ['inherit', 'pipe', 'inherit', 'ipc'] },
    );

    const results = [];
    child.on('message', (data) => {
      if (data.type === 'report' && data.rate && data.conf) {
        results.push({
          rate: data.rate,
          conf: data.conf,
        });
      }
    });

    child.on('close', (code) => {
      if (code !== 0) {
        reject(new Error(`Benchmark exited with code ${code}`));
      } else {
        resolve(results);
      }
    });
  });
}

async function main(n = startN) {
  let increaseCount = 0;
  let bestN = n;
  let bestCV = Infinity;
  let bestGroupStats = null;

  console.log(`
--------------------------------------------------------
Benchmark: ${benchmarkPath}
--------------------------------------------------------
What we are trying to find: The optimal number of iterations (n)
that produces consistent benchmark results without wasting time.

How it works:
1. Run the benchmark multiple times with a specific n value
2. Group results by configuration
3. If overall CV is above 5% or any configuration has CV above 10%, increase n and try again

Configuration:
- Starting n: ${n.toLocaleString()} iterations
- Runs per n value: ${runs}
- Target CV threshold: ${cvThreshold * 100}% (lower CV = more stable results)
- Max increases: ${maxIncreases}
- Increase factor: ${increaseFactor}x`);

  while (increaseCount < maxIncreases) {
    console.log(`\nTesting with n=${n}:`);

    const resultsData = [];
    for (let i = 0; i < runs; i++) {
      const results = await runBenchmark(n);
      // Each run might return multiple results (one per configuration)
      if (Array.isArray(results) && results.length > 0) {
        resultsData.push(...results);
      } else if (results) {
        resultsData.push(results);
      }
      process.stdout.write('.');
    }
    process.stdout.write('\n');

    const groupedResults = {};
    resultsData.forEach((result) => {
      if (!result || !result.conf) return;

      const confKey = JSON.stringify(result.conf);
      groupedResults[confKey] ||= {
        conf: result.conf,
        rates: [],
      };

      groupedResults[confKey].rates.push(result.rate);
    });

    const groupStats = [];
    for (const [confKey, group] of Object.entries(groupedResults)) {
      console.log(`\nConfiguration: ${JSON.stringify(group.conf)}`);

      const stats = calculateStats(group.rates);
      console.log(`  CV: ${(stats.cv * 100).toFixed(2)}% (lower values mean more stable results)`);

      const isStable = stats.cv <= cvThreshold;
      console.log(`  Stability: ${isStable ?
        styleText(['bold', 'green'], '✓ Stable') :
        styleText(['bold', 'red'], '✗ Unstable')}`);

      groupStats.push({
        confKey,
        stats,
        isStable,
      });
    }

    if (groupStats.length > 0) {
      // Check if any configuration has CV > 10% (too unstable)
      const tooUnstableConfigs = groupStats.filter((g) => g.stats.cv > 0.10);

      const avgCV = groupStats.reduce((sum, g) => sum + g.stats.cv, 0) / groupStats.length;
      console.log(`\nOverall average CV: ${(avgCV * 100).toFixed(2)}%`);

      const isOverallStable = avgCV < CV_THRESHOLD;
      const hasVeryUnstableConfigs = tooUnstableConfigs.length > 0;

      // Check if overall CV is below CV_THRESHOLD and no configuration has CV > 10%
      if (isOverallStable && !hasVeryUnstableConfigs) {
        console.log(styleText(['bold', 'green'], `  ✓ Overall CV is below 5% and no configuration has CV above 10%`));
      } else {
        if (!isOverallStable) {
          console.log(styleText(['bold', 'red'], `  ✗ Overall CV (${(avgCV * 100).toFixed(2)}%) is above 5%`));
        }
        if (hasVeryUnstableConfigs) {
          console.log(styleText(['bold', 'red'], `  ✗ ${tooUnstableConfigs.length} configuration(s) have CV above 10%`));
        }
      }

      if (avgCV < bestCV || !bestGroupStats) {
        bestN = n;
        bestCV = avgCV;

        bestGroupStats = [];
        for (const group of Object.values(groupedResults)) {
          if (group.rates.length >= 3) {
            const stats = calculateStats(group.rates);
            bestGroupStats.push({
              conf: group.conf,
              stats: stats,
              isStable: stats.cv <= 0.10,
            });
          }
        }
        console.log(`  → New best n: ${n} with average CV: ${(avgCV * 100).toFixed(2)}%`);
      } else {
        console.log(`  → Current best n remains: ${bestN} with average CV: ${(bestCV * 100).toFixed(2)}%`);
      }
    }

    // Check if we've reached acceptable stability based on new criteria
    // 1. Overall CV should be below CV_THRESHOLD
    // 2. No configuration should have a CV greater than 10%
    const avgCV = groupStats.length > 0 ?
      groupStats.reduce((sum, g) => sum + g.stats.cv, 0) / groupStats.length : Infinity;
    const hasUnstableConfig = groupStats.some((g) => g.stats.cv > 0.10);
    const isOverallStable = avgCV < CV_THRESHOLD;

    if (isOverallStable && !hasUnstableConfig) {
      console.log(`\n✓ Found optimal n=${n} (Overall CV=${(avgCV * 100).toFixed(2)}% < 5% and no configuration has CV > 10%)`);
      console.log('\nFinal CV for each configuration:');
      groupStats.forEach((g) => {
        console.log(`  ${JSON.stringify(groupedResults[g.confKey].conf)}: ${(g.stats.cv * 100).toFixed(2)}%`);
      });

      return n;
    }

    increaseCount++;
    n *= increaseFactor;
  }

  if (increaseCount >= maxIncreases) {
    const finalAvgCV = bestGroupStats && bestGroupStats.length > 0 ?
      bestGroupStats.reduce((sum, g) => sum + g.stats.cv, 0) / bestGroupStats.length : Infinity;

    console.log(`Maximum number of increases (${maxIncreases}) reached without achieving target stability`);
    console.log(`Best n found: ${bestN} with average CV=${(finalAvgCV * 100).toFixed(2)}%`);
    console.log(`\nCV by configuration at best n:`);

    if (bestGroupStats) {
      bestGroupStats.forEach((g) => {
        if (g.conf) {
          console.log(`  ${JSON.stringify(g.conf)}: ${(g.stats.cv * 100).toFixed(2)}%`);
          if (g.stats.cv > cvThreshold) {
            console.log(`    ⚠️ This configuration is above the target threshold of ${cvThreshold * 100}%`);
          }
        }
      });
    }
  }

  console.log(`
Recommendation: You might want to try increasing --max-increases to
continue testing with larger n values, or adjust --cv-threshold to
accept the current best result, or investigate if specific configurations
are contributing to instability.`);
  return bestN;
}

main().catch((err) => {
  console.error('Error:', err);
  process.exit(1);
});
