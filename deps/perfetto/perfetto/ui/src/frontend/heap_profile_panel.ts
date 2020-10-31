// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import * as m from 'mithril';

import {Actions} from '../common/actions';
import {
  ALLOC_SPACE_MEMORY_ALLOCATED_KEY,
  OBJECTS_ALLOCATED_KEY,
  OBJECTS_ALLOCATED_NOT_FREED_KEY,
  SPACE_MEMORY_ALLOCATED_NOT_FREED_KEY,
} from '../common/flamegraph_util';
import {HeapProfileFlamegraphViewingOption} from '../common/state';
import {timeToCode} from '../common/time';

import {Flamegraph, NodeRendering} from './flamegraph';
import {globals} from './globals';
import {Panel, PanelSize} from './panel';
import {debounce} from './rate_limiters';

interface HeapProfileDetailsPanelAttrs {}

const HEADER_HEIGHT = 30;

enum ProfileType {
  NATIVE_HEAP_PROFILE = 'native',
  JAVA_HEAP_GRAPH = 'graph',
}

function isProfileType(s: string): s is ProfileType {
  return Object.values(ProfileType).includes(s as ProfileType);
}

function toProfileType(s: string): ProfileType {
  if (!isProfileType(s)) {
    throw new Error('Unknown type ${s}');
  }
  return s;
}

const RENDER_SELF_AND_TOTAL: NodeRendering = {
  selfSize: 'Self',
  totalSize: 'Total',
};
const RENDER_OBJ_COUNT: NodeRendering = {
  selfSize: 'Self objects',
  totalSize: 'Subtree objects',
};

