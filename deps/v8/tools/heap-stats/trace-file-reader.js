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
          const textResult = pako.inflate(e.target.result, {to: 'string'});
          this.processRawText(file, textResult);
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
          this.processRawText(file, e.target.result);
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

  processRawText(file, result) {
    let contents = result.split('\n');
    const return_data = (result.includes('V8.GC_Objects_Stats')) ?
        this.createModelFromChromeTraceFile(contents) :
        this.createModelFromV8TraceFile(contents);
    this.extendAndSanitizeModel(return_data);
    this.updateLabel('Finished loading \'' + file.name + '\'.');
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: return_data}));
  }

  createOrUpdateEntryIfNeeded(data, entry) {
    console.assert(entry.isolate, 'entry should have an isolate');
    if (!(entry.isolate in data)) {
      data[entry.isolate] = new Isolate(entry.isolate);
    }
    const data_object = data[entry.isolate];
    if (('id' in entry) && !(entry.id in data_object.gcs)) {
      data_object.gcs[entry.id] = {non_empty_instance_types: new Set()};
    }
    if ('time' in entry) {
      if (data_object.end === null || data_object.end < entry.time) {
        data_object.end = entry.time;
      }
      if (data_object.start === null || data_object.start > entry.time) {
        data_object.start = entry.time;
      }
    }
  }

  createDatasetIfNeeded(data, entry, data_set) {
    if (!(data_set in data[entry.isolate].gcs[entry.id])) {
      data[entry.isolate].gcs[entry.id][data_set] = {
        instance_type_data: {},
        non_empty_instance_types: new Set(),
        overall: 0
      };
      data[entry.isolate].data_sets.add(data_set);
    }
  }

  addFieldTypeData(data, isolate, gc_id, data_set, tagged_fields,
                   inobject_smi_fields, embedder_fields, unboxed_double_fields,
                   boxed_double_fields, string_data, other_raw_fields) {
    data[isolate].gcs[gc_id][data_set].field_data = {
      tagged_fields,
      inobject_smi_fields,
      embedder_fields,
      unboxed_double_fields,
      boxed_double_fields,
      string_data,
      other_raw_fields
    };
  }

  addInstanceTypeData(data, isolate, gc_id, data_set, instance_type, entry) {
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

  extendAndSanitizeModel(data) {
    const checkNonNegativeProperty = (obj, property) => {
      console.assert(obj[property] >= 0, 'negative property', obj, property);
    };

    Object.values(data).forEach(isolate => isolate.finalize());
  }

  createModelFromChromeTraceFile(contents) {
    // Trace files support two formats.
    // {traceEvents: [ data ]}
    const kObjectTraceFile = {
      name: 'object',
      endToken: ']}',
      getDataArray: o => o.traceEvents
    };
    // [ data ]
    const kArrayTraceFile = {
      name: 'array',
      endToken: ']',
      getDataArray: o => o
    };
    const handler =
        (contents[0][0] === '{') ? kObjectTraceFile : kArrayTraceFile;
    console.log(`Processing log as chrome trace file (${handler.name}).`);

    // Pop last line in log as it might be broken.
    contents.pop();
    // Remove trailing comma.
    contents[contents.length - 1] = contents[contents.length - 1].slice(0, -1);
    // Terminate JSON.
    const sanitized_contents = [...contents, handler.endToken].join('');

    const data = Object.create(null);  // Final data container.
    try {
      const raw_data = JSON.parse(sanitized_contents);
      const raw_array_data = handler.getDataArray(raw_data);
      raw_array_data.filter(e => e.name === 'V8.GC_Objects_Stats')
          .forEach(trace_data => {
            const actual_data = trace_data.args;
            const data_sets = new Set(Object.keys(actual_data));
            Object.keys(actual_data).forEach(data_set => {
              const string_entry = actual_data[data_set];
              try {
                const entry = JSON.parse(string_entry);
                this.createOrUpdateEntryIfNeeded(data, entry);
                this.createDatasetIfNeeded(data, entry, data_set);
                const isolate = entry.isolate;
                const time = entry.time;
                const gc_id = entry.id;
                data[isolate].gcs[gc_id].time = time;

                const field_data = entry.field_data;
                this.addFieldTypeData(data, isolate, gc_id, data_set,
                  field_data.tagged_fields,
                  field_data.inobject_smi_fields,
                  field_data.embedder_fields,
                  field_data.unboxed_double_fields,
                  field_data.boxed_double_fields,
                  field_data.string_data,
                  field_data.other_raw_fields);

                data[isolate].gcs[gc_id][data_set].bucket_sizes =
                    entry.bucket_sizes;
                for (let [instance_type, value] of Object.entries(
                         entry.type_data)) {
                  // Trace file format uses markers that do not have actual
                  // properties.
                  if (!('overall' in value)) continue;
                  this.addInstanceTypeData(
                      data, isolate, gc_id, data_set, instance_type, value);
                }
              } catch (e) {
                console.log('Unable to parse data set entry', e);
              }
            });
          });
    } catch (e) {
      console.error('Unable to parse chrome trace file.', e);
    }
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
        console.log('Unable to parse line: \'' + line + '\' (' + e + ')');
      }
      return null;
    });

    const data = Object.create(null);  // Final data container.
    for (var entry of contents) {
      if (entry === null || entry.type === undefined) {
        continue;
      }
      if (entry.type === 'zone') {
        this.createOrUpdateEntryIfNeeded(data, entry);
        const stacktrace = ('stacktrace' in entry) ? entry.stacktrace : [];
        data[entry.isolate].samples.zone[entry.time] = {
          allocated: entry.allocated,
          pooled: entry.pooled,
          stacktrace: stacktrace
        };
      } else if (
          entry.type === 'zonecreation' || entry.type === 'zonedestruction') {
        this.createOrUpdateEntryIfNeeded(data, entry);
        data[entry.isolate].zonetags.push(
            Object.assign({opening: entry.type === 'zonecreation'}, entry));
      } else if (entry.type === 'gc_descriptor') {
        this.createOrUpdateEntryIfNeeded(data, entry);
        data[entry.isolate].gcs[entry.id].time = entry.time;
        if ('zone' in entry)
          data[entry.isolate].gcs[entry.id].malloced = entry.zone;
      } else if (entry.type === 'field_data') {
        this.createOrUpdateEntryIfNeeded(data, entry);
        this.createDatasetIfNeeded(data, entry, entry.key);
        this.addFieldTypeData(data, entry.isolate, entry.id, entry.key,
          entry.tagged_fields, entry.embedder_fields, entry.inobject_smi_fields,
          entry.unboxed_double_fields, entry.boxed_double_fields,
          entry.string_data, entry.other_raw_fields);
      } else if (entry.type === 'instance_type_data') {
        if (entry.id in data[entry.isolate].gcs) {
          this.createOrUpdateEntryIfNeeded(data, entry);
          this.createDatasetIfNeeded(data, entry, entry.key);
          this.addInstanceTypeData(
              data, entry.isolate, entry.id, entry.key,
              entry.instance_type_name, entry);
        }
      } else if (entry.type === 'bucket_sizes') {
        if (entry.id in data[entry.isolate].gcs) {
          this.createOrUpdateEntryIfNeeded(data, entry);
          this.createDatasetIfNeeded(data, entry, entry.key);
          data[entry.isolate].gcs[entry.id][entry.key].bucket_sizes =
              entry.sizes;
        }
      } else {
        console.log('Unknown entry type: ' + entry.type);
      }
    }
    return data;
  }
}

customElements.define('trace-file-reader', TraceFileReader);
