function _class_private_field_destructure(receiver, privateMap) {
    var descriptor = _class_extract_field_descriptor(receiver, privateMap, "set");
    return _class_apply_descriptor_destructure(receiver, descriptor);
}
