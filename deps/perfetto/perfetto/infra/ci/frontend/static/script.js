/**
 * Copyright (c) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

'use strict';

// If you add or remove job types, do not forget to fix the colspans below.
const JOB_TYPES = [
  { id: 'linux-gcc7-x86_64-release', label: 'rel' },
  { id: 'linux-clang-x86_64-debug', label: 'dbg' },
  { id: 'linux-clang-x86_64-tsan', label: 'tsan' },
  { id: 'linux-clang-x86_64-msan', label: 'msan' },
  { id: 'linux-clang-x86_64-asan_lsan', label: '{a,l}san' },
  { id: 'linux-clang-x86-asan_lsan', label: 'x86 {a,l}san' },
  { id: 'linux-clang-x86_64-libfuzzer', label: 'fuzzer' },
  { id: 'linux-clang-x86_64-bazel', label: 'bazel' },
  { id: 'ui-clang-x86_64-debug', label: 'dbg' },
  { id: 'ui-clang-x86_64-release', label: 'rel' },
  { id: 'android-clang-arm-release', label: 'rel' },
  { id: 'android-clang-arm-asan', label: 'asan' },
];

// Chart IDs from the Stackdriver daashboard
// https://app.google.stackdriver.com/dashboards/5008687313278081798?project=perfetto-ci.
const STATS_CHART_IDS = [
  '2617092544855936024',   // Job queue len.
  '15349606823829051218',  // Workers CPU usage
  '2339121267466167448',   // E2E CL test time (median).
  '6055813426334723906',   // Job run time (median).
  '13112965165933080534',  // Job queue time (P95).
  '2617092544855936456',   // Job run time (P95).
];

const state = {
  // An array of recent CL objects retrieved from Gerrit.
  gerritCls: [],

  // A map of sha1 -> Gerrit commit object.
  // See https://gerrit-review.googlesource.com/Documentation/rest-api-projects.html#get-commit
  gerritCommits: {},

  // A map of git-log ranges to commit objects:
  // 'dead..beef' -> [commit1, 2]
  gerritLogs: {},

  // Maps 'cls/1234-1' or 'branches/xxxx' -> array of job ids.
  dbJobSets: {},

  // Maps 'jobId' -> DB job object, as perf /ci/jobs/jobID.
  // A jobId looks like 20190702143507-1008614-9-android-clang-arm.
  dbJobs: {},

  // Maps 'worker id' -> DB wokrker object, as per /ci/workers.
  dbWorker: {},

  // Maps 'master-YYMMDD' -> DB branch object, as perf /ci/branches/xxx.
  dbBranches: {},
  getBranchKeys: () => Object.keys(state.dbBranches).sort().reverse(),

  // Maps 'CL number' -> true|false. Retains the collapsed/expanded information
  // for each row in the CLs table.
  expandCl: {},

  postsubmitShown: 3,

  // Lines that will be appended to the terminal on the next redraw() cycle.
  termLines: [
    'Hover a CL icon to see the log tail.',
    'Click on it to load the full log.'
  ],
  termJobId: undefined, // The job id currently being shown by the terminal.
  termClear: false,     // If true the next redraw will clear the terminal.
  redrawPending: false,

  // State for the Jobs page. These are arrays of job ids.
  jobsQueued: [],
  jobsRunning: [],
  jobsRecent: [],

  // Firebase DB listeners (the objects returned by the .ref() operator).
  realTimeLogRef: undefined, // Ref for the real-time log streaming.
  workersRef: undefined,
  jobsRunningRef: undefined,
  jobsQueuedRef: undefined,
  jobsRecentRef: undefined,
  clRefs: {},    // '1234-1' -> Ref subscribed to updates on the given cl.
  jobRefs: {},   // '....-arm-asan' -> Ref subscribed updates on the given job.
  branchRefs: {} // 'master' -> Ref subscribed updates on the given branch.
};

let term = undefined;

function main() {
  firebase.initializeApp({ databaseURL: cfg.DB_ROOT });
  Terminal.applyAddon(fit);
  Terminal.applyAddon(search);

  m.route(document.body, '/cls', {
    '/cls': CLsPageRenderer,
    '/cls/:cl': CLsPageRenderer,
    '/logs/:jobId': LogsPageRenderer,
    '/jobs': JobsPageRenderer,
    '/jobs/:jobId': JobsPageRenderer,
    '/stats/:period': StatsPageRenderer,
  });

  setInterval(fetchGerritCLs, 15000);
  fetchGerritCLs();
  fetchCIStatusForBranch('master');
}

// -----------------------------------------------------------------------------
// Rendering functions
// -----------------------------------------------------------------------------

function renderHeader() {
  const active = id => m.route.get().startsWith(`/${id}`) ? '.active' : '';
  const logUrl = 'https://goto.google.com/perfetto-ci-logs-';
  const docsUrl = 'https://docs.perfetto.dev/#/continuous-integration';
  return m('header',
    m('a[href=/#!/cls]', m('h1', 'Perfetto ', m('span', 'CI'))),
    m('nav',
      m(`div${active('cls')}`, m('a[href=/#!/cls]', 'CLs')),
      m(`div${active('jobs')}`, m('a[href=/#!/jobs]', 'Jobs')),
      m(`div${active('stats')}`, m('a[href=/#!/stats/1d]', 'Stats')),
      m(`div`, m(`a[href=${docsUrl}][target=_blank]`, 'Docs')),
      m(`div.logs`, 'Logs',
        m('div', m(`a[href=${logUrl}controller][target=_blank]`, 'Controller')),
        m('div', m(`a[href=${logUrl}workers][target=_blank]`, 'Workers')),
        m('div', m(`a[href=${logUrl}frontend][target=_blank]`, 'Frontend')),
      ),
    )
  );
}

var CLsPageRenderer = {
  view: function (vnode) {
    const allCols = 4 + JOB_TYPES.length;
    const postsubmitHeader = m('tr',
      m(`td.header[colspan=${allCols}]`, 'Post-submit')
    );

    const postsubmitLoadMore = m('tr',
      m(`td[colspan=${allCols}]`,
        m('a[href=#]',
          { onclick: () => state.postsubmitShown += 10 },
          'Load more'
        )
      )
    );

    const presubmitHeader = m('tr',
      m(`td.header[colspan=${allCols}]`, 'Pre-submit')
    );

    let branchRows = [];
    const branchKeys = state.getBranchKeys();
    for (let i = 0; i < branchKeys.length && i < state.postsubmitShown; i++) {
      const rowsForBranch = renderPostsubmitRow(branchKeys[i]);
      branchRows = branchRows.concat(rowsForBranch);
    }

    let clRows = [];
    for (const gerritCl of state.gerritCls) {
      if (vnode.attrs.cl && gerritCl.num != vnode.attrs.cl) continue;
      clRows = clRows.concat(renderCLRow(gerritCl));
    }

    let footer = [];
    if (vnode.attrs.cl) {
      footer = m('footer',
        `Showing only CL ${vnode.attrs.cl} - `,
        m(`a[href=#!/cls]`, 'Click here to see all CLs')
      );
    }

    return [
      renderHeader(),
      m('main#cls',
        m('div.table-scrolling-container',
          m('table.main-table',
            m('thead',
              m('tr',
                m('td[rowspan=4]', 'Subject'),
                m('td[rowspan=4]', 'Status'),
                m('td[rowspan=4]', 'Owner'),
                m('td[rowspan=4]', 'Updated'),
                m('td[colspan=12]', 'Bots'),
              ),
              m('tr',
                m('td[colspan=10]', 'linux'),
                m('td[colspan=2]', 'android'),
              ),
              m('tr',
                m('td', 'gcc7'),
                m('td[colspan=7]', 'clang'),
                m('td[colspan=2]', 'ui'),
                m('td[colspan=2]', 'clang-arm'),
              ),
              m('tr#cls_header',
                JOB_TYPES.map(job => m(`td#${job.id}`, job.label))
              ),
            ),
            m('tbody',
              postsubmitHeader,
              branchRows,
              postsubmitLoadMore,
              presubmitHeader,
              clRows,
            )
          ),
          footer,
        ),
        m(TermRenderer),
      ),
    ];
  }
};


function getLastUpdate(lastUpdate) {
  const lastUpdateMins = Math.ceil((Date.now() - lastUpdate) / 60000);
  if (lastUpdateMins < 60)
    return lastUpdateMins + ' mins ago';
  if (lastUpdateMins < 60 * 24)
    return Math.ceil(lastUpdateMins / 60) + ' hours ago';
  return lastUpdate.toLocaleDateString();
}

function renderCLRow(cl) {
  const expanded = !!state.expandCl[cl.num];
  const toggleExpand = () => {
    state.expandCl[cl.num] ^= 1;
    fetchCIJobsForAllPatchsetOfCL(cl.num);
  }
  const rows = [];

  // Create the row for the latest patchset (as fetched by Gerrit).
  rows.push(m(`tr.${cl.status}`,
    m('td',
      m(`i.material-icons.expand${expanded ? '.expanded' : ''}`,
        { onclick: toggleExpand },
        'arrow_right'
      ),
      m(`a[href=${cfg.GERRIT_REVIEW_URL}/+/${cl.num}/${cl.psNum}]`,
        `${cl.subject}`, m('span.ps', `#${cl.psNum}`))
    ),
    m('td', cl.status),
    m('td', stripEmail(cl.owner)),
    m('td', getLastUpdate(cl.lastUpdate)),
    JOB_TYPES.map(x => renderClJobCell(`cls/${cl.num}-${cl.psNum}`, x.id))
  ));

  // If the usere clicked on the expand button, show also the other patchsets
  // present in the CI DB.
  for (let psNum = cl.psNum; expanded && psNum > 0; psNum--) {
    const src = `cls/${cl.num}-${psNum}`;
    const jobs = state.dbJobSets[src];
    if (!jobs) continue;
    rows.push(m(`tr.nested`,
      m('td',
        m(`a[href=${cfg.GERRIT_REVIEW_URL}/+/${cl.num}/${psNum}]`,
          '  Patchset', m('span.ps', `#${psNum}`))
      ),
      m('td', ''),
      m('td', ''),
      m('td', ''),
      JOB_TYPES.map(x => renderClJobCell(src, x.id))
    ));
  }

  return rows;
}

function renderPostsubmitRow(key) {
  const branch = state.dbBranches[key];
  console.assert(branch !== undefined);
  const subject = branch.subject;
  let rows = [];
  rows.push(m(`tr`,
    m('td',
      m(`a[href=${cfg.REPO_URL}/+/${branch.rev}]`,
        subject, m('span.ps', `#${branch.rev.substr(0, 8)}`)
      )
    ),
    m('td', ''),
    m('td', stripEmail(branch.author)),
    m('td', getLastUpdate(new Date(branch.time_committed))),
    JOB_TYPES.map(x => renderClJobCell(`branches/${key}`, x.id))
  ));


  const allKeys = state.getBranchKeys();
  const curIdx = allKeys.indexOf(key);
  if (curIdx >= 0 && curIdx < allKeys.length - 1) {
    const nextKey = allKeys[curIdx + 1];
    const range = `${state.dbBranches[nextKey].rev}..${branch.rev}`;
    const logs = (state.gerritLogs[range] || []).slice(1);
    for (const log of logs) {
      if (log.parents.length < 2)
        continue;  // Show only merge commits.
      rows.push(
        m('tr.nested',
          m('td',
            m(`a[href=${cfg.REPO_URL}/+/${log.commit}]`,
              log.message.split('\n')[0],
              m('span.ps', `#${log.commit.substr(0, 8)}`)
            )
          ),
          m('td', ''),
          m('td', stripEmail(log.author.email)),
          m('td', getLastUpdate(parseGerritTime(log.committer.time))),
          m(`td[colspan=${JOB_TYPES.length}]`,
            'No post-submit was run for this revision'
          ),
        )
      );
    }
  }

  return rows;
}

function renderJobLink(jobId, jobStatus) {
  const ICON_MAP = {
    'COMPLETED': 'check_circle',
    'STARTED': 'hourglass_full',
    'QUEUED': 'schedule',
    'FAILED': 'bug_report',
    'CANCELLED': 'cancel',
    'INTERRUPTED': 'cancel',
    'TIMED_OUT': 'notification_important',
  };
  const icon = ICON_MAP[jobStatus] || 'clear';
  const eventHandlers = jobId ? { onmouseover: () => showLogTail(jobId) } : {};
  const logUrl = jobId ? `#!/logs/${jobId}` : '#';
  return m(`a.${jobStatus}[href=${logUrl}][title=${jobStatus}]`,
    eventHandlers,
    m(`i.material-icons`, icon)
  );
}

function renderClJobCell(src, jobType) {
  let jobStatus = 'UNKNOWN';
  let jobId = undefined;

  // To begin with check that the given CL/PS is present in the DB (the
  // AppEngine cron job might have not seen that at all yet).
  // If it is, find the global job id for the given jobType for the passed CL.
  for (const id of (state.dbJobSets[src] || [])) {
    const job = state.dbJobs[id];
    if (job !== undefined && job.type == jobType) {
      // We found the job object that corresponds to jobType for the given CL.
      jobStatus = job.status;
      jobId = id;
    }
  }
  return m('td.job', renderJobLink(jobId, jobStatus));
}

const TermRenderer = {
  oncreate: function (vnode) {
    console.log('Creating terminal object');
    term = new Terminal({ rows: 6, fontSize: 12, scrollback: 100000 });
    term.open(vnode.dom);
    term.fit();
    if (vnode.attrs.focused) term.focus();
  },
  onremove: function (vnode) {
    term.destroy();
  },
  onupdate: function (vnode) {
    if (state.termClear) {
      term.clear()
      state.termClear = false;
    }
    for (const line of state.termLines) {
      term.write(line + '\r\n');
    }
    state.termLines = [];
  },
  view: function () {
    return m('.term-container',
      {
        onkeydown: (e) => {
          if (e.key === 'f' && (e.ctrlKey || e.metaKey)) {
            document.querySelector('.term-search').select();
            e.preventDefault();
          }
        }
      },
      m('input[type=text][placeholder=search and press Enter].term-search', {
        onkeydown: (e) => {
          if (e.key !== 'Enter') return;
          if (e.shiftKey) {
            term.findNext(e.target.value);
          } else {
            term.findPrevious(e.target.value);
          }
          e.stopPropagation();
          e.preventDefault();
        }
      })
    );
  }
};

const LogsPageRenderer = {
  oncreate: function (vnode) {
    showFullLog(vnode.attrs.jobId);
  },
  view: function () {
    return [
      renderHeader(),
      m(TermRenderer, { focused: true })
    ];
  }
}

const JobsPageRenderer = {
  oncreate: function (vnode) {
    fetchRecentJobsStatus();
    fetchWorkers();
  },

  createWorkerTable: function () {
    const makeWokerRow = workerId => {
      const worker = state.dbWorker[workerId];
      if (worker.status === 'TERMINATED') return [];
      return m('tr',
        m('td', worker.host),
        m('td', workerId),
        m('td', worker.status),
        m('td', getLastUpdate(new Date(worker.last_update))),
        m('td', m(`a[href=#!/jobs/${worker.job_id}]`, worker.job_id)),
      );
    };
    return m('table.main-table',
      m('thead',
        m('tr', m('td[colspan=5]', 'Workers')),
        m('tr',
          m('td', 'Host'),
          m('td', 'Worker'),
          m('td', 'Status'),
          m('td', 'Last ping'),
          m('td', 'Job'),
        )
      ),
      m('tbody', Object.keys(state.dbWorker).map(makeWokerRow))
    );
  },

  createJobsTable: function (vnode, title, jobIds) {
    const tStr = function (tStart, tEnd) {
      return new Date(tEnd - tStart).toUTCString().substr(17, 9);
    };

    const makeJobRow = function (jobId) {
      const job = state.dbJobs[jobId] || {};
      let cols = [
        m('td.job.align-left',
          renderJobLink(jobId, job ? job.status : undefined),
          m(`span.status.${job.status}`, job.status)
        )
      ];
      if (job) {
        const tQ = Date.parse(job.time_queued);
        const tS = Date.parse(job.time_started);
        const tE = Date.parse(job.time_ended) || Date.now();
        let cell = m('');
        if (job.src === undefined) {
          cell = '?';
        } else if (job.src.startsWith('cls/')) {
          const cl_and_ps = job.src.substr(4).replace('-', '/');
          const href = `${cfg.GERRIT_REVIEW_URL}/+/${cl_and_ps}`;
          cell = m(`a[href=${href}][target=_blank]`, cl_and_ps);
        } else if (job.src.startsWith('branches/')) {
          cell = job.src.substr(9).split('-')[0]
        }
        cols.push(m('td', cell));
        cols.push(m('td', `${job.type}`));
        cols.push(m('td', `${job.worker || ''}`));
        cols.push(m('td', `${job.time_queued}`));
        cols.push(m(`td[title=Start ${job.time_started}]`, `${tStr(tQ, tS)}`));
        cols.push(m(`td[title=End ${job.time_ended}]`, `${tStr(tS, tE)}`));
      } else {
        cols.push(m('td[colspan=6]', jobId));
      }
      return m(`tr${vnode.attrs.jobId === jobId ? '.selected' : ''}`, cols)
    };

    return m('table.main-table',
      m('thead',
        m('tr', m('td[colspan=7]', title)),

        m('tr',
          m('td', 'Status'),
          m('td', 'CL'),
          m('td', 'Type'),
          m('td', 'Worker'),
          m('td', 'T queued'),
          m('td', 'Queue time'),
          m('td', 'Run time'),
        )
      ),
      m('tbody', jobIds.map(makeJobRow))
    );
  },

  view: function (vnode) {
    return [
      renderHeader(),
      m('main',
        m('.jobs-list',
          this.createWorkerTable(),
          this.createJobsTable(vnode, 'Queued + Running jobs',
            state.jobsRunning.concat(state.jobsQueued)),
          this.createJobsTable(vnode, 'Last 100 jobs', state.jobsRecent),
        ),
      )
    ];
  }
};

const StatsPageRenderer = {
  view: function (vnode) {
    const makeIframe = id => {
      let url = 'https://public.google.stackdriver.com/public/chart';
      url += `/${id}?timeframe=${vnode.attrs.period}`;
      url += '&drawMode=color&showLegend=false&theme=light';
      return m('iframe', {
        src: url,
        scrolling: 'no',
        seamless: 'seamless',
      });
    };

    return [
      renderHeader(),
      m('main#stats',
        m('.stats-grid', STATS_CHART_IDS.map(makeIframe))
      )
    ];
  }
}

// -----------------------------------------------------------------------------
// Business logic (handles fetching from Gerrit and Firebase DB).
// -----------------------------------------------------------------------------

function parseGerritTime(str) {
  // Gerrit timestamps are UTC (as per public docs) but obviously they are not
  // encoded in ISO format.
  return new Date(`${str} UTC`);
}

function stripEmail(email) {
  return email.replace('@google.com', '@');
}

// Fetches the list of CLs from gerrit and updates the state.
async function fetchGerritCLs() {
  console.log('Fetching CL list from Gerrit');
  let uri = '/gerrit/changes/?-age:7days';
  uri += '+-is:abandoned&o=DETAILED_ACCOUNTS&o=CURRENT_REVISION';
  const response = await fetch(uri);
  state.gerritCls = [];
  let json = '';
  if (response.status !== 200) {
    setTimeout(fetchGerritCLs, 3000);  // Retry.
    return;
  }

  json = (await response.text());
  const cls = [];
  for (const e of JSON.parse(json)) {
    const revHash = Object.keys(e.revisions)[0];
    const cl = {
      subject: e.subject,
      status: e.status,
      num: e._number,
      revHash: revHash,
      psNum: e.revisions[revHash]._number,
      lastUpdate: parseGerritTime(e.updated),
      owner: e.owner.email,
    };
    cls.push(cl);
    fetchCIJobsForCLOrBranch(`cls/${cl.num}-${cl.psNum}`);
  }
  state.gerritCls = cls;
  scheduleRedraw();
}

async function fetchGerritCommit(sha1) {
  const response = await fetch(`/gerrit/commits/${sha1}`);
  console.assert(response.status === 200);
  const json = (await response.text());
  state.gerritCommits[sha1] = JSON.parse(json);
  scheduleRedraw();
}

async function fetchGerritLog(first, second) {
  const range = `${first}..${second}`;
  const response = await fetch(`/gerrit/log/${range}`);
  if (response.status !== 200) return;
  const json = await response.text();
  state.gerritLogs[range] = JSON.parse(json).log;
  scheduleRedraw();
}

// Retrieves the status of a given (CL, PS) in the DB.
function fetchCIJobsForCLOrBranch(src) {
  if (src in state.clRefs) return;  // Aslready have a listener for this key.
  const ref = firebase.database().ref(`/ci/${src}`);
  state.clRefs[src] = ref;
  ref.on('value', (e) => {
    const obj = e.val();
    if (!obj) return;
    state.dbJobSets[src] = Object.keys(obj.jobs);
    for (var jobId of state.dbJobSets[src]) {
      fetchCIStatusForJob(jobId);
    }
    scheduleRedraw();
  });
}

function fetchCIJobsForAllPatchsetOfCL(cl) {
  let ref = firebase.database().ref('/ci/cls').orderByKey();
  ref = ref.startAt(`${cl}-0`).endAt(`${cl}-~`);
  ref.once('value', (e) => {
    const patchsets = e.val() || {};
    for (const clAndPs in patchsets) {
      const jobs = Object.keys(patchsets[clAndPs].jobs);
      state.dbJobSets[`cls/${clAndPs}`] = jobs;
      for (var jobId of jobs) {
        fetchCIStatusForJob(jobId);
      }
    }
    scheduleRedraw();
  });
}

function fetchCIStatusForJob(jobId) {
  if (jobId in state.jobRefs) return;  // Already have a listener for this key.
  const ref = firebase.database().ref(`/ci/jobs/${jobId}`);
  state.jobRefs[jobId] = ref;
  ref.on('value', (e) => {
    if (e.val()) state.dbJobs[jobId] = e.val();
    scheduleRedraw();
  });
}

function fetchCIStatusForBranch(branch) {
  if (branch in state.branchRefs) return;  // Already have a listener.
  const db = firebase.database();
  const ref = db.ref('/ci/branches').orderByKey().limitToLast(20);
  state.branchRefs[branch] = ref;
  ref.on('value', (e) => {
    const resp = e.val();
    if (!resp) return;
    // key looks like 'master-YYYYMMDDHHMMSS', where YMD is the commit datetime.
    // Iterate in most-recent-first order.
    const keys = Object.keys(resp).sort().reverse();
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      const branchInfo = resp[key];
      state.dbBranches[key] = branchInfo;
      fetchCIJobsForCLOrBranch(`branches/${key}`);
      if (i < keys.length - 1) {
        fetchGerritLog(resp[keys[i + 1]].rev, branchInfo.rev);
      }
    }
    scheduleRedraw();
  });
}

function fetchWorkers() {
  if (state.workersRef !== undefined) return;  // Aslready have a listener.
  const ref = firebase.database().ref('/ci/workers');
  state.workersRef = ref;
  ref.on('value', (e) => {
    state.dbWorker = e.val() || {};
    scheduleRedraw();
  });
}

async function showLogTail(jobId) {
  if (state.termJobId === jobId) return;  // Already on it.
  const TAIL = 20;
  state.termClear = true;
  state.termLines = [
    `Fetching last ${TAIL} lines for ${jobId}.`,
    `Click on the CI icon to see the full log.`
  ];
  state.termJobId = jobId;
  scheduleRedraw();
  const ref = firebase.database().ref(`/ci/logs/${jobId}`);
  const lines = (await ref.orderByKey().limitToLast(TAIL).once('value')).val();
  if (state.termJobId !== jobId || !lines) return;
  const lastKey = appendLogLinesAndRedraw(lines);
  startRealTimeLogs(jobId, lastKey);
}

async function showFullLog(jobId) {
  state.termClear = true;
  state.termLines = [`Fetching full for ${jobId} ...`];
  state.termJobId = jobId;
  scheduleRedraw();

  // Suspend any other real-time logging in progress.
  stopRealTimeLogs();

  // Starts a chain of async tasks that fetch the current log lines in batches.
  state.termJobId = jobId;
  const ref = firebase.database().ref(`/ci/logs/${jobId}`).orderByKey();
  let lastKey = '';
  const BATCH = 1000;
  for (; ;) {
    const batchRef = ref.startAt(`${lastKey}!`).limitToFirst(BATCH);
    const logs = (await batchRef.once('value')).val();
    if (!logs)
      break;
    lastKey = appendLogLinesAndRedraw(logs);
  }

  startRealTimeLogs(jobId, lastKey)
}

function startRealTimeLogs(jobId, lastLineKey) {
  stopRealTimeLogs();
  console.log('Starting real-time logs for ', jobId);
  state.termJobId = jobId;
  let ref = firebase.database().ref(`/ci/logs/${jobId}`);
  ref = ref.orderByKey().startAt(`${lastLineKey}!`);
  state.realTimeLogRef = ref;
  state.realTimeLogRef.on('child_added', res => {
    const line = res.val();
    if (state.termJobId !== jobId || !line) return;
    const lines = {};
    lines[res.key] = line;
    appendLogLinesAndRedraw(lines);
  });
}

function stopRealTimeLogs() {
  if (state.realTimeLogRef !== undefined) {
    state.realTimeLogRef.off();
    state.realTimeLogRef = undefined;
  }
}

function appendLogLinesAndRedraw(lines) {
  const keys = Object.keys(lines).sort();
  for (var key of keys) {
    const date = new Date(null);
    date.setSeconds(parseInt(key.substr(0, 6), 16) / 1000);
    const timeString = date.toISOString().substr(11, 8);
    const isErr = lines[key].indexOf('FAILED:') >= 0;
    let line = `[${timeString}] ${lines[key]}`;
    if (isErr) line = `\u001b[33m${line}\u001b[0m`;
    state.termLines.push(line);
  }
  scheduleRedraw();
  return keys[keys.length - 1];
}

async function fetchRecentJobsStatus() {
  const db = firebase.database();
  if (state.jobsQueuedRef === undefined) {
    state.jobsQueuedRef = db.ref(`/ci/jobs_queued`).on('value', e => {
      state.jobsQueued = Object.keys(e.val() || {}).sort().reverse();
      for (const jobId of state.jobsQueued)
        fetchCIStatusForJob(jobId);
      scheduleRedraw();
    });
  }

  if (state.jobsRunningRef === undefined) {
    state.jobsRunningRef = db.ref(`/ci/jobs_running`).on('value', e => {
      state.jobsRunning = Object.keys(e.val() || {}).sort().reverse();
      for (const jobId of state.jobsRunning)
        fetchCIStatusForJob(jobId);
      scheduleRedraw();
    });
  }

  if (state.jobsRecentRef === undefined) {
    state.jobsRecentRef = db.ref(`/ci/jobs`).orderByKey().limitToLast(100);
    state.jobsRecentRef.on('value', e => {
      state.jobsRecent = Object.keys(e.val() || {}).sort().reverse();
      for (const jobId of state.jobsRecent)
        fetchCIStatusForJob(jobId);
      scheduleRedraw();
    });
  }
}


function scheduleRedraw() {
  if (state.redrawPending) return;
  state.redrawPending = true;
  window.requestAnimationFrame(() => {
    state.redrawPending = false;
    m.redraw();
  });
}

main();
