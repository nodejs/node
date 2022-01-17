// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './timeline/timeline-track.mjs';
import './timeline/timeline-track-map.mjs';
import './timeline/timeline-track-tick.mjs';
import './timeline/timeline-track-timer.mjs';

import {SynchronizeSelectionEvent} from './events.mjs';
import {DOM, V8CustomElement} from './helper.mjs';

DOM.defineCustomElement(
    'view/timeline-panel',
    (templateText) => class TimelinePanel extends V8CustomElement {
      constructor() {
        super(templateText);
        this.addEventListener('scrolltrack', e => this.handleTrackScroll(e));
        this.addEventListener(
            SynchronizeSelectionEvent.name,
            e => this.handleSelectionSyncronization(e));
        this.$('#zoomIn').onclick = () => this.nofChunks *= 1.5;
        this.$('#zoomOut').onclick = () => this.nofChunks /= 1.5;
      }

      set nofChunks(count) {
        const time = this.currentTime
        for (const track of this.timelineTracks) {
          track.nofChunks = count;
          track.currentTime = time;
        }
      }

      get nofChunks() {
        return this.timelineTracks[0].nofChunks;
      }

      get currentTime() {
        return this.timelineTracks[0].currentTime;
      }

      get timelineTracks() {
        return this.$('slot').assignedNodes().filter(
            node => node.nodeType === Node.ELEMENT_NODE);
      }

      handleTrackScroll(event) {
        for (const track of this.timelineTracks) {
          track.scrollLeft = event.detail;
        }
      }

      handleSelectionSyncronization(event) {
        this.timeSelection = {start: event.start, end: event.end};
      }

      set timeSelection(timeSelection) {
        if (timeSelection.start > timeSelection.end) {
          throw new Error('Invalid time range');
        }
        for (const track of this.timelineTracks) {
          track.timeSelection = timeSelection;
        }
      }
    });
