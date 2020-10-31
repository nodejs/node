// Copyright 2007-2010 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#ifndef JSON_H_INCLUDED
#define JSON_H_INCLUDED

#if !defined(JSON_IS_AMALGAMATION)
#include "forwards.h"
#endif // if !defined(JSON_IS_AMALGAMATION)

// Conditional NORETURN attribute on the throw functions would:
// a) suppress false positives from static code analysis
// b) possibly improve optimization opportunities.
#if !defined(JSONCPP_NORETURN)
#if defined(_MSC_VER) && _MSC_VER == 1800
#define JSONCPP_NORETURN __declspec(noreturn)
#else
#define JSONCPP_NORETURN [[noreturn]]
#endif
#endif

// Support for '= delete' with template declarations was a late addition
// to the c++11 standard and is rejected by clang 3.8 and Apple clang 8.2
// even though these declare themselves to be c++11 compilers.
#if !defined(JSONCPP_TEMPLATE_DELETE)
#if defined(__clang__) && defined(__apple_build_version__)
#if __apple_build_version__ <= 8000042
#define JSONCPP_TEMPLATE_DELETE
#endif
#elif defined(__clang__)
#if __clang_major__ == 3 && __clang_minor__ <= 8
#define JSONCPP_TEMPLATE_DELETE
#endif
#endif
#if !defined(JSONCPP_TEMPLATE_DELETE)
#define JSONCPP_TEMPLATE_DELETE = delete
#endif
#endif

#include <array>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Disable warning C4251: <data member>: <type> needs to have dll-interface to
// be used by...
#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

#pragma pack(push, 8)

/** \brief JSON (JavaScript Object Notation).
 */
namespace Json {

#if JSON_USE_EXCEPTION
/** Base class for all exceptions we throw.
 *
 * We use nothing but these internally. Of course, STL can throw others.
 */
class JSON_API Exception : public std::exception {
public:
  Exception(String msg);
  ~Exception() noexcept override;
  char const* what() const noexcept override;

protected:
  String msg_;
};

/** Exceptions which the user cannot easily avoid.
 *
 * E.g. out-of-memory (when we use malloc), stack-overflow, malicious input
 *
 * \remark derived from Json::Exception
 */
class JSON_API RuntimeError : public Exception {
public:
  RuntimeError(String const& msg);
};

/** Exceptions thrown by JSON_ASSERT/JSON_FAIL macros.
 *
 * These are precondition-violations (user bugs) and internal errors (our bugs).
 *
 * \remark derived from Json::Exception
 */
class JSON_API LogicError : public Exception {
public:
  LogicError(String const& msg);
};
#endif

/// used internally
JSONCPP_NORETURN void throwRuntimeError(String const& msg);
/// used internally
JSONCPP_NORETURN void throwLogicError(String const& msg);

/** \brief Type of the value held by a Value object.
 */
enum ValueType {
  nullValue = 0, ///< 'null' value
  intValue,      ///< signed integer value
  uintValue,     ///< unsigned integer value
  realValue,     ///< double value
  stringValue,   ///< UTF-8 string value
  booleanValue,  ///< bool value
  arrayValue,    ///< array value (ordered list)
  objectValue    ///< object value (collection of name/value pairs).
};

enum CommentPlacement {
  commentBefore = 0,      ///< a comment placed on the line before a value
  commentAfterOnSameLine, ///< a comment just after a value on the same line
  commentAfter, ///< a comment on the line after a value (only make sense for
  /// root value)
  numberOfCommentPlacement
};

/** \brief Type of precision for formatting of real values.
 */
enum PrecisionType {
  significantDigits = 0, ///< we set max number of significant digits in string
  decimalPlaces          ///< we set max number of digits after "." in string
};

/** \brief Lightweight wrapper to tag static string.
 *
 * Value constructor and objectValue member assignment takes advantage of the
 * StaticString and avoid the cost of string duplication when storing the
 * string or the member name.
 *
 * Example of usage:
 * \code
 * Json::Value aValue( StaticString("some text") );
 * Json::Value object;
 * static const StaticString code("code");
 * object[code] = 1234;
 * \endcode
 */
class JSON_API StaticString {
public:
  explicit StaticString(const char* czstring) : c_str_(czstring) {}

