'use strict'

const { test } = require('tap')
const { getFundingInfo } = require('../../lib/utils/funding')

test('empty tree', (t) => {
  t.deepEqual(
    getFundingInfo({}),
    {
      name: null,
      dependencies: {},
      length: 0
    },
    'should return empty list'
  )
  t.end()
})

test('single item missing funding', (t) => {
  t.deepEqual(
    getFundingInfo({ name: 'project',
      dependencies: {
        'single-item': {
          name: 'single-item',
          version: '1.0.0'
        }
      }}),
    {
      name: 'project',
      dependencies: {},
      length: 0
    },
    'should return empty list'
  )
  t.end()
})

test('use path if name is missing', (t) => {
  t.deepEqual(
    getFundingInfo({ name: undefined,
      path: '/tmp/foo',
      children: {
        'single-item': {
          name: 'single-item',
          version: '1.0.0'
        }
      }}),
    {
      name: '/tmp/foo',
      dependencies: {},
      length: 0
    },
    'should use path as top level name'
  )
  t.end()
})

test('single item tree', (t) => {
  t.deepEqual(
    getFundingInfo({ name: 'project',
      dependencies: {
        'single-item': {
          name: 'single-item',
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'http://example.com'
          }
        }
      }}),
    {
      name: 'project',
      dependencies: {
        'single-item': {
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'http://example.com'
          }
        }
      },
      length: 1
    },
    'should return list with a single item'
  )
  t.end()
})

test('duplicate items along the tree', (t) => {
  t.deepEqual(
    getFundingInfo({ name: 'project',
      version: '2.3.4',
      dependencies: {
        'single-item': {
          name: 'single-item',
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'https://example.com'
          },
          dependencies: {
            'shared-top-first': {
              name: 'shared-top-first',
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              }
            },
            'sub-dep': {
              name: 'sub-dep',
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              },
              dependencies: {
                'shared-nested-first': {
                  name: 'shared-nested-first',
                  version: '1.0.0',
                  funding: {
                    type: 'foo',
                    url: 'https://example.com'
                  },
                  dependencies: {
                    'shared-top-first': {
                      name: 'shared-top-first',
                      version: '1.0.0',
                      funding: {
                        type: 'foo',
                        url: 'https://example.com'
                      }
                    }
                  }
                }
              }
            },
            'shared-nested-first': {
              name: 'shared-nested-first',
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              }
            }
          }
        }
      }}),
    {
      name: 'project',
      version: '2.3.4',
      dependencies: {
        'single-item': {
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'https://example.com'
          },
          dependencies: {
            'shared-top-first': {
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              }
            },
            'sub-dep': {
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              }
            },
            'shared-nested-first': {
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              }
            }
          }
        }
      },
      length: 4
    },
    'should return list with a single item'
  )
  t.end()
})

test('multi-level nested items tree', (t) => {
  t.deepEqual(
    getFundingInfo({ name: 'project',
      dependencies: {
        'first-level-dep': {
          name: 'first-level-dep',
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'https://example.com'
          },
          dependencies: {
            'sub-dep': {
              name: 'sub-dep',
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              },
              dependencies: {
                package: {
                  name: 'sub-sub-dep',
                  version: '1.0.0',
                  funding: {
                    type: 'foo',
                    url: 'https://example.com'
                  },
                  dependencies: {}
                }
              }
            }
          }
        }
      }}),
    {
      name: 'project',
      dependencies: {
        'first-level-dep': {
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'https://example.com'
          },
          dependencies: {
            'sub-dep': {
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              },
              dependencies: {
                'sub-sub-dep': {
                  version: '1.0.0',
                  funding: {
                    type: 'foo',
                    url: 'https://example.com'
                  }
                }
              }
            }
          }
        }
      },
      length: 3
    },
    'should return list with all items'
  )
  t.end()
})

