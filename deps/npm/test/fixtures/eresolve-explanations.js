// some real-world examples of ERESOLVE error explanation objects,
// copied from arborist or generated there.
module.exports = {
  cycleNested: {
    code: 'ERESOLVE',
    edge: {
      type: 'peer',
      name: '@isaacs/peer-dep-cycle-b',
      spec: '1',
      from: {
        name: '@isaacs/peer-dep-cycle-a',
        version: '1.0.0',
        location: 'node_modules/@isaacs/peer-dep-cycle-a',
        dependents: [
          {
            type: 'prod',
            name: '@isaacs/peer-dep-cycle-a',
            spec: '1.x',
            from: { location: '/some/project' },
          },
        ],
      },
    },
    current: {
      name: '@isaacs/peer-dep-cycle-c',
      version: '2.0.0',
      location: 'node_modules/@isaacs/peer-dep-cycle-c',
      dependents: [
        {
          type: 'prod',
          name: '@isaacs/peer-dep-cycle-c',
          spec: '2.x',
          from: { location: '/some/project' },
        },
      ],
    },
    peerConflict: {
      peer: {
        name: '@isaacs/peer-dep-cycle-c',
        version: '1.0.0',
        whileInstalling: { name: '@isaacs/peer-dep-cycle-a', version: '1.0.0' },
        location: 'node_modules/@isaacs/peer-dep-cycle-c',
        dependents: [
          {
            type: 'peer',
            name: '@isaacs/peer-dep-cycle-c',
            spec: '1',
            from: {
              name: '@isaacs/peer-dep-cycle-b',
              version: '1.0.0',
              whileInstalling: { name: '@isaacs/peer-dep-cycle-a', version: '1.0.0' },
              location: 'node_modules/@isaacs/peer-dep-cycle-b',
              dependents: [
                {
                  type: 'peer',
                  name: '@isaacs/peer-dep-cycle-b',
                  spec: '1',
                  from: {
                    name: '@isaacs/peer-dep-cycle-a',
                    version: '1.0.0',
                    location: 'node_modules/@isaacs/peer-dep-cycle-a',
                    dependents: [
                      {
                        type: 'prod',
                        name: '@isaacs/peer-dep-cycle-a',
                        spec: '1.x',
                        from: { location: '/some/project' },
                      },
                    ],
                  },
                },
              ],
            },
          },
        ],
      },
    },
    strictPeerDeps: true,
  },

  withShrinkwrap: {
    code: 'ERESOLVE',
    edge: {
      type: 'peer',
      name: '@isaacs/peer-dep-cycle-c',
      spec: '1',
      error: 'INVALID',
      from: {
        name: '@isaacs/peer-dep-cycle-b',
        version: '1.0.0',
        location: 'node_modules/@isaacs/peer-dep-cycle-b',
        whileInstalling: { name: '@isaacs/peer-dep-cycle-b', version: '1.0.0' },
        dependents: [
          {
            type: 'peer',
            name: '@isaacs/peer-dep-cycle-b',
            spec: '1',
            from: {
              name: '@isaacs/peer-dep-cycle-a',
              version: '1.0.0',
              location: 'node_modules/@isaacs/peer-dep-cycle-a',
              dependents: [
                {
                  type: 'prod',
                  name: '@isaacs/peer-dep-cycle-a',
                  spec: '1.x',
                  from: { location: '/some/project' },
                },
              ],
            },
          },
        ],
      },
    },
    current: {
      name: '@isaacs/peer-dep-cycle-c',
      version: '2.0.0',
      location: 'node_modules/@isaacs/peer-dep-cycle-c',
      dependents: [
        {
          type: 'prod',
          name: '@isaacs/peer-dep-cycle-c',
          spec: '2.x',
          from: { location: '/some/project' },
        },
      ],
    },
    strictPeerDeps: true,
  },

  'chain-conflict': {
    code: 'ERESOLVE',
    current: {
      name: '@isaacs/testing-peer-dep-conflict-chain-d',
      version: '2.0.0',
      whileInstalling: {
        name: 'project',
        version: '1.2.3',
        path: '/some/project',
      },
      location: 'node_modules/@isaacs/testing-peer-dep-conflict-chain-d',
      dependents: [
        {
          type: 'prod',
          name: '@isaacs/testing-peer-dep-conflict-chain-d',
          spec: '2',
          from: { location: '/some/project' },
        },
      ],
    },
    edge: {
      type: 'peer',
      name: '@isaacs/testing-peer-dep-conflict-chain-d',
      spec: '1',
      error: 'INVALID',
      from: {
        name: '@isaacs/testing-peer-dep-conflict-chain-c',
        version: '1.0.0',
        whileInstalling: {
          name: 'project',
          version: '1.2.3',
          path: '/some/project',
        },
        location: 'node_modules/@isaacs/testing-peer-dep-conflict-chain-c',
        dependents: [
          {
            type: 'prod',
            name: '@isaacs/testing-peer-dep-conflict-chain-c',
            spec: '1',
            from: { location: '/some/project' },
          },
        ],
      },
    },
    peerConflict: null,
    strictPeerDeps: false,
  },

  gatsby: {
    code: 'ERESOLVE',
    current: {
      name: 'ink',
      version: '3.0.0-7',
      whileInstalling: {
        name: 'gatsby-recipes',
        version: '0.2.31',
        path: '/some/project/node_modules/gatsby-recipes',
      },
      location: 'node_modules/ink',
      dependents: [
        {
          type: 'dev',
          name: 'ink',
          spec: 'next',
          from: {
            name: 'gatsby-recipes',
            version: '0.2.31',
            location: 'node_modules/gatsby-recipes',
            dependents: [
              {
                type: 'prod',
                name: 'gatsby-recipes',
                spec: '^0.2.31',
                from: {
                  name: 'gatsby-cli',
                  version: '2.12.107',
                  location: 'node_modules/gatsby-cli',
                  dependents: [
                    {
                      type: 'prod',
                      name: 'gatsby-cli',
                      spec: '^2.12.107',
                      from: {
                        name: 'gatsby',
                        version: '2.24.74',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            name: 'gatsby',
                            spec: '',
                            from: {
                              location: '/some/project/gatsby-user',
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
        },
      ],
    },
    edge: {
      type: 'peer',
      name: 'ink',
      spec: '>=2.0.0',
      error: 'INVALID',
      from: {
        name: 'ink-box',
        version: '1.0.0',
        whileInstalling: {
          name: 'gatsby-recipes',
          version: '0.2.31',
          path: '/some/project/gatsby-user/node_modules/gatsby-recipes',
        },
        location: 'node_modules/ink-box',
        dependents: [
          {
            type: 'prod',
            name: 'ink-box',
            spec: '^1.0.0',
            from: {
              name: 'gatsby-recipes',
              version: '0.2.31',
              location: 'node_modules/gatsby-recipes',
              dependents: [
                {
                  type: 'prod',
                  name: 'gatsby-recipes',
                  spec: '^0.2.31',
                  from: {
                    name: 'gatsby-cli',
                    version: '2.12.107',
                    location: 'node_modules/gatsby-cli',
                    dependents: [
                      {
                        type: 'prod',
                        name: 'gatsby-cli',
                        spec: '^2.12.107',
                        from: {
                          name: 'gatsby',
                          version: '2.24.74',
                          location: 'node_modules/gatsby',
                          dependents: [
                            {
                              type: 'prod',
                              name: 'gatsby',
                              spec: '',
                              from: {
                                location: '/some/project/gatsby-user',
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
          },
        ],
      },
    },
    peerConflict: null,
    strictPeerDeps: true,
  },

  'no current node, but has current edge': {
    code: 'ERESOLVE',
    current: null,
    currentEdge: {
      type: 'dev',
      name: 'eslint',
      spec: 'file:.',
      error: 'MISSING',
      from: {
        location: '/some/projects/eslint',
      },
    },
    edge: {
      type: 'peer',
      name: 'eslint',
      spec: '^6.0.0',
      error: 'MISSING',
      from: {
        name: 'eslint-plugin-jsdoc',
        version: '22.2.0',
        whileInstalling: {
          name: 'eslint',
          version: '7.22.0',
          path: '/Users/isaacs/dev/npm/cli/eslint',
        },
        location: 'node_modules/eslint-plugin-jsdoc',
        dependents: [
          {
            type: 'dev',
            name: 'eslint-plugin-jsdoc',
            spec: '^22.1.0',
            from: {
              location: '/some/projects/eslint',
            },
          },
        ],
      },
    },
    peerConflict: null,
    strictPeerDeps: false,
    force: false,
  },
  'no current node, no current edge, idk': {
    code: 'ERESOLVE',
    current: null,
    edge: {
      type: 'peer',
      name: 'eslint',
      spec: '^6.0.0',
      error: 'MISSING',
      from: {
        name: 'eslint-plugin-jsdoc',
        version: '22.2.0',
        whileInstalling: {
          name: 'eslint',
          version: '7.22.0',
          path: '/Users/isaacs/dev/npm/cli/eslint',
        },
        location: 'node_modules/eslint-plugin-jsdoc',
        dependents: [
          {
            type: 'dev',
            name: 'eslint-plugin-jsdoc',
            spec: '^22.1.0',
            from: {
              location: '/some/projects/eslint',
            },
          },
        ],
      },
    },
    peerConflict: null,
    strictPeerDeps: false,
    force: false,
  },

  'eslint-plugin case': {
    code: 'ERESOLVE',
    edge: {
      type: 'dev',
      name: 'eslint-plugin-eslint-plugin',
      spec: '^3.1.0',
      error: 'MISSING',
      from: {
        location: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
      },
    },
    dep: {
      name: 'eslint-plugin-eslint-plugin',
      version: '3.5.1',
      whileInstalling: {
        name: 'eslint-plugin-react',
        version: '7.24.0',
        path: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
      },
      location: 'node_modules/eslint-plugin-eslint-plugin',
      isWorkspace: false,
      dependents: [
        {
          type: 'dev',
          name: 'eslint-plugin-eslint-plugin',
          spec: '^3.1.0',
          error: 'MISSING',
          from: {
            location: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
          },
        },
      ],
    },
    current: null,
    peerConflict: {
      current: {
        name: 'eslint',
        version: '6.8.0',
        location: 'node_modules/eslint',
        isWorkspace: false,
        dependents: [
          {
            type: 'dev',
            name: 'eslint',
            spec: '^3 || ^4 || ^5 || ^6 || ^7',
            from: {
              location: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
            },
          },
          {
            type: 'peer',
            name: 'eslint',
            spec: '^5.0.0 || ^6.0.0',
            from: {
              name: '@typescript-eslint/parser',
              version: '2.34.0',
              location: 'node_modules/@typescript-eslint/parser',
              isWorkspace: false,
              dependents: [
                {
                  type: 'dev',
                  name: '@typescript-eslint/parser',
                  spec: '^2.34.0',
                  from: {
                    location: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
                  },
                },
              ],
            },
          },
          {
            type: 'peer',
            name: 'eslint',
            spec: '^5.16.0 || ^6.8.0 || ^7.2.0',
            from: {
              name: 'eslint-config-airbnb-base',
              version: '14.2.1',
              location: 'node_modules/eslint-config-airbnb-base',
              isWorkspace: false,
              dependents: [
                {
                  type: 'dev',
                  name: 'eslint-config-airbnb-base',
                  spec: '^14.2.1',
                  from: {
                    location: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
                  },
                },
              ],
            },
          },
          {
            type: 'peer',
            name: 'eslint',
            spec: '^2 || ^3 || ^4 || ^5 || ^6 || ^7.2.0',
            from: {
              name: 'eslint-plugin-import',
              version: '2.23.4',
              location: 'node_modules/eslint-plugin-import',
              isWorkspace: false,
              dependents: [
                {
                  type: 'dev',
                  name: 'eslint-plugin-import',
                  spec: '^2.23.4',
                  from: {
                    location: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
                  },
                },
                {
                  type: 'peer',
                  name: 'eslint-plugin-import',
                  spec: '^2.22.1',
                  from: {
                    name: 'eslint-config-airbnb-base',
                    version: '14.2.1',
                    location: 'node_modules/eslint-config-airbnb-base',
                    isWorkspace: false,
                    dependents: [
                      {
                        type: 'dev',
                        name: 'eslint-config-airbnb-base',
                        spec: '^14.2.1',
                        from: {
                          location: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
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
      peer: {
        name: 'eslint',
        version: '7.31.0',
        whileInstalling: {
          name: 'eslint-plugin-react',
          version: '7.24.0',
          path: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
        },
        location: 'node_modules/eslint',
        isWorkspace: false,
        dependents: [
          {
            type: 'peer',
            name: 'eslint',
            spec: '^7.0.0',
            from: {
              name: 'eslint-plugin-eslint-plugin',
              version: '3.5.1',
              whileInstalling: {
                name: 'eslint-plugin-react',
                version: '7.24.0',
                path: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
              },
              location: 'node_modules/eslint-plugin-eslint-plugin',
              isWorkspace: false,
              dependents: [
                {
                  type: 'dev',
                  name: 'eslint-plugin-eslint-plugin',
                  spec: '^3.1.0',
                  error: 'MISSING',
                  from: {
                    location: '/Users/isaacs/dev/npm/arborist/fixtures/eslint-plugin-react',
                  },
                },
              ],
            },
          },
        ],
      },
    },
    strictPeerDeps: false,
    force: false,
    isMine: true,
  },
}
