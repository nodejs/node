// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { LogReader, parseString } from "./logreader.mjs";
import { BaseArgumentsProcessor } from "./arguments.mjs";
import { formatBytes, formatMillis} from "./js/helper.mjs";

// ===========================================================================

// This is the only true formatting, why? For an international audience the
// confusion between the decimal and thousands separator is big (alternating
// between comma "," vs dot "."). The Swiss formatting uses "'" as a thousands
// separator, dropping most of that confusion.
const numberFormat = new Intl.NumberFormat('de-CH', {
  maximumFractionDigits: 2,
  minimumFractionDigits: 2,
});

function formatNumber(value) {
  return numberFormat.format(value);
}

export function BYTES(bytes, total) {
  let result = formatBytes(bytes)
  if (total !== undefined && total != 0) {
    result += PERCENT(bytes, total).padStart(5);
  }
  return result;
}

export function TIME(millis) {
  return formatMillis(millis, 1);
}

export function PERCENT(value, total) {
  return Math.round(value / total * 100) + "%";
}

// ===========================================================================
const kNoTimeMetrics = {
  __proto__: null,
  executionDuration: 0,
  firstEventTimestamp: 0,
  firstParseEventTimestamp: 0,
  lastParseEventTimestamp: 0,
  lastEventTimestamp: 0
};

class CompilationUnit {
  constructor() {
    this.isEval = false;
    this.isDeserialized = false;

    // Lazily computed properties.
    this.firstEventTimestamp = -1;
    this.firstParseEventTimestamp = -1;
    this.firstCompileEventTimestamp = -1;
    this.lastParseEventTimestamp = -1;
    this.lastEventTimestamp = -1;
    this.deserializationTimestamp = -1;

    this.preparseTimestamp = -1;
    this.parseTimestamp = -1;
    this.parse2Timestamp = -1;
    this.resolutionTimestamp = -1;
    this.compileTimestamp = -1;
    this.lazyCompileTimestamp = -1;
    this.executionTimestamp = -1;
    this.baselineTimestamp = -1;
    this.optimizeTimestamp = -1;

    this.deserializationDuration = -0.0;
    this.preparseDuration = -0.0;
    this.parseDuration = -0.0;
    this.parse2Duration = -0.0;
    this.resolutionDuration = -0.0;
    this.scopeResolutionDuration = -0.0;
    this.lazyCompileDuration = -0.0;
    this.compileDuration = -0.0;
    this.baselineDuration = -0.0;
    this.optimizeDuration = -0.0;

    this.ownBytes = -1;
    this.compilationCacheHits = [];
  }

  finalize() {
    this.firstEventTimestamp = this.timestampMin(
        this.deserializationTimestamp, this.parseTimestamp,
        this.preparseTimestamp, this.resolutionTimestamp,
        this.executionTimestamp);

    this.firstParseEventTimestamp = this.timestampMin(
        this.deserializationTimestamp, this.parseTimestamp,
        this.preparseTimestamp, this.resolutionTimestamp);

    this.firstCompileEventTimestamp = this.rawTimestampMin(
        this.deserializationTimestamp, this.compileTimestamp,
        this.baselineTimestamp, this.lazyCompileTimestamp);
    // Any excuted script needs to be compiled.
    if (this.hasBeenExecuted() &&
        (this.firstCompileEventTimestamp <= 0 ||
         this.executionTimestamp < this.firstCompileTimestamp)) {
      console.error('Compile < execution timestamp', this);
    }

    if (this.ownBytes < 0) console.error(this, 'Own bytes must be positive');
  }

  hasBeenExecuted() {
    return this.executionTimestamp > 0;
  }

  addCompilationCacheHit(timestamp) {
    this.compilationCacheHits.push(timestamp);
  }

  // Returns the smallest timestamp from the given list, ignoring
  // uninitialized (-1) values.
  rawTimestampMin(...timestamps) {
    timestamps = timestamps.length == 1 ? timestamps[0] : timestamps;
    let result = timestamps.reduce((min, item) => {
      return item == -1 ? min : (min == -1 ? item : Math.min(item, item));
    }, -1);
    return result;
  }
  timestampMin(...timestamps) {
    let result = this.rawTimestampMin(...timestamps);
    if (Number.isNaN(result) || result < 0) {
      console.error(
          'Invalid timestamp min:', {result, timestamps, script: this});
      return 0;
    }
    return result;
  }

  timestampMax(...timestamps) {
    timestamps = timestamps.length == 1 ? timestamps[0] : timestamps;
    let result = Math.max(...timestamps);
    if (Number.isNaN(result) || result < 0) {
      console.error(
          'Invalid timestamp max:', {result, timestamps, script: this});
      return 0;
    }
    return result;
  }
}

