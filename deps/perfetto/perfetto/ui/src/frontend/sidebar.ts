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

import * as m from 'mithril';

import {assertTrue} from '../base/logging';
import {Actions} from '../common/actions';
import {QueryResponse} from '../common/queries';
import {EngineMode} from '../common/state';

import {Animation} from './animation';
import {globals} from './globals';
import {toggleHelp} from './help_modal';
import {
  isLegacyTrace,
  openFileWithLegacyTraceViewer,
} from './legacy_trace_viewer';
import {showModal} from './modal';

const ALL_PROCESSES_QUERY = 'select name, pid from process order by name;';

const CPU_TIME_FOR_PROCESSES = `
select
  process.name,
  tot_proc/1e9 as cpu_sec
from
  (select
    upid,
    sum(tot_thd) as tot_proc
  from
    (select
      utid,
      sum(dur) as tot_thd
    from sched group by utid)
  join thread using(utid) group by upid)
join process using(upid)
order by cpu_sec desc limit 100;`;

const CYCLES_PER_P_STATE_PER_CPU = `
select
  cpu,
  freq,
  dur,
  sum(dur * freq)/1e6 as mcycles
from (
  select
    cpu,
    value as freq,
    lead(ts) over (partition by cpu order by ts) - ts as dur
  from counter
  inner join cpu_counter_track on counter.track_id = cpu_counter_track.id
  where name = 'cpufreq'
) group by cpu, freq
order by mcycles desc limit 32;`;

const CPU_TIME_BY_CLUSTER_BY_PROCESS = `
select process.name as process, thread, core, cpu_sec from (
  select thread.name as thread, upid,
    case when cpug = 0 then 'little' else 'big' end as core,
    cpu_sec from (select cpu/4 as cpug, utid, sum(dur)/1e9 as cpu_sec
    from sched group by utid, cpug order by cpu_sec desc
  ) inner join thread using(utid)
) inner join process using(upid) limit 30;`;

const HEAP_GRAPH_BYTES_PER_TYPE = `
select
  upid,
  graph_sample_ts,
  type_name,
  sum(self_size) as total_self_size
from heap_graph_object
group by
 upid,
 graph_sample_ts,
 type_name
order by total_self_size desc
limit 100;`;

const SQL_STATS = `
with first as (select started as ts from sqlstats limit 1)
select query,
    round((max(ended - started, 0))/1e6) as runtime_ms,
    round((max(started - queued, 0))/1e6) as latency_ms,
    round((started - first.ts)/1e6) as t_start_ms
from sqlstats, first
order by started desc`;

const TRACE_STATS = 'select * from stats order by severity, source, name, idx';

let lastTabTitle = '';

function createCannedQuery(query: string): (_: Event) => void {
  return (e: Event) => {
    e.preventDefault();
    globals.dispatch(Actions.executeQuery({
      engineId: '0',
      queryId: 'command',
      query,
    }));
  };
}

const EXAMPLE_ANDROID_TRACE_URL =
    'https://storage.googleapis.com/perfetto-misc/example_android_trace_15s';

const EXAMPLE_CHROME_TRACE_URL =
    'https://storage.googleapis.com/perfetto-misc/example_chrome_trace_4s_1.json';

