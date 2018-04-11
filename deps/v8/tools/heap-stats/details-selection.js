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
    this.$('#merge-categories')
        .addEventListener('change', e => this.notifySelectionChanged(e));
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

  buildCategory(name) {
    const div = document.createElement('div');
    div.id = name;
    div.classList.add('box');
    const span = document.createElement('span');
    div.appendChild(span);
    span.innerHTML = CATEGORY_NAMES.get(name) + ' ';
    const all_button = document.createElement('button');
    span.appendChild(all_button);
    all_button.innerHTML = 'All';
    all_button.addEventListener('click', e => this.selectCategory(name));
    const none_button = document.createElement('button');
    span.appendChild(none_button);
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

  dataChanged() {
    this.clearUI();
    this.populateSelect('#isolate-select', Object.keys(this.data));
    this.handleIsolateChange();
  }

  clearUI() {
    this.selection = {categories: {}};
    removeAllChildren(this.isolateSelect);
    removeAllChildren(this.datasetSelect);
    this.clearCategories();
  }

  handleIsolateChange(e) {
    this.selection.isolate = this.isolateSelect.value;
    if (this.selection.isolate.length === 0) {
      this.selection.isolate = null;
      return;
    }

    this.populateSelect(
        '#dataset-select', this.data[this.selection.isolate].data_sets, 'live');
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
    this.dispatchEvent(new CustomEvent(
        'change', {bubbles: true, composed: true, detail: this.selection}));
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

  createOption(text) {
    const option = document.createElement('option');
    option.value = text;
    option.text = text;
    return option;
  }

  populateSelect(id, iterable, autoselect = null) {
    for (let option_value of iterable) {
      const option = this.createOption(option_value);
      if (autoselect === option_value) {
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
      if (IGNORED_INSTANCE_TYPES.has(instance_type)) continue;
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
    input.value = instance_type;
    input.addEventListener('change', e => this.notifySelectionChanged(e));
    const label = document.createElement('label');
    div.appendChild(label);
    label.innerText = instance_type;
    label.htmlFor = instance_type + 'Checkbox';
    return div;
  }
}

customElements.define('details-selection', DetailsSelection);
