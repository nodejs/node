/*
 zip_progress.c -- progress reporting
 Copyright (C) 2017-2020 Dieter Baron and Thomas Klausner

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


#include <stdlib.h>


#define _ZIP_COMPILING_DEPRECATED
#include "zipint.h"

struct zip_progress {
    zip_t *za;

    zip_progress_callback callback_progress;
    void (*ud_progress_free)(void *);
    void *ud_progress;

    zip_cancel_callback callback_cancel;
    void (*ud_cancel_free)(void *);
    void *ud_cancel;

    double precision;

    /* state */
    double last_update; /* last value callback function was called with */

    double start; /* start of sub-progress section */
    double end;   /* end of sub-progress section */
};

static void _zip_progress_free_cancel_callback(zip_progress_t *progress);
static void _zip_progress_free_progress_callback(zip_progress_t *progress);
static zip_progress_t *_zip_progress_new(zip_t *za);
static void _zip_progress_set_cancel_callback(zip_progress_t *progress, zip_cancel_callback callback, void (*ud_free)(void *), void *ud);
static void _zip_progress_set_progress_callback(zip_progress_t *progress, double precision, zip_progress_callback callback, void (*ud_free)(void *), void *ud);

void
_zip_progress_end(zip_progress_t *progress) {
    _zip_progress_update(progress, 1.0);
}


void
_zip_progress_free(zip_progress_t *progress) {
    if (progress == NULL) {
        return;
    }

    _zip_progress_free_progress_callback(progress);
    _zip_progress_free_cancel_callback(progress);

    free(progress);
}


static zip_progress_t *
_zip_progress_new(zip_t *za) {
    zip_progress_t *progress = (zip_progress_t *)malloc(sizeof(*progress));

    if (progress == NULL) {
        zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
        return NULL;
    }

    progress->za = za;

    progress->callback_progress = NULL;
    progress->ud_progress_free = NULL;
    progress->ud_progress = NULL;
    progress->precision = 0.0;

    progress->callback_cancel = NULL;
    progress->ud_cancel_free = NULL;
    progress->ud_cancel = NULL;

    return progress;
}

static void
_zip_progress_free_progress_callback(zip_progress_t *progress) {
    if (progress->ud_progress_free) {
        progress->ud_progress_free(progress->ud_progress);
    }

    progress->callback_progress = NULL;
    progress->ud_progress = NULL;
    progress->ud_progress_free = NULL;
}

static void
_zip_progress_free_cancel_callback(zip_progress_t *progress) {
    if (progress->ud_cancel_free) {
        progress->ud_cancel_free(progress->ud_cancel);
    }

    progress->callback_cancel = NULL;
    progress->ud_cancel = NULL;
    progress->ud_cancel_free = NULL;
}

static void
_zip_progress_set_progress_callback(zip_progress_t *progress, double precision, zip_progress_callback callback, void (*ud_free)(void *), void *ud) {
    _zip_progress_free_progress_callback(progress);

    progress->callback_progress = callback;
    progress->ud_progress_free = ud_free;
    progress->ud_progress = ud;
    progress->precision = precision;
}

void
_zip_progress_set_cancel_callback(zip_progress_t *progress, zip_cancel_callback callback, void (*ud_free)(void *), void *ud) {
    _zip_progress_free_cancel_callback(progress);

    progress->callback_cancel = callback;
    progress->ud_cancel_free = ud_free;
    progress->ud_cancel = ud;
}

int
_zip_progress_start(zip_progress_t *progress) {
    if (progress == NULL) {
        return 0;
    }

    if (progress->callback_progress != NULL) {
        progress->last_update = 0.0;
        progress->callback_progress(progress->za, 0.0, progress->ud_progress);
    }

    if (progress->callback_cancel != NULL) {
        if (progress->callback_cancel(progress->za, progress->ud_cancel)) {
            return -1;
        }
    }

    return 0;
}


int
_zip_progress_subrange(zip_progress_t *progress, double start, double end) {
    if (progress == NULL) {
        return 0;
    }

    progress->start = start;
    progress->end = end;

    return _zip_progress_update(progress, 0.0);
}

int
_zip_progress_update(zip_progress_t *progress, double sub_current) {
    double current;

    if (progress == NULL) {
        return 0;
    }

    if (progress->callback_progress != NULL) {
        current = ZIP_MIN(ZIP_MAX(sub_current, 0.0), 1.0) * (progress->end - progress->start) + progress->start;

        if (current - progress->last_update > progress->precision) {
            progress->callback_progress(progress->za, current, progress->ud_progress);
            progress->last_update = current;
        }
    }

    if (progress->callback_cancel != NULL) {
        if (progress->callback_cancel(progress->za, progress->ud_cancel)) {
            return -1;
        }
    }

    return 0;
}


ZIP_EXTERN int
zip_register_progress_callback_with_state(zip_t *za, double precision, zip_progress_callback callback, void (*ud_free)(void *), void *ud) {
    if (callback != NULL) {
        if (za->progress == NULL) {
            if ((za->progress = _zip_progress_new(za)) == NULL) {
                return -1;
            }
        }

        _zip_progress_set_progress_callback(za->progress, precision, callback, ud_free, ud);
    }
    else {
        if (za->progress != NULL) {
            if (za->progress->callback_cancel == NULL) {
                _zip_progress_free(za->progress);
                za->progress = NULL;
            }
            else {
                _zip_progress_free_progress_callback(za->progress);
            }
        }
    }

    return 0;
}


ZIP_EXTERN int
zip_register_cancel_callback_with_state(zip_t *za, zip_cancel_callback callback, void (*ud_free)(void *), void *ud) {
    if (callback != NULL) {
        if (za->progress == NULL) {
            if ((za->progress = _zip_progress_new(za)) == NULL) {
                return -1;
            }
        }

        _zip_progress_set_cancel_callback(za->progress, callback, ud_free, ud);
    }
    else {
        if (za->progress != NULL) {
            if (za->progress->callback_progress == NULL) {
                _zip_progress_free(za->progress);
                za->progress = NULL;
            }
            else {
                _zip_progress_free_cancel_callback(za->progress);
            }
        }
    }

    return 0;
}


struct legacy_ud {
    zip_progress_callback_t callback;
};


static void
_zip_legacy_progress_callback(zip_t *za, double progress, void *vud) {
    struct legacy_ud *ud = (struct legacy_ud *)vud;

    ud->callback(progress);
}

ZIP_EXTERN void
zip_register_progress_callback(zip_t *za, zip_progress_callback_t progress_callback) {
    struct legacy_ud *ud;

    if (progress_callback == NULL) {
        zip_register_progress_callback_with_state(za, 0, NULL, NULL, NULL);
    }

    if ((ud = (struct legacy_ud *)malloc(sizeof(*ud))) == NULL) {
        return;
    }

    ud->callback = progress_callback;

    if (zip_register_progress_callback_with_state(za, 0.001, _zip_legacy_progress_callback, free, ud) < 0) {
        free(ud);
    }
}
