function _update(target, property, receiver, isStrict) {
    return {
        get _() {
            return _get(target, property, receiver);
        },
        set _(value) {
            _set(target, property, value, receiver, isStrict);
        }
    };
}
