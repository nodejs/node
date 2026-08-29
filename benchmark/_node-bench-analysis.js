'use strict';

const { createHistogram } = require('node:perf_hooks');
const { inspect } = require('node:util');

function createRateHistogram(rates, scale, figures) {
  const histogram = createHistogram({ figures });
  for (const rate of rates) {
    const value = Math.max(1, Math.round(rate * scale));
    if (!Number.isSafeInteger(value)) {
      throw new RangeError('Benchmark rate is too large for the histogram scale');
    }
    histogram.record(value);
  }
  return histogram;
}

function holmAdjust(pValues) {
  const order = pValues
    .map((p, index) => ({ index, p }))
    .sort((a, b) => a.p - b.p);
  const adjusted = new Array(order.length);
  let running = 0;
  for (let index = 0; index < order.length; index++) {
    running = Math.max(
      running,
      Math.min(1, (order.length - index) * order[index].p),
    );
    adjusted[order[index].index] = running;
  }
  return adjusted;
}

function thresholdPValue(oldRates, newHistogram, scale, maxRegression) {
  const factor = 1 - maxRegression / 100;
  if (factor <= 0) return 1;
  const thresholdHistogram = createRateHistogram(
    oldRates.map((rate) => rate * factor), scale, 3);
  const result = thresholdHistogram.welchTest(newHistogram);
  if (Number.isNaN(result.pValue)) return 1;
  return result.tStatistic > 0 ?
    result.pValue / 2 : 1 - result.pValue / 2;
}

function isRegressionFailure(row, maxRegression) {
  return row.pThresholdAdjusted < 0.05 &&
         row.improvement + row.ci95 < -maxRegression;
}