// ===========================================================================
class Script extends CompilationUnit {
  constructor(id) {
    super();
    if (id === undefined || id <= 0) {
      throw new Error(`Invalid id=${id} for script`);
    }
    this.file = '';
    this.id = id;

    this.isNative = false;
    this.isBackgroundCompiled = false;
    this.isStreamingCompiled = false;

    this.funktions = [];
    this.metrics = new Map();
    this.maxNestingLevel = 0;

    this.width = 0;
    this.bytesTotal = -1;
    this.finalized = false;
    this.summary = '';
    this.source = '';
  }

  setFile(name) {
    this.file = name;
    this.isNative = name.startsWith('native ');
  }

  isEmpty() {
    return this.funktions.length === 0;
  }

  getFunktionAtStartPosition(start) {
    if (!this.isEval && start === 0) {
      throw 'position 0 is reserved for the script';
    }
    if (this.finalized) {
      return this.funktions.find(funktion => funktion.start == start);
    }
    return this.funktions[start];
  }

  // Return the innermost function at the given source position.
  getFunktionForPosition(position) {
    if (!this.finalized) throw 'Incomplete script';
    for (let i = this.funktions.length - 1; i >= 0; i--) {
      let funktion = this.funktions[i];
      if (funktion.containsPosition(position)) return funktion;
    }
    return undefined;
  }

  addMissingFunktions(list) {
    if (this.finalized) throw 'script is finalized!';
    list.forEach(fn => {
      if (this.funktions[fn.start] === undefined) {
        this.addFunktion(fn);
      }
    });
  }

  addFunktion(fn) {
    if (this.finalized) throw 'script is finalized!';
    if (fn.start === undefined) throw "Funktion has no start position";
    if (this.funktions[fn.start] !== undefined) {
      fn.print();
      throw "adding same function twice to script";
    }
    this.funktions[fn.start] = fn;
  }

  finalize() {
    this.finalized = true;
    // Compact funktions as we no longer need access via start byte position.
    this.funktions = this.funktions.filter(each => true);
    let parent = null;
    let maxNesting = 0;
    // Iterate over the Funktions in byte position order.
    this.funktions.forEach(fn => {
      fn.isEval = this.isEval;
      if (parent === null) {
        parent = fn;
      } else {
        // Walk up the nested chain of Funktions to find the parent.
        while (parent !== null && !fn.isNestedIn(parent)) {
          parent = parent.parent;
        }
        fn.parent = parent;
        if (parent) {
          maxNesting = Math.max(maxNesting, parent.addNestedFunktion(fn));
        }
        parent = fn;
      }
    });
    // Sanity checks to ensure that scripts are executed and parsed before any
    // of its funktions.
    let funktionFirstParseEventTimestamp = -1;
    // Second iteration step to finalize the funktions once the proper
    // hierarchy has been set up.
    this.funktions.forEach(fn => {
      fn.finalize();

      funktionFirstParseEventTimestamp = this.timestampMin(
          funktionFirstParseEventTimestamp, fn.firstParseEventTimestamp);

      this.lastParseEventTimestamp = this.timestampMax(
          this.lastParseEventTimestamp, fn.lastParseEventTimestamp);

      this.lastEventTimestamp =
          this.timestampMax(this.lastEventTimestamp, fn.lastEventTimestamp);
    });
    this.maxNestingLevel = maxNesting;

    // Initialize sizes.
    if (this.ownBytes === -1) console.error('Invalid ownBytes');
    if (this.funktions.length == 0) {
      this.bytesTotal = this.ownBytes = 0;
      return;
    }
    let toplevelFunktionBytes = this.funktions.reduce(
        (bytes, each) => bytes + (each.isToplevel() ? each.getBytes() : 0), 0);
    if (this.isDeserialized || this.isEval || this.isStreamingCompiled) {
      if (this.getBytes() === -1) {
        this.bytesTotal = toplevelFunktionBytes;
      }
    }
    this.ownBytes = this.bytesTotal - toplevelFunktionBytes;
    // Initialize common properties.
    super.finalize();
    // Sanity checks after the minimum timestamps have been computed.
    if (funktionFirstParseEventTimestamp < this.firstParseEventTimestamp) {
      console.error(
          'invalid firstCompileEventTimestamp', this,
          funktionFirstParseEventTimestamp, this.firstParseEventTimestamp);
    }
  }

  print() {
    console.log(this.toString());
  }

  toString() {
    let str = `SCRIPT id=${this.id} file=${this.file}\n` +
      `functions[${this.funktions.length}]:`;
    this.funktions.forEach(fn => str += fn.toString());
    return str;
  }

