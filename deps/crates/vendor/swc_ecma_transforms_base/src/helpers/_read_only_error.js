function _read_only_error(name) {
    throw new TypeError("\"" + name + "\" is read-only");
}
