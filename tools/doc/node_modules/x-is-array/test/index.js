var test = require("tape")

var isArray = require("../index")

test("isArray is a function", function (assert) {
    assert.equal(typeof isArray, "function")
    assert.end()
})

test("array literal is truthy", function (assert) {
    assert.equal(isArray([]), true)
    assert.end()
})

test("string literal is false", function (assert) {
    assert.equal(isArray("hello"), false)
    assert.end()
})

test("empty string is false", function (assert) {
    assert.equal(isArray(""), false)
    assert.end()
})

test("number is falsey", function (assert) {
    assert.equal(isArray(9), false)
    assert.end()
})

test("boolean is falsey", function (assert) {
    assert.equal(isArray(true), false)
    assert.end()
})

test("date is falsey", function (assert) {
    assert.equal(isArray(new Date()), false)
    assert.end()
})

test("object is falsey", function (assert) {
    assert.equal(isArray({}), false)
    assert.end()
})
test("null is falsey", function (assert) {
    assert.equal(isArray(null), false)
    assert.end()
})
test("undefined is falsey", function (assert) {
    assert.equal(isArray(undefined), false)
    assert.end()
})
