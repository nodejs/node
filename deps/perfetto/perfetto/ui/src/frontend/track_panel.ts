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

import {hex} from 'color-convert';
import * as m from 'mithril';

import {Actions} from '../common/actions';
import {TrackState} from '../common/state';

import {TRACK_SHELL_WIDTH} from './css_constants';
import {globals} from './globals';
import {drawGridLines} from './gridline_helper';
import {BLANK_CHECKBOX, CHECKBOX, STAR, STAR_BORDER} from './icons';
import {Panel, PanelSize} from './panel';
import {verticalScrollToTrack} from './scroll_helper';
import {Track} from './track';
import {trackRegistry} from './track_registry';
import {
  drawVerticalLineAtTime,
  drawVerticalSelection
} from './vertical_line_helper';

function isPinned(id: string) {
  return globals.state.pinnedTracks.indexOf(id) !== -1;
}

function isSelected(id: string) {
  return globals.frontendLocalState.selectedArea.area &&
      globals.frontendLocalState.selectedArea.area.tracks.includes(id);
}

interface TrackShellAttrs {
  track: Track;
  trackState: TrackState;
}

class TrackShell implements m.ClassComponent<TrackShellAttrs> {
  // Set to true when we click down and drag the
  private dragging = false;
  private dropping: 'before'|'after'|undefined = undefined;
  private attrs?: TrackShellAttrs;

  oninit(vnode: m.Vnode<TrackShellAttrs>) {
    this.attrs = vnode.attrs;
  }

  view({attrs}: m.CVnode<TrackShellAttrs>) {
    // The shell should be highlighted if the current search result is inside
    // this track.
    let highlightClass = '';
    const searchIndex = globals.frontendLocalState.searchIndex;
    if (searchIndex !== -1) {
      const trackId = globals.currentSearchResults
                          .trackIds[globals.frontendLocalState.searchIndex];
      if (trackId === attrs.trackState.id) {
        highlightClass = 'flash';
      }
    }

    const dragClass = this.dragging ? `drag` : '';
    const dropClass = this.dropping ? `drop-${this.dropping}` : '';
    return m(
        `.track-shell[draggable=true]`,
        {
          class: `${highlightClass} ${dragClass} ${dropClass}`,
          onmousedown: this.onmousedown.bind(this),
          ondragstart: this.ondragstart.bind(this),
          ondragend: this.ondragend.bind(this),
          ondragover: this.ondragover.bind(this),
          ondragleave: this.ondragleave.bind(this),
          ondrop: this.ondrop.bind(this),
        },
        m('h1',
          {
            title: attrs.trackState.name,
          },
          attrs.trackState.name),
        m('.track-buttons',
          attrs.track.getTrackShellButtons(),
          m(TrackButton, {
            action: () => {
              globals.dispatch(
                  Actions.toggleTrackPinned({trackId: attrs.trackState.id}));
            },
            i: isPinned(attrs.trackState.id) ? STAR : STAR_BORDER,
            tooltip: isPinned(attrs.trackState.id) ? 'Unpin' : 'Pin to top',
            showButton: isPinned(attrs.trackState.id),
          }),
          globals.frontendLocalState.selectedArea.area ? m(TrackButton, {
            action: () => {
              globals.frontendLocalState.toggleTrackSelection(
                  attrs.trackState.id);
            },
            i: isSelected(attrs.trackState.id) ? CHECKBOX : BLANK_CHECKBOX,
            tooltip: isSelected(attrs.trackState.id) ? 'Remove track' :
                                                       'Add track to selection',
            showButton: true,
          }) :
                                                         ''));
  }

  onmousedown(e: MouseEvent) {
    // Prevent that the click is intercepted by the PanAndZoomHandler and that
    // we start panning while dragging.
    e.stopPropagation();
  }

  ondragstart(e: DragEvent) {
    const dataTransfer = e.dataTransfer;
    if (dataTransfer === null) return;
    this.dragging = true;
    globals.rafScheduler.scheduleFullRedraw();
    dataTransfer.setData('perfetto/track', `${this.attrs!.trackState.id}`);
    dataTransfer.setDragImage(new Image(), 0, 0);
    e.stopImmediatePropagation();
  }

