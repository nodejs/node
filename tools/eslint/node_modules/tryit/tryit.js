// tryit
// Simple, re-usuable try-catch, this is a performance optimization
// and provides a cleaner API.
module.exports = function (fn, cb) {
    var err;

    try {
        fn();
    } catch (e) {
        err = e;
    }

    if (cb) cb(err || null);
};
