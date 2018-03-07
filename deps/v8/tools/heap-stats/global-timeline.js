// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const KB = 1024;
const MB = KB * KB;

const global_timeline_template =
    document.currentScript.ownerDocument.querySelector(
        '#global-timeline-template');

class GlobalTimeline extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.appendChild(global_timeline_template.content.cloneNode(true));
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
    return this.data && this.selection;
  }

  hide() {
    this.$('#container').style.display = 'none';
  }

  show() {
    this.$('#container').style.display = 'block';
  }

  stateChanged() {
    if (this.isValid()) {
      this.drawChart();
    } else {
      this.hide();
    }
  }

  getCategoryData() {
    const categories = Object.keys(this.selection.categories)
                           .map(k => this.selection.category_names.get(k));
    const labels = ['Time', ...categories];
    const chart_data = [labels];
    const isolate_data = this.data[this.selection.isolate];
    Object.keys(isolate_data.gcs).forEach(gc_key => {
      const gc_data = isolate_data.gcs[gc_key];
      const data_set = gc_data[this.selection.data_set].instance_type_data;
      const data = [];
      data.push(gc_data.time);
      Object.values(this.selection.categories).forEach(instance_types => {
        data.push(
            instance_types
                .map(instance_type => {
                  return data_set[instance_type].overall;
                })
                .reduce((accu, current) => accu + current, 0) /
            KB);
      });
      chart_data.push(data);
    });
    return chart_data;
  }

  getInstanceTypeData() {
    const categories = Object.keys(this.selection.categories);
    const instance_types =
        Object.values(this.selection.categories)
            .reduce((accu, current) => accu.concat(current), []);
    const labels = ['Time', ...instance_types];
    const chart_data = [labels];
    const isolate_data = this.data[this.selection.isolate];
    Object.keys(isolate_data.gcs).forEach(gc_key => {
      const gc_data = isolate_data.gcs[gc_key];
      const data_set = gc_data[this.selection.data_set].instance_type_data;
      const data = [];
      data.push(gc_data.time);
      instance_types.forEach(instance_type => {
        data.push(data_set[instance_type].overall / KB);
      });
      chart_data.push(data);
    });
    return chart_data;
  }

  drawChart() {
    console.assert(this.data, 'invalid data');
    console.assert(this.selection, 'invalid selection');

    const chart_data = (this.selection.merge_categories) ?
        this.getCategoryData() :
        this.getInstanceTypeData();
    const data = google.visualization.arrayToDataTable(chart_data);
    const options = {
      isStacked: true,
      hAxis: {
        title: 'Time [ms]',
      },
      vAxis: {title: 'Memory consumption [KBytes]'},
      chartArea: {width: '85%', height: '70%'},
      legend: {position: 'top', maxLines: '1'},
      pointsVisible: true,
      pointSize: 5,
      explorer: {},
    };
    const chart = new google.visualization.AreaChart(this.$('#chart'));
    this.show();
    chart.draw(data, google.charts.Line.convertOptions(options));
  }
}

customElements.define('global-timeline', GlobalTimeline);