  ondragend() {
    this.dragging = false;
    globals.rafScheduler.scheduleFullRedraw();
  }

  ondragover(e: DragEvent) {
    if (this.dragging) return;
    if (!(e.target instanceof HTMLElement)) return;
    const dataTransfer = e.dataTransfer;
    if (dataTransfer === null) return;
    if (!dataTransfer.types.includes('perfetto/track')) return;
    dataTransfer.dropEffect = 'move';
    e.preventDefault();

    // Apply some hysteresis to the drop logic so that the lightened border
    // changes only when we get close enough to the border.
    if (e.offsetY < e.target.scrollHeight / 3) {
      this.dropping = 'before';
    } else if (e.offsetY > e.target.scrollHeight / 3 * 2) {
      this.dropping = 'after';
    }
    globals.rafScheduler.scheduleFullRedraw();
  }

  ondragleave() {
    this.dropping = undefined;
    globals.rafScheduler.scheduleFullRedraw();
  }

  ondrop(e: DragEvent) {
    if (this.dropping === undefined) return;
    const dataTransfer = e.dataTransfer;
    if (dataTransfer === null) return;
    globals.rafScheduler.scheduleFullRedraw();
    const srcId = dataTransfer.getData('perfetto/track');
    const dstId = this.attrs!.trackState.id;
    globals.dispatch(Actions.moveTrack({srcId, op: this.dropping, dstId}));
    this.dropping = undefined;
  }
}

export interface TrackContentAttrs { track: Track; }
export class TrackContent implements m.ClassComponent<TrackContentAttrs> {
  view({attrs}: m.CVnode<TrackContentAttrs>) {
    return m('.track-content', {
      onmousemove: (e: MouseEvent) => {
        attrs.track.onMouseMove({x: e.layerX - TRACK_SHELL_WIDTH, y: e.layerY});
        globals.rafScheduler.scheduleRedraw();
      },
      onmouseout: () => {
        attrs.track.onMouseOut();
        globals.rafScheduler.scheduleRedraw();
      },
      onclick: (e: MouseEvent) => {
        // If we are selecting a time range - do not pass the click to the
        // track.
        if (globals.frontendLocalState.selectingArea) return;
        // If the click is outside of the current time range, clear it.
        const clickTime = globals.frontendLocalState.timeScale.pxToTime(
            e.layerX - TRACK_SHELL_WIDTH);
        const area = globals.frontendLocalState.selectedArea.area;
        if (area !== undefined &&
            (clickTime < area.startSec || clickTime > area.endSec)) {
          globals.frontendLocalState.deselectArea();
          e.stopPropagation();
        } else if (attrs.track.onMouseClick(
                       {x: e.layerX - TRACK_SHELL_WIDTH, y: e.layerY})) {
          e.stopPropagation();
        }
        globals.rafScheduler.scheduleRedraw();
      }
    });
  }
}

interface TrackComponentAttrs {
  trackState: TrackState;
  track: Track;
}
class TrackComponent implements m.ClassComponent<TrackComponentAttrs> {
  view({attrs}: m.CVnode<TrackComponentAttrs>) {
    return m(
        '.track',
        {
          style: {
            height: `${Math.max(24, attrs.track.getHeight())}px`,
          },
          id: 'track_' + attrs.trackState.id,
        },
        [
          m(TrackShell, {track: attrs.track, trackState: attrs.trackState}),
          m(TrackContent, {track: attrs.track})
        ]);
  }

  oncreate({attrs}: m.CVnode<TrackComponentAttrs>) {
    if (globals.frontendLocalState.scrollToTrackId === attrs.trackState.id) {
      verticalScrollToTrack(attrs.trackState.id);
      globals.frontendLocalState.scrollToTrackId = undefined;
    }
  }
}

