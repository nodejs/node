// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const histogram_viewer_template =
    document.currentScript.ownerDocument.querySelector(
        '#histogram-viewer-template');

class HistogramViewer extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.appendChild(histogram_viewer_template.content.cloneNode(true));
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
  }

  set data(value) {
    this._data = value;
    this.stateChanged();
  }

  get data() {
    return this._data;
  }

  set selection(value) {
    this._selection = value;
    this.stateChanged();
  }

  get selection() {
    return this._selection;
  }

  isValid() {
    return this.data && this.selection &&
           (this.selection.data_view === VIEW_BY_INSTANCE_CATEGORY ||
            this.selection.data_view === VIEW_BY_INSTANCE_TYPE);
    ;
  }

  hide() {
    this.$('#container').style.display = 'none';
  }

  show() {
    this.$('#container').style.display = 'block';
  }

  getOverallValue() {
    switch (this.selection.data_view) {
      case VIEW_BY_FIELD_TYPE:
        return NaN;
      case VIEW_BY_INSTANCE_CATEGORY:
        return this.getPropertyForCategory('overall');
      case VIEW_BY_INSTANCE_TYPE:
      default:
        return this.getPropertyForInstanceTypes('overall');
    }
  }

  stateChanged() {
    if (this.isValid()) {
      const overall_bytes = this.getOverallValue();
      this.$('#overall').innerHTML = `Overall: ${overall_bytes / KB} KB`;
      this.drawChart();
    } else {
      this.hide();
    }
  }

  get selectedData() {
    console.assert(this.data, 'invalid data');
    console.assert(this.selection, 'invalid selection');
    return this.data[this.selection.isolate]
        .gcs[this.selection.gc][this.selection.data_set];
  }

  get selectedInstanceTypes() {
    console.assert(this.selection, 'invalid selection');
    return Object.values(this.selection.categories)
        .reduce((accu, current) => accu.concat(current), []);
  }

  getPropertyForCategory(property) {
    return Object.values(this.selection.categories)
        .reduce(
            (outer_accu, instance_types) => outer_accu +
                instance_types.reduce(
                    (inner_accu, instance_type) => inner_accu +
                        this.selectedData
                            .instance_type_data[instance_type][property],
                    0),
            0);
  }

  getPropertyForInstanceTypes(property) {
    return this.selectedInstanceTypes.reduce(
        (accu, instance_type) => accu +
            this.selectedData.instance_type_data[instance_type][property],
        0);
  }

  formatBytes(bytes) {
    const units = ['B', 'KiB', 'MiB'];
    const divisor = 1024;
    let index = 0;
    while (index < units.length && bytes >= divisor) {
      index++;
      bytes /= divisor;
    }
    return bytes + units[index];
  }

  getCategoryData() {
    const labels = [
      'Bucket',
      ...Object.keys(this.selection.categories)
          .map(k => this.selection.category_names.get(k))
    ];
    const data = this.selectedData.bucket_sizes.map(
        (bucket_size, index) =>
            [`<${this.formatBytes(bucket_size)}`,
             ...Object.values(this.selection.categories)
                 .map(
                     instance_types =>
                         instance_types
                             .map(
                                 instance_type =>
                                     this.selectedData
                                         .instance_type_data[instance_type]
                                         .histogram[index])
                             .reduce((accu, current) => accu + current, 0))]);
    // Adjust last histogram bucket label.
    data[data.length - 1][0] = 'rest';
    return [labels, ...data];
  }

  getInstanceTypeData() {
    const instance_types = this.selectedInstanceTypes;
    const labels = ['Bucket', ...instance_types];
    const data = this.selectedData.bucket_sizes.map(
        (bucket_size, index) =>
            [`<${bucket_size}`,
             ...instance_types.map(
                 instance_type =>
                     this.selectedData.instance_type_data[instance_type]
                         .histogram[index])]);
    // Adjust last histogram bucket label.
    data[data.length - 1][0] = 'rest';
    return [labels, ...data];
  }

  getChartData() {
    switch (this.selection.data_view) {
      case VIEW_BY_FIELD_TYPE:
        return this.getFieldData();
      case VIEW_BY_INSTANCE_CATEGORY:
        return this.getCategoryData();
      case VIEW_BY_INSTANCE_TYPE:
      default:
        return this.getInstanceTypeData();
    }
  }

  drawChart() {
    const chart_data = this.getChartData();
    const data = google.visualization.arrayToDataTable(chart_data);
    const options = {
      legend: {position: 'top', maxLines: '1'},
      chartArea: {width: '85%', height: '85%'},
      bar: {groupWidth: '80%'},
      hAxis: {
        title: 'Count',
        minValue: 0
      },
      explorer: {},
    };
    const chart = new google.visualization.BarChart(this.$('#chart'));
    this.show();
    chart.draw(data, options);
  }
}

customElements.define('histogram-viewer', HistogramViewer);
