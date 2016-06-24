var test = require('tap').test
var v = require('../npm-user-validate.js').username

test('username must be lowercase', function (t) {
  var err = v('ERRR')
  t.type(err, 'object')
  t.match(err.message, /lowercase/)
  t.end()
})

test('username may not contain non-url-safe chars', function (t) {
  var err = v('f  ')
  t.type(err, 'object')
  t.match(err.message, /url-safe/)
  t.end()
})

test('username may not start with "."', function (t) {
  var err = v('.username')
  t.type(err, 'object')
  t.match(err.message, /start with.*\./)
  t.end()
})

test('username may not be longer than 576 characters', function (t) {
  var err = v('bacon-ipsum-dolor-amet-tongue-short-loin-landjaeger-tenderloin-ball-tip-pork-loin-porchetta-pig-pork-chop-beef-ribs-pork-belly--shankle-t-bone-turducken-tongue-landjaeger-pork-loin-beef-chicken-short-loin-venison-capicola--brisket-leberkas-pork-beef-ribs-kevin-short-ribs-tail-bresaola-ham--rump-fatback-short-ribs-frankfurter-boudin--turkey-cupim-tri-tip-pork-chop-landjaeger-frankfurter-ham-hock---kielbasa-sausage-sirloin-short-loin-bacon-tenderloin-biltong-spare-ribs-cow-beef-ribs-tongue-cupim-filet-mignon-drumstick--pork-chop-tenderloin-brisket-pork-belly-leberkas-and-a-pickle')
  t.type(err, 'object')
  t.match(err.message, /less than or equal to 576/)
  t.end()
});

test('username may be as long as 576 characters', function (t) {
  var err = v('bacon-ipsum-dolor-amet-tongue-short-loin-landjaeger-tenderloin-ball-tip-pork-loin-porchetta-pig-pork-chop-beef-ribs-pork-belly--shankle-t-bone-turducken-tongue-landjaeger-pork-loin-beef-chicken-short-loin-venison-capicola--brisket-leberkas-pork-beef-ribs-kevin-short-ribs-tail-bresaola-ham--rump-fatback-short-ribs-frankfurter-boudin--turkey-cupim-tri-tip-pork-chop-landjaeger-frankfurter-ham-hock---kielbasa-sausage-sirloin-short-loin-bacon-tenderloin-biltong-spare-ribs-cow-beef-ribs-tongue-cupim-filet-mignon-drumstick--pork-chop-tenderloin-brisket-pork-belly-leberkas-beef')
  t.type(err, 'null')
  t.end()
});

test('username is ok', function (t) {
  var err = v('ente')
  t.type(err, 'null')
  t.end()
})
