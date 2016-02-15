// React JSX tests

var fbTestFixture = {
  // Taken and adapted from esprima-fb/fbtest.js.
  'JSX': {
    '<a />': {
      type: "ExpressionStatement",
      expression: {
        type: "JSXElement",
        openingElement: {
          type: "JSXOpeningElement",
          name: {
            type: "JSXIdentifier",
            name: "a",
            range: [1, 2],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 2 }
            }
          },
          selfClosing: true,
          attributes: [],
          range: [0, 5],
          loc: {
            start: { line: 1, column: 0 },
            end: { line: 1, column: 5 }
          }
        },
        closingElement: null,
        children: [],
        range: [0, 5],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 5 }
        }
      },
      range: [0, 5],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 5 }
      }
    },

    '<n:a n:v />': {
      type: 'ExpressionStatement',
      expression: {
        type: 'JSXElement',
        openingElement: {
          type: 'JSXOpeningElement',
          name: {
            type: 'JSXNamespacedName',
            namespace: {
              type: 'JSXIdentifier',
              name: 'n',
              range: [1, 2],
              loc: {
                start: { line: 1, column: 1 },
                end: { line: 1, column: 2 }
              }
            },
            name: {
              type: 'JSXIdentifier',
              name: 'a',
              range: [3, 4],
              loc: {
                start: { line: 1, column: 3 },
                end: { line: 1, column: 4 }
              }
            },
            range: [1, 4],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 4 }
            }
          },
          selfClosing: true,
          attributes: [{
            type: 'JSXAttribute',
            name: {
              type: 'JSXNamespacedName',
              namespace: {
                type: 'JSXIdentifier',
                name: 'n',
                range: [5, 6],
                loc: {
                  start: { line: 1, column: 5 },
                  end: { line: 1, column: 6 }
                }
              },
              name: {
                type: 'JSXIdentifier',
                name: 'v',
                range: [7, 8],
                loc: {
                  start: { line: 1, column: 7 },
                  end: { line: 1, column: 8 }
                }
              },
              range: [5, 8],
              loc: {
                start: { line: 1, column: 5 },
                end: { line: 1, column: 8 }
              }
            },
            value: null,
            range: [5, 8],
            loc: {
              start: { line: 1, column: 5 },
              end: { line: 1, column: 8 }
            }
          }],
          range: [0, 11],
          loc: {
            start: { line: 1, column: 0 },
            end: { line: 1, column: 11 }
          }
        },
        closingElement: null,
        children: [],
        range: [0, 11],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 11 }
        }
      },
      range: [0, 11],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 11 }
      }
    },

    '<a n:foo="bar"> {value} <b><c /></b></a>': {
      type: 'ExpressionStatement',
      expression: {
        type: 'JSXElement',
        openingElement: {
          type: 'JSXOpeningElement',
          name: {
            type: 'JSXIdentifier',
            name: 'a',
            range: [1, 2],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 2 }
            }
          },
          selfClosing: false,
          attributes: [{
            type: 'JSXAttribute',
            name: {
              type: 'JSXNamespacedName',
              namespace: {
                type: 'JSXIdentifier',
                name: 'n',
                range: [3, 4],
                loc: {
                  start: { line: 1, column: 3 },
                  end: { line: 1, column: 4 }
                }
              },
              name: {
                type: 'JSXIdentifier',
                name: 'foo',
                range: [5, 8],
                loc: {
                  start: { line: 1, column: 5 },
                  end: { line: 1, column: 8 }
                }
              },
              range: [3, 8],
              loc: {
                start: { line: 1, column: 3 },
                end: { line: 1, column: 8 }
              }
            },
            value: {
              type: 'Literal',
              value: 'bar',
              raw: '"bar"',
              range: [9, 14],
              loc: {
                start: { line: 1, column: 9 },
                end: { line: 1, column: 14 }
              }
            },
            range: [3, 14],
            loc: {
              start: { line: 1, column: 3 },
              end: { line: 1, column: 14 }
            }
          }],
          range: [0, 15],
          loc: {
            start: { line: 1, column: 0 },
            end: { line: 1, column: 15 }
          }
        },
        closingElement: {
          type: 'JSXClosingElement',
          name: {
            type: 'JSXIdentifier',
            name: 'a',
            range: [38, 39],
            loc: {
              start: { line: 1, column: 38 },
              end: { line: 1, column: 39 }
            }
          },
          range: [36, 40],
          loc: {
            start: { line: 1, column: 36 },
            end: { line: 1, column: 40 }
          }
        },
        children: [{
          type: 'Literal',
          value: ' ',
          raw: ' ',
          range: [15, 16],
          loc: {
            start: { line: 1, column: 15 },
            end: { line: 1, column: 16 }
          }
        }, {
          type: 'JSXExpressionContainer',
          expression: {
            type: 'Identifier',
            name: 'value',
            range: [17, 22],
            loc: {
              start: { line: 1, column: 17 },
              end: { line: 1, column: 22 }
            }
          },
          range: [16, 23],
          loc: {
            start: { line: 1, column: 16 },
            end: { line: 1, column: 23 }
          }
        }, {
          type: 'Literal',
          value: ' ',
          raw: ' ',
          range: [23, 24],
          loc: {
            start: { line: 1, column: 23 },
            end: { line: 1, column: 24 }
          }
        }, {
          type: 'JSXElement',
          openingElement: {
            type: 'JSXOpeningElement',
            name: {
              type: 'JSXIdentifier',
              name: 'b',
              range: [25, 26],
              loc: {
                start: { line: 1, column: 25 },
                end: { line: 1, column: 26 }
              }
            },
            selfClosing: false,
            attributes: [],
            range: [24, 27],
            loc: {
              start: { line: 1, column: 24 },
              end: { line: 1, column: 27 }
            }
          },
          closingElement: {
            type: 'JSXClosingElement',
            name: {
              type: 'JSXIdentifier',
              name: 'b',
              range: [34, 35],
              loc: {
                start: { line: 1, column: 34 },
                end: { line: 1, column: 35 }
              }
            },
            range: [32, 36],
            loc: {
              start: { line: 1, column: 32 },
              end: { line: 1, column: 36 }
            }
          },
          children: [{
            type: 'JSXElement',
            openingElement: {
              type: 'JSXOpeningElement',
              name: {
                type: 'JSXIdentifier',
                name: 'c',
                range: [28, 29],
                loc: {
                  start: { line: 1, column: 28 },
                  end: { line: 1, column: 29 }
                }
              },
              selfClosing: true,
              attributes: [],
              range: [27, 32],
              loc: {
                start: { line: 1, column: 27 },
                end: { line: 1, column: 32 }
              }
            },
            closingElement: null,
            children: [],
            range: [27, 32],
            loc: {
              start: { line: 1, column: 27 },
              end: { line: 1, column: 32 }
            }
          }],
          range: [24, 36],
          loc: {
            start: { line: 1, column: 24 },
            end: { line: 1, column: 36 }
          }
        }],
        range: [0, 40],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 40 }
        }
      },
      range: [0, 40],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 40 }
      }
    },

    '<a b={" "} c=" " d="&amp;" e="&ampr;" />': {
      type: "ExpressionStatement",
      expression: {
        type: "JSXElement",
        openingElement: {
          type: "JSXOpeningElement",
          name: {
            type: "JSXIdentifier",
            name: "a",
            range: [1, 2]
          },
          selfClosing: true,
          attributes: [
            {
              type: "JSXAttribute",
              name: {
                type: "JSXIdentifier",
                name: "b",
                range: [3, 4]
              },
              value: {
                type: "JSXExpressionContainer",
                expression: {
                  type: "Literal",
                  value: " ",
                  raw: "\" \"",
                  range: [6, 9]
                },
                range: [5, 10]
              },
              range: [3, 10]
            },
            {
              type: "JSXAttribute",
              name: {
                type: "JSXIdentifier",
                name: "c",
                range: [11, 12]
              },
              value: {
                type: "Literal",
                value: " ",
                raw: "\" \"",
                range: [13, 16]
              },
              range: [11, 16]
            },
            {
              type: "JSXAttribute",
              name: {
                type: "JSXIdentifier",
                name: "d",
                range: [17, 18]
              },
              value: {
                type: "Literal",
                value: "&",
                raw: "\"&amp;\"",
                range: [19, 26]
              },
              range: [17, 26]
            },
            {
              type: "JSXAttribute",
              name: {
                type: "JSXIdentifier",
                name: "e",
                range: [27, 28]
              },
              value: {
                type: "Literal",
                value: "&ampr;",
                raw: "\"&ampr;\"",
                range: [29, 37]
              },
              range: [27, 37]
            }
          ],
          range: [0, 40]
        },
        closingElement: null,
        children: [],
        range: [0, 40]
      },
      range: [0, 40]
    },

    '<a\n/>': {
      type: "ExpressionStatement",
      expression: {
        type: "JSXElement",
        openingElement: {
          type: "JSXOpeningElement",
          name: {
            type: "JSXIdentifier",
            name: "a",
            range: [
              1,
              2
            ],
            loc: {
              start: {
                line: 1,
                column: 1
              },
              end: {
                line: 1,
                column: 2
              }
            }
          },
          selfClosing: true,
          attributes: [],
          range: [
            0,
            5
          ],
          loc: {
            start: {
              line: 1,
              column: 0
            },
            end: {
              line: 2,
              column: 2
            }
          }
        },
        closingElement: null,
        children: [],
        range: [
          0,
          5
        ],
        loc: {
          start: {
            line: 1,
            column: 0
          },
          end: {
            line: 2,
            column: 2
          }
        }
      },
      range: [
        0,
        5
      ],
      loc: {
        start: {
          line: 1,
          column: 0
        },
        end: {
          line: 2,
          column: 2
        }
      }
    },

    '<日本語></日本語>': {
      type: "ExpressionStatement",
      expression: {
        type: "JSXElement",
        openingElement: {
          type: "JSXOpeningElement",
          name: {
            type: "JSXIdentifier",
            name: "日本語",
            range: [
              1,
              4
            ],
            loc: {
              start: {
                line: 1,
                column: 1
              },
              end: {
                line: 1,
                column: 4
              }
            }
          },
          selfClosing: false,
          attributes: [],
          range: [
            0,
            5
          ],
          loc: {
            start: {
              line: 1,
              column: 0
            },
            end: {
              line: 1,
              column: 5
            }
          }
        },
        closingElement: {
          type: "JSXClosingElement",
          name: {
            type: "JSXIdentifier",
            name: "日本語",
            range: [
              7,
              10
            ],
            loc: {
              start: {
                line: 1,
                column: 7
              },
              end: {
                line: 1,
                column: 10
              }
            }
          },
          range: [
            5,
            11
          ],
          loc: {
            start: {
              line: 1,
              column: 5
            },
            end: {
              line: 1,
              column: 11
            }
          }
        },
        children: [],
        range: [
          0,
          11
        ],
        loc: {
          start: {
            line: 1,
            column: 0
          },
          end: {
            line: 1,
            column: 11
          }
        }
      },
      range: [
        0,
        11
      ],
      loc: {
        start: {
          line: 1,
          column: 0
        },
        end: {
          line: 1,
          column: 11
        }
      }
    },

    '<AbC-def\n  test="&#x0026;&#38;">\nbar\nbaz\n</AbC-def>': {
      type: "ExpressionStatement",
      expression: {
        type: "JSXElement",
        openingElement: {
          type: "JSXOpeningElement",
          name: {
            type: "JSXIdentifier",
            name: "AbC-def",
            range: [
              1,
              8
            ],
            loc: {
              start: {
                line: 1,
                column: 1
              },
              end: {
                line: 1,
                column: 8
              }
            }
          },
          selfClosing: false,
          attributes: [
            {
              type: "JSXAttribute",
              name: {
                type: "JSXIdentifier",
                name: "test",
                range: [
                  11,
                  15
                ],
                loc: {
                  start: {
                    line: 2,
                    column: 2
                  },
                  end: {
                    line: 2,
                    column: 6
                  }
                }
              },
              value: {
                type: "Literal",
                value: "&&",
                raw: "\"&#x0026;&#38;\"",
                range: [
                  16,
                  31
                ],
                loc: {
                  start: {
                    line: 2,
                    column: 7
                  },
                  end: {
                    line: 2,
                    column: 22
                  }
                }
              },
              range: [
                11,
                31
              ],
              loc: {
                start: {
                  line: 2,
                  column: 2
                },
                end: {
                  line: 2,
                  column: 22
                }
              }
            }
          ],
          range: [
            0,
            32
          ],
          loc: {
            start: {
              line: 1,
              column: 0
            },
            end: {
              line: 2,
              column: 23
            }
          }
        },
        closingElement: {
          type: "JSXClosingElement",
          name: {
            type: "JSXIdentifier",
            name: "AbC-def",
            range: [
              43,
              50
            ],
            loc: {
              start: {
                line: 5,
                column: 2
              },
              end: {
                line: 5,
                column: 9
              }
            }
          },
          range: [
            41,
            51
          ],
          loc: {
            start: {
              line: 5,
              column: 0
            },
            end: {
              line: 5,
              column: 10
            }
          }
        },
        children: [
          {
            type: "Literal",
            value: "\nbar\nbaz\n",
            raw: "\nbar\nbaz\n",
            range: [
              32,
              41
            ],
            loc: {
              start: {
                line: 2,
                column: 23
              },
              end: {
                line: 5,
                column: 0
              }
            }
          }
        ],
        range: [
          0,
          51
        ],
        loc: {
          start: {
            line: 1,
            column: 0
          },
          end: {
            line: 5,
            column: 10
          }
        }
      },
      range: [
        0,
        51
      ],
      loc: {
        start: {
          line: 1,
          column: 0
        },
        end: {
          line: 5,
          column: 10
        }
      }
    },

    '<a b={x ? <c /> : <d />} />': {
      type: "ExpressionStatement",
      expression: {
        type: "JSXElement",
        openingElement: {
          type: "JSXOpeningElement",
          name: {
            type: "JSXIdentifier",
            name: "a",
            range: [
              1,
              2
            ],
            loc: {
              start: {
                line: 1,
                column: 1
              },
              end: {
                line: 1,
                column: 2
              }
            }
          },
          selfClosing: true,
          attributes: [
            {
              type: "JSXAttribute",
              name: {
                type: "JSXIdentifier",
                name: "b",
                range: [
                  3,
                  4
                ],
                loc: {
                  start: {
                    line: 1,
                    column: 3
                  },
                  end: {
                    line: 1,
                    column: 4
                  }
                }
              },
              value: {
                type: "JSXExpressionContainer",
                expression: {
                  type: "ConditionalExpression",
                  test: {
                    type: "Identifier",
                    name: "x",
                    range: [
                      6,
                      7
                    ],
                    loc: {
                      start: {
                        line: 1,
                        column: 6
                      },
                      end: {
                        line: 1,
                        column: 7
                      }
                    }
                  },
                  consequent: {
                    type: "JSXElement",
                    openingElement: {
                      type: "JSXOpeningElement",
                      name: {
                        type: "JSXIdentifier",
                        name: "c",
                        range: [
                          11,
                          12
                        ],
                        loc: {
                          start: {
                            line: 1,
                            column: 11
                          },
                          end: {
                            line: 1,
                            column: 12
                          }
                        }
                      },
                      selfClosing: true,
                      attributes: [],
                      range: [
                        10,
                        15
                      ],
                      loc: {
                        start: {
                          line: 1,
                          column: 10
                        },
                        end: {
                          line: 1,
                          column: 15
                        }
                      }
                    },
                    closingElement: null,
                    children: [],
                    range: [
                      10,
                      15
                    ],
                    loc: {
                      start: {
                        line: 1,
                        column: 10
                      },
                      end: {
                        line: 1,
                        column: 15
                      }
                    }
                  },
                  alternate: {
                    type: "JSXElement",
                    openingElement: {
                      type: "JSXOpeningElement",
                      name: {
                        type: "JSXIdentifier",
                        name: "d",
                        range: [
                          19,
                          20
                        ],
                        loc: {
                          start: {
                            line: 1,
                            column: 19
                          },
                          end: {
                            line: 1,
                            column: 20
                          }
                        }
                      },
                      selfClosing: true,
                      attributes: [],
                      range: [
                        18,
                        23
                      ],
                      loc: {
                        start: {
                          line: 1,
                          column: 18
                        },
                        end: {
                          line: 1,
                          column: 23
                        }
                      }
                    },
                    closingElement: null,
                    children: [],
                    range: [
                      18,
                      23
                    ],
                    loc: {
                      start: {
                        line: 1,
                        column: 18
                      },
                      end: {
                        line: 1,
                        column: 23
                      }
                    }
                  },
                  range: [
                    6,
                    23
                  ],
                  loc: {
                    start: {
                      line: 1,
                      column: 6
                    },
                    end: {
                      line: 1,
                      column: 23
                    }
                  }
                },
                range: [
                  5,
                  24
                ],
                loc: {
                  start: {
                    line: 1,
                    column: 5
                  },
                  end: {
                    line: 1,
                    column: 24
                  }
                }
              },
              range: [
                3,
                24
              ],
              loc: {
                start: {
                  line: 1,
                  column: 3
                },
                end: {
                  line: 1,
                  column: 24
                }
              }
            }
          ],
          range: [
            0,
            27
          ],
          loc: {
            start: {
              line: 1,
              column: 0
            },
            end: {
              line: 1,
              column: 27
            }
          }
        },
        closingElement: null,
        children: [],
        range: [
          0,
          27
        ],
        loc: {
          start: {
            line: 1,
            column: 0
          },
          end: {
            line: 1,
            column: 27
          }
        }
      },
      range: [
        0,
        27
      ],
      loc: {
        start: {
          line: 1,
          column: 0
        },
        end: {
          line: 1,
          column: 27
        }
      }
    },

    '<a>{}</a>': {
      type: 'ExpressionStatement',
      expression: {
        type: 'JSXElement',
        openingElement: {
          type: 'JSXOpeningElement',
          name: {
            type: 'JSXIdentifier',
            name: 'a',
            range: [1, 2],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 2 }
            }
          },
          selfClosing: false,
          attributes: [],
          range: [0, 3],
          loc: {
            start: { line: 1, column: 0 },
            end: { line: 1, column: 3 }
          }
        },
        closingElement: {
          type: 'JSXClosingElement',
          name: {
            type: 'JSXIdentifier',
            name: 'a',
            range: [7, 8],
            loc: {
              start: { line: 1, column: 7 },
              end: { line: 1, column: 8 }
            }
          },
          range: [5, 9],
          loc: {
            start: { line: 1, column: 5 },
            end: { line: 1, column: 9 }
          }
        },
        children: [{
          type: 'JSXExpressionContainer',
          expression: {
            type: 'JSXEmptyExpression',
            range: [4, 4],
            loc: {
              start: { line: 1, column: 4 },
              end: { line: 1, column: 4 }
            }
          },
          range: [3, 5],
          loc: {
            start: { line: 1, column: 3 },
            end: { line: 1, column: 5 }
          }
        }],
        range: [0, 9],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 9 }
        }
      },
      range: [0, 9],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 9 }
      }
    },

    '<a>{/* this is a comment */}</a>': {
      type: 'ExpressionStatement',
      expression: {
        type: 'JSXElement',
        openingElement: {
          type: 'JSXOpeningElement',
          name: {
            type: 'JSXIdentifier',
            name: 'a',
            range: [1, 2],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 2 }
            }
          },
          selfClosing: false,
          attributes: [],
          range: [0, 3],
          loc: {
            start: { line: 1, column: 0 },
            end: { line: 1, column: 3 }
          }
        },
        closingElement: {
          type: 'JSXClosingElement',
          name: {
            type: 'JSXIdentifier',
            name: 'a',
            range: [30, 31],
            loc: {
              start: { line: 1, column: 30 },
              end: { line: 1, column: 31 }
            }
          },
          range: [28, 32],
          loc: {
            start: { line: 1, column: 28 },
            end: { line: 1, column: 32 }
          }
        },
        children: [{
          type: 'JSXExpressionContainer',
          expression: {
            type: 'JSXEmptyExpression',
            range: [4, 27],
            loc: {
              start: { line: 1, column: 4 },
              end: { line: 1, column: 27 }
            }
          },
          range: [3, 28],
          loc: {
            start: { line: 1, column: 3 },
            end: { line: 1, column: 28 }
          }
        }],
        range: [0, 32],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 32 }
        }
      },
      range: [0, 32],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 32 }
      }
    },

    '<div>@test content</div>': {
      type: 'ExpressionStatement',
      expression: {
        type: 'JSXElement',
        openingElement: {
          type: 'JSXOpeningElement',
          name: {
            type: 'JSXIdentifier',
            name: 'div',
            range: [1, 4],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 4 }
            }
          },
          selfClosing: false,
          attributes: [],
          range: [0, 5],
          loc: {
            start: { line: 1, column: 0 },
            end: { line: 1, column: 5 }
          }
        },
        closingElement: {
          type: 'JSXClosingElement',
          name: {
            type: 'JSXIdentifier',
            name: 'div',
            range: [20, 23],
            loc: {
              start: { line: 1, column: 20 },
              end: { line: 1, column: 23 }
            }
          },
          range: [18, 24],
          loc: {
            start: { line: 1, column: 18 },
            end: { line: 1, column: 24 }
          }
        },
        children: [{
          type: 'Literal',
          value: '@test content',
          raw: '@test content',
          range: [5, 18],
          loc: {
            start: { line: 1, column: 5 },
            end: { line: 1, column: 18 }
          }
        }],
        range: [0, 24],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 24 }
        }
      },
      range: [0, 24],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 24 }
      }
    },

    '<div><br />7x invalid-js-identifier</div>': {
      type: 'ExpressionStatement',
      expression: {
        type: 'JSXElement',
        openingElement: {
          type: 'JSXOpeningElement',
          name: {
            type: 'JSXIdentifier',
            name: 'div',
            range: [
              1,
              4
            ],
            loc: {
              start: {
                line: 1,
                column: 1
              },
              end: {
                line: 1,
                column: 4
              }
            }
          },
          selfClosing: false,
          attributes: [],
          range: [
            0,
            5
          ],
          loc: {
            start: {
              line: 1,
              column: 0
            },
            end: {
              line: 1,
              column: 5
            }
          }
        },
        closingElement: {
          type: 'JSXClosingElement',
          name: {
            type: 'JSXIdentifier',
            name: 'div',
            range: [
              37,
              40
            ],
            loc: {
              start: {
                line: 1,
                column: 37
              },
              end: {
                line: 1,
                column: 40
              }
            }
          },
          range: [
            35,
            41
          ],
          loc: {
            start: {
              line: 1,
              column: 35
            },
            end: {
              line: 1,
              column: 41
            }
          }
        },
        children: [{
          type: 'JSXElement',
          openingElement: {
            type: 'JSXOpeningElement',
            name: {
              type: 'JSXIdentifier',
              name: 'br',
              range: [
                6,
                8
              ],
              loc: {
                start: {
                  line: 1,
                  column: 6
                },
                end: {
                  line: 1,
                  column: 8
                }
              }
            },
            selfClosing: true,
            attributes: [],
            range: [
              5,
              11
            ],
            loc: {
              start: {
                line: 1,
                column: 5
              },
              end: {
                line: 1,
                column: 11
              }
            }
          },
          closingElement: null,
          children: [],
          range: [
            5,
            11
          ],
          loc: {
            start: {
              line: 1,
              column: 5
            },
            end: {
              line: 1,
              column: 11
            }
          }
        }, {
          type: 'Literal',
          value: '7x invalid-js-identifier',
          raw: '7x invalid-js-identifier',
          range: [
            11,
            35
          ],
          loc: {
            start: {
              line: 1,
              column: 11
            },
            end: {
              line: 1,
              column: 35
            }
          }
        }],
        range: [
          0,
          41
        ],
        loc: {
          start: {
            line: 1,
            column: 0
          },
          end: {
            line: 1,
            column: 41
          }
        }
      },
      range: [
        0,
        41
      ],
      loc: {
        start: {
          line: 1,
          column: 0
        },
        end: {
          line: 1,
          column: 41
        }
      }
    },

    '<LeftRight left=<a /> right=<b>monkeys /> gorillas</b> />': {
      "type": "ExpressionStatement",
      "expression": {
        "type": "JSXElement",
        "openingElement": {
          "type": "JSXOpeningElement",
          "name": {
            "type": "JSXIdentifier",
            "name": "LeftRight",
            "range": [
              1,
              10
            ],
            "loc": {
              "start": {
                "line": 1,
                "column": 1
              },
              "end": {
                "line": 1,
                "column": 10
              }
            }
          },
          "selfClosing": true,
          "attributes": [
            {
              "type": "JSXAttribute",
              "name": {
                "type": "JSXIdentifier",
                "name": "left",
                "range": [
                  11,
                  15
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 11
                  },
                  "end": {
                    "line": 1,
                    "column": 15
                  }
                }
              },
              "value": {
                "type": "JSXElement",
                "openingElement": {
                  "type": "JSXOpeningElement",
                  "name": {
                    "type": "JSXIdentifier",
                    "name": "a",
                    "range": [
                      17,
                      18
                    ],
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 17
                      },
                      "end": {
                        "line": 1,
                        "column": 18
                      }
                    }
                  },
                  "selfClosing": true,
                  "attributes": [],
                  "range": [
                    16,
                    21
                  ],
                  "loc": {
                    "start": {
                      "line": 1,
                      "column": 16
                    },
                    "end": {
                      "line": 1,
                      "column": 21
                    }
                  }
                },
                closingElement: null,
                "children": [],
                "range": [
                  16,
                  21
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 16
                  },
                  "end": {
                    "line": 1,
                    "column": 21
                  }
                }
              },
              "range": [
                11,
                21
              ],
              "loc": {
                "start": {
                  "line": 1,
                  "column": 11
                },
                "end": {
                  "line": 1,
                  "column": 21
                }
              }
            },
            {
              "type": "JSXAttribute",
              "name": {
                "type": "JSXIdentifier",
                "name": "right",
                "range": [
                  22,
                  27
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 22
                  },
                  "end": {
                    "line": 1,
                    "column": 27
                  }
                }
              },
              "value": {
                "type": "JSXElement",
                "openingElement": {
                  "type": "JSXOpeningElement",
                  "name": {
                    "type": "JSXIdentifier",
                    "name": "b",
                    "range": [
                      29,
                      30
                    ],
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 29
                      },
                      "end": {
                        "line": 1,
                        "column": 30
                      }
                    }
                  },
                  "selfClosing": false,
                  "attributes": [],
                  "range": [
                    28,
                    31
                  ],
                  "loc": {
                    "start": {
                      "line": 1,
                      "column": 28
                    },
                    "end": {
                      "line": 1,
                      "column": 31
                    }
                  }
                },
                "closingElement": {
                  "type": "JSXClosingElement",
                  "name": {
                    "type": "JSXIdentifier",
                    "name": "b",
                    "range": [
                      52,
                      53
                    ],
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 52
                      },
                      "end": {
                        "line": 1,
                        "column": 53
                      }
                    }
                  },
                  "range": [
                    50,
                    54
                  ],
                  "loc": {
                    "start": {
                      "line": 1,
                      "column": 50
                    },
                    "end": {
                      "line": 1,
                      "column": 54
                    }
                  }
                },
                "children": [
                  {
                    "type": "Literal",
                    "value": "monkeys /> gorillas",
                    "raw": "monkeys /> gorillas",
                    "range": [
                      31,
                      50
                    ],
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 31
                      },
                      "end": {
                        "line": 1,
                        "column": 50
                      }
                    }
                  }
                ],
                "range": [
                  28,
                  54
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 28
                  },
                  "end": {
                    "line": 1,
                    "column": 54
                  }
                }
              },
              "range": [
                22,
                54
              ],
              "loc": {
                "start": {
                  "line": 1,
                  "column": 22
                },
                "end": {
                  "line": 1,
                  "column": 54
                }
              }
            }
          ],
          "range": [
            0,
            57
          ],
          "loc": {
            "start": {
              "line": 1,
              "column": 0
            },
            "end": {
              "line": 1,
              "column": 57
            }
          }
        },
        closingElement: null,
        "children": [],
        "range": [
          0,
          57
        ],
        "loc": {
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 1,
            "column": 57
          }
        }
      },
      "range": [
        0,
        57
      ],
      "loc": {
        "start": {
          "line": 1,
          "column": 0
        },
        "end": {
          "line": 1,
          "column": 57
        }
      }
    },

    '<a.b></a.b>': {
      type: 'ExpressionStatement',
      expression: {
        type: 'JSXElement',
        openingElement: {
          type: 'JSXOpeningElement',
          name: {
            type: 'JSXMemberExpression',
            object: {
              type: 'JSXIdentifier',
              name: 'a',
              range: [1, 2],
              loc: {
                start: { line: 1, column: 1 },
                end: { line: 1, column: 2 }
              }
            },
            property: {
              type: 'JSXIdentifier',
              name: 'b',
              range: [3, 4],
              loc: {
                start: { line: 1, column: 3 },
                end: { line: 1, column: 4 }
              }
            },
            range: [1, 4],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 4 }
            }
          },
          selfClosing: false,
          attributes: [],
          range: [0, 5],
          loc: {
            start: { line: 1, column: 0 },
            end: { line: 1, column: 5 }
          }
        },
        closingElement: {
          type: 'JSXClosingElement',
          name: {
            type: 'JSXMemberExpression',
            object: {
              type: 'JSXIdentifier',
              name: 'a',
              range: [7, 8],
              loc: {
                start: { line: 1, column: 7 },
                end: { line: 1, column: 8 }
              }
            },
            property: {
              type: 'JSXIdentifier',
              name: 'b',
              range: [9, 10],
              loc: {
                start: { line: 1, column: 9 },
                end: { line: 1, column: 10 }
              }
            },
            range: [7, 10],
            loc: {
              start: { line: 1, column: 7 },
              end: { line: 1, column: 10 }
            }
          },
          range: [5, 11],
          loc: {
            start: { line: 1, column: 5 },
            end: { line: 1, column: 11 }
          }
        },
        children: [],
        range: [0, 11],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 11 }
        }
      },
      range: [0, 11],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 11 }
      }
    },

    '<a.b.c></a.b.c>': {
      type: 'ExpressionStatement',
      expression: {
        type: 'JSXElement',
        openingElement: {
          type: 'JSXOpeningElement',
          name: {
            type: 'JSXMemberExpression',
            object: {
              type: 'JSXMemberExpression',
              object: {
                type: 'JSXIdentifier',
                name: 'a',
                range: [1, 2],
                loc: {
                  start: { line: 1, column: 1 },
                  end: { line: 1, column: 2 }
                }
              },
              property: {
                type: 'JSXIdentifier',
                name: 'b',
                range: [3, 4],
                loc: {
                  start: { line: 1, column: 3 },
                  end: { line: 1, column: 4 }
                }
              },
              range: [1, 4],
              loc: {
                start: { line: 1, column: 1 },
                end: { line: 1, column: 4 }
              }
            },
            property: {
              type: 'JSXIdentifier',
              name: 'c',
              range: [5, 6],
              loc: {
                start: { line: 1, column: 5 },
                end: { line: 1, column: 6 }
              }
            },
            range: [1, 6],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 6 }
            }
          },
          selfClosing: false,
          attributes: [],
          range: [0, 7],
          loc: {
            start: { line: 1, column: 0 },
            end: { line: 1, column: 7 }
          }
        },
        closingElement: {
          type: 'JSXClosingElement',
          name: {
            type: 'JSXMemberExpression',
            object: {
              type: 'JSXMemberExpression',
              object: {
                type: 'JSXIdentifier',
                name: 'a',
                range: [9, 10],
                loc: {
                  start: { line: 1, column: 9 },
                  end: { line: 1, column: 10 }
                }
              },
              property: {
                type: 'JSXIdentifier',
                name: 'b',
                range: [11, 12],
                loc: {
                  start: { line: 1, column: 11 },
                  end: { line: 1, column: 12 }
                }
              },
              range: [9, 12],
              loc: {
                start: { line: 1, column: 9 },
                end: { line: 1, column: 12 }
              }
            },
            property: {
              type: 'JSXIdentifier',
              name: 'c',
              range: [13, 14],
              loc: {
                start: { line: 1, column: 13 },
                end: { line: 1, column: 14 }
              }
            },
            range: [9, 14],
            loc: {
              start: { line: 1, column: 9 },
              end: { line: 1, column: 14 }
            }
          },
          range: [7, 15],
          loc: {
            start: { line: 1, column: 7 },
            end: { line: 1, column: 15 }
          }
        },
        children: [],
        range: [0, 15],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 15 }
        }
      },
      range: [0, 15],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 15 }
      }
    },

    // In order to more useful parse errors, we disallow following an
    // JSXElement by a less-than symbol. In the rare case that the binary
    // operator was intended, the tag can be wrapped in parentheses:
    '(<div />) < x;': {
      type: 'ExpressionStatement',
      expression: {
        type: 'BinaryExpression',
        operator: '<',
        left: {
          type: 'JSXElement',
          openingElement: {
            type: 'JSXOpeningElement',
            name: {
              type: 'JSXIdentifier',
              name: 'div',
              range: [2, 5],
              loc: {
                start: { line: 1, column: 2 },
                end: { line: 1, column: 5 }
              }
            },
            selfClosing: true,
            attributes: [],
            range: [1, 8],
            loc: {
              start: { line: 1, column: 1 },
              end: { line: 1, column: 8 }
            }
          },
          closingElement: null,
          children: [],
          range: [1, 8],
          loc: {
            start: { line: 1, column: 1 },
            end: { line: 1, column: 8 }
          }
        },
        right: {
          type: 'Identifier',
          name: 'x',
          range: [12, 13],
          loc: {
            start: { line: 1, column: 12 },
            end: { line: 1, column: 13 }
          }
        },
        range: [0, 13],
        loc: {
          start: { line: 1, column: 0 },
          end: { line: 1, column: 13 }
        }
      },
      range: [0, 14],
      loc: {
        start: { line: 1, column: 0 },
        end: { line: 1, column: 14 }
      }
    },

    '<div {...props} />': {
      "type": "ExpressionStatement",
      "expression": {
        "type": "JSXElement",
        "openingElement": {
          "type": "JSXOpeningElement",
          "name": {
            "type": "JSXIdentifier",
            "name": "div",
            "range": [
              1,
              4
            ],
            "loc": {
              "start": {
                "line": 1,
                "column": 1
              },
              "end": {
                "line": 1,
                "column": 4
              }
            }
          },
          "selfClosing": true,
          "attributes": [
            {
              "type": "JSXSpreadAttribute",
              "argument": {
                "type": "Identifier",
                "name": "props",
                "range": [
                  9,
                  14
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 9
                  },
                  "end": {
                    "line": 1,
                    "column": 14
                  }
                }
              },
              "range": [
                5,
                15
              ],
              "loc": {
                "start": {
                  "line": 1,
                  "column": 5
                },
                "end": {
                  "line": 1,
                  "column": 15
                }
              }
            }
          ],
          "range": [
            0,
            18
          ],
          "loc": {
            "start": {
              "line": 1,
              "column": 0
            },
            "end": {
              "line": 1,
              "column": 18
            }
          }
        },
        closingElement: null,
        "children": [],
        "range": [
          0,
          18
        ],
        "loc": {
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 1,
            "column": 18
          }
        }
      },
      "range": [
        0,
        18
      ],
      "loc": {
        "start": {
          "line": 1,
          "column": 0
        },
        "end": {
          "line": 1,
          "column": 18
        }
      }
    },

    '<div {...props} post="attribute" />': {
      "type": "ExpressionStatement",
      "expression": {
        "type": "JSXElement",
        "openingElement": {
          "type": "JSXOpeningElement",
          "name": {
            "type": "JSXIdentifier",
            "name": "div",
            "range": [
              1,
              4
            ],
            "loc": {
              "start": {
                "line": 1,
                "column": 1
              },
              "end": {
                "line": 1,
                "column": 4
              }
            }
          },
          "selfClosing": true,
          "attributes": [
            {
              "type": "JSXSpreadAttribute",
              "argument": {
                "type": "Identifier",
                "name": "props",
                "range": [
                  9,
                  14
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 9
                  },
                  "end": {
                    "line": 1,
                    "column": 14
                  }
                }
              },
              "range": [
                5,
                15
              ],
              "loc": {
                "start": {
                  "line": 1,
                  "column": 5
                },
                "end": {
                  "line": 1,
                  "column": 15
                }
              }
            },
            {
              "type": "JSXAttribute",
              "name": {
                "type": "JSXIdentifier",
                "name": "post",
                "range": [
                  16,
                  20
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 16
                  },
                  "end": {
                    "line": 1,
                    "column": 20
                  }
                }
              },
              "value": {
                "type": "Literal",
                "value": "attribute",
                "raw": "\"attribute\"",
                "range": [
                  21,
                  32
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 21
                  },
                  "end": {
                    "line": 1,
                    "column": 32
                  }
                }
              },
              "range": [
                16,
                32
              ],
              "loc": {
                "start": {
                  "line": 1,
                  "column": 16
                },
                "end": {
                  "line": 1,
                  "column": 32
                }
              }
            }
          ],
          "range": [
            0,
            35
          ],
          "loc": {
            "start": {
              "line": 1,
              "column": 0
            },
            "end": {
              "line": 1,
              "column": 35
            }
          }
        },
        closingElement: null,
        "children": [],
        "range": [
          0,
          35
        ],
        "loc": {
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 1,
            "column": 35
          }
        }
      },
      "range": [
        0,
        35
      ],
      "loc": {
        "start": {
          "line": 1,
          "column": 0
        },
        "end": {
          "line": 1,
          "column": 35
        }
      }
    },

    '<div pre="leading" pre2="attribute" {...props}></div>': {
      "type": "ExpressionStatement",
      "expression": {
        "type": "JSXElement",
        "openingElement": {
          "type": "JSXOpeningElement",
          "name": {
            "type": "JSXIdentifier",
            "name": "div",
            "range": [
              1,
              4
            ],
            "loc": {
              "start": {
                "line": 1,
                "column": 1
              },
              "end": {
                "line": 1,
                "column": 4
              }
            }
          },
          "selfClosing": false,
          "attributes": [
            {
              "type": "JSXAttribute",
              "name": {
                "type": "JSXIdentifier",
                "name": "pre",
                "range": [
                  5,
                  8
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 5
                  },
                  "end": {
                    "line": 1,
                    "column": 8
                  }
                }
              },
              "value": {
                "type": "Literal",
                "value": "leading",
                "raw": "\"leading\"",
                "range": [
                  9,
                  18
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 9
                  },
                  "end": {
                    "line": 1,
                    "column": 18
                  }
                }
              },
              "range": [
                5,
                18
              ],
              "loc": {
                "start": {
                  "line": 1,
                  "column": 5
                },
                "end": {
                  "line": 1,
                  "column": 18
                }
              }
            },
            {
              "type": "JSXAttribute",
              "name": {
                "type": "JSXIdentifier",
                "name": "pre2",
                "range": [
                  19,
                  23
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 19
                  },
                  "end": {
                    "line": 1,
                    "column": 23
                  }
                }
              },
              "value": {
                "type": "Literal",
                "value": "attribute",
                "raw": "\"attribute\"",
                "range": [
                  24,
                  35
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 24
                  },
                  "end": {
                    "line": 1,
                    "column": 35
                  }
                }
              },
              "range": [
                19,
                35
              ],
              "loc": {
                "start": {
                  "line": 1,
                  "column": 19
                },
                "end": {
                  "line": 1,
                  "column": 35
                }
              }
            },
            {
              "type": "JSXSpreadAttribute",
              "argument": {
                "type": "Identifier",
                "name": "props",
                "range": [
                  40,
                  45
                ],
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 40
                  },
                  "end": {
                    "line": 1,
                    "column": 45
                  }
                }
              },
              "range": [
                36,
                46
              ],
              "loc": {
                "start": {
                  "line": 1,
                  "column": 36
                },
                "end": {
                  "line": 1,
                  "column": 46
                }
              }
            }
          ],
          "range": [
            0,
            47
          ],
          "loc": {
            "start": {
              "line": 1,
              "column": 0
            },
            "end": {
              "line": 1,
              "column": 47
            }
          }
        },
        "closingElement": {
          "type": "JSXClosingElement",
          "name": {
            "type": "JSXIdentifier",
            "name": "div",
            "range": [
              49,
              52
            ],
            "loc": {
              "start": {
                "line": 1,
                "column": 49
              },
              "end": {
                "line": 1,
                "column": 52
              }
            }
          },
          "range": [
            47,
            53
          ],
          "loc": {
            "start": {
              "line": 1,
              "column": 47
            },
            "end": {
              "line": 1,
              "column": 53
            }
          }
        },
        "children": [],
        "range": [
          0,
          53
        ],
        "loc": {
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 1,
            "column": 53
          }
        }
      },
      "range": [
        0,
        53
      ],
      "loc": {
        "start": {
          "line": 1,
          "column": 0
        },
        "end": {
          "line": 1,
          "column": 53
        }
      }
    },

    '<A aa={aa.bb.cc} bb={bb.cc.dd}><div>{aa.b}</div></A>': {
      "type": "ExpressionStatement",
      "start": 0,
      "end": 52,
      "loc": {
        "start": {
          "line": 1,
          "column": 0
        },
        "end": {
          "line": 1,
          "column": 52
        }
      },
      "range": [
        0,
        52
      ],
      "expression": {
        "type": "JSXElement",
        "start": 0,
        "end": 52,
        "loc": {
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 1,
            "column": 52
          }
        },
        "range": [
          0,
          52
        ],
        "openingElement": {
          "type": "JSXOpeningElement",
          "start": 0,
          "end": 31,
          "loc": {
            "start": {
              "line": 1,
              "column": 0
            },
            "end": {
              "line": 1,
              "column": 31
            }
          },
          "range": [
            0,
            31
          ],
          "attributes": [
            {
              "type": "JSXAttribute",
              "start": 3,
              "end": 16,
              "loc": {
                "start": {
                  "line": 1,
                  "column": 3
                },
                "end": {
                  "line": 1,
                  "column": 16
                }
              },
              "range": [
                3,
                16
              ],
              "name": {
                "type": "JSXIdentifier",
                "start": 3,
                "end": 5,
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 3
                  },
                  "end": {
                    "line": 1,
                    "column": 5
                  }
                },
                "range": [
                  3,
                  5
                ],
                "name": "aa"
              },
              "value": {
                "type": "JSXExpressionContainer",
                "start": 6,
                "end": 16,
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 6
                  },
                  "end": {
                    "line": 1,
                    "column": 16
                  }
                },
                "range": [
                  6,
                  16
                ],
                "expression": {
                  "type": "MemberExpression",
                  "start": 7,
                  "end": 15,
                  "loc": {
                    "start": {
                      "line": 1,
                      "column": 7
                    },
                    "end": {
                      "line": 1,
                      "column": 15
                    }
                  },
                  "range": [
                    7,
                    15
                  ],
                  "object": {
                    "type": "MemberExpression",
                    "start": 7,
                    "end": 12,
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 7
                      },
                      "end": {
                        "line": 1,
                        "column": 12
                      }
                    },
                    "range": [
                      7,
                      12
                    ],
                    "object": {
                      "type": "Identifier",
                      "start": 7,
                      "end": 9,
                      "loc": {
                        "start": {
                          "line": 1,
                          "column": 7
                        },
                        "end": {
                          "line": 1,
                          "column": 9
                        }
                      },
                      "range": [
                        7,
                        9
                      ],
                      "name": "aa"
                    },
                    "property": {
                      "type": "Identifier",
                      "start": 10,
                      "end": 12,
                      "loc": {
                        "start": {
                          "line": 1,
                          "column": 10
                        },
                        "end": {
                          "line": 1,
                          "column": 12
                        }
                      },
                      "range": [
                        10,
                        12
                      ],
                      "name": "bb"
                    },
                    "computed": false
                  },
                  "property": {
                    "type": "Identifier",
                    "start": 13,
                    "end": 15,
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 13
                      },
                      "end": {
                        "line": 1,
                        "column": 15
                      }
                    },
                    "range": [
                      13,
                      15
                    ],
                    "name": "cc"
                  },
                  "computed": false
                }
              }
            },
            {
              "type": "JSXAttribute",
              "start": 17,
              "end": 30,
              "loc": {
                "start": {
                  "line": 1,
                  "column": 17
                },
                "end": {
                  "line": 1,
                  "column": 30
                }
              },
              "range": [
                17,
                30
              ],
              "name": {
                "type": "JSXIdentifier",
                "start": 17,
                "end": 19,
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 17
                  },
                  "end": {
                    "line": 1,
                    "column": 19
                  }
                },
                "range": [
                  17,
                  19
                ],
                "name": "bb"
              },
              "value": {
                "type": "JSXExpressionContainer",
                "start": 20,
                "end": 30,
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 20
                  },
                  "end": {
                    "line": 1,
                    "column": 30
                  }
                },
                "range": [
                  20,
                  30
                ],
                "expression": {
                  "type": "MemberExpression",
                  "start": 21,
                  "end": 29,
                  "loc": {
                    "start": {
                      "line": 1,
                      "column": 21
                    },
                    "end": {
                      "line": 1,
                      "column": 29
                    }
                  },
                  "range": [
                    21,
                    29
                  ],
                  "object": {
                    "type": "MemberExpression",
                    "start": 21,
                    "end": 26,
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 21
                      },
                      "end": {
                        "line": 1,
                        "column": 26
                      }
                    },
                    "range": [
                      21,
                      26
                    ],
                    "object": {
                      "type": "Identifier",
                      "start": 21,
                      "end": 23,
                      "loc": {
                        "start": {
                          "line": 1,
                          "column": 21
                        },
                        "end": {
                          "line": 1,
                          "column": 23
                        }
                      },
                      "range": [
                        21,
                        23
                      ],
                      "name": "bb"
                    },
                    "property": {
                      "type": "Identifier",
                      "start": 24,
                      "end": 26,
                      "loc": {
                        "start": {
                          "line": 1,
                          "column": 24
                        },
                        "end": {
                          "line": 1,
                          "column": 26
                        }
                      },
                      "range": [
                        24,
                        26
                      ],
                      "name": "cc"
                    },
                    "computed": false
                  },
                  "property": {
                    "type": "Identifier",
                    "start": 27,
                    "end": 29,
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 27
                      },
                      "end": {
                        "line": 1,
                        "column": 29
                      }
                    },
                    "range": [
                      27,
                      29
                    ],
                    "name": "dd"
                  },
                  "computed": false
                }
              }
            }
          ],
          "name": {
            "type": "JSXIdentifier",
            "start": 1,
            "end": 2,
            "loc": {
              "start": {
                "line": 1,
                "column": 1
              },
              "end": {
                "line": 1,
                "column": 2
              }
            },
            "range": [
              1,
              2
            ],
            "name": "A"
          },
          "selfClosing": false
        },
        "closingElement": {
          "type": "JSXClosingElement",
          "start": 48,
          "end": 52,
          "loc": {
            "start": {
              "line": 1,
              "column": 48
            },
            "end": {
              "line": 1,
              "column": 52
            }
          },
          "range": [
            48,
            52
          ],
          "name": {
            "type": "JSXIdentifier",
            "start": 50,
            "end": 51,
            "loc": {
              "start": {
                "line": 1,
                "column": 50
              },
              "end": {
                "line": 1,
                "column": 51
              }
            },
            "range": [
              50,
              51
            ],
            "name": "A"
          }
        },
        "children": [
          {
            "type": "JSXElement",
            "start": 31,
            "end": 48,
            "loc": {
              "start": {
                "line": 1,
                "column": 31
              },
              "end": {
                "line": 1,
                "column": 48
              }
            },
            "range": [
              31,
              48
            ],
            "openingElement": {
              "type": "JSXOpeningElement",
              "start": 31,
              "end": 36,
              "loc": {
                "start": {
                  "line": 1,
                  "column": 31
                },
                "end": {
                  "line": 1,
                  "column": 36
                }
              },
              "range": [
                31,
                36
              ],
              "attributes": [],
              "name": {
                "type": "JSXIdentifier",
                "start": 32,
                "end": 35,
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 32
                  },
                  "end": {
                    "line": 1,
                    "column": 35
                  }
                },
                "range": [
                  32,
                  35
                ],
                "name": "div"
              },
              "selfClosing": false
            },
            "closingElement": {
              "type": "JSXClosingElement",
              "start": 42,
              "end": 48,
              "loc": {
                "start": {
                  "line": 1,
                  "column": 42
                },
                "end": {
                  "line": 1,
                  "column": 48
                }
              },
              "range": [
                42,
                48
              ],
              "name": {
                "type": "JSXIdentifier",
                "start": 44,
                "end": 47,
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 44
                  },
                  "end": {
                    "line": 1,
                    "column": 47
                  }
                },
                "range": [
                  44,
                  47
                ],
                "name": "div"
              }
            },
            "children": [
              {
                "type": "JSXExpressionContainer",
                "start": 36,
                "end": 42,
                "loc": {
                  "start": {
                    "line": 1,
                    "column": 36
                  },
                  "end": {
                    "line": 1,
                    "column": 42
                  }
                },
                "range": [
                  36,
                  42
                ],
                "expression": {
                  "type": "MemberExpression",
                  "start": 37,
                  "end": 41,
                  "loc": {
                    "start": {
                      "line": 1,
                      "column": 37
                    },
                    "end": {
                      "line": 1,
                      "column": 41
                    }
                  },
                  "range": [
                    37,
                    41
                  ],
                  "object": {
                    "type": "Identifier",
                    "start": 37,
                    "end": 39,
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 37
                      },
                      "end": {
                        "line": 1,
                        "column": 39
                      }
                    },
                    "range": [
                      37,
                      39
                    ],
                    "name": "aa"
                  },
                  "property": {
                    "type": "Identifier",
                    "start": 40,
                    "end": 41,
                    "loc": {
                      "start": {
                        "line": 1,
                        "column": 40
                      },
                      "end": {
                        "line": 1,
                        "column": 41
                      }
                    },
                    "range": [
                      40,
                      41
                    ],
                    "name": "b"
                  },
                  "computed": false
                }
              }
            ]
          }
        ]
      }
    }
  },
  'Regression': {
    '<p>foo <a href="test"> bar</a> baz</p> ;': {
      type: "ExpressionStatement",
      start: 0,
      end: 40,
      expression: {
        type: "JSXElement",
        start: 0,
        end: 38,
        openingElement: {
          type: "JSXOpeningElement",
          start: 0,
          end: 3,
          attributes: [],
          name: {
            type: "JSXIdentifier",
            start: 1,
            end: 2,
            name: "p"
          },
          selfClosing: false
        },
        closingElement: {
          type: "JSXClosingElement",
          start: 34,
          end: 38,
          name: {
            type: "JSXIdentifier",
            start: 36,
            end: 37,
            name: "p"
          }
        },
        children: [
          {
            type: "Literal",
            start: 3,
            end: 7,
            value: "foo ",
            raw: "foo "
          },
          {
            type: "JSXElement",
            start: 7,
            end: 30,
            openingElement: {
              type: "JSXOpeningElement",
              start: 7,
              end: 22,
              attributes: [{
                type: "JSXAttribute",
                start: 10,
                end: 21,
                name: {
                  type: "JSXIdentifier",
                  start: 10,
                  end: 14,
                  name: "href"
                },
                value: {
                  type: "Literal",
                  start: 15,
                  end: 21,
                  value: "test",
                  raw: "\"test\""
                }
              }],
              name: {
                type: "JSXIdentifier",
                start: 8,
                end: 9,
                name: "a"
              },
              selfClosing: false
            },
            closingElement: {
              type: "JSXClosingElement",
              start: 26,
              end: 30,
              name: {
                type: "JSXIdentifier",
                start: 28,
                end: 29,
                name: "a"
              }
            },
            children: [{
              type: "Literal",
              start: 22,
              end: 26,
              value: " bar",
              raw: " bar"
            }]
          },
          {
            type: "Literal",
            start: 30,
            end: 34,
            value: " baz",
            raw: " baz"
          }
        ]
      }
    },

    '<div>{<div {...test} />}</div>': {
      type: 'ExpressionStatement',
      start: 0,
      end: 30,
      expression: {
        type: 'JSXElement',
        start: 0,
        end: 30,
        openingElement: {
          type: 'JSXOpeningElement',
          start: 0,
          end: 5,
          attributes: [],
          name: {
            type: 'JSXIdentifier',
            start: 1,
            end: 4,
            name: 'div'
          },
          selfClosing: false
        },
        closingElement: {
          type: 'JSXClosingElement',
          start: 24,
          end: 30,
          name: {
            type: 'JSXIdentifier',
            start: 26,
            end: 29,
            name: 'div'
          }
        },
        children: [{
          type: 'JSXExpressionContainer',
          start: 5,
          end: 24,
          expression: {
            type: 'JSXElement',
            start: 6,
            end: 23,
            openingElement: {
              type: 'JSXOpeningElement',
              start: 6,
              end: 23,
              attributes: [
                {
                  type: 'JSXSpreadAttribute',
                  start: 11,
                  end: 20,
                  argument: {
                    type: 'Identifier',
                    start: 15,
                    end: 19,
                    name: 'test'
                  }
                }
              ],
              name: {
                type: 'JSXIdentifier',
                start: 7,
                end: 10,
                name: 'div'
              },
              selfClosing: true
            },
            closingElement: null,
            children: []
          }
        }]
      }
    },

    '<div>{ {a} }</div>': {
      type: "ExpressionStatement",
      start: 0,
      end: 18,
      expression: {
        type: "JSXElement",
        start: 0,
        end: 18,
        openingElement: {
          type: "JSXOpeningElement",
          start: 0,
          end: 5,
          attributes: [],
          name: {
            type: "JSXIdentifier",
            start: 1,
            end: 4,
            name: "div"
          },
          selfClosing: false
        },
        closingElement: {
          type: "JSXClosingElement",
          start: 12,
          end: 18,
          name: {
            type: "JSXIdentifier",
            start: 14,
            end: 17,
            name: "div"
          }
        },
        children: [{
          type: "JSXExpressionContainer",
          start: 5,
          end: 12,
          expression: {
            type: "ObjectExpression",
            start: 7,
            end: 10,
            properties: [{
              type: "Property",
              start: 8,
              end: 9,
              method: false,
              shorthand: true,
              computed: false,
              key: {
                type: "Identifier",
                start: 8,
                end: 9,
                name: "a"
              },
              kind: "init",
              value: {
                type: "Identifier",
                start: 8,
                end: 9,
                name: "a"
              }
            }]
          }
        }]
      }
    },

    '<div>/text</div>': {
      type: "ExpressionStatement",
      start: 0,
      end: 16,
      expression: {
        type: "JSXElement",
        start: 0,
        end: 16,
        openingElement: {
          type: "JSXOpeningElement",
          start: 0,
          end: 5,
          attributes: [],
          name: {
            type: "JSXIdentifier",
            start: 1,
            end: 4,
            name: "div"
          },
          selfClosing: false
        },
        closingElement: {
          type: "JSXClosingElement",
          start: 10,
          end: 16,
          name: {
            type: "JSXIdentifier",
            start: 12,
            end: 15,
            name: "div"
          }
        },
        children: [{
          type: "Literal",
          start: 5,
          end: 10,
          value: "/text",
          raw: "/text"
        }]
      }
    },

    '<div>{a}{b}</div>': {
      type: "ExpressionStatement",
      start: 0,
      end: 17,
      expression: {
        type: "JSXElement",
        start: 0,
        end: 17,
        openingElement: {
          type: "JSXOpeningElement",
          start: 0,
          end: 5,
          attributes: [],
          name: {
            type: "JSXIdentifier",
            start: 1,
            end: 4,
            name: "div"
          },
          selfClosing: false
        },
        closingElement: {
          type: "JSXClosingElement",
          start: 11,
          end: 17,
          name: {
            type: "JSXIdentifier",
            start: 13,
            end: 16,
            name: "div"
          }
        },
        children: [{
            type: 'JSXExpressionContainer',
            expression: {
              type: 'Identifier',
              name: 'a',
              range: [6, 7],
              loc: {
                start: {
                  line: 1,
                  column: 6
                },
                end: {
                  line: 1,
                  column: 7
                }
              }
            },
            range: [5, 8],
            loc: {
              start: {
                line: 1,
                column: 5
              },
              end: {
                line: 1,
                column: 8
              }
            }
          }, {
            type: 'JSXExpressionContainer',
            expression: {
              type: 'Identifier',
              name: 'b',
              range: [9, 10],
              loc: {
                start: {
                  line: 1,
                  column: 9
                },
                end: {
                  line: 1,
                  column: 10
                }
              }
            },
            range: [8, 11],
            loc: {
              start: {
                line: 1,
                column: 8
              },
              end: {
                line: 1,
                column: 11
              }
            }
          }
        ]
      }
    },

    '<div pre="leading" {...props} />': {
      type: "ExpressionStatement",
      range: [0, 32],
      expression: {
        type: "JSXElement",
        range: [0, 32],
        openingElement: {
          type: "JSXOpeningElement",
          range: [0, 32],
          attributes: [
            {
              type: "JSXAttribute",
              range: [5, 18],
              name: {
                type: "JSXIdentifier",
                range: [5, 8],
                name: "pre"
              },
              value: {
                type: "Literal",
                range: [9, 18],
                value: "leading"
              }
            },
            {
              type: "JSXSpreadAttribute",
              range: [19, 29],
              argument: {
                type: "Identifier",
                range: [23, 28],
                name: "props"
              }
            }
          ],
          name: {
            type: "JSXIdentifier",
            range: [1, 4],
            name: "div"
          },
          selfClosing: true
        },
        closingElement: null,
        children: []
      }
    },
    '<path d="M230 80\n\t\tA 45 45, 0, 1, 0, 275 125 \r\n    L 275 80 Z"/>': {
      type: "ExpressionStatement",
      expression: {
        type: "JSXElement",
        range: [0, 64],
        openingElement: {
          type: "JSXOpeningElement",
          range: [0, 64],
          attributes: [
            {
              type: "JSXAttribute",
              range: [6, 62],
              name: {
                type: "JSXIdentifier",
                range: [6, 7],
                name: "d"
              },
              value: {
                type: "Literal",
                loc: {
                  start: { line: 1, column: 8 },
                  end: { line: 3, column: 15 }
                },
                range: [8, 62],
                value: "M230 80\n\t\tA 45 45, 0, 1, 0, 275 125 \r\n    L 275 80 Z",
                raw: "\"M230 80\n\t\tA 45 45, 0, 1, 0, 275 125 \r\n    L 275 80 Z\""
              }
            }
          ],
          name: {
            type: "JSXIdentifier",
            range: [1, 5],
            name: "path"
          },
          selfClosing: true
        },
        closingElement: null,
        children: []
      }
    }
  }
};

