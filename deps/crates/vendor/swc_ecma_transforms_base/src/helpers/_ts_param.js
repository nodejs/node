function _ts_param(paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
}