export interface TrackButtonAttrs {
  action: () => void;
  i: string;
  tooltip: string;
  showButton: boolean;
}
export class TrackButton implements m.ClassComponent<TrackButtonAttrs> {
  view({attrs}: m.CVnode<TrackButtonAttrs>) {
    return m(
        'i.material-icons.track-button',
        {
          class: `${attrs.showButton ? 'show' : ''}`,
          onclick: attrs.action,
          title: attrs.tooltip,
        },
        attrs.i);
  }
}

interface TrackPanelAttrs {
  id: string;
  selectable: boolean;
}

export class TrackPanel extends Panel<TrackPanelAttrs> {
  private track: Track;
  private trackState: TrackState;
  constructor(vnode: m.CVnode<TrackPanelAttrs>) {
    super();
    this.trackState = globals.state.tracks[vnode.attrs.id];
    const trackCreator = trackRegistry.get(this.trackState.kind);
    this.track = trackCreator.create(this.trackState);
  }

  view() {
    return m(TrackComponent, {trackState: this.trackState, track: this.track});
  }

  highlightIfTrackSelected(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const localState = globals.frontendLocalState;
    const area = localState.selectedArea.area;
    if (area && area.tracks.includes(this.trackState.id)) {
      const timeScale = localState.timeScale;
      ctx.fillStyle = '#ebeef9';
      ctx.fillRect(
          timeScale.timeToPx(area.startSec) + TRACK_SHELL_WIDTH,
          0,
          timeScale.deltaTimeToPx(area.endSec - area.startSec),
          size.height);
    }
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    ctx.save();

    this.highlightIfTrackSelected(ctx, size);

    drawGridLines(
        ctx,
        globals.frontendLocalState.timeScale,
        globals.frontendLocalState.visibleWindowTime,
        size.width,
        size.height);

    ctx.translate(TRACK_SHELL_WIDTH, 0);

    this.track.render(ctx);

    ctx.restore();

    const localState = globals.frontendLocalState;
    // Draw vertical line when hovering on the notes panel.
    if (localState.hoveredNoteTimestamp !== -1) {
      drawVerticalLineAtTime(
          ctx,
          localState.timeScale,
          localState.hoveredNoteTimestamp,
          size.height,
          `#aaa`);
    }
    if (localState.hoveredLogsTimestamp !== -1) {
      drawVerticalLineAtTime(
          ctx,
          localState.timeScale,
          localState.hoveredLogsTimestamp,
          size.height,
          `rgb(52,69,150)`);
    }
    if (localState.selectedArea.area !== undefined &&
        !globals.frontendLocalState.selectingArea) {
      drawVerticalSelection(
          ctx,
          localState.timeScale,
          localState.selectedArea.area.startSec,
          localState.selectedArea.area.endSec,
          size.height,
          `rgba(0,0,0,0.5)`);
    }
    if (globals.state.currentSelection !== null) {
      if (globals.state.currentSelection.kind === 'NOTE') {
        const note = globals.state.notes[globals.state.currentSelection.id];
        drawVerticalLineAtTime(ctx,
                               localState.timeScale,
                               note.timestamp,
                               size.height,
                               note.color);
        if (note.noteType === 'AREA') {
          drawVerticalLineAtTime(
              ctx,
              localState.timeScale,
              note.area.endSec,
              size.height,
              note.color);
        }
      }

      if (globals.state.currentSelection.kind === 'SLICE' &&
          globals.sliceDetails.wakeupTs !== undefined) {
        drawVerticalLineAtTime(
            ctx,
            localState.timeScale,
            globals.sliceDetails.wakeupTs,
            size.height,
            `black`);
      }
    }
    // All marked areas should have semi-transparent vertical lines
    // marking the start and end.
    for (const note of Object.values(globals.state.notes)) {
      if (note.noteType === 'AREA') {
        const transparentNoteColor =
            'rgba(' + hex.rgb(note.color.substr(1)).toString() + ', 0.65)';
        drawVerticalLineAtTime(
            ctx,
            localState.timeScale,
            note.area.startSec,
            size.height,
            transparentNoteColor,
            1);
        drawVerticalLineAtTime(
            ctx,
            localState.timeScale,
            note.area.endSec,
            size.height,
            transparentNoteColor,
            1);
      }
    }
  }
}
