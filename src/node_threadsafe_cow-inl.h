#ifndef SRC_NODE_THREADSAFE_COW_INL_H_
#define SRC_NODE_THREADSAFE_COW_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

namespace node {

template <typename T>
T* CopyOnWrite<T>::write() {
  if (data_.use_count() > 1l) {
    data_ = std::make_shared<T>(*data_);
  }
  return data_.get();
}

template <typename T>
ThreadsafeCopyOnWrite<T>::Read::Read(const ThreadsafeCopyOnWrite<T>* cow)
    : cow_(cow), lock_(cow->impl_->mutex) {}

template <typename T>
const T& ThreadsafeCopyOnWrite<T>::Read::operator*() const {
  return cow_->impl_->data;
}

template <typename T>
const T* ThreadsafeCopyOnWrite<T>::Read::operator->() const {
  return &cow_->impl_->data;
}

template <typename T>
ThreadsafeCopyOnWrite<T>::Write::Write(ThreadsafeCopyOnWrite<T>* cow)
    : cow_(cow), impl_(cow->impl_.write()), lock_(impl_->mutex) {}

template <typename T>
T& ThreadsafeCopyOnWrite<T>::Write::operator*() {
  return impl_->data;
}

template <typename T>
T* ThreadsafeCopyOnWrite<T>::Write::operator->() {
  return &impl_->data;
}

template <typename T>
ThreadsafeCopyOnWrite<T>::Impl::Impl(const Impl& other) {
  RwLock::ScopedReadLock lock(other.mutex);
  data = other.data;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_THREADSAFE_COW_INL_H_
