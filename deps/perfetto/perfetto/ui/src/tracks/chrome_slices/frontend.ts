// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {Actions} from '../../common/actions';
import {cropText, drawIncompleteSlice} from '../../common/canvas_utils';
import {TrackState} from '../../common/state';
import {toNs} from '../../common/time';
import {checkerboardExcept} from '../../frontend/checkerboard';
import {globals} from '../../frontend/globals';
import {Track} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

import {Config, Data, SLICE_TRACK_KIND} from './common';

const SLICE_HEIGHT = 18;
const TRACK_PADDING = 4;
const INCOMPLETE_SLICE_TIME_S = 0.00003;

function hash(s: string): number {
  let hash = 0x811c9dc5 & 0xfffffff;
  for (let i = 0; i < s.length; i++) {
    hash ^= s.charCodeAt(i);
    hash = (hash * 16777619) & 0xffffffff;
  }
  return hash & 0xff;
}

export class ChromeSliceTrack extends Track<Config, Data> {
  static readonly kind: string = SLICE_TRACK_KIND;
  static create(trackState: TrackState): Track {
    return new ChromeSliceTrack(trackState);
  }

  private hoveredTitleId = -1;

  constructor(trackState: TrackState) {
    super(trackState);
  }

  renderCanvas(ctx: CanvasRenderingContext2D): void {
    // TODO: fonts and colors should come from the CSS and not hardcoded here.

    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const data = this.data();

    if (data === undefined) return;  // Can't possibly draw anything.

    // If the cached trace slices don't fully cover the visible time range,
    // show a gray rectangle with a "Loading..." label.
    checkerboardExcept(
        ctx,
        this.getHeight(),
        timeScale.timeToPx(visibleWindowTime.start),
        timeScale.timeToPx(visibleWindowTime.end),
        timeScale.timeToPx(data.start),
        timeScale.timeToPx(data.end),
    );

    ctx.font = '12px Roboto Condensed';
    ctx.textAlign = 'center';

    // measuretext is expensive so we only use it once.
    const charWidth = ctx.measureText('ACBDLqsdfg').width / 10;
    const pxEnd = timeScale.timeToPx(visibleWindowTime.end);

    // The draw of the rect on the selected slice must happen after the other
    // drawings, otherwise it would result under another rect.
    let drawRectOnSelected = () => {};

    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      let tEnd = data.ends[i];
      const depth = data.depths[i];
      const titleId = data.titles[i];
      const sliceId = data.sliceIds[i];
      const title = data.strings[titleId];
      const summarizedOffset =
          data.summarizedOffset ? data.summarizedOffset[i] : -1;
      let incompleteSlice = false;

      if (toNs(tEnd) - toNs(tStart) === -1) {  // incomplete slice
        incompleteSlice = true;
        tEnd = tStart + INCOMPLETE_SLICE_TIME_S;
      }

      if (tEnd <= visibleWindowTime.start || tStart >= visibleWindowTime.end) {
        continue;
      }

      const rectXStart = Math.max(timeScale.timeToPx(tStart), 0);
      let rectXEnd = Math.min(timeScale.timeToPx(tEnd), pxEnd);
      let rectWidth = rectXEnd - rectXStart;
      // All slices should be at least 1px.
      if (rectWidth < 1) {
        rectWidth = 1;
        rectXEnd = rectXStart + 1;
      }
      const rectYStart = TRACK_PADDING + depth * SLICE_HEIGHT;
      const name = title.replace(/( )?\d+/g, '');
      const hue = hash(name);
      const saturation = 50;
      const hovered = titleId === this.hoveredTitleId;
      const color = `hsl(${hue}, ${saturation}%, ${hovered ? 30 : 65}%)`;
      if (summarizedOffset !== -1) {
        const summarizedSize = data.summarizedSize[i];
        const nameHues =
            (data.summaryNameId.slice(
                 summarizedOffset, summarizedOffset + summarizedSize))
                .map(id => hash(data.strings[id]));
        const percents = data.summaryPercent.slice(
            summarizedOffset, summarizedOffset + summarizedSize);
        colorSummarizedSlice(nameHues, percents, rectXStart, rectXEnd, hovered);
      } else {
        ctx.fillStyle = color;
      }
      if (incompleteSlice && rectWidth > SLICE_HEIGHT / 4) {
        drawIncompleteSlice(
            ctx, rectXStart, rectYStart, rectWidth, SLICE_HEIGHT, color);
      } else {
        ctx.fillRect(rectXStart, rectYStart, rectWidth, SLICE_HEIGHT);
      }

      // Selected case
      const currentSelection = globals.state.currentSelection;
      if (currentSelection && currentSelection.kind === 'CHROME_SLICE' &&
          currentSelection.id !== undefined &&
          currentSelection.id === sliceId) {
        drawRectOnSelected = () => {
          ctx.strokeStyle = `hsl(${hue}, ${saturation}%, 30%)`;
          ctx.beginPath();
          ctx.lineWidth = 3;
          ctx.strokeRect(
              rectXStart, rectYStart - 1.5, rectWidth, SLICE_HEIGHT + 3);
          ctx.closePath();
        };
      }

      ctx.fillStyle = 'white';
      const displayText = cropText(title, charWidth, rectWidth);
      const rectXCenter = rectXStart + rectWidth / 2;
      ctx.textBaseline = "middle";
      ctx.fillText(displayText, rectXCenter, rectYStart + SLICE_HEIGHT / 2);
    }
    drawRectOnSelected();