  getBytes() {
    return this.bytesTotal;
  }

  getOwnBytes() {
    return this.ownBytes;
  }

  // Also see Funktion.prototype.getMetricBytes
  getMetricBytes(name) {
    if (name == 'lazyCompileTimestamp') return this.getOwnBytes();
    return this.getOwnBytes();
  }

  getMetricDuration(name) {
    return this[name];
  }

  forEach(fn) {
    fn(this);
    this.funktions.forEach(fn);
  }

  // Container helper for TotalScript / Script.
  getScripts() {
    return [this];
  }

  calculateMetrics(printSummary) {
    let log = (str) => this.summary += str + '\n';
    log(`SCRIPT: ${this.id}`);
    let all = this.funktions;
    if (all.length === 0) return;

    let nofFunktions = all.length;
    let ownBytesSum = list => {
      return list.reduce((bytes, each) => bytes + each.getOwnBytes(), 0)
    };

    let info = (name, funktions) => {
      let ownBytes = ownBytesSum(funktions);
      let nofPercent = Math.round(funktions.length / nofFunktions * 100);
      let value = (funktions.length + "#").padStart(7) +
        (nofPercent + "%").padStart(5) +
        BYTES(ownBytes, this.bytesTotal).padStart(16);
      log((`  - ${name}`).padEnd(20) + value);
      this.metrics.set(name + "-bytes", ownBytes);
      this.metrics.set(name + "-count", funktions.length);
      this.metrics.set(name + "-count-percent", nofPercent);
      this.metrics.set(name + "-bytes-percent",
        Math.round(ownBytes / this.bytesTotal * 100));
    };

    log(`  - file:         ${this.file}`);
    log('  - details:      ' +
        'isEval=' + this.isEval + ' deserialized=' + this.isDeserialized +
        ' streamed=' + this.isStreamingCompiled);
    log("    Category               Count           Bytes");
    info("scripts", this.getScripts());
    info("functions", all);
    info("toplevel fns", all.filter(each => each.isToplevel()));
    info('preparsed', all.filter(each => each.preparseDuration > 0));

    info('fully parsed', all.filter(each => each.parseDuration > 0));
    // info("fn parsed", all.filter(each => each.parse2Duration > 0));
    // info("resolved", all.filter(each => each.resolutionDuration > 0));
    info("executed", all.filter(each => each.executionTimestamp > 0));
    info('eval', all.filter(each => each.isEval));
    info("lazy compiled", all.filter(each => each.lazyCompileTimestamp > 0));
    info("eager compiled", all.filter(each => each.compileTimestamp > 0));

    info("baseline", all.filter(each => each.baselineTimestamp > 0));
    info("optimized", all.filter(each => each.optimizeTimestamp > 0));

    log("    Cost split: executed vs non-executed functions, max = longest single event");
    const costs = [
      ['parse', each => each.parseDuration],
      ['preparse', each => each.preparseDuration],
      ['resolution', each => each.resolutionDuration],
      ['compile-eager',  each => each.compileDuration],
      ['compile-lazy',  each => each.lazyCompileDuration],
      ['baseline',  each => each.baselineDuration],
      ['optimize', each => each.optimizeDuration],
    ];
    for (let [name, fn] of costs) {
      const executionCost = new ExecutionCost(name, all, fn);
      executionCost.setMetrics(this.metrics);
      log(executionCost.toString());
    }

    let nesting = new NestingDistribution(all);
    nesting.setMetrics(this.metrics);
    log(nesting.toString());

    if (printSummary) console.log(this.summary);
  }