const SECTIONS = [
  {
    title: 'Navigation',
    summary: 'Open or record a new trace',
    expanded: true,
    items: [
      {t: 'Open trace file', a: popupFileSelectionDialog, i: 'folder_open'},
      {
        t: 'Open with legacy UI',
        a: popupFileSelectionDialogOldUI,
        i: 'filter_none'
      },
      {t: 'Record new trace', a: navigateRecord, i: 'fiber_smart_record'},
    ],
  },
  {
    title: 'Current Trace',
    summary: 'Actions on the current trace',
    expanded: true,
    hideIfNoTraceLoaded: true,
    appendOpenedTraceTitle: true,
    items: [
      {t: 'Show timeline', a: navigateViewer, i: 'line_style'},
      {
        t: 'Share',
        a: dispatchCreatePermalink,
        i: 'share',
        checkDownloadDisabled: true,
        internalUserOnly: true,
      },
      {
        t: 'Download',
        a: downloadTrace,
        i: 'file_download',
        checkDownloadDisabled: true,
      },
      {t: 'Legacy UI', a: openCurrentTraceWithOldUI, i: 'filter_none'},
    ],
  },
  {
    title: 'Example Traces',
    expanded: true,
    summary: 'Open an example trace',
    items: [
      {
        t: 'Open Android example',
        a: openTraceUrl(EXAMPLE_ANDROID_TRACE_URL),
        i: 'description'
      },
      {
        t: 'Open Chrome example',
        a: openTraceUrl(EXAMPLE_CHROME_TRACE_URL),
        i: 'description'
      },
    ],
  },
  {
    title: 'Metrics and auditors',
    summary: 'Compute summary statistics',
    items: [
      {
        t: 'All Processes',
        a: createCannedQuery(ALL_PROCESSES_QUERY),
        i: 'search',
      },
      {
        t: 'CPU Time by process',
        a: createCannedQuery(CPU_TIME_FOR_PROCESSES),
        i: 'search',
      },
      {
        t: 'Cycles by p-state by CPU',
        a: createCannedQuery(CYCLES_PER_P_STATE_PER_CPU),
        i: 'search',
      },
      {
        t: 'CPU Time by cluster by process',
        a: createCannedQuery(CPU_TIME_BY_CLUSTER_BY_PROCESS),
        i: 'search',
      },
      {
        t: 'Heap Graph: Bytes per type',
        a: createCannedQuery(HEAP_GRAPH_BYTES_PER_TYPE),
        i: 'search',
      },
      {
        t: 'Trace stats',
        a: createCannedQuery(TRACE_STATS),
        i: 'bug_report',
      },
      {
        t: 'Debug SQL performance',
        a: createCannedQuery(SQL_STATS),
        i: 'bug_report',
      },
    ],
  },
  {
    title: 'Support',
    summary: 'Documentation & Bugs',
    items: [
      {
        t: 'Controls',
        a: openHelp,
        i: 'help',
      },
      {
        t: 'Documentation',
        a: 'https://perfetto.dev',
        i: 'find_in_page',
      },
      {
        t: 'Report a bug',
        a: 'https://goto.google.com/perfetto-ui-bug',
        i: 'bug_report',
      },
    ],
  },

];

const vidSection = {
  title: 'Video',
  summary: 'Open a screen recording',
  expanded: true,
  items: [
    {t: 'Open video file', a: popupVideoSelectionDialog, i: 'folder_open'},
  ],
};

function openHelp(e: Event) {
  e.preventDefault();
  toggleHelp();
}

function getFileElement(): HTMLInputElement {
  return document.querySelector('input[type=file]')! as HTMLInputElement;
}

function popupFileSelectionDialog(e: Event) {
  e.preventDefault();
  delete getFileElement().dataset['useCatapultLegacyUi'];
  delete getFileElement().dataset['video'];
  getFileElement().click();
}

function popupFileSelectionDialogOldUI(e: Event) {
  e.preventDefault();
  delete getFileElement().dataset['video'];
  getFileElement().dataset['useCatapultLegacyUi'] = '1';
  getFileElement().click();
}

function openCurrentTraceWithOldUI(e: Event) {
  e.preventDefault();
  console.assert(isTraceLoaded());
  if (!isTraceLoaded) return;
  const engine = Object.values(globals.state.engines)[0];
  const src = engine.source;
  if (src.type === 'ARRAY_BUFFER') {
    openInOldUIWithSizeCheck(new Blob([src.buffer]));
  } else if (src.type === 'FILE') {
    openInOldUIWithSizeCheck(src.file);
  } else {
    throw new Error('Loading from a URL to catapult is not yet supported');
    // TODO(nicomazz): Find how to get the data of the current trace if it is
    // from a URL. It seems that the trace downloaded is given to the trace
    // processor, but not kept somewhere accessible. Maybe the only way is to
    // download the trace (again), and then open it. An alternative can be to
    // save a copy.
  }
}

function isTraceLoaded(): boolean {
  const engine = Object.values(globals.state.engines)[0];
  return engine !== undefined;
}

function popupVideoSelectionDialog(e: Event) {
  e.preventDefault();
  delete getFileElement().dataset['useCatapultLegacyUi'];
  getFileElement().dataset['video'] = '1';
  getFileElement().click();
}

function openTraceUrl(url: string): (e: Event) => void {
  return e => {
    e.preventDefault();
    globals.frontendLocalState.localOnlyMode = false;
    globals.dispatch(Actions.openTraceFromUrl({url}));
  };
}

