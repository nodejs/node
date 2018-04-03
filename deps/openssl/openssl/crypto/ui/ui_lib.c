/* crypto/ui/ui_lib.c */
/*
 * Written by Richard Levitte (richard@levitte.org) for the OpenSSL project
 * 2001.
 */
/* ====================================================================
 * Copyright (c) 2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <string.h>
#include "cryptlib.h"
#include <openssl/e_os2.h>
#include <openssl/buffer.h>
#include <openssl/ui.h>
#include <openssl/err.h>
#include "ui_locl.h"

IMPLEMENT_STACK_OF(UI_STRING_ST)

static const UI_METHOD *default_UI_meth = NULL;

UI *UI_new(void)
{
    return (UI_new_method(NULL));
}

UI *UI_new_method(const UI_METHOD *method)
{
    UI *ret;

    ret = (UI *)OPENSSL_malloc(sizeof(UI));
    if (ret == NULL) {
        UIerr(UI_F_UI_NEW_METHOD, ERR_R_MALLOC_FAILURE);
        return NULL;
    }
    if (method == NULL)
        ret->meth = UI_get_default_method();
    else
        ret->meth = method;

    ret->strings = NULL;
    ret->user_data = NULL;
    ret->flags = 0;
    CRYPTO_new_ex_data(CRYPTO_EX_INDEX_UI, ret, &ret->ex_data);
    return ret;
}

static void free_string(UI_STRING *uis)
{
    if (uis->flags & OUT_STRING_FREEABLE) {
        OPENSSL_free((char *)uis->out_string);
        switch (uis->type) {
        case UIT_BOOLEAN:
            OPENSSL_free((char *)uis->_.boolean_data.action_desc);
            OPENSSL_free((char *)uis->_.boolean_data.ok_chars);
            OPENSSL_free((char *)uis->_.boolean_data.cancel_chars);
            break;
        default:
            break;
        }
    }
    OPENSSL_free(uis);
}

void UI_free(UI *ui)
{
    if (ui == NULL)
        return;
    sk_UI_STRING_pop_free(ui->strings, free_string);
    CRYPTO_free_ex_data(CRYPTO_EX_INDEX_UI, ui, &ui->ex_data);
    OPENSSL_free(ui);
}

static int allocate_string_stack(UI *ui)
{
    if (ui->strings == NULL) {
        ui->strings = sk_UI_STRING_new_null();
        if (ui->strings == NULL) {
            return -1;
        }
    }
    return 0;
}

static UI_STRING *general_allocate_prompt(UI *ui, const char *prompt,
                                          int prompt_freeable,
                                          enum UI_string_types type,
                                          int input_flags, char *result_buf)
{
    UI_STRING *ret = NULL;

    if (prompt == NULL) {
        UIerr(UI_F_GENERAL_ALLOCATE_PROMPT, ERR_R_PASSED_NULL_PARAMETER);
    } else if ((type == UIT_PROMPT || type == UIT_VERIFY
                || type == UIT_BOOLEAN) && result_buf == NULL) {
        UIerr(UI_F_GENERAL_ALLOCATE_PROMPT, UI_R_NO_RESULT_BUFFER);
    } else if ((ret = (UI_STRING *)OPENSSL_malloc(sizeof(UI_STRING)))) {
        ret->out_string = prompt;
        ret->flags = prompt_freeable ? OUT_STRING_FREEABLE : 0;
        ret->input_flags = input_flags;
        ret->type = type;
        ret->result_buf = result_buf;
    }
    return ret;
}

static int general_allocate_string(UI *ui, const char *prompt,
                                   int prompt_freeable,
                                   enum UI_string_types type, int input_flags,
                                   char *result_buf, int minsize, int maxsize,
                                   const char *test_buf)
{
    int ret = -1;
    UI_STRING *s = general_allocate_prompt(ui, prompt, prompt_freeable,
                                           type, input_flags, result_buf);

    if (s != NULL) {
        if (allocate_string_stack(ui) >= 0) {
            s->_.string_data.result_minsize = minsize;
            s->_.string_data.result_maxsize = maxsize;
            s->_.string_data.test_buf = test_buf;
            ret = sk_UI_STRING_push(ui->strings, s);
            /* sk_push() returns 0 on error.  Let's addapt that */
            if (ret <= 0)
                ret--;
        } else
            free_string(s);
    }
    return ret;
}

