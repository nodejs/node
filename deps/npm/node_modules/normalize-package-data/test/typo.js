var test = require('tap').test
var normalize = require('../')
var typos = require('../lib/typos.json')

test('typos', function(t) {
  var warnings = []
  function warn(m) {
    warnings.push(m)
  }

  var expect =
    [ 'dependancies should probably be dependencies.',
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
      'publicationConfig should probably be publishConfig.',
      'No repository field.',
      'No repository field.',
      'No readme data.',
      'bugs.url field must be a string url. Deleted.',
      'Normalized value of bugs field is an empty object. Deleted.',
      'No repository field.',      
      'No readme data.' ]

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

  normalize({name:"name"
            ,version:"1.2.5"
            ,bugs:{web:"url",name:"url"}}, warn)

  normalize({name:"name"
            ,version:"1.2.5"
            ,script:{server:"start",tests:"test"}}, warn)
  t.same(warnings, expect)
  t.end();
})