function onInputElementFileSelectionChanged(e: Event) {
  if (!(e.target instanceof HTMLInputElement)) {
    throw new Error('Not an input element');
  }
  if (!e.target.files) return;
  const file = e.target.files[0];
  // Reset the value so onchange will be fired with the same file.
  e.target.value = '';

  globals.frontendLocalState.localOnlyMode = false;

  if (e.target.dataset['useCatapultLegacyUi'] === '1') {
    openWithLegacyUi(file);
    return;
  }

  if (e.target.dataset['video'] === '1') {
    // TODO(hjd): Update this to use a controller and publish.
    globals.dispatch(Actions.executeQuery({
      engineId: '0', queryId: 'command',
      query: `select ts from slices where name = 'first_frame' union ` +
             `select start_ts from trace_bounds`}));
    setTimeout(() => {
      const resp = globals.queryResults.get('command') as QueryResponse;
      // First value is screenrecord trace event timestamp
      // and second value is trace boundary's start timestamp
      const offset = (Number(resp.rows[1]['ts'].toString()) -
                      Number(resp.rows[0]['ts'].toString())) /
          1e9;
      globals.queryResults.delete('command');
      globals.rafScheduler.scheduleFullRedraw();
      globals.dispatch(Actions.deleteQuery({queryId: 'command'}));
      globals.dispatch(Actions.setVideoOffset({offset}));
    }, 1000);
    globals.dispatch(Actions.openVideoFromFile({file}));
    return;
  }

  globals.dispatch(Actions.openTraceFromFile({file}));
}

async function openWithLegacyUi(file: File) {
  // Switch back to the old catapult UI.
  if (await isLegacyTrace(file)) {
    openFileWithLegacyTraceViewer(file);
    return;
  }
  openInOldUIWithSizeCheck(file);
}

function openInOldUIWithSizeCheck(trace: Blob) {
  // Perfetto traces smaller than 50mb can be safely opened in the legacy UI.
  if (trace.size < 1024 * 1024 * 50) {
    globals.dispatch(Actions.convertTraceToJson({file: trace}));
    return;
  }

  // Give the user the option to truncate larger perfetto traces.
  const size = Math.round(trace.size / (1024 * 1024));
  showModal({
    title: 'Legacy UI may fail to open this trace',
    content:
        m('div',
          m('p',
            `This trace is ${size}mb, opening it in the legacy UI ` +
                `may fail.`),
          m('p',
            'More options can be found at ',
            m('a',
              {
                href: 'https://goto.google.com/opening-large-traces',
                target: '_blank'
              },
              'go/opening-large-traces'),
            '.')),
    buttons: [
      {
        text: 'Open full trace (not recommended)',
        primary: false,
        id: 'open',
        action: () => {
          globals.dispatch(Actions.convertTraceToJson({file: trace}));
        }
      },
      {
        text: 'Open beginning of trace',
        primary: true,
        id: 'truncate-start',
        action: () => {
          globals.dispatch(
              Actions.convertTraceToJson({file: trace, truncate: 'start'}));
        }
      },
      {
        text: 'Open end of trace',
        primary: true,
        id: 'truncate-end',
        action: () => {
          globals.dispatch(
              Actions.convertTraceToJson({file: trace, truncate: 'end'}));
        }
      }

    ]
  });
  return;
}

function navigateRecord(e: Event) {
  e.preventDefault();
  globals.dispatch(Actions.navigate({route: '/record'}));
}

function navigateViewer(e: Event) {
  e.preventDefault();
  globals.dispatch(Actions.navigate({route: '/viewer'}));
}

function isDownloadAndShareDisabled(): boolean {
  if (globals.frontendLocalState.localOnlyMode) return true;
  const engine = Object.values(globals.state.engines)[0];
  if (engine && engine.source.type === 'HTTP_RPC') return true;
  return false;
}

function dispatchCreatePermalink(e: Event) {
  e.preventDefault();
  if (isDownloadAndShareDisabled() || !isTraceLoaded()) return;

  const result = confirm(
      `Upload the trace and generate a permalink. ` +
      `The trace will be accessible by anybody with the permalink.`);
  if (result) globals.dispatch(Actions.createPermalink({}));
}

function downloadTrace(e: Event) {
  e.preventDefault();
  if (!isTraceLoaded() || isDownloadAndShareDisabled()) return;

  const engine = Object.values(globals.state.engines)[0];
  if (!engine) return;
  let url = '';
  let fileName = 'trace.pftrace';
  const src = engine.source;
  if (src.type === 'URL') {
    url = src.url;
    fileName = url.split('/').slice(-1)[0];
  } else if (src.type === 'ARRAY_BUFFER') {
    const blob = new Blob([src.buffer], {type: 'application/octet-stream'});
    url = URL.createObjectURL(blob);
  } else if (src.type === 'FILE') {
    const file = src.file;
    url = URL.createObjectURL(file);
    fileName = file.name;
  } else {
    throw new Error(`Download from ${JSON.stringify(src)} is not supported`);
  }

  const a = document.createElement('a');
  a.href = url;
  a.download = fileName;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}


