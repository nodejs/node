var test = require("tap").test

var normalize = require("../")

test("strict", function(t) {
  var threw

  try {
    threw = false
    normalize({name: "X"}, true)
  } catch (er) {
    threw = true
    t.equal(er.message, 'Invalid name: "X"')
  } finally {
    t.equal(threw, true)
  }

  try {
    threw = false
    normalize({name:" x "}, true)
  } catch (er) {
    threw = true
    t.equal(er.message, 'Invalid name: " x "')
  } finally {
    t.equal(threw, true)
  }

  try {
    threw = false
    normalize({name:"x",version:"01.02.03"}, true)
  } catch (er) {
    threw = true
    t.equal(er.message, 'Invalid version: "01.02.03"')
  } finally {
    t.equal(threw, true)
  }

  // these should not throw
  var slob = {name:" X ",version:"01.02.03",dependencies:{
    y:">01.02.03",
    z:"! 99 $$ASFJ(Aawenf90awenf as;naw.3j3qnraw || an elephant"
  }}
  normalize(slob, false)
  t.same(slob,
         { name: 'X',
           version: '1.2.3',
           dependencies:
            { y: '>01.02.03',
              z: '! 99 $$ASFJ(Aawenf90awenf as;naw.3j3qnraw || an elephant' },
           readme: 'ERROR: No README data found!',
           _id: 'X@1.2.3' })

  t.end()
})