static int general_allocate_boolean(UI *ui,
                                    const char *prompt,
                                    const char *action_desc,
                                    const char *ok_chars,
                                    const char *cancel_chars,
                                    int prompt_freeable,
                                    enum UI_string_types type,
                                    int input_flags, char *result_buf)
{
    int ret = -1;
    UI_STRING *s;
    const char *p;

    if (ok_chars == NULL) {
        UIerr(UI_F_GENERAL_ALLOCATE_BOOLEAN, ERR_R_PASSED_NULL_PARAMETER);
    } else if (cancel_chars == NULL) {
        UIerr(UI_F_GENERAL_ALLOCATE_BOOLEAN, ERR_R_PASSED_NULL_PARAMETER);
    } else {
        for (p = ok_chars; *p != '\0'; p++) {
            if (strchr(cancel_chars, *p) != NULL) {
                UIerr(UI_F_GENERAL_ALLOCATE_BOOLEAN,
                      UI_R_COMMON_OK_AND_CANCEL_CHARACTERS);
            }
        }

        s = general_allocate_prompt(ui, prompt, prompt_freeable,
                                    type, input_flags, result_buf);

        if (s != NULL) {
            if (allocate_string_stack(ui) >= 0) {
                s->_.boolean_data.action_desc = action_desc;
                s->_.boolean_data.ok_chars = ok_chars;
                s->_.boolean_data.cancel_chars = cancel_chars;
                ret = sk_UI_STRING_push(ui->strings, s);
                /*
                 * sk_push() returns 0 on error. Let's addapt that
                 */
                if (ret <= 0)
                    ret--;
            } else
                free_string(s);
        }
    }
    return ret;
}

/*
 * Returns the index to the place in the stack or -1 for error.  Uses a
 * direct reference to the prompt.
 */
int UI_add_input_string(UI *ui, const char *prompt, int flags,
                        char *result_buf, int minsize, int maxsize)
{
    return general_allocate_string(ui, prompt, 0,
                                   UIT_PROMPT, flags, result_buf, minsize,
                                   maxsize, NULL);
}

/* Same as UI_add_input_string(), excepts it takes a copy of the prompt */
int UI_dup_input_string(UI *ui, const char *prompt, int flags,
                        char *result_buf, int minsize, int maxsize)
{
    char *prompt_copy = NULL;

    if (prompt != NULL) {
        prompt_copy = BUF_strdup(prompt);
        if (prompt_copy == NULL) {
            UIerr(UI_F_UI_DUP_INPUT_STRING, ERR_R_MALLOC_FAILURE);
            return 0;
        }
    }

    return general_allocate_string(ui, prompt_copy, 1,
                                   UIT_PROMPT, flags, result_buf, minsize,
                                   maxsize, NULL);
}

int UI_add_verify_string(UI *ui, const char *prompt, int flags,
                         char *result_buf, int minsize, int maxsize,
                         const char *test_buf)
{
    return general_allocate_string(ui, prompt, 0,
                                   UIT_VERIFY, flags, result_buf, minsize,
                                   maxsize, test_buf);
}

int UI_dup_verify_string(UI *ui, const char *prompt, int flags,
                         char *result_buf, int minsize, int maxsize,
                         const char *test_buf)
{
    char *prompt_copy = NULL;

    if (prompt != NULL) {
        prompt_copy = BUF_strdup(prompt);
        if (prompt_copy == NULL) {
            UIerr(UI_F_UI_DUP_VERIFY_STRING, ERR_R_MALLOC_FAILURE);
            return -1;
        }
    }

    return general_allocate_string(ui, prompt_copy, 1,
                                   UIT_VERIFY, flags, result_buf, minsize,
                                   maxsize, test_buf);
}

int UI_add_input_boolean(UI *ui, const char *prompt, const char *action_desc,
                         const char *ok_chars, const char *cancel_chars,
                         int flags, char *result_buf)
{
    return general_allocate_boolean(ui, prompt, action_desc,
                                    ok_chars, cancel_chars, 0, UIT_BOOLEAN,
                                    flags, result_buf);
}