if (typeof exports !== "undefined") {
  var test = require("./driver.js").test;
  var testFail = require("./driver.js").testFail;
  var tokTypes = require("../").tokTypes;
}

testFail("var x = <div>one</div><div>two</div>;", "Adjacent JSX elements must be wrapped in an enclosing tag (1:22)");

test('<a>{/* foo */}</a>', {}, {
  onToken: [
    {
      type: tokTypes.jsxTagStart,
      value: undefined,
      start: 0,
      end: 1
    },
    {
      type: tokTypes.jsxName,
      value: 'a',
      start: 1,
      end: 2
    },
    {
      type: tokTypes.jsxTagEnd,
      value: undefined,
      start: 2,
      end: 3
    },
    {
      type: tokTypes.braceL,
      value: undefined,
      start: 3,
      end: 4
    },
    {
      type: tokTypes.braceR,
      value: undefined,
      start: 13,
      end: 14
    },
    {
      type: tokTypes.jsxTagStart,
      value: undefined,
      start: 14,
      end: 15
    },
    {
      type: tokTypes.slash,
      value: '/',
      start: 15,
      end: 16
    },
    {
      type: tokTypes.jsxName,
      value: 'a',
      start: 16,
      end: 17
    },
    {
      type: tokTypes.jsxTagEnd,
      value: undefined,
      start: 17,
      end: 18
    },
    {
      type: tokTypes.eof,
      value: undefined,
      start: 18,
      end: 18
    }
  ]
});

for (var ns in fbTestFixture) {
  ns = fbTestFixture[ns];
  for (var code in ns) {
    test(code, {
      type: 'Program',
      body: [ns[code]]
    }, {
      ecmaVersion: 6,
      locations: true,
      ranges: true
    });
  }
}
