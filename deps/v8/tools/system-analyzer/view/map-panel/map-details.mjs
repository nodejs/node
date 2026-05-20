// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {FocusEvent} from '../events.mjs';
import {DOM, ExpandableText, V8CustomElement} from '../helper.mjs';

DOM.defineCustomElement(
    './view/map-panel/map-details',
    (templateText) => class MapDetails extends V8CustomElement {
      _map;

      constructor() {
        super(templateText);
      }

      get _mapDetails() {
        return this.$('#mapDetails');
      }

      get _mapProperties() {
        return this.$('#mapProperties');
      }

      set map(map) {
        if (this._map === map) return;
        this._map = map;
        this.requestUpdate();
      }

      _update() {
        this._mapProperties.innerText = '';
        if (this._map) {
          let clickableDetailsTable = DOM.table('properties');

          {
            const row = clickableDetailsTable.insertRow();
            row.insertCell().innerText = 'ID';
            row.insertCell().innerText = `${this._map.id}`;
          }
          {
            const row = clickableDetailsTable.insertRow();
            row.insertCell().innerText = 'Source location';
            const sourceLocation = row.insertCell();
            new ExpandableText(sourceLocation, `${this._map.sourcePosition}`);
            sourceLocation.className = 'clickable';
            sourceLocation.onclick = e => this._handleSourcePositionClick(e);
          }

          this._mapProperties.appendChild(clickableDetailsTable);
          this._mapDetails.innerText = this._map.description;
        } else {
          this._mapDetails.innerText = '';
        }
      }

      _handleSourcePositionClick(event) {
        this.dispatchEvent(new FocusEvent(this._map.sourcePosition));
      }
    });
