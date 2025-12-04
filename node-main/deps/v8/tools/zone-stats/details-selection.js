// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {CATEGORIES, CATEGORY_NAMES, categoryByZoneName} from './categories.js';

export const VIEW_TOTALS = 'by-totals';
export const VIEW_BY_ZONE_NAME = 'by-zone-name';
export const VIEW_BY_ZONE_CATEGORY = 'by-zone-category';

export const KIND_ALLOCATED_MEMORY = 'kind-detailed-allocated';
export const KIND_USED_MEMORY = 'kind-detailed-used';
export const KIND_FREED_MEMORY = 'kind-detailed-freed';

defineCustomElement('details-selection', (templateText) =>
 class DetailsSelection extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = templateText;
    this.isolateSelect.addEventListener(
        'change', e => this.handleIsolateChange(e));
    this.dataViewSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
    this.dataKindSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
    this.showTotalsSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
    this.memoryUsageSampleSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
    this.timeStartSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
    this.timeEndSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
  }

  connectedCallback() {
    for (let category of CATEGORIES.keys()) {
      this.$('#categories').appendChild(this.buildCategory(category));
    }
  }

  set data(value) {
    this._data = value;
    this.dataChanged();
  }

  get data() {
    return this._data;
  }

  get selectedIsolate() {
    return this._data[this.selection.isolate];
  }

  get selectedData() {
    console.assert(this.data, 'invalid data');
    console.assert(this.selection, 'invalid selection');
    const time = this.selection.time;
    return this.selectedIsolate.samples.get(time);
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
  }

  querySelectorAll(query) {
    return this.shadowRoot.querySelectorAll(query);
  }

  get dataViewSelect() {
    return this.$('#data-view-select');
  }

  get dataKindSelect() {
    return this.$('#data-kind-select');
  }

  get isolateSelect() {
    return this.$('#isolate-select');
  }

  get memoryUsageSampleSelect() {
    return this.$('#memory-usage-sample-select');
  }

  get showTotalsSelect() {
    return this.$('#show-totals-select');
  }

  get timeStartSelect() {
    return this.$('#time-start-select');
  }

  get timeEndSelect() {
    return this.$('#time-end-select');
  }

  buildCategory(name) {
    const div = document.createElement('div');
    div.id = name;
    div.classList.add('box');
    const ul = document.createElement('ul');
    div.appendChild(ul);
    const name_li = document.createElement('li');
    ul.appendChild(name_li);
    name_li.innerHTML = CATEGORY_NAMES.get(name);
    const percent_li = document.createElement('li');
    ul.appendChild(percent_li);
    percent_li.innerHTML = '0%';
    percent_li.id = name + 'PercentContent';
    const all_li = document.createElement('li');
    ul.appendChild(all_li);
    const all_button = document.createElement('button');
    all_li.appendChild(all_button);
    all_button.innerHTML = 'All';
    all_button.addEventListener('click', e => this.selectCategory(name));
    const none_li = document.createElement('li');
    ul.appendChild(none_li);
    const none_button = document.createElement('button');
    none_li.appendChild(none_button);
    none_button.innerHTML = 'None';
    none_button.addEventListener('click', e => this.unselectCategory(name));
    const innerDiv = document.createElement('div');
    div.appendChild(innerDiv);
    innerDiv.id = name + 'Content';
    const percentDiv = document.createElement('div');
    div.appendChild(percentDiv);
    percentDiv.className = 'percentBackground';
    percentDiv.id = name + 'PercentBackground';
    return div;
  }

  dataChanged() {
    this.selection = {categories: {}, zones: new Map()};
    this.resetUI(true);
    this.populateIsolateSelect();
    this.handleIsolateChange();
    this.$('#dataSelectionSection').style.display = 'block';
  }

  populateIsolateSelect() {
    let isolates = Object.entries(this.data);
    // Sort by peak heap memory consumption.
    isolates.sort((a, b) => b[1].peakAllocatedMemory - a[1].peakAllocatedMemory);
    this.populateSelect(
        '#isolate-select', isolates, (key, isolate) => isolate.getLabel());
  }

  resetUI(resetIsolateSelect) {
    if (resetIsolateSelect) removeAllChildren(this.isolateSelect);

    removeAllChildren(this.dataViewSelect);
    removeAllChildren(this.dataKindSelect);
    removeAllChildren(this.memoryUsageSampleSelect);
    this.clearCategories();
  }

  handleIsolateChange(e) {
    this.selection.isolate = this.isolateSelect.value;
    if (this.selection.isolate.length === 0) {
      this.selection.isolate = null;
      return;
    }
    this.resetUI(false);
    this.populateSelect(
        '#data-view-select', [
          [VIEW_TOTALS, 'Total memory usage'],
          [VIEW_BY_ZONE_NAME, 'Selected zones types'],
          [VIEW_BY_ZONE_CATEGORY, 'Selected zone categories'],
        ],
        (key, label) => label, VIEW_TOTALS);
    this.populateSelect(
      '#data-kind-select', [
        [KIND_ALLOCATED_MEMORY, 'Allocated memory per zone'],
        [KIND_USED_MEMORY, 'Used memory per zone'],
        [KIND_FREED_MEMORY, 'Freed memory per zone'],
      ],
      (key, label) => label, KIND_ALLOCATED_MEMORY);

    this.populateSelect(
      '#memory-usage-sample-select',
      [...this.selectedIsolate.samples.entries()].filter(([time, sample]) => {
        // Remove samples that does not have detailed per-zone data.
        return sample.zones !== undefined;
      }),
      (time, sample, index) => {
        return ((index + ': ').padStart(6, '\u00A0') +
          formatSeconds(time).padStart(8, '\u00A0') + ' ' +
          formatBytes(sample.allocated).padStart(12, '\u00A0'));
      },
      this.selectedIsolate.peakUsageTime);

    this.timeStartSelect.value = this.selectedIsolate.start;
    this.timeEndSelect.value = this.selectedIsolate.end;

    this.populateCategories();
    this.notifySelectionChanged();
  }

  notifySelectionChanged(e) {
    if (!this.selection.isolate) return;

    this.selection.data_view = this.dataViewSelect.value;
    this.selection.data_kind = this.dataKindSelect.value;
    this.selection.categories = Object.create(null);
    this.selection.zones = new Map();
    this.$('#categories').style.display = 'none';
    for (let category of CATEGORIES.keys()) {
      const selected = this.selectedInCategory(category);
      if (selected.length > 0) this.selection.categories[category] = selected;
      for (const zone_name of selected) {
        this.selection.zones.set(zone_name, category);
      }
    }
    this.$('#categories').style.display = 'block';
    this.selection.category_names = CATEGORY_NAMES;
    this.selection.show_totals = this.showTotalsSelect.checked;
    this.selection.time = Number(this.memoryUsageSampleSelect.value);
    this.selection.timeStart = Number(this.timeStartSelect.value);
    this.selection.timeEnd = Number(this.timeEndSelect.value);
    this.updatePercentagesInCategory();
    this.updatePercentagesInZones();
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: this.selection}));
  }

  updatePercentagesInCategory() {
    const overalls = Object.create(null);
    let overall = 0;
    // Reset all categories.
    this.selection.category_names.forEach((_, category) => {
      overalls[category] = 0;
    });
    // Only update categories that have selections.
    Object.entries(this.selection.categories).forEach(([category, value]) => {
      overalls[category] =
          Object.values(value).reduce(
              (accu, current) => {
                  const zone_data = this.selectedData.zones.get(current);
                  return zone_data === undefined ? accu
                                                 : accu + zone_data.allocated;
              }, 0) /
          KB;
      overall += overalls[category];
    });
    Object.entries(overalls).forEach(([category, category_overall]) => {
      let percents = category_overall / overall * 100;
      this.$(`#${category}PercentContent`).innerHTML =
          `${percents.toFixed(1)}%`;
      this.$('#' + category + 'PercentBackground').style.left = percents + '%';
    });
  }

  updatePercentagesInZones() {
    const selected_data = this.selectedData;
    const zones_data = selected_data.zones;
    const total_allocated = selected_data.allocated;
    this.querySelectorAll('.zonesSelectBox  input').forEach(checkbox => {
      const zone_name = checkbox.value;
      const zone_data = zones_data.get(zone_name);
      const zone_allocated = zone_data === undefined ? 0 : zone_data.allocated;
      const percents = zone_allocated / total_allocated;
      const percent_div = checkbox.parentNode.querySelector('.percentBackground');
      percent_div.style.left = (percents * 100) + '%';
      checkbox.parentNode.style.display = 'block';
    });
  }

  selectedInCategory(category) {
    let tmp = [];
    this.querySelectorAll('input[name=' + category + 'Checkbox]:checked')
        .forEach(checkbox => tmp.push(checkbox.value));
    return tmp;
  }

  createOption(value, text) {
    const option = document.createElement('option');
    option.value = value;
    option.text = text;
    return option;
  }

  populateSelect(id, iterable, labelFn = null, autoselect = null) {
    if (labelFn == null) labelFn = e => e;
    let index = 0;
    for (let [key, value] of iterable) {
      index++;
      const label = labelFn(key, value, index);
      const option = this.createOption(key, label);
      if (autoselect === key) {
        option.selected = 'selected';
      }
      this.$(id).appendChild(option);
    }
  }

  clearCategories() {
    for (const category of CATEGORIES.keys()) {
      let f = this.$('#' + category + 'Content');
      while (f.firstChild) {
        f.removeChild(f.firstChild);
      }
    }
  }

  populateCategories() {
    this.clearCategories();
    const categories = Object.create(null);
    for (let cat of CATEGORIES.keys()) {
      categories[cat] = [];
    }

    for (const [zone_name, zone_stats] of this.selectedIsolate.zones) {
      const category = categoryByZoneName(zone_name);
      categories[category].push(zone_name);
    }
    for (let category of Object.keys(categories)) {
      categories[category].sort();
      for (let zone_name of categories[category]) {
        this.$('#' + category + 'Content')
            .appendChild(this.createCheckBox(zone_name, category));
      }
    }
  }

  unselectCategory(category) {
    this.querySelectorAll('input[name=' + category + 'Checkbox]')
        .forEach(checkbox => checkbox.checked = false);
    this.notifySelectionChanged();
  }

  selectCategory(category) {
    this.querySelectorAll('input[name=' + category + 'Checkbox]')
        .forEach(checkbox => checkbox.checked = true);
    this.notifySelectionChanged();
  }

  createCheckBox(instance_type, category) {
    const div = document.createElement('div');
    div.classList.add('zonesSelectBox');
    div.style.width = "200px";
    const input = document.createElement('input');
    div.appendChild(input);
    input.type = 'checkbox';
    input.name = category + 'Checkbox';
    input.checked = 'checked';
    input.id = instance_type + 'Checkbox';
    input.instance_type = instance_type;
    input.value = instance_type;
    input.addEventListener('change', e => this.notifySelectionChanged(e));
    const label = document.createElement('label');
    div.appendChild(label);
    label.innerText = instance_type;
    label.htmlFor = instance_type + 'Checkbox';
    const percentDiv = document.createElement('div');
    percentDiv.className = 'percentBackground';
    div.appendChild(percentDiv);
    return div;
  }
});
