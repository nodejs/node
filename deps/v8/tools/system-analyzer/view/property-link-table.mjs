// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {App} from '../index.mjs'
import {FocusEvent, SelectRelatedEvent} from './events.mjs';
import {DOM, ExpandableText, V8CustomElement} from './helper.mjs';

DOM.defineCustomElement('view/property-link-table',
                        template =>
                            class PropertyLinkTable extends V8CustomElement {
  _object;
  _propertyDict;
  _instanceLinkButtons = false;

  _showHandler = this._handleShow.bind(this);
  _showSourcePositionHandler = this._handleShowSourcePosition.bind(this);
  _showRelatedHandler = this._handleShowRelated.bind(this);
  _arrayValueSelectHandler = this._handleArrayValueSelect.bind(this);

  constructor() {
    super(template);
  }

  set instanceLinkButtons(newValue) {
    this._instanceLinkButtons = newValue;
  }

  set propertyDict(propertyDict) {
    if (this._propertyDict === propertyDict) return;
    if (typeof propertyDict !== 'object') {
      throw new Error(
          `Invalid property dict, expected object: ${propertyDict}`);
    }
    this._propertyDict = propertyDict;
    this.requestUpdate();
  }

  _update() {
    this._fragment = new DocumentFragment();
    this._table = DOM.table('properties');
    for (let key in this._propertyDict) {
      const value = this._propertyDict[key];
      this._addKeyValue(key, value);
    }
    this._addFooter();
    this._fragment.appendChild(this._table);

    const newContent = DOM.div();
    newContent.appendChild(this._fragment);
    this.$('#content').replaceWith(newContent);
    newContent.id = 'content';
    this._fragment = undefined;
  }

  _addKeyValue(key, value) {
    if (key == 'title') {
      this._addTitle(value);
      return;
    }
    if (key == '__this__') {
      this._object = value;
      return;
    }
    const row = this._table.insertRow();
    row.insertCell().innerText = key;
    const cell = row.insertCell();
    if (value == undefined) return;
    if (Array.isArray(value)) {
      cell.appendChild(this._addArrayValue(value));
      return;
    }
    if (App.isClickable(value)) {
      cell.className = 'clickable';
      cell.onclick = this._showHandler;
      cell.data = value;
    }
    new ExpandableText(cell, value.toString());
  }

  _addArrayValue(array) {
    if (array.length == 0) {
      return DOM.text('empty');
    } else if (array.length > 200) {
      return DOM.text(`${array.length} items`);
    }
    const select = DOM.element('select');
    select.onchange = this._arrayValueSelectHandler;
    for (let value of array) {
      const option = DOM.element('option');
      option.innerText = value === undefined ? '' : value.toString();
      option.data = value;
      select.add(option);
    }
    return select;
  }

  _addTitle(value) {
    const title = DOM.element('h3');
    title.innerText = value;
    this._fragment.appendChild(title);
  }

  _addFooter() {
    if (this._object === undefined) return;
    if (!this._instanceLinkButtons) return;
    const td = this._table.createTFoot().insertRow().insertCell();
    td.colSpan = 2;
    let showButton = td.appendChild(DOM.button('Show', this._showHandler));
    showButton.data = this._object;
    if (this._object.sourcePosition) {
      let showSourcePositionButton = td.appendChild(
          DOM.button('Source Position', this._showSourcePositionHandler));
      showSourcePositionButton.data = this._object;
    }
    let showRelatedButton =
        td.appendChild(DOM.button('Show Related', this._showRelatedHandler));
    showRelatedButton.data = this._object;
  }

  _handleArrayValueSelect(event) {
    const logEntry = event.currentTarget.selectedOptions[0].data;
    this.dispatchEvent(new FocusEvent(logEntry));
  }

  _handleShow(event) {
    this.dispatchEvent(new FocusEvent(event.currentTarget.data));
  }

  _handleShowSourcePosition(event) {
    this.dispatchEvent(new FocusEvent(event.currentTarget.data.sourcePosition));
  }

  _handleShowRelated(event) {
    this.dispatchEvent(new SelectRelatedEvent(event.currentTarget.data));
  }
});
