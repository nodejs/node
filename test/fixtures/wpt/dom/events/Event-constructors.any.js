// META: title=Event constructors

test(function() {
  assert_throws_js(
    TypeError,
    () => Event(""),
    "Calling Event constructor without 'new' must throw")
})
test(function() {
  assert_throws_js(TypeError, function() {
    new Event()
  })
})
test(function() {
  var test_error = { name: "test" }
  assert_throws_exactly(test_error, function() {
    new Event({ toString: function() { throw test_error; } })
  })
})
test(function() {
  var ev = new Event("")
  assert_equals(ev.type, "")
  assert_equals(ev.target, null)
  assert_equals(ev.srcElement, null)
  assert_equals(ev.currentTarget, null)
  assert_equals(ev.eventPhase, Event.NONE)
  assert_equals(ev.bubbles, false)
  assert_equals(ev.cancelable, false)
  assert_equals(ev.defaultPrevented, false)
  assert_equals(ev.returnValue, true)
  assert_equals(ev.isTrusted, false)
  assert_true(ev.timeStamp > 0)
  assert_true("initEvent" in ev)
})
test(function() {
  var ev = new Event("test")
  assert_equals(ev.type, "test")
  assert_equals(ev.target, null)
  assert_equals(ev.srcElement, null)
  assert_equals(ev.currentTarget, null)
  assert_equals(ev.eventPhase, Event.NONE)
  assert_equals(ev.bubbles, false)
  assert_equals(ev.cancelable, false)
  assert_equals(ev.defaultPrevented, false)
  assert_equals(ev.returnValue, true)
  assert_equals(ev.isTrusted, false)
  assert_true(ev.timeStamp > 0)
  assert_true("initEvent" in ev)
})
test(function() {
  assert_throws_js(TypeError, function() { Event("test") },
                   'Calling Event constructor without "new" must throw');
})
test(function() {
  var ev = new Event("I am an event", { bubbles: true, cancelable: false})
  assert_equals(ev.type, "I am an event")
  assert_equals(ev.bubbles, true)
  assert_equals(ev.cancelable, false)
})
test(function() {
  var ev = new Event("@", { bubblesIGNORED: true, cancelable: true})
  assert_equals(ev.type, "@")
  assert_equals(ev.bubbles, false)
  assert_equals(ev.cancelable, true)
})
test(function() {
  var ev = new Event("@", { "bubbles\0IGNORED": true, cancelable: true})
  assert_equals(ev.type, "@")
  assert_equals(ev.bubbles, false)
  assert_equals(ev.cancelable, true)
})
test(function() {
  var ev = new Event("Xx", { cancelable: true})
  assert_equals(ev.type, "Xx")
  assert_equals(ev.bubbles, false)
  assert_equals(ev.cancelable, true)
})
test(function() {
  var ev = new Event("Xx", {})
  assert_equals(ev.type, "Xx")
  assert_equals(ev.bubbles, false)
  assert_equals(ev.cancelable, false)
})
test(function() {
  var ev = new Event("Xx", {bubbles: true, cancelable: false, sweet: "x"})
  assert_equals(ev.type, "Xx")
  assert_equals(ev.bubbles, true)
  assert_equals(ev.cancelable, false)
  assert_equals(ev.sweet, undefined)
})
test(function() {
  var called = []
  var ev = new Event("Xx", {
    get cancelable() {
      called.push("cancelable")
      return false
    },
    get bubbles() {
      called.push("bubbles")
      return true;
    },
    get sweet() {
      called.push("sweet")
      return "x"
    }
  })
  assert_array_equals(called, ["bubbles", "cancelable"])
  assert_equals(ev.type, "Xx")
  assert_equals(ev.bubbles, true)
  assert_equals(ev.cancelable, false)
  assert_equals(ev.sweet, undefined)
})
test(function() {
  var ev = new CustomEvent("$", {detail: 54, sweet: "x", sweet2: "x", cancelable:true})
  assert_equals(ev.type, "$")
  assert_equals(ev.bubbles, false)
  assert_equals(ev.cancelable, true)
  assert_equals(ev.sweet, undefined)
  assert_equals(ev.detail, 54)
})
