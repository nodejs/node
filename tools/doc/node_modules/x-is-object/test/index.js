var test = require("tape")

var isObject = require("../index")

test("isObject is a function", function (assert) {
    assert.equal(typeof isObject, "function")
    assert.end()
})

test("object literal is true", function (assert) {
    assert.equal(isObject({}), true)
    assert.end()
})

test("array literal is true", function (assert) {
    assert.equal(isObject([]), true)
    assert.end()
})

test("regex literal is true", function (assert) {
    assert.equal(isObject(/.*/), true)
    assert.end()
})

test("regex object is true", function (assert) {
    assert.equal(isObject(new RegExp(".*")), true)
    assert.end()
})

test("function is true", function (assert) {
    assert.equal(isObject(function () {}), true)
    assert.end()
})


test("date is true", function (assert) {
    assert.equal(isObject(new Date()), true)
    assert.end()
})


test("string object is true", function (assert) {
    assert.equal(isObject(new String("hello")), true)
    assert.end()
})

test("string literal is false", function (assert) {
    assert.equal(isObject("hello"), false)
    assert.end()
})

test("empty string is false", function (assert) {
    assert.equal(isObject(""), false)
    assert.end()
})

test("number is false", function (assert) {
    assert.equal(isObject(9), false)
    assert.end()
})

test("boolean is false", function (assert) {
    assert.equal(isObject(true), false)
    assert.end()
})

test("null is false", function (assert) {
    assert.equal(isObject(null), false)
    assert.end()
})

test("undefined is false", function (assert) {
    assert.equal(isObject(undefined), false)
    assert.end()
})
