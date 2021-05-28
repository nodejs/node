const { resolve } = require('path')
const t = require('tap')
const { explainNode, printNode } = require('../../../lib/utils/explain-dep.js')
const testdir = t.testdirName

const redactCwd = (path) => {
  const normalizePath = p => p
    .replace(/\\+/g, '/')
    .replace(/\r\n/g, '\n')
  return normalizePath(path)
    .replace(new RegExp(normalizePath(process.cwd()), 'g'), '{CWD}')
}
t.cleanSnapshot = (str) => redactCwd(str)

const cases = {
  prodDep: {
    name: 'prod-dep',
    version: '1.2.3',
    location: 'node_modules/prod-dep',
    dependents: [
      {
        type: 'prod',
        name: 'prod-dep',
        spec: '1.x',
        from: {
          location: '/path/to/project',
        },
      },
    ],
  },

  deepDev: {
    name: 'deep-dev',
    version: '2.3.4',
    location: 'node_modules/deep-dev',
    dev: true,
    dependents: [
      {
        type: 'prod',
        name: 'deep-dev',
        spec: '2.x',
        from: {
          name: 'metadev',
          version: '3.4.5',
          location: 'node_modules/dev/node_modules/metadev',
          dependents: [
            {
              type: 'prod',
              name: 'metadev',
              spec: '3.x',
              from: {
                name: 'topdev',
                version: '4.5.6',
                location: 'node_modules/topdev',
                dependents: [
                  {
                    type: 'dev',
                    name: 'topdev',
                    spec: '4.x',
                    from: {
                      location: '/path/to/project',
                    },
                  },
                ],
              },
            },
          ],
        },
      },
    ],
  },

  optional: {
    name: 'optdep',
    version: '1.0.0',
    location: 'node_modules/optdep',
    optional: true,
    dependents: [
      {
        type: 'optional',
        name: 'optdep',
        spec: '1.0.0',
        from: {
          location: '/path/to/project',
        },
      },
    ],
  },

  peer: {
    name: 'peer',
    version: '1.0.0',
    location: 'node_modules/peer',
    peer: true,
    dependents: [
      {
        type: 'peer',
        name: 'peer',
        spec: '1.0.0',
        from: {
          location: '/path/to/project',
        },
      },
    ],
  },

  bundled: {
    name: 'bundle-of-joy',
    version: '1.0.0',
    location: 'node_modules/bundle-of-joy',
    bundled: true,
    dependents: [
      {
        type: 'prod',
        name: 'prod-dep',
        spec: '1.x',
        bundled: true,
        from: {
          location: '/path/to/project',
        },
      },
    ],
  },

  extraneous: {
    name: 'extra-neos',
    version: '1337.420.69-lol',
    location: 'node_modules/extra-neos',
    dependents: [],
    extraneous: true,
  },
}

cases.manyDeps = {
  name: 'manydep',
  version: '1.0.0',
  dependents: [
    {
      type: 'prod',
      name: 'manydep',
      spec: '1.0.0',
      from: cases.prodDep,
    },
    {
      type: 'optional',
      name: 'manydep',
      spec: '1.x',
      from: cases.optional,
    },
    {
      type: 'prod',
      name: 'manydep',
      spec: '1.0.x',
      from: cases.extraneous,
    },
    {
      type: 'dev',
      name: 'manydep',
      spec: '*',
      from: cases.deepDev,
    },
    {
      type: 'peer',
      name: 'manydep',
      spec: '>1.0.0-beta <1.0.1',
      from: cases.peer,
    },
    {
      type: 'prod',
      name: 'manydep',
      spec: '>1.0.0-beta <1.0.1',
      from: {
        location: '/path/to/project',
      },
    },
    {
      type: 'prod',
      name: 'manydep',
      spec: '1',
      from: {
        name: 'a package with a pretty long name',
        version: '1.2.3',
        dependents: {
          location: '/path/to/project',
        },
      },
    },
    {
      type: 'prod',
      name: 'manydep',
      spec: '1',
      from: {
        name: 'another package with a pretty long name',
        version: '1.2.3',
        dependents: {
          location: '/path/to/project',
        },
      },
    },
    {
      type: 'prod',
      name: 'manydep',
      spec: '1',
      from: {
        name: 'yet another a package with a pretty long name',
        version: '1.2.3',
        dependents: {
          location: '/path/to/project',
        },
      },
    },
  ],
}

cases.workspaces = {
  name: 'a',
  version: '1.0.0',
  location: 'a',
  isWorkspace: true,
  dependents: [],
  linksIn: [
    {
      name: 'a',
      version: '1.0.0',
      location: 'node_modules/a',
      isWorkspace: true,
      dependents: [
        {
          type: 'workspace',
          name: 'a',
          spec: `file:${resolve(testdir, 'ws-project', 'a')}`,
          from: { location: resolve(testdir, 'ws-project') },
        },
      ],
    },
  ],
}

for (const [name, expl] of Object.entries(cases)) {
  t.test(name, t => {
    t.matchSnapshot(printNode(expl, true), 'print color')
    t.matchSnapshot(printNode(expl, false), 'print nocolor')
    t.matchSnapshot(explainNode(expl, Infinity, true), 'explain color deep')
    t.matchSnapshot(explainNode(expl, 2, false), 'explain nocolor shallow')
    t.end()
  })
}

// make sure that we show the last one if it's the only one that would
// hit the ...
cases.manyDeps.dependents.pop()
t.matchSnapshot(explainNode(cases.manyDeps, 2, false), 'ellipses test one')
cases.manyDeps.dependents.pop()
t.matchSnapshot(explainNode(cases.manyDeps, 2, false), 'ellipses test two')