function analyzeCompare(samples, scale, maxRegression) {
  const groups = new Map();
  for (const sample of samples) {
    let group = groups.get(sample.identity);
    if (group === undefined) {
      const suffix = sample.configuration === '' ?
        '' : ` ${sample.configuration}`;
      group = {
        name: `${sample.name}${suffix}`,
        new: [],
        old: [],
      };
      groups.set(sample.identity, group);
    }
    group[sample.binary].push(sample.rate);
  }

  const rows = [];
  let skipped = 0;
  for (const { name, old: oldRates, new: newRates } of groups.values()) {
    if (oldRates.length < 2 || newRates.length < 2) {
      skipped++;
      continue;
    }

    const oldHistogram = createRateHistogram(oldRates, scale, 3);
    const newHistogram = createRateHistogram(newRates, scale, 3);
    const oldMean = oldRates.reduce((sum, rate) => sum + rate, 0) /
      oldRates.length;
    const newMean = newRates.reduce((sum, rate) => sum + rate, 0) /
      newRates.length;
    const improvement = ((newMean - oldMean) / oldMean) * 100;
    const w95 = oldHistogram.welchTest(newHistogram, { confidence: 0.95 });
    const w99 = oldHistogram.welchTest(newHistogram, { confidence: 0.99 });
    const w999 = oldHistogram.welchTest(newHistogram, { confidence: 0.999 });
    let stars = '';
    if (w95.pValue < 0.001) stars = '***';
    else if (w95.pValue < 0.01) stars = ' **';
    else if (w95.pValue < 0.05) stars = '  *';
    const ciPercent = (result) => {
      const half = (result.confidenceInterval.upper -
                    result.confidenceInterval.lower) / 2;
      return (half / (oldMean * scale)) * 100;
    };
    const row = {
      ci95: ciPercent(w95),
      ci99: ciPercent(w99),
      ci999: ciPercent(w999),
      improvement,
      name,
      pValue: Number.isNaN(w95.pValue) ? 1 : w95.pValue,
      stars,
    };
    if (maxRegression !== undefined) {
      row.pThreshold = thresholdPValue(
        oldRates, newHistogram, scale, maxRegression);
    }
    rows.push(row);
  }

  const adjusted = holmAdjust(rows.map(({ pValue }) => pValue));
  const thresholdAdjusted = maxRegression === undefined ? null :
    holmAdjust(rows.map(({ pThreshold }) => pThreshold));
  let underpowered = 0;
  for (let index = 0; index < rows.length; index++) {
    const row = rows[index];
    row.pAdjusted = adjusted[index];
    if (thresholdAdjusted !== null) {
      row.pThresholdAdjusted = thresholdAdjusted[index];
    }
    row.inconclusive = maxRegression > 0 &&
                       row.stars.trim() === '' &&
                       row.ci95 > maxRegression;
    if (row.inconclusive) underpowered++;
  }

  const output = [];
  const maxNameLength = rows.reduce(
    (maximum, { name }) => Math.max(maximum, name.length), 0);
  const pad = (value, length) =>
    value + ' '.repeat(Math.max(0, length - value.length));
  const padStart = (value, length) =>
    ' '.repeat(Math.max(0, length - value.length)) + value;
  output.push(`${pad('', maxNameLength)}  confidence` +
              '   improvement   accuracy (*)    (**)   (***)');
  for (const row of rows) {
    const improvement =
      `${row.improvement >= 0 ? '+' : ''}${row.improvement.toFixed(2)} %`;
    output.push(
      `${pad(row.name, maxNameLength)}  ${pad(row.stars, 10)}` +
      `  ${padStart(improvement, 11)}` +
      `   ±${row.ci95.toFixed(2)}%` +
      `  ±${row.ci99.toFixed(2)}%` +
      `  ±${row.ci999.toFixed(2)}%` +
      `${row.inconclusive ? '  (inconclusive)' : ''}`,
    );
  }

  if (skipped > 0) {
    output.push('');
    output.push(
      `Note: ${skipped} configuration${skipped === 1 ? ' was' : 's were'}` +
      ' skipped because Welch\'s t-test requires at least 2 samples per' +
      ' binary. Use --runs 2 or higher.',
    );
  }
  printCompareChart(output, rows, maxNameLength);

  output.push('');
  output.push(
    `Rates were scaled by ${scale}x into HdrHistogram (3 significant figures).`,
    'Use --scale to adjust precision if needed.',
    '',
  );
  const significant = rows.filter(({ pAdjusted }) => pAdjusted < 0.05).length;
  output.push(
    'The confidence markers above are per-benchmark and uncorrected. ' +
    `After Holm-Bonferroni correction across ${rows.length} comparison` +
    `${rows.length === 1 ? '' : 's'}, ${significant} remain` +
    `${significant === 1 ? 's' : ''} significant at 5%.`,
  );
  if (maxRegression !== undefined) {
    output.push(
      `For --max-regression, one-sided p-values against the ` +
      `${maxRegression}% threshold were corrected separately.`,
    );
  }

  if (maxRegression > 0 && underpowered > 0) {
    output.push('');
    output.push(
      `Note: ${underpowered} of ${rows.length} comparison` +
      `${rows.length === 1 ? '' : 's'} could not resolve an effect as small ` +
      `as ${maxRegression}% and are marked (inconclusive). Raise --runs to ` +
      'narrow their confidence intervals.',
    );
  }

  const failures = maxRegression !== undefined ?
    rows.filter((row) => isRegressionFailure(row, maxRegression)) : [];
  if (failures.length > 0) {
    output.push('');
    output.push(
      `FAIL: ${failures.length} benchmark${failures.length === 1 ? '' : 's'}` +
      ` regressed by more than ${maxRegression}% (the 95% interval excludes ` +
      `the threshold and its one-sided test is family-wise corrected across ` +
      `${rows.length} comparisons):`,
    );
    for (const failure of failures) {
      output.push(
        `  ${failure.name}  ${failure.improvement.toFixed(2)}% ` +
        `(95% CI up to ${(failure.improvement + failure.ci95).toFixed(2)}%, ` +
        `adjusted threshold p=` +
        `${failure.pThresholdAdjusted.toExponential(2)})`,
      );
    }
  }

  return {
    failed: failures.length > 0,
    output: `${output.join('\n')}\n`,
    rows,
  };
}

