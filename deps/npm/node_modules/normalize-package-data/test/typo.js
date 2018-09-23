var test = require('tap').test

var normalize = require('../')
var typos = require('../lib/typos.json')
var warningMessages = require("../lib/warning_messages.json")
var safeFormat = require("../lib/safe_format")

test('typos', function(t) {
  var warnings = []
  function warn(m) {
    warnings.push(m)
  }
  
  var typoMessage = safeFormat.bind(undefined, warningMessages.typo)

  var expect =
    [ warningMessages.missingRepository,
      warningMessages.missingLicense,
      typoMessage('dependancies', 'dependencies'),
      typoMessage('dependecies', 'dependencies'),
      typoMessage('depdenencies', 'dependencies'),
      typoMessage('devEependencies', 'devDependencies'),
      typoMessage('depends', 'dependencies'),
      typoMessage('dev-dependencies', 'devDependencies'),
      typoMessage('devDependences', 'devDependencies'),
      typoMessage('devDepenencies', 'devDependencies'),
      typoMessage('devdependencies', 'devDependencies'),
      typoMessage('repostitory', 'repository'),
      typoMessage('repo', 'repository'),
      typoMessage('prefereGlobal', 'preferGlobal'),
      typoMessage('hompage', 'homepage'),
      typoMessage('hampage', 'homepage'),
      typoMessage('autohr', 'author'),
      typoMessage('autor', 'author'),
      typoMessage('contributers', 'contributors'),
      typoMessage('publicationConfig', 'publishConfig') ]

  normalize({"dependancies": "dependencies"
            ,"dependecies": "dependencies"
            ,"depdenencies": "dependencies"
            ,"devEependencies": "devDependencies"
            ,"depends": "dependencies"
            ,"dev-dependencies": "devDependencies"
            ,"devDependences": "devDependencies"
            ,"devDepenencies": "devDependencies"
            ,"devdependencies": "devDependencies"
            ,"repostitory": "repository"
            ,"repo": "repository"
            ,"prefereGlobal": "preferGlobal"
            ,"hompage": "homepage"
            ,"hampage": "homepage"
            ,"autohr": "author"
            ,"autor": "author"
            ,"contributers": "contributors"
            ,"publicationConfig": "publishConfig"
            ,readme:"asdf"
            ,name:"name"
            ,version:"1.2.5"}, warn)

  t.same(warnings, expect)

  warnings.length = 0
  var expect =
    [ warningMessages.missingDescription,
      warningMessages.missingRepository,
      typoMessage("bugs['web']", "bugs['url']"),
      typoMessage("bugs['name']", "bugs['url']"),
      warningMessages.nonUrlBugsUrlField,
      warningMessages.emptyNormalizedBugs,
      warningMessages.missingReadme,
      warningMessages.missingLicense]

  normalize({name:"name"
            ,version:"1.2.5"
            ,bugs:{web:"url",name:"url"}}, warn)

  t.same(warnings, expect)

  warnings.length = 0
  var expect =
    [ warningMessages.missingDescription,
      warningMessages.missingRepository,
      warningMessages.missingReadme,
      warningMessages.missingLicense,
      typoMessage('script', 'scripts') ]

  normalize({name:"name"
            ,version:"1.2.5"
            ,script:{server:"start",tests:"test"}}, warn)

  t.same(warnings, expect)

  warnings.length = 0
  expect =
    [ warningMessages.missingDescription,
      warningMessages.missingRepository,
      typoMessage("scripts['server']", "scripts['start']"),
      typoMessage("scripts['tests']", "scripts['test']"),
      warningMessages.missingReadme,
      warningMessages.missingLicense]

  normalize({name:"name"
            ,version:"1.2.5"
            ,scripts:{server:"start",tests:"test"}}, warn)

  t.same(warnings, expect)

  warnings.length = 0
  expect =
    [ warningMessages.missingDescription,
      warningMessages.missingRepository,
      warningMessages.missingReadme,
      warningMessages.missingLicense]

  normalize({name:"name"
            ,version:"1.2.5"
            ,scripts:{server:"start",tests:"test"
                     ,start:"start",test:"test"}}, warn)

  t.same(warnings, expect)

  warnings.length = 0
  expect = []

  normalize({private: true
            ,name:"name"
            ,version:"1.2.5"
            ,scripts:{server:"start",tests:"test"}}, warn)

  t.same(warnings, expect)

  t.end();
})
