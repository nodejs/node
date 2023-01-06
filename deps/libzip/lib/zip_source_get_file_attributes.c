/*
  zip_source_get_file_attributes.c -- get attributes for file from source
  Copyright (C) 2020 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "zipint.h"

ZIP_EXTERN void
zip_file_attributes_init(zip_file_attributes_t *attributes) {
    attributes->valid = 0;
    attributes->version = 1;
}

int
zip_source_get_file_attributes(zip_source_t *src, zip_file_attributes_t *attributes) {
    if (src->source_closed) {
        return -1;
    }
    if (attributes == NULL) {
        zip_error_set(&src->error, ZIP_ER_INVAL, 0);
        return -1;
    }

    zip_file_attributes_init(attributes);

    if (src->supports & ZIP_SOURCE_MAKE_COMMAND_BITMASK(ZIP_SOURCE_GET_FILE_ATTRIBUTES)) {
        if (_zip_source_call(src, attributes, sizeof(*attributes), ZIP_SOURCE_GET_FILE_ATTRIBUTES) < 0) {
            return -1;
        }
    }

    if (ZIP_SOURCE_IS_LAYERED(src)) {
        zip_file_attributes_t lower_attributes;

        zip_file_attributes_init(&lower_attributes);

        if (zip_source_get_file_attributes(src->src, &lower_attributes) < 0) {
            zip_error_set_from_source(&src->error, src->src);
            return -1;
        }

        if ((lower_attributes.valid & ZIP_FILE_ATTRIBUTES_HOST_SYSTEM) && (attributes->valid & ZIP_FILE_ATTRIBUTES_HOST_SYSTEM) == 0) {
            attributes->host_system = lower_attributes.host_system;
            attributes->valid |= ZIP_FILE_ATTRIBUTES_HOST_SYSTEM;
        }
        if ((lower_attributes.valid & ZIP_FILE_ATTRIBUTES_ASCII) && (attributes->valid & ZIP_FILE_ATTRIBUTES_ASCII) == 0) {
            attributes->ascii = lower_attributes.ascii;
            attributes->valid |= ZIP_FILE_ATTRIBUTES_ASCII;
        }
        if ((lower_attributes.valid & ZIP_FILE_ATTRIBUTES_VERSION_NEEDED)) {
            if (attributes->valid & ZIP_FILE_ATTRIBUTES_VERSION_NEEDED) {
                attributes->version_needed = ZIP_MAX(lower_attributes.version_needed, attributes->version_needed);
            }
            else {
                attributes->version_needed = lower_attributes.version_needed;
                attributes->valid |= ZIP_FILE_ATTRIBUTES_VERSION_NEEDED;
            }
        }
        if ((lower_attributes.valid & ZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES) && (attributes->valid & ZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES) == 0) {
            attributes->external_file_attributes = lower_attributes.external_file_attributes;
            attributes->valid |= ZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES;
        }
        if ((lower_attributes.valid & ZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS)) {
            if (attributes->valid & ZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS) {
		/* only take from lower level what is not defined at current level */
		lower_attributes.general_purpose_bit_mask &= ~attributes->general_purpose_bit_mask;

                attributes->general_purpose_bit_flags |= lower_attributes.general_purpose_bit_flags & lower_attributes.general_purpose_bit_mask;
                attributes->general_purpose_bit_mask |= lower_attributes.general_purpose_bit_mask;
            }
            else {
                attributes->valid |= ZIP_FILE_ATTRIBUTES_GENERAL_PURPOSE_BIT_FLAGS;
                attributes->general_purpose_bit_flags = lower_attributes.general_purpose_bit_flags;
                attributes->general_purpose_bit_mask = lower_attributes.general_purpose_bit_mask;
            }
        }
    }

    return 0;
}
