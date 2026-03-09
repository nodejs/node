function _sliced_to_array_loose(arr, i) {
    return _array_with_holes(arr) || _iterable_to_array_limit_loose(arr, i) || _unsupported_iterable_to_array(arr, i)
        || _non_iterable_rest();
}