  operator const char*() const { return c_str_; }

  const char* c_str() const { return c_str_; }

private:
  const char* c_str_;
};

/** \brief Represents a <a HREF="http://www.json.org">JSON</a> value.
 *
 * This class is a discriminated union wrapper that can represents a:
 * - signed integer [range: Value::minInt - Value::maxInt]
 * - unsigned integer (range: 0 - Value::maxUInt)
 * - double
 * - UTF-8 string
 * - boolean
 * - 'null'
 * - an ordered list of Value
 * - collection of name/value pairs (javascript object)
 *
 * The type of the held value is represented by a #ValueType and
 * can be obtained using type().
 *
 * Values of an #objectValue or #arrayValue can be accessed using operator[]()
 * methods.
 * Non-const methods will automatically create the a #nullValue element
 * if it does not exist.
 * The sequence of an #arrayValue will be automatically resized and initialized
 * with #nullValue. resize() can be used to enlarge or truncate an #arrayValue.
 *
 * The get() methods can be used to obtain default value in the case the
 * required element does not exist.
 *
 * It is possible to iterate over the list of member keys of an object using
 * the getMemberNames() method.
 *
 * \note #Value string-length fit in size_t, but keys must be < 2^30.
 * (The reason is an implementation detail.) A #CharReader will raise an
 * exception if a bound is exceeded to avoid security holes in your app,
 * but the Value API does *not* check bounds. That is the responsibility
 * of the caller.
 */
class JSON_API Value {
  friend class ValueIteratorBase;

public:
  using Members = std::vector<String>;
  using iterator = ValueIterator;
  using const_iterator = ValueConstIterator;
  using UInt = Json::UInt;
  using Int = Json::Int;
#if defined(JSON_HAS_INT64)
  using UInt64 = Json::UInt64;
  using Int64 = Json::Int64;
#endif // defined(JSON_HAS_INT64)
  using LargestInt = Json::LargestInt;
  using LargestUInt = Json::LargestUInt;
  using ArrayIndex = Json::ArrayIndex;

  // Required for boost integration, e. g. BOOST_TEST
  using value_type = std::string;

#if JSON_USE_NULLREF
  // Binary compatibility kludges, do not use.
  static const Value& null;
  static const Value& nullRef;
#endif

  // null and nullRef are deprecated, use this instead.
  static Value const& nullSingleton();

  /// Minimum signed integer value that can be stored in a Json::Value.
  static constexpr LargestInt minLargestInt =
      LargestInt(~(LargestUInt(-1) / 2));
  /// Maximum signed integer value that can be stored in a Json::Value.
  static constexpr LargestInt maxLargestInt = LargestInt(LargestUInt(-1) / 2);
  /// Maximum unsigned integer value that can be stored in a Json::Value.
  static constexpr LargestUInt maxLargestUInt = LargestUInt(-1);

  /// Minimum signed int value that can be stored in a Json::Value.
  static constexpr Int minInt = Int(~(UInt(-1) / 2));
  /// Maximum signed int value that can be stored in a Json::Value.
  static constexpr Int maxInt = Int(UInt(-1) / 2);
  /// Maximum unsigned int value that can be stored in a Json::Value.
  static constexpr UInt maxUInt = UInt(-1);

#if defined(JSON_HAS_INT64)
  /// Minimum signed 64 bits int value that can be stored in a Json::Value.
  static constexpr Int64 minInt64 = Int64(~(UInt64(-1) / 2));
  /// Maximum signed 64 bits int value that can be stored in a Json::Value.
  static constexpr Int64 maxInt64 = Int64(UInt64(-1) / 2);
  /// Maximum unsigned 64 bits int value that can be stored in a Json::Value.
  static constexpr UInt64 maxUInt64 = UInt64(-1);
#endif // defined(JSON_HAS_INT64)
  /// Default precision for real value for string representation.
  static constexpr UInt defaultRealPrecision = 17;
  // The constant is hard-coded because some compiler have trouble
  // converting Value::maxUInt64 to a double correctly (AIX/xlC).
  // Assumes that UInt64 is a 64 bits integer.
  static constexpr double maxUInt64AsDouble = 18446744073709551615.0;
// Workaround for bug in the NVIDIAs CUDA 9.1 nvcc compiler
// when using gcc and clang backend compilers.  CZString
// cannot be defined as private.  See issue #486
#ifdef __NVCC__
public:
#else
private:
#endif
#ifndef JSONCPP_DOC_EXCLUDE_IMPLEMENTATION
  class CZString {
  public:
    enum DuplicationPolicy { noDuplication = 0, duplicate, duplicateOnCopy };
    CZString(ArrayIndex index);
    CZString(char const* str, unsigned length, DuplicationPolicy allocate);
    CZString(CZString const& other);
    CZString(CZString&& other);
    ~CZString();
    CZString& operator=(const CZString& other);
    CZString& operator=(CZString&& other);

