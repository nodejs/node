#if __has_feature(objc_arc)
#error "ObjC++ files without CLANG_ENABLE_OBJC_ARC should not be ARC'd!"
#endif

void mm_fun() {}
