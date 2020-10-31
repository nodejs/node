// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_DEFAULT_VALUE_OBJECTWRITER_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_DEFAULT_VALUE_OBJECTWRITER_H__

#include <memory>
#include <stack>
#include <vector>

#include <google/protobuf/stubs/callback.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/util/internal/type_info.h>
#include <google/protobuf/util/internal/datapiece.h>
#include <google/protobuf/util/internal/object_writer.h>
#include <google/protobuf/util/internal/utility.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/stubs/stringpiece.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// An ObjectWriter that renders non-repeated primitive fields of proto messages
// with their default values. DefaultValueObjectWriter holds objects, lists and
// fields it receives in a tree structure and writes them out to another
// ObjectWriter when EndObject() is called on the root object. It also writes
// out all non-repeated primitive fields that haven't been explicitly rendered
// with their default values (0 for numbers, "" for strings, etc).
class PROTOBUF_EXPORT DefaultValueObjectWriter : public ObjectWriter {
 public:
  // A Callback function to check whether a field needs to be scrubbed.
  //
  // Returns true if the field should not be present in the output. Returns
  // false otherwise.
  //
  // The 'path' parameter is a vector of path to the field from root. For
  // example: if a nested field "a.b.c" (b is the parent message field of c and
  // a is the parent message field of b), then the vector should contain { "a",
  // "b", "c" }.
  //
  // The Field* should point to the google::protobuf::Field of "c".
  typedef ResultCallback2<bool /*return*/,
                          const std::vector<std::string>& /*path of the field*/,
                          const google::protobuf::Field* /*field*/>
      FieldScrubCallBack;

  // A unique pointer to a DefaultValueObjectWriter::FieldScrubCallBack.
  typedef std::unique_ptr<FieldScrubCallBack> FieldScrubCallBackPtr;

  DefaultValueObjectWriter(TypeResolver* type_resolver,
                           const google::protobuf::Type& type,
                           ObjectWriter* ow);

  virtual ~DefaultValueObjectWriter();

  // ObjectWriter methods.
  DefaultValueObjectWriter* StartObject(StringPiece name) override;

  DefaultValueObjectWriter* EndObject() override;

  DefaultValueObjectWriter* StartList(StringPiece name) override;

  DefaultValueObjectWriter* EndList() override;

  DefaultValueObjectWriter* RenderBool(StringPiece name,
                                       bool value) override;

  DefaultValueObjectWriter* RenderInt32(StringPiece name,
                                        int32 value) override;

  DefaultValueObjectWriter* RenderUint32(StringPiece name,
                                         uint32 value) override;

  DefaultValueObjectWriter* RenderInt64(StringPiece name,
                                        int64 value) override;

  DefaultValueObjectWriter* RenderUint64(StringPiece name,
                                         uint64 value) override;

  DefaultValueObjectWriter* RenderDouble(StringPiece name,
                                         double value) override;

  DefaultValueObjectWriter* RenderFloat(StringPiece name,
                                        float value) override;

  DefaultValueObjectWriter* RenderString(StringPiece name,
                                         StringPiece value) override;
  DefaultValueObjectWriter* RenderBytes(StringPiece name,
                                        StringPiece value) override;

  virtual DefaultValueObjectWriter* RenderNull(StringPiece name);

  // Register the callback for scrubbing of fields. Owership of
  // field_scrub_callback pointer is also transferred to this class
  void RegisterFieldScrubCallBack(FieldScrubCallBackPtr field_scrub_callback);

  // If set to true, empty lists are suppressed from output when default values
  // are written.
  void set_suppress_empty_list(bool value) { suppress_empty_list_ = value; }

  // If set to true, original proto field names are used
  void set_preserve_proto_field_names(bool value) {
    preserve_proto_field_names_ = value;
  }

  // If set to true, enums are rendered as ints from output when default values
  // are written.
  void set_print_enums_as_ints(bool value) { use_ints_for_enums_ = value; }

 protected:
  enum NodeKind {
    PRIMITIVE = 0,
    OBJECT = 1,
    LIST = 2,
    MAP = 3,
  };

  // "Node" represents a node in the tree that holds the input of
  // DefaultValueObjectWriter.
  class PROTOBUF_EXPORT Node {
   public:
    Node(const std::string& name, const google::protobuf::Type* type,
         NodeKind kind, const DataPiece& data, bool is_placeholder,
         const std::vector<std::string>& path, bool suppress_empty_list,
         bool preserve_proto_field_names, bool use_ints_for_enums,
         FieldScrubCallBack* field_scrub_callback);
    virtual ~Node() {
      for (int i = 0; i < children_.size(); ++i) {
        delete children_[i];
      }
    }

    // Adds a child to this node. Takes ownership of this child.
    void AddChild(Node* child) { children_.push_back(child); }

    // Finds the child given its name.
    Node* FindChild(StringPiece name);

    // Populates children of this Node based on its type. If there are already
    // children created, they will be merged to the result. Caller should pass
    // in TypeInfo for looking up types of the children.
    virtual void PopulateChildren(const TypeInfo* typeinfo);