    bool operator<(CZString const& other) const;
    bool operator==(CZString const& other) const;
    ArrayIndex index() const;
    // const char* c_str() const; ///< \deprecated
    char const* data() const;
    unsigned length() const;
    bool isStaticString() const;

  private:
    void swap(CZString& other);

    struct StringStorage {
      unsigned policy_ : 2;
      unsigned length_ : 30; // 1GB max
    };

    char const* cstr_; // actually, a prefixed string, unless policy is noDup
    union {
      ArrayIndex index_;
      StringStorage storage_;
    };
  };

public:
  typedef std::map<CZString, Value> ObjectValues;
#endif // ifndef JSONCPP_DOC_EXCLUDE_IMPLEMENTATION

public:
  /**
   * \brief Create a default Value of the given type.
   *
   * This is a very useful constructor.
   * To create an empty array, pass arrayValue.
   * To create an empty object, pass objectValue.
   * Another Value can then be set to this one by assignment.
   * This is useful since clear() and resize() will not alter types.
   *
   * Examples:
   *   \code
   *   Json::Value null_value; // null
   *   Json::Value arr_value(Json::arrayValue); // []
   *   Json::Value obj_value(Json::objectValue); // {}
   *   \endcode
   */
  Value(ValueType type = nullValue);
  Value(Int value);
  Value(UInt value);
#if defined(JSON_HAS_INT64)
  Value(Int64 value);
  Value(UInt64 value);
#endif // if defined(JSON_HAS_INT64)
  Value(double value);
  Value(const char* value); ///< Copy til first 0. (NULL causes to seg-fault.)
  Value(const char* begin, const char* end); ///< Copy all, incl zeroes.
  /**
   * \brief Constructs a value from a static string.
   *
   * Like other value string constructor but do not duplicate the string for
   * internal storage. The given string must remain alive after the call to
   * this constructor.
   *
   * \note This works only for null-terminated strings. (We cannot change the
   * size of this class, so we have nowhere to store the length, which might be
   * computed later for various operations.)
   *
   * Example of usage:
   *   \code
   *   static StaticString foo("some text");
   *   Json::Value aValue(foo);
   *   \endcode
   */
  Value(const StaticString& value);
  Value(const String& value);
  Value(bool value);
  Value(const Value& other);
  Value(Value&& other);
  ~Value();

  /// \note Overwrite existing comments. To preserve comments, use
  /// #swapPayload().
  Value& operator=(const Value& other);
  Value& operator=(Value&& other);

  /// Swap everything.
  void swap(Value& other);
  /// Swap values but leave comments and source offsets in place.
  void swapPayload(Value& other);

  /// copy everything.
  void copy(const Value& other);
  /// copy values but leave comments and source offsets in place.
  void copyPayload(const Value& other);

  ValueType type() const;

  /// Compare payload only, not comments etc.
  bool operator<(const Value& other) const;
  bool operator<=(const Value& other) const;
  bool operator>=(const Value& other) const;
  bool operator>(const Value& other) const;
  bool operator==(const Value& other) const;
  bool operator!=(const Value& other) const;
  int compare(const Value& other) const;

  const char* asCString() const; ///< Embedded zeroes could cause you trouble!
#if JSONCPP_USING_SECURE_MEMORY
  unsigned getCStringLength() const; // Allows you to understand the length of
                                     // the CString
#endif
  String asString() const; ///< Embedded zeroes are possible.
  /** Get raw char* of string-value.
   *  \return false if !string. (Seg-fault if str or end are NULL.)
   */
  bool getString(char const** begin, char const** end) const;
  Int asInt() const;
  UInt asUInt() const;
#if defined(JSON_HAS_INT64)
  Int64 asInt64() const;
  UInt64 asUInt64() const;
#endif // if defined(JSON_HAS_INT64)
  LargestInt asLargestInt() const;
  LargestUInt asLargestUInt() const;
  float asFloat() const;
  double asDouble() const;
  bool asBool() const;

