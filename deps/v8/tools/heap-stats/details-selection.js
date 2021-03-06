// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {CATEGORIES, CATEGORY_NAMES} from './categories.js';

export const VIEW_BY_INSTANCE_TYPE = 'by-instance-type';
export const VIEW_BY_INSTANCE_CATEGORY = 'by-instance-category';
export const VIEW_BY_FIELD_TYPE = 'by-field-type';

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
    this.datasetSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
    this.gcSelect.addEventListener(
      'change', e => this.notifySelectionChanged(e));
    this.$('#csv-export-btn')
        .addEventListener('click', e => this.exportCurrentSelection(e));
    this.$('#category-filter-btn')
        .addEventListener('click', e => this.filterCurrentSelection(e));
    this.$('#category-auto-filter-btn')
        .addEventListener('click', e => this.filterTop20Categories(e));
    this._data = undefined;
    this.selection = undefined;
  }

  connectedCallback() {
    for (let category of CATEGORIES.keys()) {
      this.$('#categories').appendChild(this.buildCategory(category));
    }
  }

  dataChanged() {
    this.selection = {categories: {}};
    this.resetUI(true);
    this.populateIsolateSelect();
    this.handleIsolateChange();
    this.$('#dataSelectionSection').style.display = 'block';
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
    return this.selectedIsolate.gcs[this.selection.gc][this.selection.data_set];
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

  get datasetSelect() {
    return this.$('#dataset-select');
  }

  get isolateSelect() {
    return this.$('#isolate-select');
  }

  get gcSelect() {
    return this.$('#gc-select');
  }

  buildCategory(name) {
    const div = document.createElement('div');
    div.id = name;
    div.classList.add('box');

    let ul = document.createElement('ul');
    ul.className = 'categoryLabels'
    {
      const name_li = document.createElement('li');
      name_li.textContent = CATEGORY_NAMES.get(name);
      ul.appendChild(name_li);

      const percent_li = document.createElement('li');
      percent_li.textContent = '0%';
      percent_li.id = name + 'PercentContent';
      ul.appendChild(percent_li);
    }
    div.appendChild(ul);

    ul = document.createElement('ul');
    ul.className = 'categorySelectionButtons'
    {
      const all_li = document.createElement('li');
      const all_button = document.createElement('button');
      all_button.textContent = 'All';
      all_button.addEventListener('click', e => this.selectCategory(name));
      all_li.appendChild(all_button);
      ul.appendChild(all_li);

      const top_li = document.createElement('li');
      const top_button = document.createElement('button');
      top_button.textContent = 'Top 10';
      top_button.addEventListener(
          'click', e => this.selectCategoryTopEntries(name));
      top_li.appendChild(top_button);
      ul.appendChild(top_li);

      const none_li = document.createElement('li');
      const none_button = document.createElement('button');
      none_button.textContent = 'None';
      none_button.addEventListener('click', e => this.unselectCategory(name));
      none_li.appendChild(none_button);
      ul.appendChild(none_li);
    }
    div.appendChild(ul);

    const innerDiv = document.createElement('div');
    innerDiv.id = name + 'Content';
    innerDiv.className = 'categoryContent';
    div.appendChild(innerDiv);

    const percentDiv = document.createElement('div');
    percentDiv.className = 'percentBackground';
    percentDiv.id = name + 'PercentBackground';
    div.appendChild(percentDiv);
    return div;
  }

  populateIsolateSelect() {
    let isolates = Object.entries(this.data);
    // Sorty by peak heap memory consumption.
    isolates.sort((a, b) => b[1].peakMemory - a[1].peakMemory);
    this.populateSelect(
        '#isolate-select', isolates, (key, isolate) => isolate.getLabel());
  }

  resetUI(resetIsolateSelect) {
    if (resetIsolateSelect) removeAllChildren(this.isolateSelect);

    removeAllChildren(this.dataViewSelect);
    removeAllChildren(this.datasetSelect);
    removeAllChildren(this.gcSelect);
    this.clearCategories();
    this.setButtonState('disabled');
  }

  setButtonState(disabled) {
    this.$('#csv-export-btn').disabled = disabled;
    this.$('#category-filter').disabled = disabled;
    this.$('#category-filter-btn').disabled = disabled;
    this.$('#category-auto-filter-btn').disabled = disabled;
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
          [VIEW_BY_INSTANCE_TYPE, 'Selected instance types'],
          [VIEW_BY_INSTANCE_CATEGORY, 'Selected type categories'],
          [VIEW_BY_FIELD_TYPE, 'Field type statistics']
        ],
        (key, label) => label, VIEW_BY_INSTANCE_TYPE);
    this.populateSelect(
        '#dataset-select', this.selectedIsolate.data_sets.entries(), null,
        'live');
    this.populateSelect(
        '#gc-select',
        Object.keys(this.selectedIsolate.gcs)
            .map(id => [id, this.selectedIsolate.gcs[id].time]),
        (key, time, index) => {
          return (index + ': ').padStart(4, '0') +
              formatSeconds(time).padStart(6, '0') + ' ' +
              formatBytes(this.selectedIsolate.gcs[key].live.overall)
                  .padStart(9, '0');
        });
    this.populateCategories();
    this.notifySelectionChanged();
  }

  notifySelectionChanged(e) {
    if (!this.selection.isolate) return;

    this.selection.data_view = this.dataViewSelect.value;
    this.selection.categories = {};
    if (this.selection.data_view === VIEW_BY_FIELD_TYPE) {
      this.$('#categories').style.display = 'none';
    } else {
      for (let category of CATEGORIES.keys()) {
        const selected = this.selectedInCategory(category);
        if (selected.length > 0) this.selection.categories[category] = selected;
      }
      this.$('#categories').style.display = 'block';
    }
    this.selection.category_names = CATEGORY_NAMES;
    this.selection.data_set = this.datasetSelect.value;
    this.selection.gc = this.gcSelect.value;
    this.setButtonState(false);
    this.updatePercentagesInCategory();
    this.updatePercentagesInInstanceTypes();
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: this.selection}));
  }

  filterCurrentSelection(e) {
    const minSize = this.$('#category-filter').value * KB;
    this.filterCurrentSelectionWithThresold(minSize);
  }

  filterTop20Categories(e) {
    // Limit to show top 20 categories only.
    let minSize = 0;
    let count = 0;
    let sizes = this.selectedIsolate.instanceTypePeakMemory;
    for (let key in sizes) {
      if (count == 20) break;
      minSize = sizes[key];
      count++;
    }
    this.filterCurrentSelectionWithThresold(minSize);
  }

  filterCurrentSelectionWithThresold(minSize) {
    if (minSize === 0) return;

    this.selection.category_names.forEach((_, category) => {
      for (let checkbox of this.querySelectorAll(
               'input[name=' + category + 'Checkbox]')) {
        checkbox.checked =
            this.selectedData.instance_type_data[checkbox.instance_type]
                .overall > minSize;
        console.log(
            checkbox.instance_type, checkbox.checked,
            this.selectedData.instance_type_data[checkbox.instance_type]
                .overall);
      }
    });
    this.notifySelectionChanged();
  }

  updatePercentagesInCategory() {
    const overalls = {};
    let overall = 0;
    // Reset all categories.
    this.selection.category_names.forEach((_, category) => {
      overalls[category] = 0;
    });
    // Only update categories that have selections.
    Object.entries(this.selection.categories).forEach(([category, value]) => {
      overalls[category] =
          Object.values(value).reduce(
              (accu, current) =>
                  accu + this.selectedData.instance_type_data[current].overall,
              0) /
          KB;
      overall += overalls[category];
    });
    Object.entries(overalls).forEach(([category, category_overall]) => {
      let percents = category_overall / overall * 100;
      this.$(`#${category}PercentContent`).textContent =
          `${percents.toFixed(1)}%`;
      this.$('#' + category + 'PercentBackground').style.left = percents + '%';
    });
  }

  updatePercentagesInInstanceTypes() {
    const instanceTypeData = this.selectedData.instance_type_data;
    const maxInstanceType = this.selectedData.singleInstancePeakMemory;
    this.querySelectorAll('.instanceTypeSelectBox  input').forEach(checkbox => {
      let instanceType = checkbox.value;
      let instanceTypeSize = instanceTypeData[instanceType].overall;
      let percents = instanceTypeSize / maxInstanceType;
      let percentDiv = checkbox.parentNode.querySelector('.percentBackground');
      percentDiv.style.left = (percents * 100) + '%';

    });
  }

  selectedInCategory(category) {
    let tmp = [];
    this.querySelectorAll('input[name=' + category + 'Checkbox]:checked')
        .forEach(checkbox => tmp.push(checkbox.value));
    return tmp;
  }

  categoryForType(instance_type) {
    for (let [key, value] of CATEGORIES.entries()) {
      if (value.has(instance_type)) return key;
    }
    return 'unclassified';
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
    const categories = {__proto__:null};
    for (let cat of CATEGORIES.keys()) {
      categories[cat] = [];
    }

    for (let instance_type of this.selectedIsolate.non_empty_instance_types) {
      const category = this.categoryForType(instance_type);
      categories[category].push(instance_type);
    }
    for (let category of Object.keys(categories)) {
      categories[category].sort();
      for (let instance_type of categories[category]) {
        this.$('#' + category + 'Content')
            .appendChild(this.createCheckBox(instance_type, category));
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

  selectCategoryTopEntries(category) {
    // unselect all checkboxes in this category.
    this.querySelectorAll('input[name=' + category + 'Checkbox]')
        .forEach(checkbox => checkbox.checked = false);
    const data = this.selectedData.instance_type_data;

    // Get the max values for instance_types in this category
    const categoryInstanceTypes = Array.from(CATEGORIES.get(category));
    categoryInstanceTypes.filter(each => each in data)
      .sort((a,b) => {
        return data[b].overall - data[a].overall;
      }).slice(0, 10).forEach((category) => {
        this.$('#' + category + 'Checkbox').checked = true;
      });
    this.notifySelectionChanged();
  }

  createCheckBox(instance_type, category) {
    const div = document.createElement('div');
    div.classList.add('instanceTypeSelectBox');
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

  exportCurrentSelection(e) {
    const data = [];
    const selected_data =
        this.selectedIsolate.gcs[this.selection.gc][this.selection.data_set]
            .instance_type_data;
    Object.values(this.selection.categories).forEach(instance_types => {
      instance_types.forEach(instance_type => {
        data.push([instance_type, selected_data[instance_type].overall / KB]);
      });
    });
    const createInlineContent = arrayOfRows => {
      const content = arrayOfRows.reduce(
          (accu, rowAsArray) => {return accu + `${rowAsArray.join(',')}\n`},
          '');
      return `data:text/csv;charset=utf-8,${content}`;
    };
    const encodedUri = encodeURI(createInlineContent(data));
    const link = document.createElement('a');
    link.setAttribute('href', encodedUri);
    link.setAttribute(
        'download',
        `heap_objects_data_${this.selection.isolate}_${this.selection.gc}.csv`);
    this.shadowRoot.appendChild(link);
    link.click();
    this.shadowRoot.removeChild(link);
  }
});
