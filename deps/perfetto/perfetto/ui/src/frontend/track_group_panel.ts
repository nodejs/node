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

import {assertExists} from '../base/logging';
import {Actions} from '../common/actions';
import {
  getContainingTrackId,
  TrackGroupState,
  TrackState
} from '../common/state';

import {globals} from './globals';
import {drawGridLines} from './gridline_helper';
import {
  BLANK_CHECKBOX,
  CHECKBOX,
  EXPAND_DOWN,
  EXPAND_UP,
  INDETERMINATE_CHECKBOX
} from './icons';
import {Panel, PanelSize} from './panel';
import {Track} from './track';
import {TrackContent} from './track_panel';
import {trackRegistry} from './track_registry';
import {
  drawVerticalLineAtTime,
  drawVerticalSelection,
} from './vertical_line_helper';

interface Attrs {
  trackGroupId: string;
  selectable: boolean;
}

export class TrackGroupPanel extends Panel<Attrs> {
  private readonly trackGroupId: string;
  private shellWidth = 0;
  private backgroundColor = '#ffffff';  // Updated from CSS later.
  private summaryTrack: Track;

  constructor({attrs}: m.CVnode<Attrs>) {
    super();
    this.trackGroupId = attrs.trackGroupId;
    const trackCreator = trackRegistry.get(this.summaryTrackState.kind);
    this.summaryTrack = trackCreator.create(this.summaryTrackState);
  }

  get trackGroupState(): TrackGroupState {
    return assertExists(globals.state.trackGroups[this.trackGroupId]);
  }

  get summaryTrackState(): TrackState {
    return assertExists(
        globals.state.tracks[this.trackGroupState.summaryTrackId]);
  }

  view({attrs}: m.CVnode<Attrs>) {
    const collapsed = this.trackGroupState.collapsed;
    let name = this.trackGroupState.name;
    if (name[0] === '/') {
      name = StripPathFromExecutable(name);
    }

    // The shell should be highlighted if the current search result is inside
    // this track group.
    let highlightClass = '';
    const searchIndex = globals.frontendLocalState.searchIndex;
    if (searchIndex !== -1) {
      const trackId = globals.currentSearchResults
                          .trackIds[globals.frontendLocalState.searchIndex];
      const parentTrackId = getContainingTrackId(globals.state, trackId);
      if (parentTrackId === attrs.trackGroupId) {
        highlightClass = 'flash';
      }
    }

    const selectedArea = globals.frontendLocalState.selectedArea.area;
    const trackGroup = globals.state.trackGroups[attrs.trackGroupId];
    let checkBox = BLANK_CHECKBOX;
    if (selectedArea) {
      if (selectedArea.tracks.includes(attrs.trackGroupId) &&
          trackGroup.tracks.every(id => selectedArea.tracks.includes(id))) {
        checkBox = CHECKBOX;
      } else if (
          selectedArea.tracks.includes(attrs.trackGroupId) ||
          trackGroup.tracks.some(id => selectedArea.tracks.includes(id))) {
        checkBox = INDETERMINATE_CHECKBOX;
      }
    }

    return m(
        `.track-group-panel[collapsed=${collapsed}]`,
        {id: 'track_' + this.trackGroupId},
        m(`.shell`,
          {
            onclick: (e: MouseEvent) => {
              globals.dispatch(Actions.toggleTrackGroupCollapsed({
                trackGroupId: attrs.trackGroupId,
              })),
                  e.stopPropagation();
            },
            class: `${highlightClass}`,
          },

          m('.fold-button',
            m('i.material-icons',
              this.trackGroupState.collapsed ? EXPAND_DOWN : EXPAND_UP)),
          m('h1',
            {
              title: name,
            },
            name),
          selectedArea ? m('i.material-icons.track-button',
                           {
                             onclick: (e: MouseEvent) => {
                               globals.frontendLocalState.toggleTrackSelection(
                                   attrs.trackGroupId, true /*trackGroup*/);
                               e.stopPropagation();
                             }
                           },
                           checkBox) :
                         ''),

        this.summaryTrack ? m(TrackContent, {track: this.summaryTrack}) : null);
  }

  oncreate(vnode: m.CVnodeDOM<Attrs>) {
    this.onupdate(vnode);
  }

  onupdate({dom}: m.CVnodeDOM<Attrs>) {
    const shell = assertExists(dom.querySelector('.shell'));
    this.shellWidth = shell.getBoundingClientRect().width;
    this.backgroundColor =
        getComputedStyle(dom).getPropertyValue('--collapsed-background');
  }

  highlightIfTrackSelected(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const localState = globals.frontendLocalState;
    const area = localState.selectedArea.area;
    if (area && area.tracks.includes(this.trackGroupId)) {
      ctx.fillStyle = '#ebeef9';
      ctx.fillRect(
          localState.timeScale.timeToPx(area.startSec) + this.shellWidth,
          0,
          localState.timeScale.deltaTimeToPx(area.endSec - area.startSec),
          size.height);
    }
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const collapsed = this.trackGroupState.collapsed;
    if (!collapsed) return;

    ctx.save();

    ctx.fillStyle = this.backgroundColor;
    ctx.fillRect(0, 0, size.width, size.height);

    this.highlightIfTrackSelected(ctx, size);

    drawGridLines(
        ctx,
        globals.frontendLocalState.timeScale,
        globals.frontendLocalState.visibleWindowTime,
        size.width,
        size.height);

    ctx.translate(this.shellWidth, 0);

    if (this.summaryTrack) {
      this.summaryTrack.render(ctx);
    }
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

function StripPathFromExecutable(path: string) {
  return path.split('/').slice(-1)[0];
}