  bool isNull() const;
  bool isBool() const;
  bool isInt() const;
  bool isInt64() const;
  bool isUInt() const;
  bool isUInt64() const;
  bool isIntegral() const;
  bool isDouble() const;
  bool isNumeric() const;
  bool isString() const;
  bool isArray() const;
  bool isObject() const;

  /// The `as<T>` and `is<T>` member function templates and specializations.
  template <typename T> T as() const JSONCPP_TEMPLATE_DELETE;
  template <typename T> bool is() const JSONCPP_TEMPLATE_DELETE;

  bool isConvertibleTo(ValueType other) const;

  /// Number of values in array or object
  ArrayIndex size() const;

  /// \brief Return true if empty array, empty object, or null;
  /// otherwise, false.
  bool empty() const;

  /// Return !isNull()
  explicit operator bool() const;

  /// Remove all object members and array elements.
  /// \pre type() is arrayValue, objectValue, or nullValue
  /// \post type() is unchanged
  void clear();

  /// Resize the array to newSize elements.
  /// New elements are initialized to null.
  /// May only be called on nullValue or arrayValue.
  /// \pre type() is arrayValue or nullValue
  /// \post type() is arrayValue
  void resize(ArrayIndex newSize);

  //@{
  /// Access an array element (zero based index). If the array contains less
  /// than index element, then null value are inserted in the array so that
  /// its size is index+1.
  /// (You may need to say 'value[0u]' to get your compiler to distinguish
  /// this from the operator[] which takes a string.)
  Value& operator[](ArrayIndex index);
  Value& operator[](int index);
  //@}

  //@{
  /// Access an array element (zero based index).
  /// (You may need to say 'value[0u]' to get your compiler to distinguish
  /// this from the operator[] which takes a string.)
  const Value& operator[](ArrayIndex index) const;
  const Value& operator[](int index) const;
  //@}

  /// If the array contains at least index+1 elements, returns the element
  /// value, otherwise returns defaultValue.
  Value get(ArrayIndex index, const Value& defaultValue) const;
  /// Return true if index < size().
  bool isValidIndex(ArrayIndex index) const;
  /// \brief Append value to array at the end.
  ///
  /// Equivalent to jsonvalue[jsonvalue.size()] = value;
  Value& append(const Value& value);
  Value& append(Value&& value);

  /// \brief Insert value in array at specific index
  bool insert(ArrayIndex index, const Value& newValue);
  bool insert(ArrayIndex index, Value&& newValue);