function printCompareChart(output, rows, maxNameLength) {
  if (rows.length === 0) return;
  const width = 40;
  const halfWidth = width / 2;
  let maximum = 0;
  for (const row of rows) {
    maximum = Math.max(maximum, Math.abs(row.improvement) + row.ci95);
  }
  if (maximum === 0) maximum = 1;
  const left = `-${maximum.toFixed(1)}%`;
  const right = `+${maximum.toFixed(1)}%`;
  const centerLabel = '0%';
  const labelPadding = maxNameLength + 5;
  output.push('');
  output.push(
    ' '.repeat(labelPadding) + left +
    ' '.repeat(Math.max(
      0, halfWidth - left.length - Math.floor(centerLabel.length / 2))) +
    centerLabel +
    ' '.repeat(Math.max(
      0, halfWidth - Math.ceil(centerLabel.length / 2) - right.length)) +
    right,
  );
  for (const row of rows) {
    const center = halfWidth;
    const result = center + (row.improvement / maximum) * halfWidth;
    const lower = center +
      ((row.improvement - row.ci95) / maximum) * halfWidth;
    const upper = center +
      ((row.improvement + row.ci95) / maximum) * halfWidth;
    let bar = '';
    for (let index = 0; index < width; index++) {
      const position = index + 0.5;
      if (index === Math.floor(center)) {
        bar += '|';
      } else if ((row.improvement >= 0 &&
                  position > center && position <= result) ||
                 (row.improvement < 0 &&
                  position < center && position >= result)) {
        bar += row.stars === '' ? '▓' : '█';
      } else if (position >= lower && position <= upper) {
        bar += '░';
      } else {
        bar += ' ';
      }
    }
    const label = `${row.improvement >= 0 ? '+' : ''}` +
      `${row.improvement.toFixed(2)}%`;
    output.push(
      `${row.name.padEnd(maxNameLength)}  ${bar}  ${label} ${row.stars.trim()}`,
    );
  }
}

function histogramScale(rates) {
  let minimum = Infinity;
  let maximum = 0;
  for (const rate of rates) {
    if (rate > 0 && rate < minimum) minimum = rate;
    if (rate > maximum) maximum = rate;
  }
  if (!Number.isFinite(minimum) || maximum === 0) return 1;
  let scale = 1;
  while (minimum * scale < 1e6 && maximum * scale < 1e15) scale *= 10;
  return scale;
}

function validateScatterParameters(samples, xAxis, category) {
  if (category !== undefined && category === xAxis) {
    throw new Error('--xaxis and --category must name different parameters');
  }
  for (const key of [xAxis, category]) {
    if (key === undefined) continue;
    if (samples.some(({ params }) =>
      !Object.hasOwn(params, key))) {
      const available = [...new Set(samples.flatMap(
        ({ params }) => Object.keys(params)))].sort();
      throw new Error(
        `The variable '${key}' is not present in every configuration. ` +
        `Available variables: ${available.join(', ')}`,
      );
    }
  }
}

