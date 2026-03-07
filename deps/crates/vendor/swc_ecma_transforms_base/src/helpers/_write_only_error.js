function _write_only_error(name) {
    throw new TypeError("\"" + name + "\" is write-only");
}
