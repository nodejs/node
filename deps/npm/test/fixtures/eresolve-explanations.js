// some real-world examples of ERESOLVE error explaination objects,
// copied from arborist or generated there.
module.exports = {
  cycleNested: {
    code: 'ERESOLVE',
    dep: {
      name: '@isaacs/peer-dep-cycle-b',
      version: '1.0.0',
      whileInstalling: { name: '@isaacs/peer-dep-cycle-a', version: '1.0.0' },
      location: 'node_modules/@isaacs/peer-dep-cycle-b',
      dependents: [
        {
          type: 'peer',
          spec: '1',
          from: {
            name: '@isaacs/peer-dep-cycle-a',
            version: '1.0.0',
            location: 'node_modules/@isaacs/peer-dep-cycle-a',
            dependents: [
              {
                type: 'prod',
                spec: '1.x',
                from: { location: '/some/project' }
              }
            ]
          }
        }
      ]
    },
    current: {
      name: '@isaacs/peer-dep-cycle-c',
      version: '2.0.0',
      location: 'node_modules/@isaacs/peer-dep-cycle-c',
      dependents: [
        {
          type: 'prod',
          spec: '2.x',
          from: { location: '/some/project' }
        }
      ]
    },
    peerConflict: {
      name: '@isaacs/peer-dep-cycle-c',
      version: '1.0.0',
      whileInstalling: { name: '@isaacs/peer-dep-cycle-a', version: '1.0.0' },
      location: 'node_modules/@isaacs/peer-dep-cycle-c',
      dependents: [
        {
          type: 'peer',
          spec: '1',
          from: {
            name: '@isaacs/peer-dep-cycle-b',
            version: '1.0.0',
            whileInstalling: { name: '@isaacs/peer-dep-cycle-a', version: '1.0.0' },
            location: 'node_modules/@isaacs/peer-dep-cycle-b',
            dependents: [
              {
                type: 'peer',
                spec: '1',
                from: {
                  name: '@isaacs/peer-dep-cycle-a',
                  version: '1.0.0',
                  location: 'node_modules/@isaacs/peer-dep-cycle-a',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '1.x',
                      from: { location: '/some/project' }
                    }
                  ]
                }
              }
            ]
          }
        }
      ]
    },
    fixWithForce: false,
    type: 'peer',
    isPeer: true
  },

  withShrinkwrap: {
    code: 'ERESOLVE',
    dep: {
      name: '@isaacs/peer-dep-cycle-c',
      version: '1.0.0',
      whileInstalling: { name: '@isaacs/peer-dep-cycle-b', version: '1.0.0' },
      location: 'node_modules/@isaacs/peer-dep-cycle-c',
      dependents: [
        {
          type: 'peer',
          spec: '1',
          error: 'INVALID',
          from: {
            name: '@isaacs/peer-dep-cycle-b',
            version: '1.0.0',
            location: 'node_modules/@isaacs/peer-dep-cycle-b',
            dependents: [
              {
                type: 'peer',
                spec: '1',
                from: {
                  name: '@isaacs/peer-dep-cycle-a',
                  version: '1.0.0',
                  location: 'node_modules/@isaacs/peer-dep-cycle-a',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '1.x',
                      from: { location: '/some/project' }
                    }
                  ]
                }
              }
            ]
          }
        }
      ]
    },
    current: {
      name: '@isaacs/peer-dep-cycle-c',
      version: '2.0.0',
      location: 'node_modules/@isaacs/peer-dep-cycle-c',
      dependents: [
        {
          type: 'prod',
          spec: '2.x',
          from: { location: '/some/project' }
        }
      ]
    },
    fixWithForce: true,
    type: 'peer'
  },

  gatsby: {
    code: 'ERESOLVE',
    dep: {
      name: 'react',
      version: '16.8.1',
      whileInstalling: {
        name: 'gatsby-interface',
        version: '0.0.166'
      },
      location: 'node_modules/react',
      dependents: [
        {
          type: 'peer',
          spec: '16.8.1',
          error: 'INVALID',
          from: {
            name: 'gatsby-interface',
            version: '0.0.166',
            location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
            dependents: [
              {
                type: 'prod',
                spec: '^0.0.166',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        }
      ]
    },
    current: {
      name: 'react',
      version: '16.13.1',
      location: 'node_modules/react',
      dependents: [
        {
          type: 'peer',
          spec: '^16.4.2',
          from: {
            name: 'gatsby',
            version: '2.24.53',
            location: 'node_modules/gatsby',
            dependents: [
              {
                type: 'prod',
                spec: '',
                from: {
                  location: '/path/to/gatsby-user'
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.13.1',
          from: {
            name: 'react-dom',
            version: '16.13.1',
            location: 'node_modules/react-dom',
            dependents: [
              {
                type: 'peer',
                spec: '^16.4.2',
                from: {
                  name: 'gatsby',
                  version: '2.24.53',
                  location: 'node_modules/gatsby',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '',
                      from: {
                        location: '/path/to/gatsby-user'
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '15.x || 16.x || 16.4.0-alpha.0911da3',
                from: {
                  name: '@reach/router',
                  version: '1.3.4',
                  location: 'node_modules/@reach/router',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^1.3.4',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    },
                    {
                      type: 'peer',
                      spec: '^1.3.3',
                      from: {
                        name: 'gatsby-link',
                        version: '2.4.13',
                        location: 'node_modules/gatsby-link',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.4.13',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    },
                    {
                      type: 'peer',
                      spec: '^1.0.0',
                      from: {
                        name: 'gatsby-react-router-scroll',
                        version: '3.0.12',
                        location: 'node_modules/gatsby-react-router-scroll',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^3.0.12',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^16.4.2',
                from: {
                  name: 'gatsby-link',
                  version: '2.4.13',
                  location: 'node_modules/gatsby-link',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^2.4.13',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^16.4.2',
                from: {
                  name: 'gatsby-react-router-scroll',
                  version: '3.0.12',
                  location: 'node_modules/gatsby-react-router-scroll',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^3.0.12',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^15.0.0 || ^16.0.0',
                from: {
                  name: 'react-hot-loader',
                  version: '4.12.21',
                  location: 'node_modules/react-hot-loader',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^4.12.21',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '15.x || 16.x || 16.4.0-alpha.0911da3',
          from: {
            name: '@reach/router',
            version: '1.3.4',
            location: 'node_modules/@reach/router',
            dependents: [
              {
                type: 'prod',
                spec: '^1.3.4',
                from: {
                  name: 'gatsby',
                  version: '2.24.53',
                  location: 'node_modules/gatsby',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '',
                      from: {
                        location: '/path/to/gatsby-user'
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^1.3.3',
                from: {
                  name: 'gatsby-link',
                  version: '2.4.13',
                  location: 'node_modules/gatsby-link',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^2.4.13',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^1.0.0',
                from: {
                  name: 'gatsby-react-router-scroll',
                  version: '3.0.12',
                  location: 'node_modules/gatsby-react-router-scroll',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^3.0.12',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'prod',
          spec: '^16.8.0',
          from: {
            name: 'gatsby-cli',
            version: '2.12.91',
            location: 'node_modules/gatsby-cli',
            dependents: [
              {
                type: 'prod',
                spec: '^2.12.91',
                from: {
                  name: 'gatsby',
                  version: '2.24.53',
                  location: 'node_modules/gatsby',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '',
                      from: {
                        location: '/path/to/gatsby-user'
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.4.2',
          from: {
            name: 'gatsby-link',
            version: '2.4.13',
            location: 'node_modules/gatsby-link',
            dependents: [
              {
                type: 'prod',
                spec: '^2.4.13',
                from: {
                  name: 'gatsby',
                  version: '2.24.53',
                  location: 'node_modules/gatsby',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '',
                      from: {
                        location: '/path/to/gatsby-user'
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.4.2',
          from: {
            name: 'gatsby-react-router-scroll',
            version: '3.0.12',
            location: 'node_modules/gatsby-react-router-scroll',
            dependents: [
              {
                type: 'prod',
                spec: '^3.0.12',
                from: {
                  name: 'gatsby',
                  version: '2.24.53',
                  location: 'node_modules/gatsby',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '',
                      from: {
                        location: '/path/to/gatsby-user'
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^15.0.0 || ^16.0.0',
          from: {
            name: 'react-hot-loader',
            version: '4.12.21',
            location: 'node_modules/react-hot-loader',
            dependents: [
              {
                type: 'prod',
                spec: '^4.12.21',
                from: {
                  name: 'gatsby',
                  version: '2.24.53',
                  location: 'node_modules/gatsby',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '',
                      from: {
                        location: '/path/to/gatsby-user'
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^0.14.0 || ^15.0.0 || ^16.0.0',
          from: {
            name: 'create-react-context',
            version: '0.3.0',
            location: 'node_modules/create-react-context',
            dependents: [
              {
                type: 'prod',
                spec: '0.3.0',
                from: {
                  name: '@reach/router',
                  version: '1.3.4',
                  location: 'node_modules/@reach/router',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^1.3.4',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    },
                    {
                      type: 'peer',
                      spec: '^1.3.3',
                      from: {
                        name: 'gatsby-link',
                        version: '2.4.13',
                        location: 'node_modules/gatsby-link',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.4.13',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    },
                    {
                      type: 'peer',
                      spec: '^1.0.0',
                      from: {
                        name: 'gatsby-react-router-scroll',
                        version: '3.0.12',
                        location: 'node_modules/gatsby-react-router-scroll',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^3.0.12',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.12.0',
          from: {
            name: 'gatsby-recipes',
            version: '0.2.20',
            location: 'node_modules/gatsby-recipes',
            dependents: [
              {
                type: 'prod',
                spec: '^0.2.20',
                from: {
                  name: 'gatsby-cli',
                  version: '2.12.91',
                  location: 'node_modules/gatsby-cli',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^2.12.91',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '>=16.8.0',
          from: {
            name: 'ink',
            version: '2.7.1',
            location: 'node_modules/ink',
            dependents: [
              {
                type: 'prod',
                spec: '^2.7.1',
                from: {
                  name: 'gatsby-cli',
                  version: '2.12.91',
                  location: 'node_modules/gatsby-cli',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^2.12.91',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^2.0.0',
                from: {
                  name: 'ink-spinner',
                  version: '3.1.0',
                  location: 'node_modules/ink-spinner',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^3.1.0',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '>=2.0.0',
                from: {
                  name: 'ink-box',
                  version: '1.0.0',
                  location: 'node_modules/ink-box',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^1.0.0',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.8.2',
          from: {
            name: 'ink-spinner',
            version: '3.1.0',
            location: 'node_modules/ink-spinner',
            dependents: [
              {
                type: 'prod',
                spec: '^3.1.0',
                from: {
                  name: 'gatsby-cli',
                  version: '2.12.91',
                  location: 'node_modules/gatsby-cli',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^2.12.91',
                      from: {
                        name: 'gatsby',
                        version: '2.24.53',
                        location: 'node_modules/gatsby',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '',
                            from: {
                              location: '/path/to/gatsby-user'
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '>=16.3.0',
          from: {
            name: '@emotion/core',
            version: '10.0.35',
            location: 'node_modules/@emotion/core',
            dependents: [
              {
                type: 'prod',
                spec: '^10.0.14',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^10.0.27',
                from: {
                  name: '@emotion/styled',
                  version: '10.0.27',
                  location: 'node_modules/@emotion/styled',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^10.0.14',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    },
                    {
                      type: 'peer',
                      spec: '^10.0.14',
                      from: {
                        name: 'gatsby-interface',
                        version: '0.0.166',
                        location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.0.166',
                            from: {
                              name: 'gatsby-recipes',
                              version: '0.2.20',
                              location: 'node_modules/gatsby-recipes',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^0.2.20',
                                  from: {
                                    name: 'gatsby-cli',
                                    version: '2.12.91',
                                    location: 'node_modules/gatsby-cli',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '^2.12.91',
                                        from: {
                                          name: 'gatsby',
                                          version: '2.24.53',
                                          location: 'node_modules/gatsby',
                                          dependents: [
                                            {
                                              type: 'prod',
                                              spec: '',
                                              from: {
                                                location: '/path/to/gatsby-user'
                                              }
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^10.0.14',
                from: {
                  name: 'gatsby-interface',
                  version: '0.0.166',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.0.166',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^10.0.28',
                from: {
                  name: '@emotion/styled-base',
                  version: '10.0.31',
                  location: 'node_modules/@emotion/styled-base',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^10.0.27',
                      from: {
                        name: '@emotion/styled',
                        version: '10.0.27',
                        location: 'node_modules/@emotion/styled',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^10.0.14',
                            from: {
                              name: 'gatsby-recipes',
                              version: '0.2.20',
                              location: 'node_modules/gatsby-recipes',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^0.2.20',
                                  from: {
                                    name: 'gatsby-cli',
                                    version: '2.12.91',
                                    location: 'node_modules/gatsby-cli',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '^2.12.91',
                                        from: {
                                          name: 'gatsby',
                                          version: '2.24.53',
                                          location: 'node_modules/gatsby',
                                          dependents: [
                                            {
                                              type: 'prod',
                                              spec: '',
                                              from: {
                                                location: '/path/to/gatsby-user'
                                              }
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          },
                          {
                            type: 'peer',
                            spec: '^10.0.14',
                            from: {
                              name: 'gatsby-interface',
                              version: '0.0.166',
                              location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^0.0.166',
                                  from: {
                                    name: 'gatsby-recipes',
                                    version: '0.2.20',
                                    location: 'node_modules/gatsby-recipes',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '^0.2.20',
                                        from: {
                                          name: 'gatsby-cli',
                                          version: '2.12.91',
                                          location: 'node_modules/gatsby-cli',
                                          dependents: [
                                            {
                                              type: 'prod',
                                              spec: '^2.12.91',
                                              from: {
                                                name: 'gatsby',
                                                version: '2.24.53',
                                                location: 'node_modules/gatsby',
                                                dependents: [
                                                  {
                                                    type: 'prod',
                                                    spec: '',
                                                    from: {
                                                      location: '/path/to/gatsby-user'
                                                    }
                                                  }
                                                ]
                                              }
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '>=16.3.0',
          from: {
            name: '@emotion/styled',
            version: '10.0.27',
            location: 'node_modules/@emotion/styled',
            dependents: [
              {
                type: 'prod',
                spec: '^10.0.14',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^10.0.14',
                from: {
                  name: 'gatsby-interface',
                  version: '0.0.166',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.0.166',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.13.1',
          from: {
            name: '@mdx-js/react',
            version: '2.0.0-next.7',
            location: 'node_modules/@mdx-js/react',
            dependents: [
              {
                type: 'prod',
                spec: '^2.0.0-next.4',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'prod',
                spec: '^2.0.0-next.7',
                from: {
                  name: '@mdx-js/runtime',
                  version: '2.0.0-next.7',
                  location: 'node_modules/@mdx-js/runtime',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^2.0.0-next.4',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.13.1',
          from: {
            name: '@mdx-js/runtime',
            version: '2.0.0-next.7',
            location: 'node_modules/@mdx-js/runtime',
            dependents: [
              {
                type: 'prod',
                spec: '^2.0.0-next.4',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '>=16.8.0',
          from: {
            name: 'formik',
            version: '2.1.5',
            location: 'node_modules/formik',
            dependents: [
              {
                type: 'prod',
                spec: '^2.0.8',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^2.0.8',
                from: {
                  name: 'gatsby-interface',
                  version: '0.0.166',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.0.166',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.4.2',
          from: {
            name: 'gatsby',
            version: '2.6.0',
            location: 'node_modules/gatsby-recipes/node_modules/gatsby',
            dependents: [
              {
                type: 'peer',
                spec: '2.6.0',
                from: {
                  name: 'gatsby-interface',
                  version: '0.0.166',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.0.166',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.0.0',
          from: {
            name: 'react-dom',
            version: '16.8.1',
            location: 'node_modules/gatsby-recipes/node_modules/react-dom',
            dependents: [
              {
                type: 'peer',
                spec: '16.8.1',
                from: {
                  name: 'gatsby-interface',
                  version: '0.0.166',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.0.166',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^16.4.2',
                from: {
                  name: 'gatsby',
                  version: '2.6.0',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby',
                  dependents: [
                    {
                      type: 'peer',
                      spec: '2.6.0',
                      from: {
                        name: 'gatsby-interface',
                        version: '0.0.166',
                        location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.0.166',
                            from: {
                              name: 'gatsby-recipes',
                              version: '0.2.20',
                              location: 'node_modules/gatsby-recipes',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^0.2.20',
                                  from: {
                                    name: 'gatsby-cli',
                                    version: '2.12.91',
                                    location: 'node_modules/gatsby-cli',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '^2.12.91',
                                        from: {
                                          name: 'gatsby',
                                          version: '2.24.53',
                                          location: 'node_modules/gatsby',
                                          dependents: [
                                            {
                                              type: 'prod',
                                              spec: '',
                                              from: {
                                                location: '/path/to/gatsby-user'
                                              }
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^0.14.0 || ^15.0.0 || ^16.0.0',
                from: {
                  name: 'gatsby-react-router-scroll',
                  version: '2.3.1',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby-react-router-scroll',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^2.0.7',
                      from: {
                        name: 'gatsby',
                        version: '2.6.0',
                        location: 'node_modules/gatsby-recipes/node_modules/gatsby',
                        dependents: [
                          {
                            type: 'peer',
                            spec: '2.6.0',
                            from: {
                              name: 'gatsby-interface',
                              version: '0.0.166',
                              location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^0.0.166',
                                  from: {
                                    name: 'gatsby-recipes',
                                    version: '0.2.20',
                                    location: 'node_modules/gatsby-recipes',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '^0.2.20',
                                        from: {
                                          name: 'gatsby-cli',
                                          version: '2.12.91',
                                          location: 'node_modules/gatsby-cli',
                                          dependents: [
                                            {
                                              type: 'prod',
                                              spec: '^2.12.91',
                                              from: {
                                                name: 'gatsby',
                                                version: '2.24.53',
                                                location: 'node_modules/gatsby',
                                                dependents: [
                                                  {
                                                    type: 'prod',
                                                    spec: '',
                                                    from: {
                                                      location: '/path/to/gatsby-user'
                                                    }
                                                  }
                                                ]
                                              }
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '*',
          from: {
            name: 'react-icons',
            version: '3.11.0',
            location: 'node_modules/react-icons',
            dependents: [
              {
                type: 'prod',
                spec: '^3.0.1',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              },
              {
                type: 'peer',
                spec: '^3.2.1',
                from: {
                  name: 'gatsby-interface',
                  version: '0.0.166',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.0.166',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '>=16.8.0',
          from: {
            name: 'ink-box',
            version: '1.0.0',
            location: 'node_modules/ink-box',
            dependents: [
              {
                type: 'prod',
                spec: '^1.0.0',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^0.14.0 || ^15.0.0 || ^16.0.0',
          from: {
            name: 'react-circular-progressbar',
            version: '2.0.3',
            location: 'node_modules/react-circular-progressbar',
            dependents: [
              {
                type: 'prod',
                spec: '^2.0.0',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.13.1',
          from: {
            name: 'react-reconciler',
            version: '0.25.1',
            location: 'node_modules/react-reconciler',
            dependents: [
              {
                type: 'prod',
                spec: '^0.25.1',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '>= 16.8.0',
          from: {
            name: 'urql',
            version: '1.10.0',
            location: 'node_modules/urql',
            dependents: [
              {
                type: 'prod',
                spec: '^1.9.7',
                from: {
                  name: 'gatsby-recipes',
                  version: '0.2.20',
                  location: 'node_modules/gatsby-recipes',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.2.20',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '>=16.3.0',
          from: {
            name: '@emotion/styled-base',
            version: '10.0.31',
            location: 'node_modules/@emotion/styled-base',
            dependents: [
              {
                type: 'prod',
                spec: '^10.0.27',
                from: {
                  name: '@emotion/styled',
                  version: '10.0.27',
                  location: 'node_modules/@emotion/styled',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^10.0.14',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    },
                    {
                      type: 'peer',
                      spec: '^10.0.14',
                      from: {
                        name: 'gatsby-interface',
                        version: '0.0.166',
                        location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.0.166',
                            from: {
                              name: 'gatsby-recipes',
                              version: '0.2.20',
                              location: 'node_modules/gatsby-recipes',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^0.2.20',
                                  from: {
                                    name: 'gatsby-cli',
                                    version: '2.12.91',
                                    location: 'node_modules/gatsby-cli',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '^2.12.91',
                                        from: {
                                          name: 'gatsby',
                                          version: '2.24.53',
                                          location: 'node_modules/gatsby',
                                          dependents: [
                                            {
                                              type: 'prod',
                                              spec: '',
                                              from: {
                                                location: '/path/to/gatsby-user'
                                              }
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.0.0',
          from: {
            name: 'react-reconciler',
            version: '0.24.0',
            location: 'node_modules/ink/node_modules/react-reconciler',
            dependents: [
              {
                type: 'prod',
                spec: '^0.24.0',
                from: {
                  name: 'ink',
                  version: '2.7.1',
                  location: 'node_modules/ink',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^2.7.1',
                      from: {
                        name: 'gatsby-cli',
                        version: '2.12.91',
                        location: 'node_modules/gatsby-cli',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^2.12.91',
                            from: {
                              name: 'gatsby',
                              version: '2.24.53',
                              location: 'node_modules/gatsby',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '',
                                  from: {
                                    location: '/path/to/gatsby-user'
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    },
                    {
                      type: 'peer',
                      spec: '^2.0.0',
                      from: {
                        name: 'ink-spinner',
                        version: '3.1.0',
                        location: 'node_modules/ink-spinner',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^3.1.0',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    },
                    {
                      type: 'peer',
                      spec: '>=2.0.0',
                      from: {
                        name: 'ink-box',
                        version: '1.0.0',
                        location: 'node_modules/ink-box',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^1.0.0',
                            from: {
                              name: 'gatsby-recipes',
                              version: '0.2.20',
                              location: 'node_modules/gatsby-recipes',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^0.2.20',
                                  from: {
                                    name: 'gatsby-cli',
                                    version: '2.12.91',
                                    location: 'node_modules/gatsby-cli',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '^2.12.91',
                                        from: {
                                          name: 'gatsby',
                                          version: '2.24.53',
                                          location: 'node_modules/gatsby',
                                          dependents: [
                                            {
                                              type: 'prod',
                                              spec: '',
                                              from: {
                                                location: '/path/to/gatsby-user'
                                              }
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^0.14.0 || ^15.0.0 || ^16.0.0',
          from: {
            name: 'gatsby-react-router-scroll',
            version: '2.3.1',
            location: 'node_modules/gatsby-recipes/node_modules/gatsby-react-router-scroll',
            dependents: [
              {
                type: 'prod',
                spec: '^2.0.7',
                from: {
                  name: 'gatsby',
                  version: '2.6.0',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby',
                  dependents: [
                    {
                      type: 'peer',
                      spec: '2.6.0',
                      from: {
                        name: 'gatsby-interface',
                        version: '0.0.166',
                        location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.0.166',
                            from: {
                              name: 'gatsby-recipes',
                              version: '0.2.20',
                              location: 'node_modules/gatsby-recipes',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^0.2.20',
                                  from: {
                                    name: 'gatsby-cli',
                                    version: '2.12.91',
                                    location: 'node_modules/gatsby-cli',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '^2.12.91',
                                        from: {
                                          name: 'gatsby',
                                          version: '2.24.53',
                                          location: 'node_modules/gatsby',
                                          dependents: [
                                            {
                                              type: 'prod',
                                              spec: '',
                                              from: {
                                                location: '/path/to/gatsby-user'
                                              }
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        },
        {
          type: 'peer',
          spec: '^16.13.1',
          from: {
            name: '@mdx-js/react',
            version: '1.6.16',
            location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface/node_modules/@mdx-js/react',
            dependents: [
              {
                type: 'prod',
                spec: '^1.5.2',
                from: {
                  name: 'gatsby-interface',
                  version: '0.0.166',
                  location: 'node_modules/gatsby-recipes/node_modules/gatsby-interface',
                  dependents: [
                    {
                      type: 'prod',
                      spec: '^0.0.166',
                      from: {
                        name: 'gatsby-recipes',
                        version: '0.2.20',
                        location: 'node_modules/gatsby-recipes',
                        dependents: [
                          {
                            type: 'prod',
                            spec: '^0.2.20',
                            from: {
                              name: 'gatsby-cli',
                              version: '2.12.91',
                              location: 'node_modules/gatsby-cli',
                              dependents: [
                                {
                                  type: 'prod',
                                  spec: '^2.12.91',
                                  from: {
                                    name: 'gatsby',
                                    version: '2.24.53',
                                    location: 'node_modules/gatsby',
                                    dependents: [
                                      {
                                        type: 'prod',
                                        spec: '',
                                        from: {
                                          location: '/path/to/gatsby-user'
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }
                        ]
                      }
                    }
                  ]
                }
              }
            ]
          }
        }
      ]
    },
    peerConflict: null,
    fixWithForce: true,
    type: 'peer',
    isPeer: true
  }
}
