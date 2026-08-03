// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CSSColor, DOM, SVG, V8CustomElement} from '../helper.mjs';

import {TimelineTrackBase} from './timeline-track-base.mjs'

DOM.defineCustomElement(
    'view/timeline/timeline-track',
    (templateText) => class TimelineTrack extends TimelineTrackBase {
      constructor() {
        super(templateText);
      }
    })