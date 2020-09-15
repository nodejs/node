// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import "./stats-panel.mjs";
import "./map-panel/map-details.mjs";
import "./map-panel/map-transitions.mjs";
import { FocusEvent } from './events.mjs';
import { MapLogEvent } from "./log/map.mjs";
import { defineCustomElement, V8CustomElement } from './helper.mjs';

defineCustomElement('map-panel', (templateText) =>
  class MapPanel extends V8CustomElement {
    #map;
    constructor() {
      super(templateText);
      this.searchBarBtn.addEventListener(
        'click', e => this.handleSearchBar(e));
      this.addEventListener(
        FocusEvent.name, e => this.handleUpdateMapDetails(e));
    }

    handleUpdateMapDetails(e) {
      if (e.entry instanceof MapLogEvent) {
        this.mapDetailsPanel.mapDetails = e.entry;
      }
    }

    get statsPanel() {
      return this.$('#stats-panel');
    }

    get mapTransitionsPanel() {
      return this.$('#map-transitions');
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

    get mapDetails() {
      return this.mapDetailsPanel.mapDetails;
    }

    // send a timeline to the stats-panel
    set timeline(value) {
      console.assert(value !== undefined, "timeline undefined!");
      this.statsPanel.timeline = value;
      this.statsPanel.update();
    }
    get transitions() {
      return this.statsPanel.transitions;
    }
    set transitions(value) {
      this.statsPanel.transitions = value;
    }

    set map(value) {
      this.#map = value;
      this.mapTransitionsPanel.map = this.#map;
    }

    handleSearchBar(e) {
      let searchBar = this.$('#searchBarInput');
      let searchBarInput = searchBar.value;
      //access the map from model cache
      let selectedMap = MapLogEvent.get(parseInt(searchBarInput));
      if (selectedMap) {
        searchBar.className = "success";
      } else {
        searchBar.className = "failure";
      }
      this.dispatchEvent(new FocusEvent(selectedMap));
    }

    set selectedMapLogEvents(list) {
      this.mapTransitionsPanel.selectedMapLogEvents = list;
    }
    get selectedMapLogEvents() {
      return this.mapTransitionsPanel.selectedMapLogEvents;
    }

  });