function analyzeScatter(samples, xAxis, category, showChart) {
  validateScatterParameters(samples, xAxis, category);

  const parameterNames = [...new Set(samples.flatMap(
    ({ params }) => Object.keys(params)))];
  const aggregated = parameterNames.filter((name) => {
    if (name === xAxis || name === category) return false;
    const first = samples[0].params[name];
    return samples.some(({ params }) => params[name] !== first);
  });
  const groups = new Map();
  for (const sample of samples) {
    const xValue = sample.params[xAxis];
    const categoryValue = category === undefined ?
      undefined : sample.params[category];
    const key = valueKey([xValue, categoryValue]);
    let group = groups.get(key);
    if (group === undefined) {
      group = {
        categoryValue,
        members: [],
        observations: new Map(),
        xValue,
      };
      groups.set(key, group);
    }
    group.members.push(sample);
    let rates = group.observations.get(sample.observation);
    if (rates === undefined) {
      rates = [];
      group.observations.set(sample.observation, rates);
    }
    rates.push(sample.rate);
  }
  for (const group of groups.values()) {
    group.processRates = [...group.observations].map(([observation, rates]) => ({
      observation,
      rate: rates.reduce((sum, rate) => sum + rate, 0) / rates.length,
    }));
    group.rates = group.processRates.map(({ rate }) => rate);
  }

  const scale = histogramScale(samples.map(({ rate }) => rate));
  const compareValues = (a, b) => {
    if (typeof a === 'number' && typeof b === 'number') return a - b;
    return String(a).localeCompare(String(b));
  };
  const rows = [...groups.values()]
    .sort((a, b) => compareValues(a.xValue, b.xValue) ||
                    compareValues(a.categoryValue, b.categoryValue))
    .map((group) => {
      const histogram = createRateHistogram(group.rates, scale, 5);
      const count = group.rates.length;
      const mean = group.rates.reduce((sum, rate) => sum + rate, 0) / count;
      const meanInterval = histogram.meanCI();
      const confidenceInterval = count > 1 ?
        (meanInterval.upper - meanInterval.lower) / (2 * scale) : NaN;
      const medianInterval = histogram.percentileCI(50);
      const median = rawMedian(group.rates);
      const skewed = count > 1 &&
        (Math.abs(histogram.skewness) > 1 ||
         Math.abs(median - mean) > confidenceInterval);
      return {
        ...group,
        confidenceInterval,
        count,
        histogram,
        mean,
        median,
        medianLower: medianInterval.lower / scale,
        medianUpper: medianInterval.upper / scale,
        skewed,
      };
    });

  const legend = assignLabels(rows, 'xValue', 'xLabel');
  if (category !== undefined) {
    legend.push(...assignLabels(rows, 'categoryValue', 'categoryLabel'));
  }
  const output = [];
  const contamination = new Map();
  for (const variable of aggregated) {
    const share = varianceShare([...groups.values()], variable);
    contamination.set(variable, share);
    const percent = share === 1 ?
      '100' : (share >= 0.995 ? '>99' : (share * 100).toFixed(0));
    const suffix = Number.isNaN(share) ?
      '' : ` (explains ${percent}% of within-group variance)`;
    output.push(`aggregating variable: ${variable}${suffix}`);
  }
  const dominant = aggregated.filter(
    (variable) => contamination.get(variable) > 0.5);
  if (dominant.length > 0) {
    output.push('');
    wrapOutput(
      output,
      `${dominant.join(', ')} ${dominant.length === 1 ? 'explains' : 'explain'} ` +
      'most of the spread within each group. Pin the parameter or use it as ' +
      '--category; increasing --runs will not remove this source of variance.',
    );
  }
  if (aggregated.length > 0) output.push('');
  printScatterTable(output, rows, xAxis, category);
  if (showChart) printScatterChart(output, rows, xAxis, category);
  printScatterComparisons(output, rows, xAxis, category);
  if (legend.length > 0) {
    output.push('', 'Abbreviated values:');
    for (const { full, label } of legend) {
      output.push(`  ${label}`, `    = ${full}`);
    }
  }
  const singleSample = rows.filter(({ count }) => count < 2).length;
  if (singleSample > 0) {
    output.push('');
    wrapOutput(
      output,
      `Note: ${singleSample} group${singleSample === 1 ? ' has' : 's have'} ` +
      'only one sample, so no confidence interval could be estimated. Use ' +
      '--runs 2 or higher.',
    );
  }
  if (rows.some(({ skewed }) => skewed)) {
    output.push('');
    wrapOutput(
      output,
      '(!) marks groups where the median falls outside the mean confidence ' +
      'interval or the sample is strongly skewed. The median and its interval ' +
      'describe the typical run better for those groups.',
    );
  }
  return `${output.join('\n')}\n`;
}