  getAccumulatedTimeMetrics(
      metrics, start, end, delta, cumulative = true, useDuration = false) {
    // Returns an array of the following format:
    // [ [start,         acc(metric0, start, start), acc(metric1, ...), ...],
    //   [start+delta,   acc(metric0, start, start+delta), ...],
    //   [start+delta*2, acc(metric0, start, start+delta*2), ...],
    //   ...
    // ]
    if (end <= start) throw `Invalid ranges [${start},${end}]`;
    const timespan = end - start;
    const kSteps = Math.ceil(timespan / delta);
    // To reduce the time spent iterating over the funktions of this script
    // we iterate once over all funktions and add the metric changes to each
    // timepoint:
    // [ [0, 300, ...], [1,  15, ...], [2, 100, ...], [3,   0, ...] ... ]
    // In a second step we accumulate all values:
    // [ [0, 300, ...], [1, 315, ...], [2, 415, ...], [3, 415, ...] ... ]
    //
    // To limit the number of data points required in the resulting graphs,
    // only the rows for entries with actual changes are created.

    const metricProperties = ["time"];
    metrics.forEach(each => {
      metricProperties.push(each + 'Timestamp');
      if (useDuration) metricProperties.push(each + 'Duration');
    });
    // Create a packed {rowTemplate} which is copied later-on.
    let indexToTime = (t) => (start + t * delta) / kSecondsToMillis;
    let rowTemplate = [indexToTime(0)];
    for (let i = 1; i < metricProperties.length; i++) rowTemplate.push(0.0);
    // Create rows with 0-time entry.
    let rows = new Array(rowTemplate.slice());
    for (let t = 1; t <= kSteps; t++) rows.push(null);
    // Create the real metric's property name on the Funktion object.
    // Add the increments of each Funktion's metric to the result.
    this.forEach(funktionOrScript => {
      // Iterate over the Funktion's metric names, skipping position 0 which
      // is the time.
      const kMetricIncrement = useDuration ? 2 : 1;
      for (let i = 1; i < metricProperties.length; i += kMetricIncrement) {
        let timestampPropertyName = metricProperties[i];
        let timestamp = funktionOrScript[timestampPropertyName];
        if (timestamp === undefined) continue;
        if (timestamp < start || end < timestamp) continue;
        timestamp -= start;
        let index = Math.floor(timestamp / delta);
        let row = rows[index];
        if (row === null) {
          // Add a new row if it didn't exist,
          row = rows[index] = rowTemplate.slice();
          // .. add the time offset.
          row[0] = indexToTime(index);
        }
        // Add the metric value.
        row[i] += funktionOrScript.getMetricBytes(timestampPropertyName);
        if (!useDuration) continue;
        let durationPropertyName = metricProperties[i + 1];
        row[i + 1] += funktionOrScript.getMetricDuration(durationPropertyName);
      }
    });
    // Create a packed array again with only the valid entries.
    // Accumulate the incremental results by adding the metric values from
    // the previous time window.
    let previous = rows[0];
    let result = [previous];
    for (let t = 1; t < rows.length; t++) {
      let current = rows[t];
      if (current === null) {
        // Ensure a zero data-point after each non-zero point.
        if (!cumulative && rows[t - 1] !== null) {
          let duplicate = rowTemplate.slice();
          duplicate[0] = indexToTime(t);
          result.push(duplicate);
        }
        continue;
      }
      if (cumulative) {
        // Skip i==0 where the corresponding time value in seconds is.
        for (let i = 1; i < metricProperties.length; i++) {
          current[i] += previous[i];
        }
      }
      // Make sure we have a data-point in time right before the current one.
      if (rows[t - 1] === null) {
        let duplicate = (!cumulative ? rowTemplate : previous).slice();
        duplicate[0] = indexToTime(t - 1);
        result.push(duplicate);
      }
      previous = current;
      result.push(current);
    }
    // Make sure there is an entry at the last position to make sure all graphs
    // have the same width.
    const lastIndex = rows.length - 1;
    if (rows[lastIndex] === null) {
      let duplicate = previous.slice();
      duplicate[0] = indexToTime(lastIndex);
      result.push(duplicate);
    }
    return result;
  }

  getFunktionsAtTime(time, delta, metric) {
    // Returns a list of Funktions whose metric changed in the
    // [time-delta, time+delta] range.
    return this.funktions.filter(
      funktion => funktion.didMetricChange(time, delta, metric));
  }
}


class TotalScript extends Script {
  constructor() {
    super('all files', 'all files');
    this.scripts = [];
  }

  addAllFunktions(script) {
    // funktions is indexed by byte offset and as such not packed. Add every
    // Funktion one by one to keep this.funktions packed.
    script.funktions.forEach(fn => this.funktions.push(fn));
    this.scripts.push(script);
    this.bytesTotal += script.bytesTotal;
  }

  // Iterate over all Scripts and nested Funktions.
  forEach(fn) {
    this.scripts.forEach(script => script.forEach(fn));
  }

  getScripts() {
    return this.scripts;
  }
}


// ===========================================================================

class NestingDistribution {
  constructor(funktions) {
    // Stores the nof bytes per function nesting level.
    this.accumulator = [0, 0, 0, 0, 0];
    // Max nof bytes encountered at any nesting level.
    this.max = 0;
    // avg bytes per nesting level.
    this.avg = 0;
    this.totalBytes = 0;

    funktions.forEach(each => each.accumulateNestingLevel(this.accumulator));
    this.max = this.accumulator.reduce((max, each) => Math.max(max, each), 0);
    this.totalBytes = this.accumulator.reduce((sum, each) => sum + each, 0);
    for (let i = 0; i < this.accumulator.length; i++) {
      this.avg += this.accumulator[i] * i;
    }
    this.avg /= this.totalBytes;
  }

