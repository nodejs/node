/**
 * @file merve_c.h
 * @brief Includes the C definitions for merve. This is a C file, not C++.
 */
#ifndef MERVE_C_H
#define MERVE_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Non-owning string reference.
 *
 * The data pointer is NOT null-terminated. Always use the length field.
 *
 * The data is valid as long as:
 * - The merve_analysis handle that produced it has not been freed.
 * - For string_view-backed exports: the original source buffer is alive.
 */
typedef struct {
  const char* data;
  size_t length;
} merve_string;

/**
 * @brief Opaque handle to a CommonJS parse result.
 *
 * Created by merve_parse_commonjs(). Must be freed with merve_free().
 */
typedef void* merve_analysis;

/**
 * @brief Version number components.
 */
typedef struct {
  int major;
  int minor;
  int revision;
} merve_version_components;

/**
 * @brief Source location for a parse error.
 *
 * - line and column are 1-based.
 * - column is byte-oriented.
 *
 * A zeroed location (`{0, 0}`) means the location is unavailable.
 */
typedef struct {
  uint32_t line;
  uint32_t column;
} merve_error_loc;

/* Error codes corresponding to lexer::lexer_error values. */
#define MERVE_ERROR_TODO 0
#define MERVE_ERROR_UNEXPECTED_PAREN 1
#define MERVE_ERROR_UNEXPECTED_BRACE 2
#define MERVE_ERROR_UNTERMINATED_PAREN 3
#define MERVE_ERROR_UNTERMINATED_BRACE 4
#define MERVE_ERROR_UNTERMINATED_TEMPLATE_STRING 5
#define MERVE_ERROR_UNTERMINATED_STRING_LITERAL 6
#define MERVE_ERROR_UNTERMINATED_REGEX_CHARACTER_CLASS 7
#define MERVE_ERROR_UNTERMINATED_REGEX 8
#define MERVE_ERROR_UNEXPECTED_ESM_IMPORT_META 9
#define MERVE_ERROR_UNEXPECTED_ESM_IMPORT 10
#define MERVE_ERROR_UNEXPECTED_ESM_EXPORT 11
#define MERVE_ERROR_TEMPLATE_NEST_OVERFLOW 12

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parse CommonJS source code and optionally return error location.
 *
 * The source buffer must remain valid while accessing string_view-backed
 * export names from the returned handle.
 *
 * If @p out_err is non-NULL, it is always written:
 * - On success: set to {0, 0}.
 * - On parse failure with known location: set to that location.
 * - On parse failure without available location: set to {0, 0}.
 *
 * You must call merve_free() on the returned handle when done.
 *
 * @param input   Pointer to the JavaScript source (need not be
 *                null-terminated). NULL is treated as an empty string.
 * @param length  Length of the input in bytes.
 * @param out_err Optional output pointer for parse error location.
 * @return A handle to the parse result, or NULL on out-of-memory.
 *         Use merve_is_valid() to check if parsing succeeded.
 */
#ifdef __cplusplus
merve_analysis merve_parse_commonjs(const char* input, size_t length,
                                    merve_error_loc* out_err = nullptr);
#else
merve_analysis merve_parse_commonjs(const char* input, size_t length,
                                    merve_error_loc* out_err);
#endif

/**
 * Check whether the parse result is valid (parsing succeeded).
 *
 * @param result Handle returned by merve_parse_commonjs(). NULL returns false.
 * @return true if parsing succeeded, false otherwise.
 */
bool merve_is_valid(merve_analysis result);

/**
 * Free a parse result and all associated memory.
 *
 * @param result Handle returned by merve_parse_commonjs(). NULL is a no-op.
 */
void merve_free(merve_analysis result);

/**
 * Get the number of named exports found.
 *
 * @param result A parse result handle. NULL returns 0.
 * @return Number of exports, or 0 if result is NULL or invalid.
 */
size_t merve_get_exports_count(merve_analysis result);

/**
 * Get the number of re-export module specifiers found.
 *
 * @param result A parse result handle. NULL returns 0.
 * @return Number of re-exports, or 0 if result is NULL or invalid.
 */
size_t merve_get_reexports_count(merve_analysis result);

/**
 * Get the name of an export at the given index.
 *
 * @param result A valid parse result handle.
 * @param index  Zero-based index (must be < merve_get_exports_count()).
 * @return Non-owning string reference. Returns {NULL, 0} on error.
 */
merve_string merve_get_export_name(merve_analysis result, size_t index);

/**
 * Get the 1-based source line number of an export.
 *
 * @param result A valid parse result handle.
 * @param index  Zero-based index (must be < merve_get_exports_count()).
 * @return 1-based line number, or 0 on error.
 */
uint32_t merve_get_export_line(merve_analysis result, size_t index);

/**
 * Get the module specifier of a re-export at the given index.
 *
 * @param result A valid parse result handle.
 * @param index  Zero-based index (must be < merve_get_reexports_count()).
 * @return Non-owning string reference. Returns {NULL, 0} on error.
 */
merve_string merve_get_reexport_name(merve_analysis result, size_t index);

/**
 * Get the 1-based source line number of a re-export.
 *
 * @param result A valid parse result handle.
 * @param index  Zero-based index (must be < merve_get_reexports_count()).
 * @return 1-based line number, or 0 on error.
 */
uint32_t merve_get_reexport_line(merve_analysis result, size_t index);

/**
 * Get the error code from the last merve_parse_commonjs() call.
 *
 * @return One of the MERVE_ERROR_* constants, or -1 if the last parse
 *         succeeded.
 * @note This is global state, overwritten by each merve_parse_commonjs() call.
 */
int merve_get_last_error(void);

/**
 * Get the merve library version string.
 *
 * @return Null-terminated version string (e.g. "1.0.1"). Never NULL.
 */
const char* merve_get_version(void);

/**
 * Get the merve library version as individual components.
 *
 * @return Struct with major, minor, and revision fields.
 */
merve_version_components merve_get_version_components(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MERVE_C_H */