int UI_dup_input_boolean(UI *ui, const char *prompt, const char *action_desc,
                         const char *ok_chars, const char *cancel_chars,
                         int flags, char *result_buf)
{
    char *prompt_copy = NULL;
    char *action_desc_copy = NULL;
    char *ok_chars_copy = NULL;
    char *cancel_chars_copy = NULL;

    if (prompt != NULL) {
        prompt_copy = BUF_strdup(prompt);
        if (prompt_copy == NULL) {
            UIerr(UI_F_UI_DUP_INPUT_BOOLEAN, ERR_R_MALLOC_FAILURE);
            goto err;
        }
    }

    if (action_desc != NULL) {
        action_desc_copy = BUF_strdup(action_desc);
        if (action_desc_copy == NULL) {
            UIerr(UI_F_UI_DUP_INPUT_BOOLEAN, ERR_R_MALLOC_FAILURE);
            goto err;
        }
    }

    if (ok_chars != NULL) {
        ok_chars_copy = BUF_strdup(ok_chars);
        if (ok_chars_copy == NULL) {
            UIerr(UI_F_UI_DUP_INPUT_BOOLEAN, ERR_R_MALLOC_FAILURE);
            goto err;
        }
    }

    if (cancel_chars != NULL) {
        cancel_chars_copy = BUF_strdup(cancel_chars);
        if (cancel_chars_copy == NULL) {
            UIerr(UI_F_UI_DUP_INPUT_BOOLEAN, ERR_R_MALLOC_FAILURE);
            goto err;
        }
    }

    return general_allocate_boolean(ui, prompt_copy, action_desc_copy,
                                    ok_chars_copy, cancel_chars_copy, 1,
                                    UIT_BOOLEAN, flags, result_buf);
 err:
    if (prompt_copy)
        OPENSSL_free(prompt_copy);
    if (action_desc_copy)
        OPENSSL_free(action_desc_copy);
    if (ok_chars_copy)
        OPENSSL_free(ok_chars_copy);
    if (cancel_chars_copy)
        OPENSSL_free(cancel_chars_copy);
    return -1;
}

int UI_add_info_string(UI *ui, const char *text)
{
    return general_allocate_string(ui, text, 0, UIT_INFO, 0, NULL, 0, 0,
                                   NULL);
}

int UI_dup_info_string(UI *ui, const char *text)
{
    char *text_copy = NULL;

    if (text != NULL) {
        text_copy = BUF_strdup(text);
        if (text_copy == NULL) {
            UIerr(UI_F_UI_DUP_INFO_STRING, ERR_R_MALLOC_FAILURE);
            return -1;
        }
    }

    return general_allocate_string(ui, text_copy, 1, UIT_INFO, 0, NULL,
                                   0, 0, NULL);
}

int UI_add_error_string(UI *ui, const char *text)
{
    return general_allocate_string(ui, text, 0, UIT_ERROR, 0, NULL, 0, 0,
                                   NULL);
}

int UI_dup_error_string(UI *ui, const char *text)
{
    char *text_copy = NULL;

    if (text != NULL) {
        text_copy = BUF_strdup(text);
        if (text_copy == NULL) {
            UIerr(UI_F_UI_DUP_ERROR_STRING, ERR_R_MALLOC_FAILURE);
            return -1;
        }
    }
    return general_allocate_string(ui, text_copy, 1, UIT_ERROR, 0, NULL,
                                   0, 0, NULL);
}

char *UI_construct_prompt(UI *ui, const char *object_desc,
                          const char *object_name)
{
    char *prompt = NULL;

    if (ui->meth->ui_construct_prompt != NULL)
        prompt = ui->meth->ui_construct_prompt(ui, object_desc, object_name);
    else {
        char prompt1[] = "Enter ";
        char prompt2[] = " for ";
        char prompt3[] = ":";
        int len = 0;

        if (object_desc == NULL)
            return NULL;
        len = sizeof(prompt1) - 1 + strlen(object_desc);
        if (object_name != NULL)
            len += sizeof(prompt2) - 1 + strlen(object_name);
        len += sizeof(prompt3) - 1;

        prompt = (char *)OPENSSL_malloc(len + 1);
        if (prompt == NULL)
            return NULL;
        BUF_strlcpy(prompt, prompt1, len + 1);
        BUF_strlcat(prompt, object_desc, len + 1);
        if (object_name != NULL) {
            BUF_strlcat(prompt, prompt2, len + 1);
            BUF_strlcat(prompt, object_name, len + 1);
        }
        BUF_strlcat(prompt, prompt3, len + 1);
    }
    return prompt;
}