  print() {
    console.log(this.toString());
  }

  toString() {
    let ticks = " ▁▂▃▄▅▆▇█";
    let accString = this.accumulator.reduce((str, each) => {
      let index = Math.round(each / this.max * (ticks.length - 1));
      return str + ticks[index];
    }, '');
    let percent0 = this.accumulator[0]
    let percent1 = this.accumulator[1];
    let percent2plus = this.accumulator.slice(2)
      .reduce((sum, each) => sum + each, 0);
    return "  - fn nesting level:     " +
      ' avg=' + formatNumber(this.avg) +
      ' l0=' + PERCENT(percent0, this.totalBytes) +
      ' l1=' + PERCENT(percent1, this.totalBytes) +
      ' l2+=' + PERCENT(percent2plus, this.totalBytes) +
      ' nesting-histogram=[' + accString + ']';

  }

  setMetrics(dict) {}
}

class ExecutionCost {
  constructor(prefix, funktions, time_fn) {
    this.prefix = prefix;
    // Time spent on executed functions.
    this.executedCost = 0;
    // Time spent on not executed functions.
    this.nonExecutedCost = 0;
    this.maxDuration = 0;

    for (let i = 0; i < funktions.length; i++) {
      const funktion = funktions[i];
      const value = time_fn(funktion);
      if (funktion.hasBeenExecuted()) {
        this.executedCost +=  value;
      } else {
        this.nonExecutedCost += value;
      }
      this.maxDuration = Math.max(this.maxDuration, value);
    }
  }

  print() {
    console.log(this.toString())
  }

  toString() {
    return `  - ${this.prefix}-time:`.padEnd(24) +
      ` Σ executed=${TIME(this.executedCost)}`.padEnd(20) +
      ` Σ non-executed=${TIME(this.nonExecutedCost)}`.padEnd(24) +
      ` max=${TIME(this.maxDuration)}`;
  }

  setMetrics(dict) {
    dict.set('parseMetric', this.executionCost);
    dict.set('parseMetricNegative', this.nonExecutionCost);
  }
}

// ===========================================================================

class Funktion extends CompilationUnit {
  constructor(name, start, end, script) {
    super();
    if (start < 0) throw `invalid start position: ${start}`;
    if (script.isEval) {
      if (end < start) throw 'invalid start end positions';
    } else {
      if (end <= 0) throw `invalid end position: ${end}`;
      if (end < start) throw 'invalid start end positions';
      if (end == start) console.error("Possibly invalid start/end position")
    }

    this.name = name;
    this.start = start;
    this.end = end;
    this.script = script;
    this.parent = null;
    this.nested = [];
    this.nestingLevel = 0;

    if (script) this.script.addFunktion(this);
  }

  finalize() {
    this.lastParseEventTimestamp = Math.max(
        this.preparseTimestamp + this.preparseDuration,
        this.parseTimestamp + this.parseDuration,
        this.resolutionTimestamp + this.resolutionDuration);
    if (!(this.lastParseEventTimestamp > 0)) this.lastParseEventTimestamp = 0;

    this.lastEventTimestamp =
        Math.max(this.lastParseEventTimestamp, this.executionTimestamp);
    if (!(this.lastEventTimestamp > 0)) this.lastEventTimestamp = 0;

    this.ownBytes = this.nested.reduce(
        (bytes, each) => bytes - each.getBytes(), this.getBytes());

    super.finalize();
  }

  getMetricBytes(name) {
    if (name == 'lazyCompileTimestamp') return this.getOwnBytes();
    return this.getOwnBytes();
  }

  getMetricDuration(name) {
    if (name in kNoTimeMetrics) return 0;
    return this[name];
  }

  isNestedIn(funktion) {
    if (this.script != funktion.script) throw "Incompatible script";
    return funktion.start < this.start && this.end <= funktion.end;
  }

  isToplevel() {
    return this.parent === null;
  }

  containsPosition(position) {
    return this.start <= position && position <= this.end;
  }

  accumulateNestingLevel(accumulator) {
    let value = accumulator[this.nestingLevel] || 0;
    accumulator[this.nestingLevel] = value + this.getOwnBytes();
  }

  addNestedFunktion(child) {
    if (this.script != child.script) throw "Incompatible script";
    if (child == null) throw "Nesting non child";
    this.nested.push(child);
    if (this.nested.length > 1) {
      // Make sure the nested children don't overlap and have been inserted in
      // byte start position order.
      let last = this.nested[this.nested.length - 2];
      if (last.end > child.start || last.start > child.start ||
        last.end > child.end || last.start > child.end) {
        throw "Wrongly nested child added";
      }
    }
    child.nestingLevel = this.nestingLevel + 1;
    return child.nestingLevel;
  }

