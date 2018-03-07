// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const trace_file_reader_template =
    document.currentScript.ownerDocument.querySelector(
        '#trace-file-reader-template');

class TraceFileReader extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.appendChild(trace_file_reader_template.content.cloneNode(true));
    this.addEventListener('click', e => this.handleClick(e));
    this.addEventListener('dragover', e => this.handleDragOver(e));
    this.addEventListener('drop', e => this.handleChange(e));
    this.$('#file').addEventListener('change', e => this.handleChange(e));
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
  }

  updateLabel(text) {
    this.$('#label').innerText = text;
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

  connectedCallback() {}

  readFile(file) {
    if (!file) {
      this.updateLabel('Failed to load file.');
      return;
    }

    const result = new FileReader();
    result.onload = (e) => {
      let contents = e.target.result.split('\n');
      const return_data = (e.target.result.includes('V8.GC_Objects_Stats')) ?
          this.createModelFromChromeTraceFile(contents) :
          this.createModelFromV8TraceFile(contents);
      this.updateLabel('Finished loading \'' + file.name + '\'.');
      this.dispatchEvent(new CustomEvent(
          'change', {bubbles: true, composed: true, detail: return_data}));
    };
    result.readAsText(file);
  }

  createOrUpdateEntryIfNeeded(data, keys, entry) {
    console.assert(entry.isolate, 'entry should have an isolate');
    if (!(entry.isolate in keys)) {
      keys[entry.isolate] = new Set();
    }
    if (!(entry.isolate in data)) {
      data[entry.isolate] = {
        non_empty_instance_types: new Set(),
        gcs: {},
        zonetags: [],
        samples: {zone: {}},
        start: null,
        end: null,
        data_sets: new Set()
      };
    }
    const data_object = data[entry.isolate];
    if (('id' in entry) && !(entry.id in data_object.gcs)) {
      data_object.gcs[entry.id] = {non_empty_instance_types: new Set()};
    }
    if ('time' in entry) {
      if (data_object.end === null || data_object.end < entry.time)
        data_object.end = entry.time;
      if (data_object.start === null || data_object.start > entry.time)
        data_object.start = entry.time;
    }
  }

  createDatasetIfNeeded(data, keys, entry, data_set) {
    if (!(data_set in data[entry.isolate].gcs[entry.id])) {
      data[entry.isolate].gcs[entry.id][data_set] = {
        instance_type_data: {},
        non_empty_instance_types: new Set(),
        overall: 0
      };
      data[entry.isolate].data_sets.add(data_set);
    }
  }

  addInstanceTypeData(
      data, keys, isolate, gc_id, data_set, instance_type, entry) {
    keys[isolate].add(data_set);
    data[isolate].gcs[gc_id][data_set].instance_type_data[instance_type] = {
      overall: entry.overall,
      count: entry.count,
      histogram: entry.histogram,
      over_allocated: entry.over_allocated,
      over_allocated_histogram: entry.over_allocated_histogram
    };
    data[isolate].gcs[gc_id][data_set].overall += entry.overall;
    if (entry.overall !== 0) {
      data[isolate].gcs[gc_id][data_set].non_empty_instance_types.add(
          instance_type);
      data[isolate].gcs[gc_id].non_empty_instance_types.add(instance_type);
      data[isolate].non_empty_instance_types.add(instance_type);
    }
  }

  extendAndSanitizeModel(data, keys) {
    const checkNonNegativeProperty = (obj, property) => {
      console.assert(obj[property] >= 0, 'negative property', obj, property);
    };

    for (const isolate of Object.keys(data)) {
      for (const gc of Object.keys(data[isolate].gcs)) {
        for (const data_set_key of keys[isolate]) {
          const data_set = data[isolate].gcs[gc][data_set_key];
          // 1. Create a ranked instance type array that sorts instance
          // types by memory size (overall).
          data_set.ranked_instance_types =
              [...data_set.non_empty_instance_types].sort(function(a, b) {
                if (data_set.instance_type_data[a].overall >
                    data_set.instance_type_data[b].overall) {
                  return 1;
                } else if (
                    data_set.instance_type_data[a].overall <
                    data_set.instance_type_data[b].overall) {
                  return -1;
                }
                return 0;
              });

          let known_count = 0;
          let known_overall = 0;
          let known_histogram =
              Array(
                  data_set.instance_type_data.FIXED_ARRAY_TYPE.histogram.length)
                  .fill(0);
          for (const instance_type in data_set.instance_type_data) {
            if (!instance_type.startsWith('*FIXED_ARRAY')) continue;
            const subtype = data_set.instance_type_data[instance_type];
            known_count += subtype.count;
            known_overall += subtype.count;
            for (let i = 0; i < subtype.histogram.length; i++) {
              known_histogram[i] += subtype.histogram[i];
            }
          }

          const fixed_array_data = data_set.instance_type_data.FIXED_ARRAY_TYPE;
          const unknown_entry = {
            count: fixed_array_data.count - known_count,
            overall: fixed_array_data.overall - known_overall,
            histogram: fixed_array_data.histogram.map(
                (value, index) => value - known_histogram[index])
          };

          // Check for non-negative values.
          checkNonNegativeProperty(unknown_entry, 'count');
          checkNonNegativeProperty(unknown_entry, 'overall');
          for (let i = 0; i < unknown_entry.histogram.length; i++) {
            checkNonNegativeProperty(unknown_entry.histogram, i);
          }

          data_set.instance_type_data['*FIXED_ARRAY_UNKNOWN_SUB_TYPE'] =
              unknown_entry;
          data_set.non_empty_instance_types.add(
              '*FIXED_ARRAY_UNKNOWN_SUB_TYPE');
        }
      }
    }
  }

  createModelFromChromeTraceFile(contents) {
    console.log('Processing log as chrome trace file.');
    const data = Object.create(null);  // Final data container.
    const keys = Object.create(null);  // Collecting 'keys' per isolate.

    // Pop last line in log as it might be broken.
    contents.pop();
    // Remove trailing comma.
    contents[contents.length - 1] = contents[contents.length - 1].slice(0, -1);
    // Terminate JSON.
    const sanitized_contents = [...contents, ']}'].join('');
    try {
      const raw_data = JSON.parse(sanitized_contents);
      const objects_stats_data =
          raw_data.traceEvents.filter(e => e.name == 'V8.GC_Objects_Stats');
      objects_stats_data.forEach(trace_data => {
        const actual_data = trace_data.args;
        const data_sets = new Set(Object.keys(actual_data));
        Object.keys(actual_data).forEach(data_set => {
          const string_entry = actual_data[data_set];
          try {
            const entry = JSON.parse(string_entry);
            this.createOrUpdateEntryIfNeeded(data, keys, entry);
            this.createDatasetIfNeeded(data, keys, entry, data_set);
            const isolate = entry.isolate;
            const time = entry.time;
            const gc_id = entry.id;
            data[isolate].gcs[gc_id].time = time;
            data[isolate].gcs[gc_id][data_set].bucket_sizes =
                entry.bucket_sizes;
            for (let [instance_type, value] of Object.entries(
                     entry.type_data)) {
              // Trace file format uses markers that do not have actual
              // properties.
              if (!('overall' in value)) continue;
              this.addInstanceTypeData(
                  data, keys, isolate, gc_id, data_set, instance_type, value);
            }
          } catch (e) {
            console.log('Unable to parse data set entry', e);
          }
        });
      });
    } catch (e) {
      console.log('Unable to parse chrome trace file.', e);
    }
    this.extendAndSanitizeModel(data, keys);
    return data;
  }

  createModelFromV8TraceFile(contents) {
    console.log('Processing log as V8 trace file.');
    contents = contents.map(function(line) {
      try {
        // Strip away a potentially present adb logcat prefix.
        line = line.replace(/^I\/v8\s*\(\d+\):\s+/g, '');
        return JSON.parse(line);
      } catch (e) {
        console.log('Unable to parse line: \'' + line + '\'\' (' + e + ')');
      }
      return null;
    });

    const data = Object.create(null);  // Final data container.
    const keys = Object.create(null);  // Collecting 'keys' per isolate.

    for (var entry of contents) {
      if (entry === null || entry.type === undefined) {
        continue;
      }
      if (entry.type === 'zone') {
        this.createOrUpdateEntryIfNeeded(data, keys, entry);
        const stacktrace = ('stacktrace' in entry) ? entry.stacktrace : [];
        data[entry.isolate].samples.zone[entry.time] = {
          allocated: entry.allocated,
          pooled: entry.pooled,
          stacktrace: stacktrace
        };
      } else if (
          entry.type === 'zonecreation' || entry.type === 'zonedestruction') {
        this.createOrUpdateEntryIfNeeded(data, keys, entry);
        data[entry.isolate].zonetags.push(
            Object.assign({opening: entry.type === 'zonecreation'}, entry));
      } else if (entry.type === 'gc_descriptor') {
        this.createOrUpdateEntryIfNeeded(data, keys, entry);
        data[entry.isolate].gcs[entry.id].time = entry.time;
        if ('zone' in entry)
          data[entry.isolate].gcs[entry.id].malloced = entry.zone;
      } else if (entry.type === 'instance_type_data') {
        if (entry.id in data[entry.isolate].gcs) {
          this.createOrUpdateEntryIfNeeded(data, keys, entry);
          this.createDatasetIfNeeded(data, keys, entry, entry.key);
          this.addInstanceTypeData(
              data, keys, entry.isolate, entry.id, entry.key,
              entry.instance_type_name, entry);
        }
      } else if (entry.type === 'bucket_sizes') {
        if (entry.id in data[entry.isolate].gcs) {
          this.createOrUpdateEntryIfNeeded(data, keys, entry);
          this.createDatasetIfNeeded(data, keys, entry, entry.key);
          data[entry.isolate].gcs[entry.id][entry.key].bucket_sizes =
              entry.sizes;
        }
      } else {
        console.log('Unknown entry type: ' + entry.type);
      }
    }
    this.extendAndSanitizeModel(data, keys);
    return data;
  }
}

customElements.define('trace-file-reader', TraceFileReader);
