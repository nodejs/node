#ifndef SRC_BLOB_SERIALIZER_DESERIALIZER_H_
#define SRC_BLOB_SERIALIZER_DESERIALIZER_H_

#include <string>
#include <vector>

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// This is related to the blob that is used in snapshots and single executable
// applications and has nothing to do with `node_blob.h`.

namespace node {

class BlobSerializerDeserializer {
 public:
  explicit BlobSerializerDeserializer(bool is_debug_v) : is_debug(is_debug_v) {}

  template <typename... Args>
  void Debug(const char* format, Args&&... args) const;

  template <typename T>
  std::string ToStr(const T& arg) const;

  template <typename T>
  std::string GetName() const;

  bool is_debug = false;
};

enum class StringLogMode {
  kAddressOnly,  // Can be used when the string contains binary content.
  kAddressAndContent,
};

// Child classes are expected to implement T Read<T>() where
// !std::is_arithmetic_v<T> && !std::is_same_v<T, std::string>
template <typename Impl>
class BlobDeserializer : public BlobSerializerDeserializer {
 public:
  explicit BlobDeserializer(bool is_debug_v, std::string_view s)
      : BlobSerializerDeserializer(is_debug_v), sink(s) {}
  ~BlobDeserializer() = default;

  size_t read_total = 0;
  std::string_view sink;

  Impl* impl() { return static_cast<Impl*>(this); }
  const Impl* impl() const { return static_cast<const Impl*>(this); }

  // Helper for reading numeric types.
  template <typename T>
  T ReadArithmetic();

  // Layout of vectors:
  // [ 4/8 bytes ] count
  // [   ...     ] contents (count * size of individual elements)
  template <typename T>
  std::vector<T> ReadVector();

  // ReadString() creates a copy of the data. ReadStringView() doesn't.
  std::string ReadString();
  std::string_view ReadStringView(StringLogMode mode);

  // Helper for reading an array of numeric types.
  template <typename T>
  void ReadArithmetic(T* out, size_t count);

  // Helper for reading numeric vectors.
  template <typename Number>
  std::vector<Number> ReadArithmeticVector(size_t count);

 private:
  // Helper for reading non-numeric vectors.
  template <typename T>
  std::vector<T> ReadNonArithmeticVector(size_t count);

  template <typename T>
  T ReadElement();
};

// Child classes are expected to implement size_t Write<T>(const T&) where
// !std::is_arithmetic_v<T> && !std::is_same_v<T, std::string>
template <typename Impl>
class BlobSerializer : public BlobSerializerDeserializer {
 public:
  explicit BlobSerializer(bool is_debug_v)
      : BlobSerializerDeserializer(is_debug_v) {}
  ~BlobSerializer() = default;

  Impl* impl() { return static_cast<Impl*>(this); }
  const Impl* impl() const { return static_cast<const Impl*>(this); }

  std::vector<char> sink;

  // Helper for writing numeric types.
  template <typename T>
  size_t WriteArithmetic(const T& data);

  // Layout of vectors:
  // [ 4/8 bytes ] count
  // [   ...     ] contents (count * size of individual elements)
  template <typename T>
  size_t WriteVector(const std::vector<T>& data);

  // The layout of a written string:
  // [  4/8 bytes     ] length
  // [ |length| bytes ] contents
  size_t WriteStringView(std::string_view data, StringLogMode mode);
  size_t WriteString(const std::string& data);

  // Helper for writing an array of numeric types.
  template <typename T>
  size_t WriteArithmetic(const T* data, size_t count);

  // Helper for writing numeric vectors.
  template <typename Number>
  size_t WriteArithmeticVector(const std::vector<Number>& data);

 private:
  // Helper for writing non-numeric vectors.
  template <typename T>
  size_t WriteNonArithmeticVector(const std::vector<T>& data);

  template <typename T>
  size_t WriteElement(const T& data);
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BLOB_SERIALIZER_DESERIALIZER_H_
