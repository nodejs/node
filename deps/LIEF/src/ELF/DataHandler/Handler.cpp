/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <algorithm>
#include <utility>

#include "logging.hpp"

#include "LIEF/BinaryStream/MemoryStream.hpp"
#include "LIEF/BinaryStream/DumpStream.hpp"
#include "LIEF/BinaryStream/VectorStream.hpp"
#include "LIEF/BinaryStream/SpanStream.hpp"
#include "LIEF/BinaryStream/FileStream.hpp"

#include "ELF/DataHandler/Handler.hpp"


namespace LIEF::ELF::DataHandler {

class DataHandlerStream : public BinaryStream {
  public:
  DataHandlerStream(std::vector<uint8_t>& ref) :
    BinaryStream(STREAM_TYPE::ELF_DATA_HANDLER),
    data_{ref} {}

  ~DataHandlerStream() override = default;

  uint64_t size() const override {
    return data_.size();
  }

  result<const void*> read_at(uint64_t offset, uint64_t size,
                              uint64_t /*va*/) const override {
    if (offset > data_.size() || (offset + size) > data_.size()) {
      return make_error_code(lief_errors::read_error);
    }
    return data_.data() + offset;
  }

  private:
  std::vector<uint8_t>& data_;
};

std::unique_ptr<Handler>
    Handler::from_stream(std::unique_ptr<BinaryStream>& stream) {
  auto hdl = std::unique_ptr<Handler>(new Handler{});
  if (auto* vs = stream->cast<VectorStream>()) {
    hdl->data_ = std::move(vs->move_content());
    const uint64_t pos = vs->pos();
    auto new_stream = std::make_unique<DataHandlerStream>(hdl->data_);
    new_stream->setpos(pos);
    stream = std::move(new_stream);
    return hdl;
  }

  if (auto* span_strm = stream->cast<SpanStream>()) {
    hdl->data_ = span_strm->content();
    return hdl;
  }

  if (auto* fs = stream->cast<FileStream>()) {
    hdl->data_ = fs->content();
    const uint64_t pos = fs->pos();
    auto new_stream = std::make_unique<DataHandlerStream>(hdl->data_);
    new_stream->setpos(pos);
    stream = std::move(new_stream);
    return hdl;
  }

  if (auto* memstream = stream->cast<MemoryStream>()) {
    const uint8_t* start = memstream->start();
    hdl->data_.assign(start, start + memstream->size());
    const uint64_t pos = memstream->pos();
    auto new_stream = std::make_unique<DataHandlerStream>(hdl->data_);
    new_stream->setpos(pos);
    stream = std::move(new_stream);
    return hdl;
  }

  if (auto* dump = stream->cast<DumpStream>()) {
    hdl->data_ = dump->content();
    const uint64_t pos = dump->pos();
    auto new_stream = std::make_unique<DataHandlerStream>(hdl->data_);
    new_stream->setpos(pos);
    stream = std::move(new_stream);
    return hdl;
  }

  LIEF_ERR("Unknown stream type for Handler");
  return nullptr;
}

bool Handler::has(uint64_t offset, uint64_t size, Node::Type type) {
  Node tmp{offset, size, type};
  const auto it_node = std::find_if(nodes_.begin(), nodes_.end(),
                                    [&tmp](const std::unique_ptr<Node>& node) {
                                      return tmp == *node;
                                    });
  return it_node != nodes_.end();
}

result<Handler::ref_t<Node>> Handler::get(uint64_t offset, uint64_t size,
                                          Node::Type type) {
  Node tmp{offset, size, type};

  const auto it_node = std::find_if(nodes_.begin(), nodes_.end(),
                                    [&tmp](const std::unique_ptr<Node>& node) {
                                      return tmp == *node;
                                    });

  if (it_node == nodes_.end()) {
    return make_error_code(lief_errors::not_found);
  }
  return **it_node;
}


void Handler::remove(uint64_t offset, uint64_t size, Node::Type type) {

  Node tmp{offset, size, type};

  const auto it_node = std::find_if(nodes_.begin(), nodes_.end(),
                                    [&tmp](const std::unique_ptr<Node>& node) {
                                      return tmp == *node;
                                    });

  if (it_node == nodes_.end()) {
    LIEF_ERR("Node not found");
    return;
  }

  nodes_.erase(it_node);
}


Node& Handler::create(uint64_t offset, uint64_t size, Node::Type type) {
  nodes_.push_back(std::make_unique<Node>(offset, size, type));
  return *nodes_.back();
}


Node& Handler::add(const Node& node) {
  nodes_.push_back(std::make_unique<Node>(node));
  return *nodes_.back();
}

ok_error_t Handler::make_hole(uint64_t offset, uint64_t size) {
  auto res = reserve(offset, size);
  if (!res) {
    return res;
  }
  data_.insert(data_.begin() + offset, size, 0);
  return ok();
}


ok_error_t Handler::reserve(uint64_t offset, uint64_t size) {
  static constexpr auto MAX_MEMORY_SIZE = 6_GB;
  const auto full_size = static_cast<int64_t>(offset) + static_cast<int64_t>(size);
  if (full_size < 0) {
    return make_error_code(lief_errors::corrupted);
  }

  if (static_cast<uint64_t>(full_size) > data_.max_size()) {
    return make_error_code(lief_errors::corrupted);
  }

  if (static_cast<uint64_t>(full_size) > MAX_MEMORY_SIZE) {
    return make_error_code(lief_errors::corrupted);
  }

  const bool must_resize = data_.size() < (offset + size);
  if (!must_resize) {
    return ok();
  }

  data_.resize(offset + size, 0);
  return ok();
}


} // namespace LIEF::ELF::DataHandler
