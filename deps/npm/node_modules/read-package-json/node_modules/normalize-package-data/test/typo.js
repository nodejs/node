var test = require('tap').test
var normalize = require('../')
var typos = require('../lib/typos.json')

test('typos', function(t) {
  var warnings = []
  function warn(m) {
    warnings.push(m)
  }

  var expect =
    [ 'No repository field.',
      'dependancies should probably be dependencies.',
      'dependecies should probably be dependencies.',
      'depdenencies should probably be dependencies.',
      'devEependencies should probably be devDependencies.',
      'depends should probably be dependencies.',
      'dev-dependencies should probably be devDependencies.',
      'devDependences should probably be devDependencies.',
      'devDepenencies should probably be devDependencies.',
      'devdependencies should probably be devDependencies.',
      'repostitory should probably be repository.',
      'prefereGlobal should probably be preferGlobal.',
      'hompage should probably be homepage.',
      'hampage should probably be homepage.',
      'autohr should probably be author.',
      'autor should probably be author.',
      'contributers should probably be contributors.',
      'publicationConfig should probably be publishConfig.' ]

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
    [ 'No description',
      'No repository field.',
      'bugs[\'web\'] should probably be bugs[\'url\'].',
      'bugs[\'name\'] should probably be bugs[\'url\'].',
      'bugs.url field must be a string url. Deleted.',
      'Normalized value of bugs field is an empty object. Deleted.',
      "No README data" ]

  normalize({name:"name"
            ,version:"1.2.5"
            ,bugs:{web:"url",name:"url"}}, warn)

  t.same(warnings, expect)

  warnings.length = 0
  var expect =
    [ 'No description',
      'No repository field.',
      "No README data",
      'script should probably be scripts.' ]

  normalize({name:"name"
            ,version:"1.2.5"
            ,script:{server:"start",tests:"test"}}, warn)

  t.same(warnings, expect)

  warnings.length = 0
  expect =
    [ 'No description',
      'No repository field.',
      'scripts[\'server\'] should probably be scripts[\'start\'].',
      'scripts[\'tests\'] should probably be scripts[\'test\'].',
      "No README data" ]

  normalize({name:"name"
            ,version:"1.2.5"
            ,scripts:{server:"start",tests:"test"}}, warn)

  t.same(warnings, expect)

  t.end();
})
