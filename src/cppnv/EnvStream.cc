#include "EnvStream.h"

cppnv::EnvStream::EnvStream(std::string* data) {
  this->data_ = data;
  this->length_ = this->data_->length();
  this->is_good_ = this->index_ < this->length_;
}

char cppnv::EnvStream::get() {
  if (this->index_ >= this->length_) {
    return -1;
  }

  const auto ret = this->data_->at(this->index_);
  this->index_++;
  this->is_good_ = this->index_ < this->length_;
  return ret;
}

bool cppnv::EnvStream::good() const {
  return this->is_good_;
}

bool cppnv::EnvStream::eof() const {
  return !good();
}