void *UI_add_user_data(UI *ui, void *user_data)
{
    void *old_data = ui->user_data;
    ui->user_data = user_data;
    return old_data;
}

void *UI_get0_user_data(UI *ui)
{
    return ui->user_data;
}

const char *UI_get0_result(UI *ui, int i)
{
    if (i < 0) {
        UIerr(UI_F_UI_GET0_RESULT, UI_R_INDEX_TOO_SMALL);
        return NULL;
    }
    if (i >= sk_UI_STRING_num(ui->strings)) {
        UIerr(UI_F_UI_GET0_RESULT, UI_R_INDEX_TOO_LARGE);
        return NULL;
    }
    return UI_get0_result_string(sk_UI_STRING_value(ui->strings, i));
}

static int print_error(const char *str, size_t len, UI *ui)
{
    UI_STRING uis;

    memset(&uis, 0, sizeof(uis));
    uis.type = UIT_ERROR;
    uis.out_string = str;

    if (ui->meth->ui_write_string != NULL
        && ui->meth->ui_write_string(ui, &uis) <= 0)
        return -1;
    return 0;
}

int UI_process(UI *ui)
{
    int i, ok = 0;

    if (ui->meth->ui_open_session != NULL
        && ui->meth->ui_open_session(ui) <= 0) {
        ok = -1;
        goto err;
    }

    if (ui->flags & UI_FLAG_PRINT_ERRORS)
        ERR_print_errors_cb((int (*)(const char *, size_t, void *))
                            print_error, (void *)ui);

    for (i = 0; i < sk_UI_STRING_num(ui->strings); i++) {
        if (ui->meth->ui_write_string != NULL
            && (ui->meth->ui_write_string(ui,
                                          sk_UI_STRING_value(ui->strings, i))
                <= 0))
        {
            ok = -1;
            goto err;
        }
    }

    if (ui->meth->ui_flush != NULL)
        switch (ui->meth->ui_flush(ui)) {
        case -1:               /* Interrupt/Cancel/something... */
            ok = -2;
            goto err;
        case 0:                /* Errors */
            ok = -1;
            goto err;
        default:               /* Success */
            ok = 0;
            break;
        }

    for (i = 0; i < sk_UI_STRING_num(ui->strings); i++) {
        if (ui->meth->ui_read_string != NULL) {
            switch (ui->meth->ui_read_string(ui,
                                             sk_UI_STRING_value(ui->strings,
                                                                i))) {
            case -1:           /* Interrupt/Cancel/something... */
                ok = -2;
                goto err;
            case 0:            /* Errors */
                ok = -1;
                goto err;
            default:           /* Success */
                ok = 0;
                break;
            }
        }
    }

 err:
    if (ui->meth->ui_close_session != NULL
        && ui->meth->ui_close_session(ui) <= 0)
        return -1;
    return ok;
}

int UI_ctrl(UI *ui, int cmd, long i, void *p, void (*f) (void))
{
    if (ui == NULL) {
        UIerr(UI_F_UI_CTRL, ERR_R_PASSED_NULL_PARAMETER);
        return -1;
    }
    switch (cmd) {
    case UI_CTRL_PRINT_ERRORS:
        {
            int save_flag = ! !(ui->flags & UI_FLAG_PRINT_ERRORS);
            if (i)
                ui->flags |= UI_FLAG_PRINT_ERRORS;
            else
                ui->flags &= ~UI_FLAG_PRINT_ERRORS;
            return save_flag;
        }
    case UI_CTRL_IS_REDOABLE:
        return ! !(ui->flags & UI_FLAG_REDOABLE);
    default:
        break;
    }
    UIerr(UI_F_UI_CTRL, UI_R_UNKNOWN_CONTROL_COMMAND);
    return -1;
}

