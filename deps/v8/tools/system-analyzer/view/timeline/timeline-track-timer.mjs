// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CSSColor, DOM, SVG, V8CustomElement} from '../helper.mjs';

import {TimelineTrackBase} from './timeline-track-base.mjs'
import {TimelineTrackStackedBase} from './timeline-track-stacked-base.mjs'

DOM.defineCustomElement(
    'view/timeline/timeline-track', 'timeline-track-timer',
    (templateText) =>
        class TimelineTrackTimer extends TimelineTrackStackedBase {
      constructor() {
        super(templateText);
      }

      _prepareDrawableItems() {
        const stack = [];
        let maxDepth = 0;
        for (let i = 0; i < this._timeline.length; i++) {
          const timer = this._timeline.at(i);
          let insertDepth = -1;
          for (let depth = 0; depth < stack.length; depth++) {
            const pendingTimer = stack[depth];
            if (pendingTimer === undefined) {
              if (insertDepth === -1) insertDepth = depth;
            } else if (pendingTimer.endTime <= timer.startTime) {
              stack[depth] == undefined;
              if (insertDepth === -1) insertDepth = depth;
            }
          }
          if (insertDepth === -1) insertDepth = stack.length;
          stack[insertDepth] = timer;
          timer.depth = insertDepth;
          maxDepth = Math.max(maxDepth, insertDepth);
        }
        this._drawableItems = this._timeline;
        this._adjustStackDepth(maxDepth++);
      }
    });