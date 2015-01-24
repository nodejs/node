module.exports = function (xs, fn) {
    var res = [];
    for (var i = 0; i < xs.length; i++) {
        var x = fn(xs[i], i);
        if (Array.isArray(x)) res.push.apply(res, x);
        else res.push(x);
    }
    return res;
};