const EngineRPCWidget: m.Component = {
  view() {
    let cssClass = '';
    let title = 'Number of pending SQL queries';
    let label: string;
    let failed = false;
    let mode: EngineMode|undefined;

    // We are assuming we have at most one engine here.
    const engines = Object.values(globals.state.engines);
    assertTrue(engines.length <= 1);
    for (const engine of engines) {
      mode = engine.mode;
      if (engine.failed !== undefined) {
        cssClass += '.red';
        title = 'Query engine crashed\n' + engine.failed;
        failed = true;
      }
    }

    // If we don't have an engine yet, guess what will be the mode that will
    // be used next time we'll create one. Even if we guess it wrong (somehow
    // trace_controller.ts takes a different decision later, e.g. because the
    // RPC server is shut down after we load the UI and cached httpRpcState)
    // this will eventually become  consistent once the engine is created.
    if (mode === undefined) {
      if (globals.frontendLocalState.httpRpcState.connected &&
          globals.state.newEngineMode === 'USE_HTTP_RPC_IF_AVAILABLE') {
        mode = 'HTTP_RPC';
      } else {
        mode = 'WASM';
      }
    }

    if (mode === 'HTTP_RPC') {
      cssClass += '.green';
      label = 'RPC';
      title += '\n(Query engine: native accelerator over HTTP+RPC)';
    } else {
      label = 'WSM';
      title += '\n(Query engine: built-in WASM)';
    }

    return m(
        `.dbg-info-square${cssClass}`,
        {title},
        m('div', label),
        m('div', `${failed ? 'FAIL' : globals.numQueuedQueries}`));
  }
};

const ServiceWorkerWidget: m.Component = {
  view() {
    let cssClass = '';
    let title = 'Service Worker: ';
    let label = 'N/A';
    const ctl = globals.serviceWorkerController;
    if ((!('serviceWorker' in navigator))) {
      label = 'N/A';
      title += 'not supported by the browser (requires HTTPS)';
    } else if (ctl.bypassed) {
      label = 'OFF';
      cssClass = '.red';
      title += 'Bypassed, using live network. Double-click to re-enable';
    } else if (ctl.installing) {
      label = 'UPD';
      cssClass = '.amber';
      title += 'Installing / updating ...';
    } else if (!navigator.serviceWorker.controller) {
      label = 'N/A';
      title += 'Not available, using network';
    } else {
      label = 'ON';
      cssClass = '.green';
      title += 'Serving from cache. Ready for offline use';
    }

    const toggle = async () => {
      if (globals.serviceWorkerController.bypassed) {
        globals.serviceWorkerController.setBypass(false);
        return;
      }
      showModal({
        title: 'Disable service worker?',
        content: m(
            'div',
            m('p', `If you continue the service worker will be disabled until
                      manually re-enabled.`),
            m('p', `All future requests will be served from the network and the
                    UI won't be available offline.`),
            m('p', `You should do this only if you are debugging the UI
                    or if you are experiencing caching-related problems.`),
            m('p', `Disabling will cause a refresh of the UI, the current state
                    will be lost.`),
            ),
        buttons: [
          {
            text: 'Disable and reload',
            primary: true,
            id: 'sw-bypass-enable',
            action: () => {
              globals.serviceWorkerController.setBypass(true).then(
                  () => location.reload());
            }
          },
          {
            text: 'Cancel',
            primary: false,
            id: 'sw-bypass-cancel',
            action: () => {}
          }
        ]
      });
    };

    return m(
        `.dbg-info-square${cssClass}`,
        {title, ondblclick: toggle},
        m('div', 'SW'),
        m('div', label));
  }
};

const SidebarFooter: m.Component = {
  view() {
    return m(
        '.sidebar-footer',
        m('button',
          {
            onclick: () => globals.frontendLocalState.togglePerfDebug(),
          },
          m('i.material-icons',
            {title: 'Toggle Perf Debug Mode'},
            'assessment')),
        m(EngineRPCWidget),
        m(ServiceWorkerWidget),
    );
  }
};