function rawMedian(rates) {
  const sorted = [...rates].sort((a, b) => a - b);
  const middle = Math.floor(sorted.length / 2);
  return sorted.length % 2 === 0 ?
    (sorted[middle - 1] + sorted[middle]) / 2 : sorted[middle];
}

function valueKey(value) {
  return JSON.stringify(value, (_, item) => {
    if (typeof item === 'bigint') return { bigint: String(item) };
    return item;
  });
}

function varianceShare(groups, variable) {
  let between = 0;
  let total = 0;
  for (const group of groups) {
    if (group.members.length < 2) continue;
    const mean = group.members.reduce(
      (sum, sample) => sum + sample.rate, 0) / group.members.length;
    const levels = new Map();
    for (const sample of group.members) {
      const key = valueKey(sample.params[variable]);
      let level = levels.get(key);
      if (level === undefined) {
        level = { count: 0, sum: 0 };
        levels.set(key, level);
      }
      level.count++;
      level.sum += sample.rate;
    }
    for (const level of levels.values()) {
      between += level.count * ((level.sum / level.count) - mean) ** 2;
    }
    for (const sample of group.members) total += (sample.rate - mean) ** 2;
  }
  return total === 0 ? NaN : Math.min(1, between / total);
}

function effectSizeLabel(delta) {
  const absolute = Math.abs(delta);
  if (absolute < 0.147) return 'negligible';
  if (absolute < 0.33) return 'small';
  if (absolute < 0.474) return 'medium';
  return 'large';
}

const mannWhitneyFloors = new Map();
function mannWhitneyFloor(firstCount, secondCount) {
  const key = `${firstCount},${secondCount}`;
  let floor = mannWhitneyFloors.get(key);
  if (floor !== undefined) return floor;
  const low = createHistogram({ figures: 5 });
  const high = createHistogram({ figures: 5 });
  for (let index = 0; index < firstCount; index++) low.record(10000 + index);
  for (let index = 0; index < secondCount; index++) {
    high.record(10000 + firstCount + index);
  }
  floor = high.mannWhitneyTest(low).pValue;
  mannWhitneyFloors.set(key, floor);
  return floor;
}

