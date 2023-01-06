#include "zipint.h"

zip_encryption_implementation
_zip_get_encryption_implementation(zip_uint16_t em, int operation) {
    return NULL;
}

zip_source_t *
zip_source_file_create(const char *fname, zip_uint64_t start, zip_int64_t length, zip_error_t *error) {
    zip_error_set(error, ZIP_ER_OPNOTSUPP, 0);
    return NULL;
}