export class Sidebar implements m.ClassComponent {
  private _redrawWhileAnimating =
      new Animation(() => globals.rafScheduler.scheduleFullRedraw());
  view() {
    const vdomSections = [];
    for (const section of SECTIONS) {
      if (section.hideIfNoTraceLoaded && !isTraceLoaded()) continue;
      const vdomItems = [];
      for (const item of section.items) {
        let attrs = {
          onclick: typeof item.a === 'function' ? item.a : null,
          href: typeof item.a === 'string' ? item.a : '#',
          target: typeof item.a === 'string' ? '_blank' : null,
          disabled: false,
        };
        if ((item as {internalUserOnly: boolean}).internalUserOnly === true) {
          if (!globals.isInternalUser) continue;
        }
        if (isDownloadAndShareDisabled() &&
            item.hasOwnProperty('checkDownloadDisabled')) {
          attrs = {
            onclick: () => alert('Can not download or share external trace.'),
            href: '#',
            target: null,
            disabled: true,
          };
        }
        vdomItems.push(
            m('li', m('a', attrs, m('i.material-icons', item.i), item.t)));
      }
      if (section.appendOpenedTraceTitle) {
        const engines = Object.values(globals.state.engines);
        if (engines.length === 1) {
          let traceTitle = '';
          switch (engines[0].source.type) {
            case 'FILE':
              // Split on both \ and / (because C:\Windows\paths\are\like\this).
              traceTitle = engines[0].source.file.name.split(/[/\\]/).pop()!;
              const fileSizeMB = Math.ceil(engines[0].source.file.size / 1e6);
              traceTitle += ` (${fileSizeMB} MB)`;
              break;
            case 'URL':
              traceTitle = engines[0].source.url.split('/').pop()!;
              break;
            case 'ARRAY_BUFFER':
              traceTitle = 'External trace';
              break;
            case 'HTTP_RPC':
              traceTitle = 'External trace (RPC)';
              break;
            default:
              break;
          }
          if (traceTitle !== '') {
            const tabTitle = `${traceTitle} - Perfetto UI`;
            if (tabTitle !== lastTabTitle) {
              document.title = lastTabTitle = tabTitle;
            }
            vdomItems.unshift(m('li', m('a.trace-file-name', traceTitle)));
          }
        }
      }
      vdomSections.push(
          m(`section${section.expanded ? '.expanded' : ''}`,
            m('.section-header',
              {
                onclick: () => {
                  section.expanded = !section.expanded;
                  globals.rafScheduler.scheduleFullRedraw();
                }
              },
              m('h1', {title: section.summary}, section.title),
              m('h2', section.summary)),
            m('.section-content', m('ul', vdomItems))));
    }
    if (globals.state.videoEnabled) {
      const videoVdomItems = [];
      for (const item of vidSection.items) {
        videoVdomItems.push(
          m('li',
            m(`a`,
              {
                onclick: typeof item.a === 'function' ? item.a : null,
                href: typeof item.a === 'string' ? item.a : '#',
              },
              m('i.material-icons', item.i),
              item.t)));
      }
      vdomSections.push(
        m(`section${vidSection.expanded ? '.expanded' : ''}`,
          m('.section-header',
            {
              onclick: () => {
                vidSection.expanded = !vidSection.expanded;
                globals.rafScheduler.scheduleFullRedraw();
              }
            },
            m('h1', vidSection.title),
            m('h2', vidSection.summary), ),
          m('.section-content', m('ul', videoVdomItems))));
    }
    return m(
        'nav.sidebar',
        {
          class: globals.frontendLocalState.sidebarVisible ? 'show-sidebar' :
                                                             'hide-sidebar',
          // 150 here matches --sidebar-timing in the css.
          ontransitionstart: () => this._redrawWhileAnimating.start(150),
          ontransitionend: () => this._redrawWhileAnimating.stop(),
        },
        m(
            'header',
            m('img[src=assets/brand.png].brand'),
            m('button.sidebar-button',
              {
                onclick: () => {
                  globals.frontendLocalState.toggleSidebar();
                },
              },
              m('i.material-icons',
                {
                  title: globals.frontendLocalState.sidebarVisible ?
                      'Hide menu' :
                      'Show menu',
                },
                'menu')),
            ),
        m('input[type=file]', {onchange: onInputElementFileSelectionChanged}),
        m('.sidebar-scroll',
          m(
              '.sidebar-scroll-container',
              ...vdomSections,
              m(SidebarFooter),
              )),
    );
  }
}