function printScatterComparisons(output, rows, xAxis, category) {
  const usable = rows.filter(({ count }) => count > 1);
  if (usable.length < 2) return;
  const series = new Map();
  for (const row of usable) {
    const key = valueKey(row.categoryValue);
    if (!series.has(key)) series.set(key, []);
    series.get(key).push(row);
  }
  const sections = [];
  let floor = 0;
  for (const group of series.values()) {
    if (group.length < 2) continue;
    const entries = [];
    for (let index = 1; index < group.length; index++) {
      const previous = group[index - 1];
      const current = group[index];
      // Configurations in one file share a process. Split consecutive groups
      // across disjoint outer-process sets so the unpaired test does not treat
      // correlated observations as independent.
      const parity = index % 2;
      const previousRates = previous.processRates
        .filter(({ observation }) => observation % 2 === parity)
        .map(({ rate }) => rate);
      const currentRates = current.processRates
        .filter(({ observation }) => observation % 2 !== parity)
        .map(({ rate }) => rate);
      if (previousRates.length === 0 || currentRates.length === 0) continue;
      const comparisonScale = histogramScale([
        ...previousRates,
        ...currentRates,
      ]);
      const previousHistogram =
        createRateHistogram(previousRates, comparisonScale, 5);
      const currentHistogram =
        createRateHistogram(currentRates, comparisonScale, 5);
      const { pValue } = currentHistogram.mannWhitneyTest(previousHistogram);
      const delta = currentHistogram.cliffsD(previousHistogram);
      const previousMean = previousRates.reduce(
        (sum, rate) => sum + rate, 0) / previousRates.length;
      const currentMean = currentRates.reduce(
        (sum, rate) => sum + rate, 0) / currentRates.length;
      const change = ((currentMean - previousMean) / previousMean) * 100;
      floor = Math.max(
        floor, mannWhitneyFloor(previousRates.length, currentRates.length));
      let ratio = '';
      let exponent = '';
      if (typeof previous.xValue === 'number' &&
          typeof current.xValue === 'number' &&
          previous.xValue > 0 && current.xValue > 0 &&
          previous.xValue !== current.xValue &&
          previousMean > 0 && currentMean > 0) {
        const xRatio = current.xValue / previous.xValue;
        const value = Math.log(currentMean / previousMean) / Math.log(xRatio);
        ratio = `${xRatio.toFixed(1)}x`;
        exponent = `${value >= 0 ? '+' : ''}${value.toFixed(2)}`;
      }
      entries.push(
        `    ${previous.xLabel} -> ${current.xLabel}` +
        `  ${change >= 0 ? '+' : ''}${change.toFixed(2)}%` +
        (exponent === '' ? '' : `  ${ratio}  exponent=${exponent}`) +
        `  p=${pValue < 1e-4 ? pValue.toExponential(1) : pValue.toFixed(4)}` +
        `  delta=${delta >= 0 ? '+' : ''}${delta.toFixed(3)}` +
        ` (${effectSizeLabel(delta)})`,
      );
    }
    if (entries.length > 0) {
      sections.push({
        entries,
        heading: category === undefined ?
          undefined : `${category}=${group[0].categoryLabel}`,
      });
    }
  }
  if (sections.length === 0) return;
  output.push('', `Change between consecutive ${xAxis} values ` +
                  `(Mann-Whitney U on disjoint process sets, Cliff's delta):`);
  for (const section of sections) {
    output.push('');
    if (section.heading !== undefined) output.push(`  ${section.heading}`);
    output.push(...section.entries);
  }
  if (floor >= 0.05) {
    output.push('');
    wrapOutput(
      output,
      `Warning: at this sample size the smallest p-value this test can ` +
      `produce is ${floor.toFixed(4)}, so no comparison above can reach ` +
      'significance. Raise --runs.',
    );
  } else if (floor >= 0.005) {
    output.push('');
    wrapOutput(
      output,
      `Note: at this sample size the smallest p-value this test can produce ` +
      `is ${floor.toFixed(4)}. Raise --runs to strengthen non-significant ` +
      'results.',
    );
  }
}

function formatRate(rate) {
  return rate.toLocaleString('en-US', {
    maximumFractionDigits: 1,
    minimumFractionDigits: 1,
  });
}

function displayWidth(value) {
  return [...value].length;
}

function pad(value, width, right) {
  const padding = ' '.repeat(Math.max(0, width - displayWidth(value)));
  return right ? padding + value : value + padding;
}

function truncateMiddle(value, maximum = 24) {
  const characters = [...value];
  if (characters.length <= maximum) return value;
  const retained = maximum - 3;
  const head = Math.ceil(retained / 2);
  const tail = Math.floor(retained / 2);
  return `${characters.slice(0, head).join('')}...` +
         characters.slice(-tail).join('');
}

function assignLabels(rows, valueName, labelName) {
  const assigned = new Map();
  const used = new Map();
  const legend = [];
  for (const row of rows) {
    const key = valueKey(row[valueName]);
    let label = assigned.get(key);
    if (label === undefined) {
      const full = typeof row[valueName] === 'string' ?
        inspect(row[valueName]) : String(row[valueName]);
      const abbreviated = truncateMiddle(full);
      const collisions = used.get(abbreviated) ?? 0;
      used.set(abbreviated, collisions + 1);
      label = collisions === 0 ?
        abbreviated : `${abbreviated}~${collisions + 1}`;
      assigned.set(key, label);
      if (label !== full) legend.push({ full, label });
    }
    row[labelName] = label;
  }
  return legend;
}