int UI_get_ex_new_index(long argl, void *argp, CRYPTO_EX_new *new_func,
                        CRYPTO_EX_dup *dup_func, CRYPTO_EX_free *free_func)
{
    return CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_UI, argl, argp,
                                   new_func, dup_func, free_func);
}

int UI_set_ex_data(UI *r, int idx, void *arg)
{
    return (CRYPTO_set_ex_data(&r->ex_data, idx, arg));
}

void *UI_get_ex_data(UI *r, int idx)
{
    return (CRYPTO_get_ex_data(&r->ex_data, idx));
}

void UI_set_default_method(const UI_METHOD *meth)
{
    default_UI_meth = meth;
}

const UI_METHOD *UI_get_default_method(void)
{
    if (default_UI_meth == NULL) {
        default_UI_meth = UI_OpenSSL();
    }
    return default_UI_meth;
}

const UI_METHOD *UI_get_method(UI *ui)
{
    return ui->meth;
}

const UI_METHOD *UI_set_method(UI *ui, const UI_METHOD *meth)
{
    ui->meth = meth;
    return ui->meth;
}

UI_METHOD *UI_create_method(char *name)
{
    UI_METHOD *ui_method = (UI_METHOD *)OPENSSL_malloc(sizeof(UI_METHOD));

    if (ui_method) {
        memset(ui_method, 0, sizeof(*ui_method));
        ui_method->name = BUF_strdup(name);
    }
    return ui_method;
}

/*
 * BIG FSCKING WARNING!!!! If you use this on a statically allocated method
 * (that is, it hasn't been allocated using UI_create_method(), you deserve
 * anything Murphy can throw at you and more! You have been warned.
 */
void UI_destroy_method(UI_METHOD *ui_method)
{
    OPENSSL_free(ui_method->name);
    ui_method->name = NULL;
    OPENSSL_free(ui_method);
}

int UI_method_set_opener(UI_METHOD *method, int (*opener) (UI *ui))
{
    if (method != NULL) {
        method->ui_open_session = opener;
        return 0;
    }
    return -1;
}

int UI_method_set_writer(UI_METHOD *method,
                         int (*writer) (UI *ui, UI_STRING *uis))
{
    if (method != NULL) {
        method->ui_write_string = writer;
        return 0;
    }
    return -1;
}

int UI_method_set_flusher(UI_METHOD *method, int (*flusher) (UI *ui))
{
    if (method != NULL) {
        method->ui_flush = flusher;
        return 0;
    }
    return -1;
}

int UI_method_set_reader(UI_METHOD *method,
                         int (*reader) (UI *ui, UI_STRING *uis))
{
    if (method != NULL) {
        method->ui_read_string = reader;
        return 0;
    }
    return -1;
}

int UI_method_set_closer(UI_METHOD *method, int (*closer) (UI *ui))
{
    if (method != NULL) {
        method->ui_close_session = closer;
        return 0;
    }
    return -1;
}

int UI_method_set_prompt_constructor(UI_METHOD *method,
                                     char *(*prompt_constructor) (UI *ui,
                                                                  const char
                                                                  *object_desc,
                                                                  const char
                                                                  *object_name))
{
    if (method != NULL) {
        method->ui_construct_prompt = prompt_constructor;
        return 0;
    }
    return -1;
}

int (*UI_method_get_opener(UI_METHOD *method)) (UI *)
{
    if (method != NULL)
        return method->ui_open_session;
    return NULL;
}

int (*UI_method_get_writer(UI_METHOD *method)) (UI *, UI_STRING *)
{
    if (method != NULL)
        return method->ui_write_string;
    return NULL;
}

int (*UI_method_get_flusher(UI_METHOD *method)) (UI *)
{
    if (method != NULL)
        return method->ui_flush;
    return NULL;
}

int (*UI_method_get_reader(UI_METHOD *method)) (UI *, UI_STRING *)
{
    if (method != NULL)
        return method->ui_read_string;
    return NULL;
}

int (*UI_method_get_closer(UI_METHOD *method)) (UI *)
{
    if (method != NULL)
        return method->ui_close_session;
    return NULL;
}

char *(*UI_method_get_prompt_constructor(UI_METHOD *method)) (UI *,
                                                              const char *,
                                                              const char *)
{
    if (method != NULL)
        return method->ui_construct_prompt;
    return NULL;
}

