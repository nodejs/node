#include "EnvStream.h"

cppnv::EnvStream::EnvStream(std::string* data) {
  this->data_ = data;
  this->length_ = this->data_->length();
}

char cppnv::EnvStream::get() {
  if (this->index_ >= this->length_) {
    return -1;
  }

  auto ret =  this->data_->at(this->index_);
  this->index_++;
  this->is_good = this->index_ < this->length_;
  return ret;
}

bool cppnv::EnvStream::good() {
  return this->is_good;
}

bool cppnv::EnvStream::eof() {
  return !good();
}