    // Make a gradient ordered most common to least common slices within the
    // summarized slice.
    function colorSummarizedSlice(
        nameHues: Uint16Array,
        percents: Float64Array,
        rectStart: number,
        rectEnd: number,
        hovered: boolean) {
      const gradient = ctx.createLinearGradient(
          rectStart, SLICE_HEIGHT, rectEnd, SLICE_HEIGHT);
      let colorStop = 0;
      for (let i = 0; i < nameHues.length; i++) {
        const colorString = `hsl(${nameHues[i]}, 50%, ${hovered ? 30 : 65}%)`;
        colorStop = Math.max(0, Math.min(1, colorStop + percents[i]));
        gradient.addColorStop(colorStop, colorString);
      }
      ctx.fillStyle = gradient;
    }
  }

  getSliceIndex({x, y}: {x: number, y: number}): number|void {
    const data = this.data();
    this.hoveredTitleId = -1;
    if (data === undefined) return;
    const {timeScale} = globals.frontendLocalState;
    if (y < TRACK_PADDING) return;
    const t = timeScale.pxToTime(x);
    const depth = Math.floor(y / SLICE_HEIGHT);
    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      let tEnd = data.ends[i];
      if (toNs(tEnd) - toNs(tStart) === -1) {
        tEnd = tStart + INCOMPLETE_SLICE_TIME_S;
      }
      if (tStart <= t && t <= tEnd && depth === data.depths[i]) {
        return i;
      }
    }
  }

  onMouseMove({x, y}: {x: number, y: number}) {
    const sliceIndex = this.getSliceIndex({x, y});
    if (sliceIndex === undefined) return;
    const data = this.data();
    if (data === undefined) return;
    const titleId = data.titles[sliceIndex];
    this.hoveredTitleId = titleId;
  }

  onMouseOut() {
    this.hoveredTitleId = -1;
  }

  onMouseClick({x, y}: {x: number, y: number}): boolean {
    const sliceIndex = this.getSliceIndex({x, y});
    if (sliceIndex === undefined) return false;
    const data = this.data();
    if (data === undefined) return false;
    const sliceId = data.sliceIds[sliceIndex];
    if (sliceId !== undefined && sliceId !== -1) {
      globals.makeSelection(Actions.selectChromeSlice(
          {id: sliceId, trackId: this.trackState.id}));
      return true;
    }
    return false;
  }

  getHeight() {
    return SLICE_HEIGHT * (this.config.maxDepth + 1) + 2 * TRACK_PADDING;
  }
}

trackRegistry.register(ChromeSliceTrack);
