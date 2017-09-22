module.exports = isObject

function isObject(x) {
    return x === Object(x)
}
