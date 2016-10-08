var dateRe = /^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}Z$/
var shaRe = /^[a-f0-9]{40}$/i
var semverRe = /^v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][a-zA-Z0-9-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][a-zA-Z0-9-]*))*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$/
var revRe = /^[0-9]+\-[0-9a-f]{32}$/i

module.exports =
{
  "norevs": {
    "rows": []
  },
  "mixedcase": {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  "conflicts": {
    "rows": []
  },
  "oddhost": {
    "rows": [
      {
        "key": null,
        "value": 3
      }
    ]
  },
  "updated": {
    "total_rows": 1,
    "offset": 0,
    "rows": [
      {
        "id": "package",
        "key": dateRe,
        "value": 1
      }
    ]
  },
  "listAll": {
    "total_rows": 1,
    "offset": 0,
    "rows": [
      {
        "id": "package",
        "key": "package",
        "value": {
          "_id": "package",
          "_rev": revRe,
          "name": "package",
          "description": "just an npm test, but with a **markdown** readme.",
          "dist-tags": {
            "latest": "0.2.3",
            "alpha": "0.2.3-alpha"
          },
          "versions": {
            "0.0.2": {
              "name": "package",
              "version": "0.0.2",
              "description": "just an npm test",
              "_id": "package@0.0.2",
              "scripts": {},
              "_shasum": shaRe,
              "_from": ".",
              "_npmVersion": semverRe,
              "_npmUser": {
                "name": "user",
                "email": "email@example.com"
              },
              "maintainers": [
                {
                  "name": "user",
                  "email": "email@example.com"
                }
              ],
              "dist": {
                "shasum": shaRe,
                "tarball": "http://127.0.0.1:15986/package/-/package-0.0.2.tgz"
              }
            },
            "0.2.3-alpha": {
              "name": "package",
              "version": "0.2.3-alpha",
              "description": "just an npm test, but with a **markdown** readme.",
              "_id": "package@0.2.3-alpha",
              "scripts": {},
              "_shasum": shaRe,
              "_from": ".",
              "_npmVersion": semverRe,
              "_npmUser": {
                "name": "user",
                "email": "email@example.com"
              },
              "maintainers": [
                {
                  "name": "user",
                  "email": "email@example.com"
                }
              ],
              "dist": {
                "shasum": shaRe,
                "tarball": "http://127.0.0.1:15986/package/-/package-0.2.3-alpha.tgz"
              }
            },
            "0.2.3": {
              "name": "package",
              "version": "0.2.3",
              "description": "just an npm test, but with a **markdown** readme.",
              "_id": "package@0.2.3",
              "scripts": {},
              "_shasum": shaRe,
              "_from": ".",
              "_npmVersion": semverRe,
              "_npmUser": {
                "name": "other",
                "email": "other@example.com"
              },
              "maintainers": [
                {
                  "name": "user",
                  "email": "email@example.com"
                },
                {
                  "name": "other",
                  "email": "other@example.com"
                }
              ],
              "dist": {
                "shasum": shaRe,
                "tarball": "http://127.0.0.1:15986/package/-/package-0.2.3.tgz"
              }
            }
          },
          "readme": "just an npm test, but with a **markdown** readme.\n",
          "maintainers": [
            {
              "name": "user",
              "email": "email@example.com"
            },
            {
              "name": "other",
              "email": "other@example.com"
            }
          ],
          "time": {
            "modified": dateRe,
            "created": dateRe,
            "0.0.2": dateRe,
            "0.2.3-alpha": dateRe,
            "0.2.3": dateRe
          },
          "readmeFilename": "README.md",
          "users": {}
        }
      }
    ]
  },
  "allVersions": {
    "rows": [
      {
        "key": null,
        "value": 3
      }
    ]
  },
  "noShasum": {
    "rows": []
  },
  "byEngine": {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  "countVersions": {
    "rows": [
      {
        "key": null,
        "value": 1
      }
    ]
  },
  "byKeyword": {
    "rows": []
  },
  "byField": {
    "total_rows": 1,
    "offset": 0,
    "rows": [
      {
        "id": "package",
        "key": "package",
        "value": {
          "name": "package",
          "version": "0.2.3",
          "description": "just an npm test, but with a **markdown** readme.",
          "_id": "package@0.2.3",
          "scripts": {},
          "_shasum": shaRe,
          "_from": ".",
          "_npmVersion": semverRe,
          "_npmUser": {
            "name": "other",
            "email": "other@example.com"
          },
          "maintainers": [
            {
              "name": "user",
              "email": "email@example.com"
            },
            {
              "name": "other",
              "email": "other@example.com"
            }
          ],
          "dist": {
            "shasum": shaRe,
            "tarball": "http://127.0.0.1:15986/package/-/package-0.2.3.tgz"
          }
        }
      }
    ]
  },
  "needBuild": {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  "scripts": {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  "nodeWafInstall": {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  "orphanAttachments": {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  "noAttachment": {
    "rows": [
      {
        "key": null,
        "value": 3
      }
    ]
  },
  "starredByUser": {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  "starredByPackage": {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  "byUser": {
    "total_rows": 2,
    "offset": 0,
    "rows": [
      {
        "id": "package",
        "key": "other",
        "value": "package"
      },
      {
        "id": "package",
        "key": "user",
        "value": "package"
      }
    ]
  },
  "browseAuthorsRecent": {
    "rows": [
      {
        "key": null,
        "value": 1
      }
    ]
  },
  "npmTop": {
    "rows": [
      {
        "key": null,
        "value": 2
      }
    ]
  },
  "browseAuthors": {
    "rows": [
      {
        "key": null,
        "value": 2
      }
    ]
  },
  "browseUpdated": {
    "rows": [
      {
        "key": null,
        "value": 1
      }
    ]
  },
  "browseAll": {
    "rows": [
      {
        "key": null,
        "value": 1
      }
    ]
  },
  "analytics": {
    "rows": [
      {
        "key": null,
        "value": 5
      }
    ]
  },
  "dependedUpon": {
    "rows": []
  },
  "dependentVersions": {
    "rows": []
  },
  "browseStarUser": {
    "rows": []
  },
  "browseStarPackage": {
    "rows": []
  },
  "fieldsInUse": {
    "rows": [
      {
        "key": null,
        "value": 11
      }
    ]
  },
  "norevs?group=true": {
    "rows": []
  },
  "conflicts?group=true": {
    "rows": []
  },
  "oddhost?group=true": {
    "rows": [
      {
        "key": [
          "package",
          "package@0.0.2",
          "http://127.0.0.1:15986/package/-/package-0.0.2.tgz"
        ],
        "value": 1
      },
      {
        "key": [
          "package",
          "package@0.2.3",
          "http://127.0.0.1:15986/package/-/package-0.2.3.tgz"
        ],
        "value": 1
      },
      {
        "key": [
          "package",
          "package@0.2.3-alpha",
          "http://127.0.0.1:15986/package/-/package-0.2.3-alpha.tgz"
        ],
        "value": 1
      }
    ]
  },
  "allVersions?group=true": {
    "rows": [
      {
        "key": [
          "0.0.2",
          "package"
        ],
        "value": 1
      },
      {
        "key": [
          "0.2.3",
          "package"
        ],
        "value": 1
      },
      {
        "key": [
          "0.2.3-alpha",
          "package"
        ],
        "value": 1
      }
    ]
  },
  "noShasum?group=true": {
    "rows": []
  },
  "countVersions?group=true": {
    "rows": [
      {
        "key": [
          3,
          "package"
        ],
        "value": 1
      }
    ]
  },
  "byKeyword?group=true": {
    "rows": []
  },
  "noAttachment?group=true": {
    "rows": [
      {
        "key": [
          "package",
          "0.0.2",
          "http://127.0.0.1:15986/package/-/package-0.0.2.tgz"
        ],
        "value": 1
      },
      {
        "key": [
          "package",
          "0.2.3",
          "http://127.0.0.1:15986/package/-/package-0.2.3.tgz"
        ],
        "value": 1
      },
      {
        "key": [
          "package",
          "0.2.3-alpha",
          "http://127.0.0.1:15986/package/-/package-0.2.3-alpha.tgz"
        ],
        "value": 1
      }
    ]
  },
  "browseAuthorsRecent?group=true": {
    "rows": [
      {
        "key": [
          dateRe,
          "other",
          "package",
          "just an npm test, but with a **markdown** readme."
        ],
        "value": 1
      }
    ]
  },
  "npmTop?group=true": {
    "rows": [
      {
        "key": [
          "other",
          "package",
          "just an npm test, but with a **markdown** readme.",
          dateRe
        ],
        "value": 1
      },
      {
        "key": [
          "user",
          "package",
          "just an npm test, but with a **markdown** readme.",
          dateRe
        ],
        "value": 1
      }
    ]
  },
  "browseAuthors?group=true": {
    "rows": [
      {
        "key": [
          "other",
          "package",
          "just an npm test, but with a **markdown** readme.",
          dateRe
        ],
        "value": 1
      },
      {
        "key": [
          "user",
          "package",
          "just an npm test, but with a **markdown** readme.",
          dateRe
        ],
        "value": 1
      }
    ]
  },
  "browseUpdated?group=true": {
    "rows": [
      {
        "key": [
          dateRe,
          "package",
          "just an npm test, but with a **markdown** readme."
        ],
        "value": 1
      }
    ]
  },
  "browseAll?group=true": {
    "rows": [
      {
        "key": [
          "package",
          "just an npm test, but with a **markdown** readme."
        ],
        "value": 1
      }
    ]
  },
  "analytics?group=true": {
    "rows": [
      {
        "key": [
          "created",
          2014,
          8,
          28,
          "package"
        ],
        "value": 1
      },
      {
        "key": [
          "latest",
          2014,
          8,
          28,
          "package"
        ],
        "value": 1
      },
      {
        "key": [
          "update",
          2014,
          8,
          28,
          "package"
        ],
        "value": 3
      }
    ]
  },
  "dependedUpon?group=true": {
    "rows": []
  },
  "dependentVersions?group=true": {
    "rows": []
  },
  "browseStarUser?group=true": {
    "rows": []
  },
  "browseStarPackage?group=true": {
    "rows": []
  },
  "fieldsInUse?group=true": {
    "rows": [
      {
        "key": "_from",
        "value": 1
      },
      {
        "key": "_id",
        "value": 1
      },
      {
        "key": "_npmUser",
        "value": 1
      },
      {
        "key": "_npmVersion",
        "value": 1
      },
      {
        "key": "_shasum",
        "value": 1
      },
      {
        "key": "description",
        "value": 1
      },
      {
        "key": "dist",
        "value": 1
      },
      {
        "key": "maintainers",
        "value": 1
      },
      {
        "key": "name",
        "value": 1
      },
      {
        "key": "scripts",
        "value": 1
      },
      {
        "key": "version",
        "value": 1
      }
    ]
  }
}

if (module === require.main) {
  console.log('1..1')
  console.log('ok - just setting things up for other tests')
}
