// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { defineCustomElement, V8CustomElement } from './helper.mjs';
import { SynchronizeSelectionEvent } from './events.mjs';
import './timeline/timeline-track.mjs';

defineCustomElement('timeline-panel', (templateText) =>
  class TimelinePanel extends V8CustomElement {
    #timeSelection = { start: 0, end: Infinity };
    constructor() {
      super(templateText);
      this.timelineOverview.addEventListener(
        'mousemove', e => this.handleTimelineIndicatorMove(e));
      this.addEventListener(
        'scrolltrack', e => this.handleTrackScroll(e));
      this.addEventListener(
        SynchronizeSelectionEvent.name, e => this.handleMouseMoveSelection(e));
      this.backgroundCanvas = document.createElement('canvas');
      this.isLocked = false;
    }

    get timelineOverview() {
      return this.$('#timelineOverview');
    }

    get timelineOverviewIndicator() {
      return this.$('#timelineOverviewIndicator');
    }

    //TODO(zcankara) Remove dependency to timelineCanvas here
    get timelineCanvas() {
      return this.timelineTracks[0].timelineCanvas;
    }
    //TODO(zcankara) Remove dependency to timeline here
    get timeline() {
      return this.timelineTracks[0].timeline;
    }
    set nofChunks(count) {
      for (const track of this.timelineTracks) {
        track.nofChunks = count;
      }
    }
    get nofChunks() {
      return this.timelineTracks[0].nofChunks;
    }
    get timelineTracks() {
      return this.$("slot").assignedNodes().filter(
        track => track.nodeType === Node.ELEMENT_NODE);
    }
    handleTrackScroll(event) {
      //TODO(zcankara) add forEachTrack  helper method
      for (const track of this.timelineTracks) {
        track.scrollLeft = event.detail;
      }
    }

    handleMouseMoveSelection(event) {
      this.selectionMouseMove(event.start, event.end);
    }

    selectionMouseMove(start, end) {
      for (const track of this.timelineTracks) {
        track.startTime = start;
        track.endTime = end;
      }
    }

    handleTimelineIndicatorMove(event) {
      if (event.buttons == 0) return;
      let timelineTotalWidth = this.timelineCanvas.offsetWidth;
      let factor = this.timelineOverview.offsetWidth / timelineTotalWidth;
      for (const track of this.timelineTracks) {
        track.timelineIndicatorMove(event.movementX / factor);
      }
      this.updateOverviewWindow();
    }

    updateOverviewWindow() {
      let indicator = this.timelineOverviewIndicator;
      let totalIndicatorWidth =
        this.timelineOverview.offsetWidth;
      let div = this.timeline;
      let timelineTotalWidth = this.timelineCanvas.offsetWidth;
      let factor = totalIndicatorWidth / timelineTotalWidth;
      let width = div.offsetWidth * factor;
      let left = div.scrollLeft * factor;
      indicator.style.width = width + 'px';
      indicator.style.left = left + 'px';
    }

  });
