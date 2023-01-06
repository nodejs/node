#include "diff_output.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compat.h"

static void ensure_header(diff_output_t *output) {
    if (output->archive_names[0] != NULL) {
        printf("--- %s\n", output->archive_names[0]);
        printf("+++ %s\n", output->archive_names[1]);
        output->archive_names[0] = NULL;
        output->archive_names[1] = NULL;
    }
}

void diff_output_init(diff_output_t *output, int verbose, char *const archive_names[]) {
    output->archive_names[0] = archive_names[0];
    output->archive_names[1] = archive_names[1];
    output->verbose = verbose;
    output->file_name = NULL;
    output->file_size = 0;
    output->file_crc = 0;
}

void diff_output_start_file(diff_output_t *output, const char *name, zip_uint64_t size, zip_uint32_t crc) {
    output->file_name = name;
    output->file_size = size;
    output->file_crc = crc;
}

void diff_output_end_file(diff_output_t *output) {
    output->file_name = NULL;
}

void diff_output(diff_output_t *output, int side, const char *fmt, ...) {
    va_list ap;

    if (!output->verbose) {
        return;
    }

    ensure_header(output);
    
    if (output->file_name != NULL) {
        diff_output_file(output, ' ', output->file_name, output->file_size, output->file_crc);
        output->file_name = NULL;
    }
    
    printf("%c ", side);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void diff_output_file(diff_output_t *output, char side, const char *name, zip_uint64_t size, zip_uint32_t crc) {
    if (!output->verbose) {
        return;
    }
    
    ensure_header(output);
    
    if (size == 0 && crc == 0 && name[0] != '\0' && name[strlen(name) - 1] == '/') {
        printf("%c directory '%s'\n", side, name);
    }
    else {
        printf("%c file '%s', size %" PRIu64 ", crc %08x\n", side, name, size, crc);
    }
}

#define MAX_BYTES 64
void diff_output_data(diff_output_t *output, int side, const zip_uint8_t *data, zip_uint64_t data_length, const char *fmt, ...) {
    char prefix[1024];
    char hexdata[MAX_BYTES * 3 + 6];
    size_t i, offset;
    va_list ap;

    if (!output->verbose) {
        return;
    }
    
    offset = 0;
    for (i = 0; i < data_length; i++) {
        hexdata[offset++] = (i == 0 ? '<' : ' ');

        if (i >= MAX_BYTES) {
            snprintf(hexdata + offset, sizeof(hexdata) - offset, "...");
            break;
        }
        snprintf(hexdata + offset, sizeof(hexdata) - offset, "%02x", data[i]);
        offset += 2;
    }

    hexdata[offset++] = '>';
    hexdata[offset] = '\0';
    
    va_start(ap, fmt);
    vsnprintf(prefix, sizeof(prefix), fmt, ap);
    va_end(ap);
    prefix[sizeof(prefix) - 1] = '\0';
    
    diff_output(output, side, "%s, length %" PRIu64 ", data %s", prefix, data_length, hexdata);
}