  getBytes() {
    return this.end - this.start;
  }

  getOwnBytes() {
    return this.ownBytes;
  }

  didMetricChange(time, delta, name) {
    let value = this[name + 'Timestamp'];
    return (time - delta) <= value && value <= (time + delta);
  }

  print() {
    console.log(this.toString());
  }

  toString(details = true) {
    let result = `function${this.name ? ` ${this.name}` : ''}` +
        `() range=${this.start}-${this.end}`;
    if (details) result += ` script=${this.script ? this.script.id : 'X'}`;
    return result;
  }
}


// ===========================================================================

export const kTimestampFactor = 1000;
export const kSecondsToMillis = 1000;

function toTimestamp(microseconds) {
  return microseconds / kTimestampFactor
}

function startOf(timestamp, time) {
  let result = toTimestamp(timestamp) - time;
  if (result < 0) throw "start timestamp cannot be negative";
  return result;
}


export class ParseProcessor extends LogReader {
  constructor() {
    super();
    this.setDispatchTable({
      // Avoid accidental leaking of __proto__ properties and force this object
      // to be in dictionary-mode.
      __proto__: null,
      // "function",{event type},
      // {script id},{start position},{end position},{time},{timestamp},
      // {function name}
      'function': {
        parsers: [
          parseString, parseInt, parseInt, parseInt, parseFloat, parseInt,
          parseString
        ],
        processor: this.processFunctionEvent
      },
      // "compilation-cache","hit"|"put",{type},{scriptid},{start position},
      // {end position},{timestamp}
      'compilation-cache': {
        parsers:
            [parseString, parseString, parseInt, parseInt, parseInt, parseInt],
        processor: this.processCompilationCacheEvent
      },
      'script': {
        parsers: [parseString, parseInt, parseInt],
        processor: this.processScriptEvent
      },
      // "script-details", {script_id}, {file}, {line}, {column}, {size}
      'script-details': {
        parsers: [parseInt, parseString, parseInt, parseInt, parseInt],
        processor: this.processScriptDetails
      },
      'script-source': {
        parsers: [parseInt, parseString, parseString],
        processor: this.processScriptSource
      },
    });
    this.functionEventDispatchTable_ = {
      // Avoid accidental leaking of __proto__ properties and force this object
      // to be in dictionary-mode.
      __proto__: null,
      'full-parse': this.processFull.bind(this),
      'parse-function': this.processParseFunction.bind(this),
      // TODO(cbruni): make sure arrow functions emit a normal parse-function
      // event.
      'parse': this.processParseFunction.bind(this),
      'parse-script': this.processParseScript.bind(this),
      'parse-eval': this.processParseEval.bind(this),
      'preparse-no-resolution': this.processPreparseNoResolution.bind(this),
      'preparse-resolution': this.processPreparseResolution.bind(this),
      'first-execution': this.processFirstExecution.bind(this),
      'interpreter-lazy': this.processCompileLazy.bind(this),
      'interpreter': this.processCompile.bind(this),
      'interpreter-eval': this.processCompileEval.bind(this),
      'baseline': this.processBaselineLazy.bind(this),
      'baseline-lazy': this.processBaselineLazy.bind(this),
      'optimize-lazy': this.processOptimizeLazy.bind(this),
      'deserialize': this.processDeserialize.bind(this),
    };

    this.idToScript = new Map();
    this.fileToScript = new Map();
    this.nameToFunction = new Map();
    this.scripts = [];
    this.totalScript = new TotalScript();
    this.firstEventTimestamp = -1;
    this.lastParseEventTimestamp = -1;
    this.lastEventTimestamp = -1;
  }

  print() {
    console.log("scripts:");
    this.idToScript.forEach(script => script.print());
  }

  async processString(string) {
    await this.processLogChunk(string);
    this.postProcess();
  }

  async processLogFile(fileName) {
    this.collectEntries = true
    this.lastLogFileName_ = fileName;
    let line;
    while (line = readline()) {
      await this.processLogLine(line);
    }
    this.postProcess();
  }

