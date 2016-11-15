var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.join(__dirname, 'install-shrinkwrap-equals-ls')

var EXEC_OPTS = {cwd: pkg}

var json = {
  "name": "install-shrinkwrap-equals-ls",
  "version": "1.0.0",
  "dependencies": {
    "react": "^0.14.0",
    "react-bootstrap": "^0.28.1",
    "react-dom": "^0.14.0"
  }
}

var shrinkwrap = {
  "name": "install-shrinkwrap-equals-ls",
  "version": "1.0.0",
  "dependencies": {
    "react": {
      "version": "0.14.8",
      "from": "react@>=0.14.0 <0.15.0",
      "resolved": "https://registry.npmjs.org/react/-/react-0.14.8.tgz",
      "dependencies": {
        "envify": {
          "version": "3.4.0",
          "from": "envify@>=3.0.0 <4.0.0",
          "resolved": "https://registry.npmjs.org/envify/-/envify-3.4.0.tgz",
          "dependencies": {
            "through": {
              "version": "2.3.8",
              "from": "through@>=2.3.4 <2.4.0",
              "resolved": "https://registry.npmjs.org/through/-/through-2.3.8.tgz"
            },
            "jstransform": {
              "version": "10.1.0",
              "from": "jstransform@>=10.0.1 <11.0.0",
              "resolved": "https://registry.npmjs.org/jstransform/-/jstransform-10.1.0.tgz",
              "dependencies": {
                "base62": {
                  "version": "0.1.1",
                  "from": "base62@0.1.1",
                  "resolved": "https://registry.npmjs.org/base62/-/base62-0.1.1.tgz"
                },
                "esprima-fb": {
                  "version": "13001.1001.0-dev-harmony-fb",
                  "from": "esprima-fb@13001.1001.0-dev-harmony-fb",
                  "resolved": "https://registry.npmjs.org/esprima-fb/-/esprima-fb-13001.1001.0-dev-harmony-fb.tgz"
                },
                "source-map": {
                  "version": "0.1.31",
                  "from": "source-map@0.1.31",
                  "resolved": "https://registry.npmjs.org/source-map/-/source-map-0.1.31.tgz",
                  "dependencies": {
                    "amdefine": {
                      "version": "1.0.0",
                      "from": "amdefine@>=0.0.4",
                      "resolved": "https://registry.npmjs.org/amdefine/-/amdefine-1.0.0.tgz"
                    }
                  }
                }
              }
            }
          }
        },
        "fbjs": {
          "version": "0.6.1",
          "from": "fbjs@>=0.6.1 <0.7.0",
          "resolved": "https://registry.npmjs.org/fbjs/-/fbjs-0.6.1.tgz",
          "dependencies": {
            "core-js": {
              "version": "1.2.6",
              "from": "core-js@>=1.0.0 <2.0.0",
              "resolved": "https://registry.npmjs.org/core-js/-/core-js-1.2.6.tgz"
            },
            "loose-envify": {
              "version": "1.1.0",
              "from": "loose-envify@>=1.0.0 <2.0.0",
              "resolved": "https://registry.npmjs.org/loose-envify/-/loose-envify-1.1.0.tgz",
              "dependencies": {
                "js-tokens": {
                  "version": "1.0.3",
                  "from": "js-tokens@>=1.0.1 <2.0.0",
                  "resolved": "https://registry.npmjs.org/js-tokens/-/js-tokens-1.0.3.tgz"
                }
              }
            },
            "promise": {
              "version": "7.1.1",
              "from": "promise@>=7.0.3 <8.0.0",
              "resolved": "https://registry.npmjs.org/promise/-/promise-7.1.1.tgz",
              "dependencies": {
                "asap": {
                  "version": "2.0.3",
                  "from": "asap@>=2.0.3 <2.1.0",
                  "resolved": "https://registry.npmjs.org/asap/-/asap-2.0.3.tgz"
                }
              }
            },
            "ua-parser-js": {
              "version": "0.7.10",
              "from": "ua-parser-js@>=0.7.9 <0.8.0",
              "resolved": "https://registry.npmjs.org/ua-parser-js/-/ua-parser-js-0.7.10.tgz"
            },
            "whatwg-fetch": {
              "version": "0.9.0",
              "from": "whatwg-fetch@>=0.9.0 <0.10.0",
              "resolved": "https://registry.npmjs.org/whatwg-fetch/-/whatwg-fetch-0.9.0.tgz"
            }
          }
        }
      }
    },
    "react-bootstrap": {
      "version": "0.28.5",
      "from": "react-bootstrap@>=0.28.1 <0.29.0",
      "resolved": "https://registry.npmjs.org/react-bootstrap/-/react-bootstrap-0.28.5.tgz",
      "dependencies": {
        "babel-runtime": {
          "version": "5.8.38",
          "from": "babel-runtime@>=5.8.25 <6.0.0",
          "resolved": "https://registry.npmjs.org/babel-runtime/-/babel-runtime-5.8.38.tgz",
          "dependencies": {
            "core-js": {
              "version": "1.2.6",
              "from": "core-js@>=1.0.0 <2.0.0",
              "resolved": "https://registry.npmjs.org/core-js/-/core-js-1.2.6.tgz"
            }
          }
        },
        "classnames": {
          "version": "2.2.3",
          "from": "classnames@>=2.1.5 <3.0.0",
          "resolved": "https://registry.npmjs.org/classnames/-/classnames-2.2.3.tgz"
        },
        "dom-helpers": {
          "version": "2.4.0",
          "from": "dom-helpers@>=2.4.0 <3.0.0",
          "resolved": "https://registry.npmjs.org/dom-helpers/-/dom-helpers-2.4.0.tgz"
        },
        "invariant": {
          "version": "2.2.1",
          "from": "invariant@>=2.1.2 <3.0.0",
          "resolved": "https://registry.npmjs.org/invariant/-/invariant-2.2.1.tgz",
          "dependencies": {
            "loose-envify": {
              "version": "1.1.0",
              "from": "loose-envify@>=1.0.0 <2.0.0",
              "resolved": "https://registry.npmjs.org/loose-envify/-/loose-envify-1.1.0.tgz",
              "dependencies": {
                "js-tokens": {
                  "version": "1.0.3",
                  "from": "js-tokens@>=1.0.1 <2.0.0",
                  "resolved": "https://registry.npmjs.org/js-tokens/-/js-tokens-1.0.3.tgz"
                }
              }
            }
          }
        },
        "keycode": {
          "version": "2.1.1",
          "from": "keycode@>=2.1.0 <3.0.0",
          "resolved": "https://registry.npmjs.org/keycode/-/keycode-2.1.1.tgz"
        },
        "lodash-compat": {
          "version": "3.10.2",
          "from": "lodash-compat@>=3.10.1 <4.0.0",
          "resolved": "https://registry.npmjs.org/lodash-compat/-/lodash-compat-3.10.2.tgz"
        },
        "react-overlays": {
          "version": "0.6.3",
          "from": "react-overlays@>=0.6.0 <0.7.0",
          "resolved": "https://registry.npmjs.org/react-overlays/-/react-overlays-0.6.3.tgz",
          "dependencies": {
            "react-prop-types": {
              "version": "0.2.2",
              "from": "react-prop-types@>=0.2.1 <0.3.0",
              "resolved": "https://registry.npmjs.org/react-prop-types/-/react-prop-types-0.2.2.tgz"
            }
          }
        },
        "react-prop-types": {
          "version": "0.3.0",
          "from": "react-prop-types@>=0.3.0 <0.4.0",
          "resolved": "https://registry.npmjs.org/react-prop-types/-/react-prop-types-0.3.0.tgz"
        },
        "uncontrollable": {
          "version": "3.2.3",
          "from": "uncontrollable@>=3.1.3 <4.0.0",
          "resolved": "https://registry.npmjs.org/uncontrollable/-/uncontrollable-3.2.3.tgz"
        },
        "warning": {
          "version": "2.1.0",
          "from": "warning@>=2.1.0 <3.0.0",
          "resolved": "https://registry.npmjs.org/warning/-/warning-2.1.0.tgz",
          "dependencies": {
            "loose-envify": {
              "version": "1.1.0",
              "from": "loose-envify@>=1.0.0 <2.0.0",
              "resolved": "https://registry.npmjs.org/loose-envify/-/loose-envify-1.1.0.tgz",
              "dependencies": {
                "js-tokens": {
                  "version": "1.0.3",
                  "from": "js-tokens@>=1.0.1 <2.0.0",
                  "resolved": "https://registry.npmjs.org/js-tokens/-/js-tokens-1.0.3.tgz"
                }
              }
            }
          }
        }
      }
    },
    "react-dom": {
      "version": "0.14.8",
      "from": "react-dom@>=0.14.0 <0.15.0",
      "resolved": "https://registry.npmjs.org/react-dom/-/react-dom-0.14.8.tgz"
    }
  }
}


test('setup', function (t) {
  setup()
  t.end()
})

test('An npm install with shrinkwrap equals npm ls --json', function (t) {
  common.npm(
    [
      '--loglevel', 'silent',
      'install'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')
      common.npm(
        [
          '--loglevel', 'silent',
          'ls', '--json'
        ],
        EXEC_OPTS,
        function (err, code, out) {
          t.ifError(err, 'npm ls --json ran without issue')
          t.notOk(code, 'npm ls --json exited with code 0')
          var actual = common.rmFromInShrinkwrap(JSON.parse(out))
          var expected = common.rmFromInShrinkwrap(
            JSON.parse(JSON.stringify(shrinkwrap))
          )
          t.deepEqual(actual, expected)
          t.end()
        })
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(__dirname)
  rimraf.sync(pkg)
}

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  process.chdir(pkg)
  fs.writeFileSync(
    'package.json',
    JSON.stringify(json, null, 2)
  )
  fs.writeFileSync(
    'npm-shrinkwrap.json',
    JSON.stringify(shrinkwrap, null, 2)
  )
}
