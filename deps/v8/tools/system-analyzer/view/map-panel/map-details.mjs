// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {FocusEvent} from '../events.mjs';
import {DOM, V8CustomElement} from '../helper.mjs';

DOM.defineCustomElement(
    './view/map-panel/map-details',
    (templateText) => class MapDetails extends V8CustomElement {
      _map;

      constructor() {
        super(templateText);
        this._filePositionNode.onclick = e => this._handleFilePositionClick(e);
      }

      get _mapDetails() {
        return this.$('#mapDetails');
      }

      get _filePositionNode() {
        return this.$('#filePositionNode');
      }

      set map(map) {
        if (this._map === map) return;
        this._map = map;
        this.update();
      }

      _update() {
        let details = '';
        let clickableDetails = '';
        if (this._map) {
          clickableDetails = `ID: ${this._map.id}`;
          clickableDetails += `\nSource location: ${this._map.filePosition}`;
          details = this._map.description;
        }
        this._filePositionNode.innerText = clickableDetails;
        this._filePositionNode.classList.add('clickable');
        this._mapDetails.innerText = details;
      }

      _handleFilePositionClick(event) {
        this.dispatchEvent(new FocusEvent(this._map.sourcePosition));
      }
    });