function printScatterTable(output, rows, xAxis, category) {
  const header = [xAxis];
  if (category !== undefined) header.push(category);
  header.push(
    'samples', 'rate', 'confidence.interval', 'median', 'median.interval', '');
  const body = rows.map((row) => {
    const values = [row.xLabel];
    if (category !== undefined) values.push(row.categoryLabel);
    const medianInterval = row.count > 1 ?
      `[${(((row.medianLower - row.median) / row.median) * 100).toFixed(2)}%, ` +
      `+${(((row.medianUpper - row.median) / row.median) * 100).toFixed(2)}%]` :
      'NA';
    values.push(
      String(row.count),
      formatRate(row.mean),
      Number.isNaN(row.confidenceInterval) ?
        'NA' :
        `${formatRate(row.confidenceInterval)} ` +
        `(±${((row.confidenceInterval / row.mean) * 100).toFixed(2)}%)`,
      formatRate(row.median),
      medianInterval,
      row.skewed ? '(!)' : '',
    );
    return values;
  });
  const widths = header.map((value, index) => Math.max(
    displayWidth(value),
    ...body.map((values) => displayWidth(values[index])),
  ));
  const right = [typeof rows[0].xValue === 'number'];
  if (category !== undefined) {
    right.push(typeof rows[0].categoryValue === 'number');
  }
  right.push(true, true, true, true, true, false);
  const format = (values) => values.map(
    (value, index) => pad(value, widths[index], right[index])).join('  ').trimEnd();
  output.push(format(header));
  for (const values of body) output.push(format(values));
}

function printScatterChart(output, rows, xAxis, category) {
  if (rows.length === 0) return;
  const width = 40;
  let maximum = 0;
  for (const row of rows) {
    maximum = Math.max(
      maximum,
      row.mean + (Number.isNaN(row.confidenceInterval) ?
        0 : row.confidenceInterval),
    );
  }
  if (maximum === 0) return;
  const labels = rows.map((row) => {
    let label = `${xAxis}=${row.xLabel}`;
    if (category !== undefined) label += ` ${category}=${row.categoryLabel}`;
    return truncateMiddle(label, 44);
  });
  const labelWidth = Math.max(...labels.map(displayWidth));
  const rateWidth = Math.max(...rows.map(({ mean }) =>
    displayWidth(formatRate(mean))));
  const axis = formatRate(maximum);
  const indent = ' '.repeat(labelWidth + 2);
  output.push(
    '',
    'Rate in operations/second; longer is faster. │ marks the mean and the',
    'shaded band (░) is its 95% confidence interval.',
    '',
    `${indent}0${' '.repeat(Math.max(1, width - 1 - displayWidth(axis)))}${axis}`,
    `${indent}+${'-'.repeat(width - 2)}+`,
  );
  let previous;
  for (let index = 0; index < rows.length; index++) {
    const row = rows[index];
    if (previous !== undefined && previous !== row.xValue) output.push('');
    previous = row.xValue;
    const interval = Number.isNaN(row.confidenceInterval) ?
      0 : row.confidenceInterval;
    const end = (row.mean / maximum) * width;
    const lower = ((row.mean - interval) / maximum) * width;
    const upper = ((row.mean + interval) / maximum) * width;
    const meanCell = Math.min(width - 1, Math.floor(end));
    let bar = '';
    for (let cell = 0; cell < width; cell++) {
      const position = cell + 0.5;
      if (cell === meanCell) bar += '│';
      else if (position >= lower && position <= upper) bar += '░';
      else if (position <= end) bar += '█';
      else bar += ' ';
    }
    output.push(
      `${pad(labels[index], labelWidth, false)}  ${bar}  ` +
      pad(formatRate(row.mean), rateWidth, true),
    );
  }
}

function wrapOutput(output, text, width = 76) {
  let line = '';
  for (const word of text.split(/\s+/)) {
    if (line === '') line = word;
    else if (displayWidth(line) + displayWidth(word) + 1 <= width) {
      line += ` ${word}`;
    } else {
      output.push(line);
      line = word;
    }
  }
  if (line !== '') output.push(line);
}

module.exports = {
  analyzeCompare,
  analyzeScatter,
  holmAdjust,
  isRegressionFailure,
  validateScatterParameters,
};