  postProcess() {
    this.scripts = Array.from(this.idToScript.values())
      .filter(each => !each.isNative);

    if (this.scripts.length == 0) {
      console.error("Could not find any scripts!");
      return false;
    }

    this.scripts.forEach(script => {
      script.finalize();
      script.calculateMetrics(false);
    });

    this.scripts.forEach(script => this.totalScript.addAllFunktions(script));
    this.totalScript.calculateMetrics(true);

    this.firstEventTimestamp = this.totalScript.timestampMin(
        this.scripts.map(each => each.firstEventTimestamp));
    this.lastParseEventTimestamp = this.totalScript.timestampMax(
        this.scripts.map(each => each.lastParseEventTimestamp));
    this.lastEventTimestamp = this.totalScript.timestampMax(
        this.scripts.map(each => each.lastEventTimestamp));

    const series = {
      firstParseEvent: 'Any Parse Event',
      parse: 'Parsing',
      preparse: 'Preparsing',
      resolution: 'Preparsing with Var. Resolution',
      lazyCompile: 'Lazy Compilation',
      compile: 'Eager Compilation',
      execution: 'First Execution',
    };
    let metrics = Object.keys(series);
    this.totalScript.getAccumulatedTimeMetrics(
        metrics, 0, this.lastEventTimestamp, 10);
  }

  processFunctionEvent(
      eventName, scriptId, startPosition, endPosition, duration, timestamp,
      functionName) {
    let handlerFn = this.functionEventDispatchTable_[eventName];
    if (handlerFn === undefined) {
      console.error(`Couldn't find handler for function event:${eventName}`);
    }
    handlerFn(
        scriptId, startPosition, endPosition, duration, timestamp,
        functionName);
  }

  addEntry(entry) {
    this.entries.push(entry);
  }

  lookupScript(id) {
    return this.idToScript.get(id);
  }

  getOrCreateFunction(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    if (scriptId == -1) {
      return this.lookupFunktionByRange(startPosition, endPosition);
    }
    let script = this.lookupScript(scriptId);
    let funktion = script.getFunktionAtStartPosition(startPosition);
    if (funktion === undefined) {
      funktion = new Funktion(functionName, startPosition, endPosition, script);
    }
    return funktion;
  }

  // Iterates over all functions and tries to find matching ones.
  lookupFunktionsByRange(start, end) {
    let results = [];
    this.idToScript.forEach(script => {
      script.forEach(funktion => {
        if (funktion.startPosition == start && funktion.endPosition == end) {
          results.push(funktion);
        }
      });
    });
    return results;
  }
  lookupFunktionByRange(start, end) {
    let results = this.lookupFunktionsByRange(start, end);
    if (results.length != 1) throw "Could not find unique function by range";
    return results[0];
  }

  processScriptEvent(eventName, scriptId, timestamp) {
    let script = this.idToScript.get(scriptId);
    switch (eventName) {
      case 'create':
      case 'reserve-id':
      case 'deserialize': {
        if (script !== undefined) return;
        script = new Script(scriptId);
        this.idToScript.set(scriptId, script);
        if (eventName == 'deserialize') {
          script.deserializationTimestamp = toTimestamp(timestamp);
        }
        return;
      }
      case 'background-compile':
        if (script.isBackgroundCompiled) {
          throw 'Cannot background-compile twice';
        }
        script.isBackgroundCompiled = true;
        // TODO(cbruni): remove once backwards compatibility is no longer needed.
        script.isStreamingCompiled = true;
        // TODO(cbruni): fix parse events for background compilation scripts
        script.preparseTimestamp = toTimestamp(timestamp);
        return;
      case 'streaming-compile':
        if (script.isStreamingCompiled) throw 'Cannot stream-compile twice';
        // TODO(cbruni): remove once backwards compatibility is no longer needed.
        script.isBackgroundCompiled = true;
        script.isStreamingCompiled = true;
        // TODO(cbruni): fix parse events for background compilation scripts
        script.preparseTimestamp = toTimestamp(timestamp);
        return;
      default:
        console.error(`Unhandled script event: ${eventName}`);
    }
  }

  processScriptDetails(scriptId, file, startLine, startColumn, size) {
    let script = this.lookupScript(scriptId);
    script.setFile(file);
  }

  processScriptSource(scriptId, url, source) {
    let script = this.lookupScript(scriptId);
    script.source = source;
  }

  processParseEval(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    if (startPosition != 0 && startPosition != -1) {
      console.error('Invalid start position for parse-eval', arguments);
    }
    let script = this.processParseScript(...arguments);
    script.isEval = true;
  }

  processFull(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    if (startPosition == 0) {
      // This should only happen for eval.
      let script = this.lookupScript(scriptId);
      script.isEval = true;
      return;
    }
    let funktion = this.getOrCreateFunction(...arguments);
    // TODO(cbruni): this should never happen, emit differen event from the
    // parser.
    if (funktion.parseTimestamp > 0) return;
    funktion.parseTimestamp = startOf(timestamp, duration);
    funktion.parseDuration = duration;
  }