enum UI_string_types UI_get_string_type(UI_STRING *uis)
{
    if (!uis)
        return UIT_NONE;
    return uis->type;
}

int UI_get_input_flags(UI_STRING *uis)
{
    if (!uis)
        return 0;
    return uis->input_flags;
}

const char *UI_get0_output_string(UI_STRING *uis)
{
    if (!uis)
        return NULL;
    return uis->out_string;
}

const char *UI_get0_action_string(UI_STRING *uis)
{
    if (!uis)
        return NULL;
    switch (uis->type) {
    case UIT_BOOLEAN:
        return uis->_.boolean_data.action_desc;
    default:
        return NULL;
    }
}

const char *UI_get0_result_string(UI_STRING *uis)
{
    if (!uis)
        return NULL;
    switch (uis->type) {
    case UIT_PROMPT:
    case UIT_VERIFY:
        return uis->result_buf;
    default:
        return NULL;
    }
}

const char *UI_get0_test_string(UI_STRING *uis)
{
    if (!uis)
        return NULL;
    switch (uis->type) {
    case UIT_VERIFY:
        return uis->_.string_data.test_buf;
    default:
        return NULL;
    }
}

int UI_get_result_minsize(UI_STRING *uis)
{
    if (!uis)
        return -1;
    switch (uis->type) {
    case UIT_PROMPT:
    case UIT_VERIFY:
        return uis->_.string_data.result_minsize;
    default:
        return -1;
    }
}

int UI_get_result_maxsize(UI_STRING *uis)
{
    if (!uis)
        return -1;
    switch (uis->type) {
    case UIT_PROMPT:
    case UIT_VERIFY:
        return uis->_.string_data.result_maxsize;
    default:
        return -1;
    }
}

int UI_set_result(UI *ui, UI_STRING *uis, const char *result)
{
    int l = strlen(result);

    ui->flags &= ~UI_FLAG_REDOABLE;

    if (!uis)
        return -1;
    switch (uis->type) {
    case UIT_PROMPT:
    case UIT_VERIFY:
        {
            char number1[DECIMAL_SIZE(uis->_.string_data.result_minsize) + 1];
            char number2[DECIMAL_SIZE(uis->_.string_data.result_maxsize) + 1];

            BIO_snprintf(number1, sizeof(number1), "%d",
                         uis->_.string_data.result_minsize);
            BIO_snprintf(number2, sizeof(number2), "%d",
                         uis->_.string_data.result_maxsize);

            if (l < uis->_.string_data.result_minsize) {
                ui->flags |= UI_FLAG_REDOABLE;
                UIerr(UI_F_UI_SET_RESULT, UI_R_RESULT_TOO_SMALL);
                ERR_add_error_data(5, "You must type in ",
                                   number1, " to ", number2, " characters");
                return -1;
            }
            if (l > uis->_.string_data.result_maxsize) {
                ui->flags |= UI_FLAG_REDOABLE;
                UIerr(UI_F_UI_SET_RESULT, UI_R_RESULT_TOO_LARGE);
                ERR_add_error_data(5, "You must type in ",
                                   number1, " to ", number2, " characters");
                return -1;
            }
        }

        if (!uis->result_buf) {
            UIerr(UI_F_UI_SET_RESULT, UI_R_NO_RESULT_BUFFER);
            return -1;
        }

        BUF_strlcpy(uis->result_buf, result,
                    uis->_.string_data.result_maxsize + 1);
        break;
    case UIT_BOOLEAN:
        {
            const char *p;

            if (!uis->result_buf) {
                UIerr(UI_F_UI_SET_RESULT, UI_R_NO_RESULT_BUFFER);
                return -1;
            }

            uis->result_buf[0] = '\0';
            for (p = result; *p; p++) {
                if (strchr(uis->_.boolean_data.ok_chars, *p)) {
                    uis->result_buf[0] = uis->_.boolean_data.ok_chars[0];
                    break;
                }
                if (strchr(uis->_.boolean_data.cancel_chars, *p)) {
                    uis->result_buf[0] = uis->_.boolean_data.cancel_chars[0];
                    break;
                }
            }
        }
    default:
        break;
    }
    return 0;
}
