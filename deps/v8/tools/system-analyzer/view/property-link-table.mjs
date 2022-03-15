// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {App} from '../index.mjs'
import {FocusEvent} from './events.mjs';
import {DOM, ExpandableText, V8CustomElement} from './helper.mjs';

DOM.defineCustomElement(
    'view/property-link-table',
    template => class PropertyLinkTable extends V8CustomElement {
      _instance;
      _propertyDict;
      _instanceLinkButtons = false;
      _logEntryClickHandler = this._handleLogEntryClick.bind(this);
      _logEntryRelatedHandler = this._handleLogEntryRelated.bind(this);
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
          this._instance = value;
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
          cell.onclick = this._logEntryClickHandler;
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
          option.innerText = value.toString();
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
        if (this._instance === undefined) return;
        if (!this._instanceLinkButtons) return;
        const td = this._table.createTFoot().insertRow().insertCell();
        td.colSpan = 2;
        let showButton =
            td.appendChild(DOM.button('Show', this._logEntryClickHandler));
        showButton.data = this._instance;
        let showRelatedButton = td.appendChild(
            DOM.button('Show Related', this._logEntryRelatedClickHandler));
        showRelatedButton.data = this._instance;
      }

      _handleArrayValueSelect(event) {
        const logEntry = event.currentTarget.selectedOptions[0].data;
        this.dispatchEvent(new FocusEvent(logEntry));
      }
      _handleLogEntryClick(event) {
        this.dispatchEvent(new FocusEvent(event.currentTarget.data));
      }

      _handleLogEntryRelated(event) {
        this.dispatchEvent(new SelectRelatedEvent(event.currentTarget.data));
      }
    });