  /// Access an object value by name, create a null member if it does not exist.
  /// \note Because of our implementation, keys are limited to 2^30 -1 chars.
  /// Exceeding that will cause an exception.
  Value& operator[](const char* key);
  /// Access an object value by name, returns null if there is no member with
  /// that name.
  const Value& operator[](const char* key) const;
  /// Access an object value by name, create a null member if it does not exist.
  /// \param key may contain embedded nulls.
  Value& operator[](const String& key);
  /// Access an object value by name, returns null if there is no member with
  /// that name.
  /// \param key may contain embedded nulls.
  const Value& operator[](const String& key) const;
  /** \brief Access an object value by name, create a null member if it does not
   * exist.
   *
   * If the object has no entry for that name, then the member name used to
   * store the new entry is not duplicated.
   * Example of use:
   *   \code
   *   Json::Value object;
   *   static const StaticString code("code");
   *   object[code] = 1234;
   *   \endcode
   */
  Value& operator[](const StaticString& key);
  /// Return the member named key if it exist, defaultValue otherwise.
  /// \note deep copy
  Value get(const char* key, const Value& defaultValue) const;
  /// Return the member named key if it exist, defaultValue otherwise.
  /// \note deep copy
  /// \note key may contain embedded nulls.
  Value get(const char* begin, const char* end,
            const Value& defaultValue) const;
  /// Return the member named key if it exist, defaultValue otherwise.
  /// \note deep copy
  /// \param key may contain embedded nulls.
  Value get(const String& key, const Value& defaultValue) const;
  /// Most general and efficient version of isMember()const, get()const,
  /// and operator[]const
  /// \note As stated elsewhere, behavior is undefined if (end-begin) >= 2^30
  Value const* find(char const* begin, char const* end) const;
  /// Most general and efficient version of object-mutators.
  /// \note As stated elsewhere, behavior is undefined if (end-begin) >= 2^30
  /// \return non-zero, but JSON_ASSERT if this is neither object nor nullValue.
  Value* demand(char const* begin, char const* end);
  /// \brief Remove and return the named member.
  ///
  /// Do nothing if it did not exist.
  /// \pre type() is objectValue or nullValue
  /// \post type() is unchanged
  void removeMember(const char* key);
  /// Same as removeMember(const char*)
  /// \param key may contain embedded nulls.
  void removeMember(const String& key);
  /// Same as removeMember(const char* begin, const char* end, Value* removed),
  /// but 'key' is null-terminated.
  bool removeMember(const char* key, Value* removed);
  /** \brief Remove the named map member.
   *
   *  Update 'removed' iff removed.
   *  \param key may contain embedded nulls.
   *  \return true iff removed (no exceptions)
   */
  bool removeMember(String const& key, Value* removed);
  /// Same as removeMember(String const& key, Value* removed)
  bool removeMember(const char* begin, const char* end, Value* removed);
  /** \brief Remove the indexed array element.
   *
   *  O(n) expensive operations.
   *  Update 'removed' iff removed.
   *  \return true if removed (no exceptions)
   */
  bool removeIndex(ArrayIndex index, Value* removed);

  /// Return true if the object has a member named key.
  /// \note 'key' must be null-terminated.
  bool isMember(const char* key) const;
  /// Return true if the object has a member named key.
  /// \param key may contain embedded nulls.
  bool isMember(const String& key) const;
  /// Same as isMember(String const& key)const
  bool isMember(const char* begin, const char* end) const;

  /// \brief Return a list of the member names.
  ///
  /// If null, return an empty list.
  /// \pre type() is objectValue or nullValue
  /// \post if type() was nullValue, it remains nullValue
  Members getMemberNames() const;

  /// \deprecated Always pass len.
  JSONCPP_DEPRECATED("Use setComment(String const&) instead.")
  void setComment(const char* comment, CommentPlacement placement) {
    setComment(String(comment, strlen(comment)), placement);
  }
  /// Comments must be //... or /* ... */
  void setComment(const char* comment, size_t len, CommentPlacement placement) {
    setComment(String(comment, len), placement);
  }
  /// Comments must be //... or /* ... */
  void setComment(String comment, CommentPlacement placement);
  bool hasComment(CommentPlacement placement) const;
  /// Include delimiters and embedded newlines.
  String getComment(CommentPlacement placement) const;

  String toStyledString() const;

  const_iterator begin() const;
  const_iterator end() const;

  iterator begin();
  iterator end();

  // Accessors for the [start, limit) range of bytes within the JSON text from
  // which this value was parsed, if any.
  void setOffsetStart(ptrdiff_t start);
  void setOffsetLimit(ptrdiff_t limit);
  ptrdiff_t getOffsetStart() const;
  ptrdiff_t getOffsetLimit() const;

private:
  void setType(ValueType v) {
    bits_.value_type_ = static_cast<unsigned char>(v);
  }
  bool isAllocated() const { return bits_.allocated_; }
  void setIsAllocated(bool v) { bits_.allocated_ = v; }

  void initBasic(ValueType type, bool allocated = false);
  void dupPayload(const Value& other);
  void releasePayload();
  void dupMeta(const Value& other);

  Value& resolveReference(const char* key);
  Value& resolveReference(const char* key, const char* end);

  // struct MemberNamesTransform
  //{
  //   typedef const char *result_type;
  //   const char *operator()( const CZString &name ) const
  //   {
  //      return name.c_str();
  //   }
  //};

  union ValueHolder {
    LargestInt int_;
    LargestUInt uint_;
    double real_;
    bool bool_;
    char* string_; // if allocated_, ptr to { unsigned, char[] }.
    ObjectValues* map_;
  } value_;

