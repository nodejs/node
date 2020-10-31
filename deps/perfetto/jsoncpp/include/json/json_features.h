// Copyright 2007-2010 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#ifndef JSON_FEATURES_H_INCLUDED
#define JSON_FEATURES_H_INCLUDED

#if !defined(JSON_IS_AMALGAMATION)
#include "forwards.h"
#endif // if !defined(JSON_IS_AMALGAMATION)

#pragma pack(push, 8)

namespace Json {

/** \brief Configuration passed to reader and writer.
 * This configuration object can be used to force the Reader or Writer
 * to behave in a standard conforming way.
 */
class JSON_API Features {
public:
  /** \brief A configuration that allows all features and assumes all strings
   * are UTF-8.
   * - C & C++ comments are allowed
   * - Root object can be any JSON value
   * - Assumes Value strings are encoded in UTF-8
   */
  static Features all();

  /** \brief A configuration that is strictly compatible with the JSON
   * specification.
   * - Comments are forbidden.
   * - Root object must be either an array or an object value.
   * - Assumes Value strings are encoded in UTF-8
   */
  static Features strictMode();

  /** \brief Initialize the configuration like JsonConfig::allFeatures;
   */
  Features();

  /// \c true if comments are allowed. Default: \c true.
  bool allowComments_{true};

  /// \c true if root must be either an array or an object value. Default: \c
  /// false.
  bool strictRoot_{false};

  /// \c true if dropped null placeholders are allowed. Default: \c false.
  bool allowDroppedNullPlaceholders_{false};

  /// \c true if numeric object key are allowed. Default: \c false.
  bool allowNumericKeys_{false};
};

} // namespace Json

#pragma pack(pop)

#endif // JSON_FEATURES_H_INCLUDED
