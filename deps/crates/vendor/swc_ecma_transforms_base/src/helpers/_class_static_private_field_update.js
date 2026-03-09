function _class_static_private_field_update(receiver, classConstructor, descriptor) {
    _class_check_private_static_access(receiver, classConstructor);
    _class_check_private_static_field_descriptor(descriptor, "update");
    return _class_apply_descriptor_update(receiver, descriptor);
}
