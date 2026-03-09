function _call_super(_this, derived, args) {
    // Super
    derived = _get_prototype_of(derived);
    return _possible_constructor_return(
        _this,
        _is_native_reflect_construct()
            // NOTE: This doesn't work if this.__proto__.constructor has been modified.
            ? Reflect.construct(derived, args || [], _get_prototype_of(_this).constructor)
            : derived.apply(_this, args)
    );
}
