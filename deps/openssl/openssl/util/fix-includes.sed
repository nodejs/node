s|internal/([a-z0-9_]+)_int\.h|crypto/\1.h|g ;
s@internal/(aria.h|async.h|bn_conf.h|bn_dh.h|bn_srp.h|chacha.h|ctype.h|__DECC_INCLUDE_EPILOGUE.H|__DECC_INCLUDE_PROLOGUE.H|dso_conf.h|engine.h|lhash.h|md32_common.h|objects.h|poly1305.h|sha.h|siphash.h|sm2err.h|sm2.h|sm3.h|sm4.h|store.h|foobar)@crypto/\1@g ;
s/constant_time_locl/constant_time/g ;
s/_lo?cl\.h/_local.h/g ;
s/_int\.h/_local.h/g ;