export class HeapProfileDetailsPanel extends
    Panel<HeapProfileDetailsPanelAttrs> {
  private profileType?: ProfileType = undefined;
  private ts = 0;
  private pid = 0;
  private flamegraph: Flamegraph = new Flamegraph([]);
  private focusRegex = '';
  private updateFocusRegexDebounced = debounce(() => {
    this.updateFocusRegex();
  }, 20);

  view() {
    const heapDumpInfo = globals.heapProfileDetails;
    if (heapDumpInfo && heapDumpInfo.type !== undefined &&
        heapDumpInfo.ts !== undefined && heapDumpInfo.tsNs !== undefined &&
        heapDumpInfo.pid !== undefined && heapDumpInfo.upid !== undefined) {
      this.profileType = toProfileType(heapDumpInfo.type);
      this.ts = heapDumpInfo.tsNs;
      this.pid = heapDumpInfo.pid;
      if (heapDumpInfo.flamegraph) {
        this.flamegraph.updateDataIfChanged(
            this.nodeRendering(), heapDumpInfo.flamegraph);
      }
      const height = heapDumpInfo.flamegraph ?
          this.flamegraph.getHeight() + HEADER_HEIGHT :
          0;
      return m(
          '.details-panel',
          {
            onclick: (e: MouseEvent) => {
              if (this.flamegraph !== undefined) {
                this.onMouseClick({y: e.layerY, x: e.layerX});
              }
              return false;
            },
            onmousemove: (e: MouseEvent) => {
              if (this.flamegraph !== undefined) {
                this.onMouseMove({y: e.layerY, x: e.layerX});
                globals.rafScheduler.scheduleRedraw();
              }
              return false;
            },
            onmouseout: () => {
              if (this.flamegraph !== undefined) {
                this.onMouseOut();
              }
            }
          },
          m('.details-panel-heading.heap-profile',
            {onclick: (e: MouseEvent) => e.stopPropagation()},
            [
              m('div.options',
                [
                  m('div.title', this.getTitle()),
                  this.getViewingOptionButtons(),
                ]),
              m('div.details',
                [
                  m('div.time',
                    `Snapshot time: ${timeToCode(heapDumpInfo.ts)}`),
                  m('input[type=text][placeholder=Focus]', {
                    oninput: (e: Event) => {
                      const target = (e.target as HTMLInputElement);
                      this.focusRegex = target.value;
                      this.updateFocusRegexDebounced();
                    },
                    // Required to stop hot-key handling:
                    onkeydown: (e: Event) => e.stopPropagation(),
                  }),
                  m('button.download',
                    {
                      onclick: () => {
                        this.downloadPprof();
                      }
                    },
                    m('i.material-icons', 'file_download'),
                    'Download profile'),
                ]),
            ]),
          m(`div[style=height:${height}px]`),
      );
    } else {
      return m(
          '.details-panel',
          m('.details-panel-heading', m('h2', `Heap Profile`)));
    }
  }

  private getTitle(): string {
    switch (this.profileType!) {
      case ProfileType.NATIVE_HEAP_PROFILE:
        return 'Heap Profile:';
      case ProfileType.JAVA_HEAP_GRAPH:
        return 'Java Heap:';
      default:
        throw new Error('unknown type');
    }
  }

  private nodeRendering(): NodeRendering {
    if (this.profileType === undefined) {
      return {};
    }
    const viewingOption =
        globals.state.currentHeapProfileFlamegraph!.viewingOption;
    switch (this.profileType) {
      case ProfileType.NATIVE_HEAP_PROFILE:
        return RENDER_SELF_AND_TOTAL;
      case ProfileType.JAVA_HEAP_GRAPH:
        if (viewingOption === OBJECTS_ALLOCATED_NOT_FREED_KEY) {
          return RENDER_OBJ_COUNT;
        } else {
          return RENDER_SELF_AND_TOTAL;
        }
      default:
        throw new Error('unknown type');
    }
  }

  private updateFocusRegex() {
    globals.dispatch(Actions.changeFocusHeapProfileFlamegraph({
      focusRegex: this.focusRegex,
    }));
  }

  getButtonsClass(button: HeapProfileFlamegraphViewingOption): string {
    if (globals.state.currentHeapProfileFlamegraph === null) return '';
    return globals.state.currentHeapProfileFlamegraph.viewingOption === button ?
        '.chosen' :
        '';
  }

  getViewingOptionButtons(): m.Children {
    const viewingOptions = [
      m(`button${this.getButtonsClass(SPACE_MEMORY_ALLOCATED_NOT_FREED_KEY)}`,
        {
          onclick: () => {
            this.changeViewingOption(SPACE_MEMORY_ALLOCATED_NOT_FREED_KEY);
          }
        },
        'space'),
      m(`button${this.getButtonsClass(OBJECTS_ALLOCATED_NOT_FREED_KEY)}`,
        {
          onclick: () => {
            this.changeViewingOption(OBJECTS_ALLOCATED_NOT_FREED_KEY);
          }
        },
        'objects'),
    ];

    if (this.profileType === ProfileType.NATIVE_HEAP_PROFILE) {
      viewingOptions.push(
          m(`button${this.getButtonsClass(ALLOC_SPACE_MEMORY_ALLOCATED_KEY)}`,
            {
              onclick: () => {
                this.changeViewingOption(ALLOC_SPACE_MEMORY_ALLOCATED_KEY);
              }
            },
            'alloc space'),
          m(`button${this.getButtonsClass(OBJECTS_ALLOCATED_KEY)}`,
            {
              onclick: () => {
                this.changeViewingOption(OBJECTS_ALLOCATED_KEY);
              }
            },
            'alloc objects'));
    }
    return m('div', ...viewingOptions);
  }

  changeViewingOption(viewingOption: HeapProfileFlamegraphViewingOption) {
    globals.dispatch(Actions.changeViewHeapProfileFlamegraph({viewingOption}));
  }

  downloadPprof() {
    const engine = Object.values(globals.state.engines)[0];
    if (!engine) return;
    const src = engine.source;
    globals.dispatch(
        Actions.convertTraceToPprof({pid: this.pid, ts1: this.ts, src}));
  }

  private changeFlamegraphData() {
    const data = globals.heapProfileDetails;
    const flamegraphData = data.flamegraph === undefined ? [] : data.flamegraph;
    this.flamegraph.updateDataIfChanged(
        this.nodeRendering(), flamegraphData, data.expandedCallsite);
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    this.changeFlamegraphData();
    const current = globals.state.currentHeapProfileFlamegraph;
    if (current === null) return;
    const unit =
        current.viewingOption === SPACE_MEMORY_ALLOCATED_NOT_FREED_KEY ||
            current.viewingOption === ALLOC_SPACE_MEMORY_ALLOCATED_KEY ?
        'B' :
        '';
    this.flamegraph.draw(ctx, size.width, size.height, 0, HEADER_HEIGHT, unit);
  }

  onMouseClick({x, y}: {x: number, y: number}): boolean {
    const expandedCallsite = this.flamegraph.onMouseClick({x, y});
    globals.dispatch(Actions.expandHeapProfileFlamegraph({expandedCallsite}));
    return true;
  }

  onMouseMove({x, y}: {x: number, y: number}): boolean {
    this.flamegraph.onMouseMove({x, y});
    return true;
  }

  onMouseOut() {
    this.flamegraph.onMouseOut();
  }
}
