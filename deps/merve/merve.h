/* auto-generated on 2026-01-21 14:02:13 -0500. Do not edit! */
/* begin file include/lexer.h */
#ifndef MERVE_H
#define MERVE_H

/* begin file include/lexer/parser.h */
#ifndef MERVE_PARSER_H
#define MERVE_PARSER_H

/* begin file include/lexer/version.h */
/**
 * @file version.h
 * @brief Definitions for merve's version number.
 */
#ifndef MERVE_VERSION_H
#define MERVE_VERSION_H

#define MERVE_VERSION "1.0.0"

namespace lexer {

enum {
  MERVE_VERSION_MAJOR = 1,
  MERVE_VERSION_MINOR = 0,
  MERVE_VERSION_REVISION = 0,
};

}  // namespace lexer

#endif  // MERVE_VERSION_H
/* end file include/lexer/version.h */

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lexer {

/**
 * @brief Error codes returned by the lexer when parsing fails.
 */
enum lexer_error {
  TODO,  // Reserved for future use

  // Syntax errors - indicate malformed JavaScript
  UNEXPECTED_PAREN,                   ///< Unexpected closing parenthesis
  UNEXPECTED_BRACE,                   ///< Unexpected closing brace
  UNTERMINATED_PAREN,                 ///< Unclosed parenthesis
  UNTERMINATED_BRACE,                 ///< Unclosed brace
  UNTERMINATED_TEMPLATE_STRING,       ///< Unclosed template literal
  UNTERMINATED_STRING_LITERAL,        ///< Unclosed string literal
  UNTERMINATED_REGEX_CHARACTER_CLASS, ///< Unclosed regex character class
  UNTERMINATED_REGEX,                 ///< Unclosed regular expression

  // ESM syntax errors - indicate the file should be parsed as ESM instead
  UNEXPECTED_ESM_IMPORT_META, ///< Found import.meta (ESM only)
  UNEXPECTED_ESM_IMPORT,      ///< Found import declaration (ESM only)
  UNEXPECTED_ESM_EXPORT,      ///< Found export declaration (ESM only)

  // Resource limit errors
  TEMPLATE_NEST_OVERFLOW, ///< Template literal nesting too deep
};

/**
 * @brief Type alias for export names.
 *
 * Uses std::variant to optimize memory:
 * - std::string_view: For simple identifiers (zero-copy, points to source)
 * - std::string: For exports requiring unescaping (e.g., Unicode escapes)
 *
 * Use get_string_view() to access the value uniformly.
 */
using export_string = std::variant<std::string, std::string_view>;

/**
 * @brief Result of parsing a CommonJS module.
 */
struct lexer_analysis {
  /**
   * @brief Named exports found in the module.
   *
   * Includes exports from patterns like:
   * - exports.foo = value
   * - exports['bar'] = value
   * - module.exports.baz = value
   * - module.exports = { a, b, c }
   * - Object.defineProperty(exports, 'name', {...})
   */
  std::vector<export_string> exports{};

  /**
   * @brief Module specifiers from re-export patterns.
   *
   * Includes specifiers from patterns like:
   * - module.exports = require('other')
   * - module.exports = { ...require('other') }
   * - __export(require('other'))
   * - Object.keys(require('other')).forEach(...)
   */
  std::vector<export_string> re_exports{};
};

/**
 * @brief Get a string_view from an export_string variant.
 *
 * @param s The export_string to convert
 * @return std::string_view A view into the string data
 *
 * @note The returned string_view is valid as long as:
 *       - For string_view variant: the original source is valid
 *       - For string variant: the export_string is valid
 */
inline std::string_view get_string_view(const export_string& s) {
  return std::visit([](const auto& v) -> std::string_view { return v; }, s);
}

/**
 * @brief Parse CommonJS source code and extract export information.
 *
 * Performs static analysis to detect CommonJS export patterns without
 * executing the code. Handles various patterns including:
 * - Direct exports (exports.x, module.exports.x)
 * - Bracket notation (exports['x'])
 * - Object literal assignment (module.exports = {...})
 * - Object.defineProperty patterns
 * - Re-export patterns from transpilers (Babel, TypeScript)
 *
 * @param file_contents The JavaScript source code to analyze
 * @return std::optional<lexer_analysis> The analysis result, or std::nullopt
 *         if parsing failed. Use get_last_error() to get error details.
 *
 * @note The source must remain valid while using string_view exports.
 * @note ESM syntax (import/export declarations) will cause an error.
 *
 * Example:
 * @code
 * auto result = lexer::parse_commonjs("exports.foo = 1;");
 * if (result) {
 *   for (const auto& exp : result->exports) {
 *     std::cout << lexer::get_string_view(exp) << std::endl;
 *   }
 * }
 * @endcode
 */
std::optional<lexer_analysis> parse_commonjs(std::string_view file_contents);

/**
 * @brief Get the error from the last failed parse operation.
 *
 * @return const std::optional<lexer_error>& The last error, or std::nullopt
 *         if the last parse succeeded.
 *
 * @note This is a global state and may be overwritten by subsequent calls
 *       to parse_commonjs().
 */
const std::optional<lexer_error>& get_last_error();

}  // namespace lexer

#endif  // MERVE_PARSER_H
/* end file include/lexer/parser.h */

#endif  // MERVE_H
/* end file include/lexer.h */
