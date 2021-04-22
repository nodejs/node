// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './map-panel/map-details.mjs';
import './map-panel/map-transitions.mjs';

import {MapLogEntry} from '../log/map.mjs';

import {FocusEvent} from './events.mjs';
import {DOM, V8CustomElement} from './helper.mjs';

DOM.defineCustomElement(
    'view/map-panel', (templateText) => class MapPanel extends V8CustomElement {
      _map;
      _timeline;
      _selectedLogEntries = [];
      _displayedLogEntries = [];

      constructor() {
        super(templateText);
        this.searchBarBtn.addEventListener('click', e => this._handleSearch(e));
        this.showAllRadio.onclick = _ => this._showEntries(this._timeline);
        this.showTimerangeRadio.onclick = _ =>
            this._showEntries(this._timeline.selectionOrSelf);
        this.showSelectionRadio.onclick = _ =>
            this._showEntries(this._selectedLogEntries);
      }

      get showAllRadio() {
        return this.$('#show-all');
      }
      get showTimerangeRadio() {
        return this.$('#show-timerange');
      }
      get showSelectionRadio() {
        return this.$('#show-selection');
      }

      get mapTransitionsPanel() {
        return this.$('#map-transitions');
      }

      get mapDetailsTransitionsPanel() {
        return this.$('#map-details-transitions');
      }

      get mapDetailsPanel() {
        return this.$('#map-details');
      }

      get searchBarBtn() {
        return this.$('#searchBarBtn');
      }

      get searchBar() {
        return this.$('#searchBar');
      }

      set timeline(timeline) {
        console.assert(timeline !== undefined, 'timeline undefined!');
        this._timeline = timeline;
        this.$('.panel').style.display =
            timeline.isEmpty() ? 'none' : 'inherit';
        this.mapTransitionsPanel.timeline = timeline;
        this.mapDetailsTransitionsPanel.timeline = timeline;
      }

      set selectedLogEntries(entries) {
        if (entries === this._timeline.selection) {
          this.showTimerangeRadio.click();
        } else if (entries == this._timeline) {
          this.showAllRadio.click();
        } else {
          this._selectedLogEntries = entries;
          this.showSelectionRadio.click();
        }
      }

      set map(map) {
        this._map = map;
        this.mapDetailsTransitionsPanel.selectedLogEntries = [map];
        this.mapDetailsPanel.map = map;
      }

      _showEntries(entries) {
        this._displayedLogEntries = entries;
        this.mapTransitionsPanel.selectedLogEntries = entries;
      }

      update() {
        // nothing to do
      }

      _handleSearch(e) {
        let searchBar = this.$('#searchBarInput');
        let searchBarInput = searchBar.value;
        // access the map from model cache
        let selectedMap = MapLogEntry.get(searchBarInput);
        if (selectedMap) {
          searchBar.className = 'success';
          this.dispatchEvent(new FocusEvent(selectedMap));
        } else {
          searchBar.className = 'failure';
        }
      }
    });
