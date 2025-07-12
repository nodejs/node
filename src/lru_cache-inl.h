#ifndef SRC_LRU_CACHE_INL_H_
#define SRC_LRU_CACHE_INL_H_

#include <list>
#include <unordered_map>
#include <utility>

template <typename key_t, typename value_t>
class LruCache {
 public:
  using key_value_pair_t = typename std::pair<key_t, value_t>;
  using iterator = typename std::list<key_value_pair_t>::iterator;

  explicit LruCache(size_t capacity) : capacity_(capacity) {}

  void put(const key_t& key, const value_t& value) {
    auto it = lookup_map_.find(key);
    if (it != lookup_map_.end()) {
      lru_list_.erase(it->second);
      lookup_map_.erase(it);
    }

    lru_list_.push_front(std::make_pair(key, value));
    lookup_map_[key] = lru_list_.begin();

    if (lookup_map_.size() > capacity_) {
      auto last = lru_list_.end();
      last--;
      lookup_map_.erase(last->first);
      lru_list_.pop_back();
    }
  }

  value_t& get(const key_t& key) {
    auto it = lookup_map_.find(key);
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
    return it->second->second;
  }

  void erase(const key_t& key) {
    auto it = lookup_map_.find(key);
    if (it != lookup_map_.end()) {
      lru_list_.erase(it->second);
      lookup_map_.erase(it);
    }
  }

  bool exists(const key_t& key) const { return lookup_map_.count(key) > 0; }

  size_t size() const { return lookup_map_.size(); }

  size_t capacity() const { return capacity_; }

  void clear() {
    lru_list_.clear();
    lookup_map_.clear();
  }

 private:
  std::list<key_value_pair_t> lru_list_;
  std::unordered_map<key_t, iterator> lookup_map_;
  size_t capacity_;
};

#endif  // SRC_LRU_CACHE_INL_H_