    // If this node is a leaf (has data), writes the current node to the
    // ObjectWriter; if not, then recursively writes the children to the
    // ObjectWriter.
    virtual void WriteTo(ObjectWriter* ow);

    // Accessors
    const std::string& name() const { return name_; }

    const std::vector<std::string>& path() const { return path_; }

    const google::protobuf::Type* type() const { return type_; }

    void set_type(const google::protobuf::Type* type) { type_ = type; }

    NodeKind kind() const { return kind_; }

    int number_of_children() const { return children_.size(); }

    void set_data(const DataPiece& data) { data_ = data; }

    bool is_any() const { return is_any_; }

    void set_is_any(bool is_any) { is_any_ = is_any; }

    void set_is_placeholder(bool is_placeholder) {
      is_placeholder_ = is_placeholder;
    }

   protected:
    // Returns the Value Type of a map given the Type of the map entry and a
    // TypeInfo instance.
    const google::protobuf::Type* GetMapValueType(
        const google::protobuf::Type& entry_type, const TypeInfo* typeinfo);

    // Calls WriteTo() on every child in children_.
    void WriteChildren(ObjectWriter* ow);

    // The name of this node.
    std::string name_;
    // google::protobuf::Type of this node. Owned by TypeInfo.
    const google::protobuf::Type* type_;
    // The kind of this node.
    NodeKind kind_;
    // Whether this is a node for "Any".
    bool is_any_;
    // The data of this node when it is a leaf node.
    DataPiece data_;
    // Children of this node.
    std::vector<Node*> children_;
    // Whether this node is a placeholder for an object or list automatically
    // generated when creating the parent node. Should be set to false after
    // the parent node's StartObject()/StartList() method is called with this
    // node's name.
    bool is_placeholder_;

    // Path of the field of this node
    std::vector<std::string> path_;

    // Whether to suppress empty list output.
    bool suppress_empty_list_;

    // Whether to preserve original proto field names
    bool preserve_proto_field_names_;

    // Whether to always print enums as ints
    bool use_ints_for_enums_;

    // Pointer to function for determining whether a field needs to be scrubbed
    // or not. This callback is owned by the creator of this node.
    FieldScrubCallBack* field_scrub_callback_;

   private:
    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Node);
  };

  // Creates a new Node and returns it. Caller owns memory of returned object.
  virtual Node* CreateNewNode(const std::string& name,
                              const google::protobuf::Type* type, NodeKind kind,
                              const DataPiece& data, bool is_placeholder,
                              const std::vector<std::string>& path,
                              bool suppress_empty_list,
                              bool preserve_proto_field_names,
                              bool use_ints_for_enums,
                              FieldScrubCallBack* field_scrub_callback);

  // Creates a DataPiece containing the default value of the type of the field.
  static DataPiece CreateDefaultDataPieceForField(
      const google::protobuf::Field& field, const TypeInfo* typeinfo) {
    return CreateDefaultDataPieceForField(field, typeinfo, false);
  }

  // Same as the above but with a flag to use ints instead of enum names.
  static DataPiece CreateDefaultDataPieceForField(
      const google::protobuf::Field& field, const TypeInfo* typeinfo,
      bool use_ints_for_enums);

 protected:
  // Returns a pointer to current Node in tree.
  Node* current() { return current_; }

 private:
  // Populates children of "node" if it is an "any" Node and its real type has
  // been given.
  void MaybePopulateChildrenOfAny(Node* node);

  // Writes the root_ node to ow_ and resets the root_ and current_ pointer to
  // nullptr.
  void WriteRoot();

  // Adds or replaces the data_ of a primitive child node.
  void RenderDataPiece(StringPiece name, const DataPiece& data);

  // Returns the default enum value as a DataPiece, or the first enum value if
  // there is no default. For proto3, where we cannot specify an explicit
  // default, a zero value will always be returned.
  static DataPiece FindEnumDefault(const google::protobuf::Field& field,
                                   const TypeInfo* typeinfo,
                                   bool use_ints_for_enums);

  // Type information for all the types used in the descriptor. Used to find
  // google::protobuf::Type of nested messages/enums.
  const TypeInfo* typeinfo_;
  // Whether the TypeInfo object is owned by this class.
  bool own_typeinfo_;
  // google::protobuf::Type of the root message type.
  const google::protobuf::Type& type_;
  // Holds copies of strings passed to RenderString.
  std::vector<std::unique_ptr<std::string>> string_values_;

  // The current Node. Owned by its parents.
  Node* current_;
  // The root Node.
  std::unique_ptr<Node> root_;
  // The stack to hold the path of Nodes from current_ to root_;
  std::stack<Node*> stack_;

  // Whether to suppress output of empty lists.
  bool suppress_empty_list_;

  // Whether to preserve original proto field names
  bool preserve_proto_field_names_;

  // Whether to always print enums as ints
  bool use_ints_for_enums_;

  // Unique Pointer to function for determining whether a field needs to be
  // scrubbed or not.
  FieldScrubCallBackPtr field_scrub_callback_;

  ObjectWriter* ow_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(DefaultValueObjectWriter);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_DEFAULT_VALUE_OBJECTWRITER_H__