  struct {
    // Really a ValueType, but types should agree for bitfield packing.
    unsigned int value_type_ : 8;
    // Unless allocated_, string_ must be null-terminated.
    unsigned int allocated_ : 1;
  } bits_;

  class Comments {
  public:
    Comments() = default;
    Comments(const Comments& that);
    Comments(Comments&& that);
    Comments& operator=(const Comments& that);
    Comments& operator=(Comments&& that);
    bool has(CommentPlacement slot) const;
    String get(CommentPlacement slot) const;
    void set(CommentPlacement slot, String comment);

  private:
    using Array = std::array<String, numberOfCommentPlacement>;
    std::unique_ptr<Array> ptr_;
  };
  Comments comments_;

  // [start, limit) byte offsets in the source JSON text from which this Value
  // was extracted.
  ptrdiff_t start_;
  ptrdiff_t limit_;
};

template <> inline bool Value::as<bool>() const { return asBool(); }
template <> inline bool Value::is<bool>() const { return isBool(); }

template <> inline Int Value::as<Int>() const { return asInt(); }
template <> inline bool Value::is<Int>() const { return isInt(); }

template <> inline UInt Value::as<UInt>() const { return asUInt(); }
template <> inline bool Value::is<UInt>() const { return isUInt(); }

#if defined(JSON_HAS_INT64)
template <> inline Int64 Value::as<Int64>() const { return asInt64(); }
template <> inline bool Value::is<Int64>() const { return isInt64(); }

template <> inline UInt64 Value::as<UInt64>() const { return asUInt64(); }
template <> inline bool Value::is<UInt64>() const { return isUInt64(); }
#endif

template <> inline double Value::as<double>() const { return asDouble(); }
template <> inline bool Value::is<double>() const { return isDouble(); }

template <> inline String Value::as<String>() const { return asString(); }
template <> inline bool Value::is<String>() const { return isString(); }

/// These `as` specializations are type conversions, and do not have a
/// corresponding `is`.
template <> inline float Value::as<float>() const { return asFloat(); }
template <> inline const char* Value::as<const char*>() const {
  return asCString();
}

/** \brief Experimental and untested: represents an element of the "path" to
 * access a node.
 */
class JSON_API PathArgument {
public:
  friend class Path;

  PathArgument();
  PathArgument(ArrayIndex index);
  PathArgument(const char* key);
  PathArgument(String key);

private:
  enum Kind { kindNone = 0, kindIndex, kindKey };
  String key_;
  ArrayIndex index_{};
  Kind kind_{kindNone};
};

/** \brief Experimental and untested: represents a "path" to access a node.
 *
 * Syntax:
 * - "." => root node
 * - ".[n]" => elements at index 'n' of root node (an array value)
 * - ".name" => member named 'name' of root node (an object value)
 * - ".name1.name2.name3"
 * - ".[0][1][2].name1[3]"
 * - ".%" => member name is provided as parameter
 * - ".[%]" => index is provided as parameter
 */
class JSON_API Path {
public:
  Path(const String& path, const PathArgument& a1 = PathArgument(),
       const PathArgument& a2 = PathArgument(),
       const PathArgument& a3 = PathArgument(),
       const PathArgument& a4 = PathArgument(),
       const PathArgument& a5 = PathArgument());

  const Value& resolve(const Value& root) const;
  Value resolve(const Value& root, const Value& defaultValue) const;
  /// Creates the "path" to access the specified node and returns a reference on
  /// the node.
  Value& make(Value& root) const;

private:
  using InArgs = std::vector<const PathArgument*>;
  using Args = std::vector<PathArgument>;

  void makePath(const String& path, const InArgs& in);
  void addPathInArg(const String& path, const InArgs& in,
                    InArgs::const_iterator& itInArg, PathArgument::Kind kind);
  static void invalidPath(const String& path, int location);

  Args args_;
};

/** \brief base class for Value iterators.
 *
 */
class JSON_API ValueIteratorBase {
public:
  using iterator_category = std::bidirectional_iterator_tag;
  using size_t = unsigned int;
  using difference_type = int;
  using SelfType = ValueIteratorBase;

  bool operator==(const SelfType& other) const { return isEqual(other); }

