// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {Isolate} from './model.js';

defineCustomElement('trace-file-reader', (templateText) =>
 class TraceFileReader extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = templateText;
    this.addEventListener('click', e => this.handleClick(e));
    this.addEventListener('dragover', e => this.handleDragOver(e));
    this.addEventListener('drop', e => this.handleChange(e));
    this.$('#file').addEventListener('change', e => this.handleChange(e));
    this.$('#fileReader').addEventListener('keydown', e => this.handleKeyEvent(e));
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
  }

  get section() {
    return this.$('#fileReaderSection');
  }

  updateLabel(text) {
    this.$('#label').innerText = text;
  }

  handleKeyEvent(event) {
    if (event.key == "Enter") this.handleClick(event);
  }

  handleClick(event) {
    this.$('#file').click();
  }

  handleChange(event) {
    // Used for drop and file change.
    event.preventDefault();
    var host = event.dataTransfer ? event.dataTransfer : event.target;
    this.readFile(host.files[0]);
  }

  handleDragOver(event) {
    event.preventDefault();
  }

  connectedCallback() {
    this.$('#fileReader').focus();
  }

  readFile(file) {
    if (!file) {
      this.updateLabel('Failed to load file.');
      return;
    }
    this.$('#fileReader').blur();

    this.section.className = 'loading';
    const reader = new FileReader();

    if (['application/gzip', 'application/x-gzip'].includes(file.type)) {
      reader.onload = (e) => {
        try {
          // Decode data as strings of 64Kb chunks. Bigger chunks may cause
          // parsing failures in Oboe.js.
          const chunkedInflate = new pako.Inflate(
            {to: 'string', chunkSize: 65536}
          );
          let processingState = undefined;
          chunkedInflate.onData = (chunk) => {
            if (processingState === undefined) {
              processingState = this.startProcessing(file, chunk);
            } else {
              processingState.processChunk(chunk);
            }
          };
          chunkedInflate.onEnd = () => {
            if (processingState !== undefined) {
              const result_data = processingState.endProcessing();
              this.processLoadedData(file, result_data);
            }
          };
          console.log("======");
          const textResult = chunkedInflate.push(e.target.result);

          this.section.className = 'success';
          this.$('#fileReader').classList.add('done');
        } catch (err) {
          console.error(err);
          this.section.className = 'failure';
        }
      };
      // Delay the loading a bit to allow for CSS animations to happen.
      setTimeout(() => reader.readAsArrayBuffer(file), 0);
    } else {
      reader.onload = (e) => {
        try {
          // Process the whole file in at once.
          const processingState = this.startProcessing(file, e.target.result);
          const dataModel = processingState.endProcessing();
          this.processLoadedData(file, dataModel);

          this.section.className = 'success';
          this.$('#fileReader').classList.add('done');
        } catch (err) {
          console.error(err);
          this.section.className = 'failure';
        }
      };
      // Delay the loading a bit to allow for CSS animations to happen.
      setTimeout(() => reader.readAsText(file), 0);
    }
  }

  processLoadedData(file, dataModel) {
    console.log("Trace file parsed successfully.");
    this.extendAndSanitizeModel(dataModel);
    this.updateLabel('Finished loading \'' + file.name + '\'.');
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: dataModel}));
  }

  createOrUpdateEntryIfNeeded(data, entry) {
    console.assert(entry.isolate, 'entry should have an isolate');
    if (!(entry.isolate in data)) {
      data[entry.isolate] = new Isolate(entry.isolate);
    }
  }

  extendAndSanitizeModel(data) {
    const checkNonNegativeProperty = (obj, property) => {
      console.assert(obj[property] >= 0, 'negative property', obj, property);
    };

    Object.values(data).forEach(isolate => isolate.finalize());
  }

  processOneZoneStatsEntry(data, entry_stats) {
    this.createOrUpdateEntryIfNeeded(data, entry_stats);
    const isolate_data = data[entry_stats.isolate];
    let zones = undefined;
    const entry_zones = entry_stats.zones;
    if (entry_zones !== undefined) {
      zones = new Map();
      entry_zones.forEach(zone => {
        // There might be multiple occurrences of the same zone in the set,
        // combine numbers in this case.
        const existing_zone_stats = zones.get(zone.name);
        if (existing_zone_stats !== undefined) {
          existing_zone_stats.allocated += zone.allocated;
          existing_zone_stats.used += zone.used;
          existing_zone_stats.freed += zone.freed;
        } else {
          zones.set(zone.name, { allocated: zone.allocated,
                                 used: zone.used,
                                 freed: zone.freed });
        }
      });
    }
    const time = entry_stats.time;
    const sample = {
      time: time,
      allocated: entry_stats.allocated,
      used: entry_stats.used,
      freed: entry_stats.freed,
      zones: zones
    };
    isolate_data.samples.set(time, sample);
  }

  startProcessing(file, chunk) {
    const isV8TraceFile = chunk.includes('v8-zone-trace');
    const processingState =
        isV8TraceFile ? this.startProcessingAsV8TraceFile(file)
                      : this.startProcessingAsChromeTraceFile(file);

    processingState.processChunk(chunk);
    return processingState;
  }

  startProcessingAsChromeTraceFile(file) {
    console.log(`Processing log as chrome trace file.`);
    const data = Object.create(null);  // Final data container.
    const parseOneZoneEvent = (actual_data) => {
      if ('stats' in actual_data) {
        try {
          const entry_stats = JSON.parse(actual_data.stats);
          this.processOneZoneStatsEntry(data, entry_stats);
        } catch (e) {
          console.error('Unable to parse data set entry', e);
        }
      }
    };
    const zone_events_filter = (event) => {
      if (event.name == 'V8.Zone_Stats') {
        parseOneZoneEvent(event.args);
      }
      return oboe.drop;
    };

    const oboe_stream = oboe();
    // Trace files support two formats.
    oboe_stream
        // 1) {traceEvents: [ data ]}
        .node('traceEvents.*', zone_events_filter)
        // 2) [ data ]
        .node('!.*', zone_events_filter)
        .fail((errorReport) => {
          throw new Error("Trace data parse failed: " + errorReport.thrown);
        });

    let failed = false;

    const processingState = {
      file: file,

      processChunk(chunk) {
        if (failed) return false;
        try {
          oboe_stream.emit('data', chunk);
          return true;
        } catch (e) {
          console.error('Unable to parse chrome trace file.', e);
          failed = true;
          return false;
        }
      },

      endProcessing() {
        if (failed) return null;
        oboe_stream.emit('end');
        return data;
      },
    };
    return processingState;
  }

  startProcessingAsV8TraceFile(file) {
    console.log('Processing log as V8 trace file.');
    const data = Object.create(null);  // Final data container.

    const processOneLine = (line) => {
      try {
        // Strip away a potentially present adb logcat prefix.
        line = line.replace(/^I\/v8\s*\(\d+\):\s+/g, '');

        const entry = JSON.parse(line);
        if (entry === null || entry.type === undefined) return;
        if ((entry.type === 'v8-zone-trace') && ('stats' in entry)) {
          const entry_stats = entry.stats;
          this.processOneZoneStatsEntry(data, entry_stats);
        } else {
          console.log('Unknown entry type: ' + entry.type);
        }
      } catch (e) {
        console.log('Unable to parse line: \'' + line + '\' (' + e + ')');
      }
    };

    let prev_chunk_leftover = "";

    const processingState = {
      file: file,

      processChunk(chunk) {
        const contents = chunk.split('\n');
        const last_line = contents.pop();
        const linesCount = contents.length;
        if (linesCount == 0) {
          // There was only one line in the chunk, it may still be unfinished.
          prev_chunk_leftover += last_line;
        } else {
          contents[0] = prev_chunk_leftover + contents[0];
          prev_chunk_leftover = last_line;
          for (let line of contents) {
            processOneLine(line);
          }
        }
        return true;
      },

      endProcessing() {
        if (prev_chunk_leftover.length > 0) {
          processOneLine(prev_chunk_leftover);
          prev_chunk_leftover = "";
        }
        return data;
      },
    };
    return processingState;
  }
});