test('missing fund nested items tree', (t) => {
  t.deepEqual(
    getFundingInfo({ name: 'project',
      dependencies: {
        'first-level-dep': {
          name: 'first-level-dep',
          version: '1.0.0',
          funding: {
            type: 'foo'
          },
          dependencies: {
            'sub-dep': {
              name: 'sub-dep',
              version: '1.0.0',
              dependencies: {
                'sub-sub-dep-01': {
                  name: 'sub-sub-dep-01',
                  version: '1.0.0',
                  funding: {
                    type: 'foo',
                    url: 'https://example.com'
                  },
                  dependencies: {
                    'non-funding-child': {
                      name: 'non-funding-child',
                      version: '1.0.0',
                      dependencies: {
                        'sub-sub-sub-dep': {
                          name: 'sub-sub-sub-dep',
                          version: '1.0.0',
                          funding: {
                            type: 'foo',
                            url: 'https://example.com'
                          }
                        }
                      }
                    }
                  }
                },
                'sub-sub-dep-02': {
                  name: 'sub-sub-dep-02',
                  version: '1.0.0',
                  funding: {
                    type: 'foo',
                    url: 'https://example.com'
                  },
                  dependencies: {}
                },
                'sub-sub-dep-03': {
                  name: 'sub-sub-dep-03',
                  version: '1.0.0',
                  funding: {
                    type: 'foo',
                    url: 'git://example.git'
                  },
                  dependencies: {
                    'sub-sub-sub-dep-03': {
                      name: 'sub-sub-sub-dep-03',
                      version: '1.0.0',
                      dependencies: {
                        'sub-sub-sub-sub-dep': {
                          name: 'sub-sub-sub-sub-dep',
                          version: '1.0.0',
                          funding: {
                            type: 'foo',
                            url: 'http://example.com'
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }}),
    {
      name: 'project',
      dependencies: {
        'sub-sub-dep-01': {
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'https://example.com'
          },
          dependencies: {
            'sub-sub-sub-dep': {
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              }
            }
          }
        },
        'sub-sub-dep-02': {
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'https://example.com'
          }
        },
        'sub-sub-sub-sub-dep': {
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'http://example.com'
          }
        }
      },
      length: 4
    },
    'should return list excluding missing funding items'
  )
  t.end()
})

test('countOnly option', (t) => {
  t.deepEqual(
    getFundingInfo({ name: 'project',
      dependencies: {
        'first-level-dep': {
          name: 'first-level-dep',
          version: '1.0.0',
          funding: {
            type: 'foo'
          },
          dependencies: {
            'sub-dep': {
              name: 'sub-dep',
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              },
              dependencies: {
                'sub-sub-dep': {
                  name: 'sub-sub-dep',
                  version: '1.0.0',
                  funding: {
                    type: 'foo',
                    url: 'https://example.com'
                  }
                },
                dependencies: {}
              }
            },
            'sub-sub-dep': {
              name: 'sub-sub-dep',
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              }
            }
          }
        }
      }}, { countOnly: true }),
    {
      length: 2
    },
    'should return only the length property'
  )
  t.end()
})

test('handle different versions', (t) => {
  t.deepEqual(
    getFundingInfo({ name: 'project',
      dependencies: {
        foo: {
          name: 'foo',
          version: '1.0.0',
          funding: {
            type: 'foo',
            url: 'https://example.com'
          },
          dependencies: {
            bar: {
              name: 'bar',
              version: '1.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              }
            }
          }
        },
        lorem: {
          dependencies: {
            fooo: {
              name: 'foo',
              version: '2.0.0',
              funding: {
                type: 'foo',
                url: 'https://example.com'
              },
              dependencies: {
                'foo-bar': {
                  name: 'foo-bar',
                  version: '1.0.0',
                  funding: {
                    type: 'foo',
                    url: 'https://example.com'
                  }
                }
              }
            }
          }
        }
      }
    }, { countOnly: true }),
    {
      length: 4
    },
    'should treat different versions as diff packages'
  )
  t.end()
})