  bool operator!=(const SelfType& other) const { return !isEqual(other); }

  difference_type operator-(const SelfType& other) const {
    return other.computeDistance(*this);
  }

  /// Return either the index or the member name of the referenced value as a
  /// Value.
  Value key() const;

  /// Return the index of the referenced Value, or -1 if it is not an
  /// arrayValue.
  UInt index() const;

  /// Return the member name of the referenced Value, or "" if it is not an
  /// objectValue.
  /// \note Avoid `c_str()` on result, as embedded zeroes are possible.
  String name() const;

  /// Return the member name of the referenced Value. "" if it is not an
  /// objectValue.
  /// \deprecated This cannot be used for UTF-8 strings, since there can be
  /// embedded nulls.
  JSONCPP_DEPRECATED("Use `key = name();` instead.")
  char const* memberName() const;
  /// Return the member name of the referenced Value, or NULL if it is not an
  /// objectValue.
  /// \note Better version than memberName(). Allows embedded nulls.
  char const* memberName(char const** end) const;

protected:
  /*! Internal utility functions to assist with implementing
   *   other iterator functions. The const and non-const versions
   *   of the "deref" protected methods expose the protected
   *   current_ member variable in a way that can often be
   *   optimized away by the compiler.
   */
  const Value& deref() const;
  Value& deref();

  void increment();

  void decrement();

  difference_type computeDistance(const SelfType& other) const;

  bool isEqual(const SelfType& other) const;

  void copy(const SelfType& other);

private:
  Value::ObjectValues::iterator current_;
  // Indicates that iterator is for a null value.
  bool isNull_{true};

public:
  // For some reason, BORLAND needs these at the end, rather
  // than earlier. No idea why.
  ValueIteratorBase();
  explicit ValueIteratorBase(const Value::ObjectValues::iterator& current);
};

/** \brief const iterator for object and array value.
 *
 */
class JSON_API ValueConstIterator : public ValueIteratorBase {
  friend class Value;

public:
  using value_type = const Value;
  // typedef unsigned int size_t;
  // typedef int difference_type;
  using reference = const Value&;
  using pointer = const Value*;
  using SelfType = ValueConstIterator;

  ValueConstIterator();
  ValueConstIterator(ValueIterator const& other);

private:
  /*! \internal Use by Value to create an iterator.
   */
  explicit ValueConstIterator(const Value::ObjectValues::iterator& current);

public:
  SelfType& operator=(const ValueIteratorBase& other);

  SelfType operator++(int) {
    SelfType temp(*this);
    ++*this;
    return temp;
  }

  SelfType operator--(int) {
    SelfType temp(*this);
    --*this;
    return temp;
  }

  SelfType& operator--() {
    decrement();
    return *this;
  }

  SelfType& operator++() {
    increment();
    return *this;
  }

  reference operator*() const { return deref(); }

  pointer operator->() const { return &deref(); }
};

/** \brief Iterator for object and array value.
 */
class JSON_API ValueIterator : public ValueIteratorBase {
  friend class Value;

public:
  using value_type = Value;
  using size_t = unsigned int;
  using difference_type = int;
  using reference = Value&;
  using pointer = Value*;
  using SelfType = ValueIterator;

  ValueIterator();
  explicit ValueIterator(const ValueConstIterator& other);
  ValueIterator(const ValueIterator& other);

private:
  /*! \internal Use by Value to create an iterator.
   */
  explicit ValueIterator(const Value::ObjectValues::iterator& current);

public:
  SelfType& operator=(const SelfType& other);

  SelfType operator++(int) {
    SelfType temp(*this);
    ++*this;
    return temp;
  }

  SelfType operator--(int) {
    SelfType temp(*this);
    --*this;
    return temp;
  }

  SelfType& operator--() {
    decrement();
    return *this;
  }

  SelfType& operator++() {
    increment();
    return *this;
  }

  /*! The return value of non-const iterators can be
   *  changed, so the these functions are not const
   *  because the returned references/pointers can be used
   *  to change state of the base class.
   */
  reference operator*() { return deref(); }
  pointer operator->() { return &deref(); }
};

inline void swap(Value& a, Value& b) { a.swap(b); }

} // namespace Json

#pragma pack(pop)

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(pop)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

#endif // JSON_H_INCLUDED