  processParseFunction(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let funktion = this.getOrCreateFunction(...arguments);
    funktion.parseTimestamp = startOf(timestamp, duration);
    funktion.parseDuration = duration;
  }

  processParseScript(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    // TODO timestamp and duration
    let script = this.lookupScript(scriptId);
    let ts = startOf(timestamp, duration);
    script.parseTimestamp = ts;
    script.parseDuration = duration;
    return script;
  }

  processPreparseResolution(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let funktion = this.getOrCreateFunction(...arguments);
    // TODO(cbruni): this should never happen, emit different event from the
    // parser.
    if (funktion.resolutionTimestamp > 0) return;
    funktion.resolutionTimestamp = startOf(timestamp, duration);
    funktion.resolutionDuration = duration;
  }

  processPreparseNoResolution(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let funktion = this.getOrCreateFunction(...arguments);
    funktion.preparseTimestamp = startOf(timestamp, duration);
    funktion.preparseDuration = duration;
  }

  processFirstExecution(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let script = this.lookupScript(scriptId);
    if (startPosition === 0) {
      // undefined = eval fn execution
      if (script) {
        script.executionTimestamp = toTimestamp(timestamp);
      }
    } else {
      let funktion = script.getFunktionAtStartPosition(startPosition);
      if (funktion) {
        funktion.executionTimestamp = toTimestamp(timestamp);
      } else {
        // TODO(cbruni): handle funktions from  compilation-cache hits.
      }
    }
  }

  processCompileLazy(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let funktion = this.getOrCreateFunction(...arguments);
    funktion.lazyCompileTimestamp = startOf(timestamp, duration);
    funktion.lazyCompileDuration = duration;
  }

  processCompile(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let script = this.lookupScript(scriptId);
    if (startPosition === 0) {
      script.compileTimestamp = startOf(timestamp, duration);
      script.compileDuration = duration;
      script.bytesTotal = endPosition;
      return script;
    } else {
      let funktion = script.getFunktionAtStartPosition(startPosition);
      if (funktion === undefined) {
        // This should not happen since any funktion has to be parsed first.
        console.error('processCompile funktion not found', ...arguments);
        return;
      }
      funktion.compileTimestamp = startOf(timestamp, duration);
      funktion.compileDuration = duration;
      return funktion;
    }
  }

  processCompileEval(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let compilationUnit = this.processCompile(...arguments);
    compilationUnit.isEval = true;
  }

  processBaselineLazy(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let compilationUnit = this.lookupScript(scriptId);
    if (startPosition > 0) {
      compilationUnit =
          compilationUnit.getFunktionAtStartPosition(startPosition);
      if (compilationUnit === undefined) {
        // This should not happen since any funktion has to be parsed first.
        console.error('processBaselineLazy funktion not found', ...arguments);
        return;
      }
    }
    compilationUnit.baselineTimestamp = startOf(timestamp, duration);
    compilationUnit.baselineDuration = duration;
  }

  processOptimizeLazy(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let compilationUnit = this.lookupScript(scriptId);
    if (startPosition > 0) {
      compilationUnit =
          compilationUnit.getFunktionAtStartPosition(startPosition);
      if (compilationUnit === undefined) {
        // This should not happen since any funktion has to be parsed first.
        console.error('processOptimizeLazy funktion not found', ...arguments);
        return;
      }
    }
    compilationUnit.optimizeTimestamp = startOf(timestamp, duration);
    compilationUnit.optimizeDuration = duration;
  }

  processDeserialize(
      scriptId, startPosition, endPosition, duration, timestamp, functionName) {
    let compilationUnit = this.lookupScript(scriptId);
    if (startPosition === 0) {
      compilationUnit.bytesTotal = endPosition;
    } else {
      compilationUnit = this.getOrCreateFunction(...arguments);
    }
    compilationUnit.deserializationTimestamp = startOf(timestamp, duration);
    compilationUnit.deserializationDuration = duration;
    compilationUnit.isDeserialized = true;
  }

  processCompilationCacheEvent(
      eventType, cacheType, scriptId, startPosition, endPosition, timestamp) {
    if (eventType !== 'hit') return;
    let compilationUnit = this.lookupScript(scriptId);
    if (startPosition > 0) {
      compilationUnit =
          compilationUnit.getFunktionAtStartPosition(startPosition);
    }
    compilationUnit.addCompilationCacheHit(toTimestamp(timestamp));
  }

}


export class ArgumentsProcessor extends BaseArgumentsProcessor {
  getArgsDispatch() {
    return {};
  }

  getDefaultResults() {
    return {
      logFileName: 'v8.log',
      range: 'auto,auto',
    };
  }
}
