// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const details_selection_template =
    document.currentScript.ownerDocument.querySelector(
        '#details-selection-template');

class DetailsSelection extends HTMLElement {
  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.appendChild(details_selection_template.content.cloneNode(true));
    this.isolateSelect.addEventListener(
        'change', e => this.handleIsolateChange(e));
    this.datasetSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
    this.gcSelect.addEventListener(
        'change', e => this.notifySelectionChanged(e));
    this.$('#csv-export-btn')
        .addEventListener('click', e => this.exportCurrentSelection(e));
    this.$('#merge-categories')
        .addEventListener('change', e => this.notifySelectionChanged(e));
    this.$('#category-filter-btn')
        .addEventListener('click', e => this.filterCurrentSeclection(e));
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

  get selectedData() {
    console.assert(this.data, 'invalid data');
    console.assert(this.selection, 'invalid selection');
    return this.data[this.selection.isolate]
        .gcs[this.selection.gc][this.selection.data_set];
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
    return div;
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
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

  dataChanged() {
    this.selection = {categories: {}};
    this.resetUI(true);
    this.populateIsolateSelect();
    this.handleIsolateChange();
    this.$('#dataSelectionSection').style.display = 'block';
  }

  populateIsolateSelect() {
    let entries = Object.entries(this.data);
    // Sorty by peak heap memory consumption.
    entries.sort((a, b) => b[1].peakMemory - a[1].peakMemory);
    this.populateSelect(
        '#isolate-select', entries, (key, isolate) => isolate.getLabel());
  }

  resetUI(resetIsolateSelect) {
    if (resetIsolateSelect) removeAllChildren(this.isolateSelect);

    removeAllChildren(this.datasetSelect);
    removeAllChildren(this.gcSelect);
    this.clearCategories();
    this.$('#csv-export-btn').disabled = 'disabled';
    this.$('#category-filter-btn').disabled = 'disabled';
    this.$('#category-filter').disabled = 'disabled';
  }

  handleIsolateChange(e) {
    this.selection.isolate = this.isolateSelect.value;
    if (this.selection.isolate.length === 0) {
      this.selection.isolate = null;
      return;
    }
    this.resetUI(false);
    this.populateSelect(
        '#dataset-select',
        this.data[this.selection.isolate].data_sets.entries(), null, 'live');
    this.populateSelect(
        '#gc-select',
        Object.keys(this.data[this.selection.isolate].gcs)
            .map(v => [v, this.data[this.selection.isolate].gcs[v].time]),
        time => time + 'ms');
    this.populateCategories();
    this.notifySelectionChanged();
  }

  notifySelectionChanged(e) {
    if (!this.selection.isolate) return;

    this.selection.categories = {};
    for (let category of CATEGORIES.keys()) {
      const selected = this.selectedInCategory(category);
      if (selected.length > 0) this.selection.categories[category] = selected;
    }
    this.selection.category_names = CATEGORY_NAMES;
    this.selection.data_set = this.datasetSelect.value;
    this.selection.merge_categories = this.$('#merge-categories').checked;
    this.selection.gc = this.gcSelect.value;
    this.$('#csv-export-btn').disabled = false;
    this.$('#category-filter-btn').disabled = false;
    this.$('#category-filter').disabled = false;
    this.updatePercentagesInCategory();
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: this.selection}));
  }

  filterCurrentSeclection(e) {
    const filter_value = this.$('#category-filter').value * KB;
    if (filter_value === 0) return;

    this.selection.category_names.forEach((_, category) => {
      for (let checkbox of this.shadowRoot.querySelectorAll(
               'input[name=' + category + 'Checkbox]')) {
        checkbox.checked =
            this.selectedData.instance_type_data[checkbox.instance_type]
                .overall > filter_value;
      }
    });
    this.notifySelectionChanged();
  }

  updatePercentagesInCategory() {
    const overalls = {};
    let overall = 0;
    // Reset all categories.
    this.selection.category_names.forEach((_, category) => {
      this.$(`#${category}PercentContent`).innerHTML = '0%';
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
      this.$(`#${category}PercentContent`).innerHTML =
          `${(category_overall / overall * 100).toFixed(1)}%`;
    });
  }

  selectedInCategory(category) {
    const selected = this.shadowRoot.querySelectorAll(
        'input[name=' + category + 'Checkbox]:checked');
    var tmp = [];
    for (var val of selected.values()) tmp.push(val.value);
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
    for (let [key, value] of iterable) {
      const label = labelFn(key, value);
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
    const categories = {};
    for (let cat of CATEGORIES.keys()) {
      categories[cat] = [];
    }

    for (let instance_type of this.data[this.selection.isolate]
             .non_empty_instance_types) {
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
    for (let checkbox of this.shadowRoot.querySelectorAll(
             'input[name=' + category + 'Checkbox]')) {
      checkbox.checked = false;
    }
    this.notifySelectionChanged();
  }

  selectCategory(category) {
    for (let checkbox of this.shadowRoot.querySelectorAll(
             'input[name=' + category + 'Checkbox]')) {
      checkbox.checked = true;
    }
    this.notifySelectionChanged();
  }

  createCheckBox(instance_type, category) {
    const div = document.createElement('div');
    div.classList.add('boxDiv');
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
    return div;
  }

  exportCurrentSelection(e) {
    const data = [];
    const selected_data = this.data[this.selection.isolate]
                              .gcs[this.selection.gc][this.selection.data_set]
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
}

customElements.define('details-selection', DetailsSelection);
