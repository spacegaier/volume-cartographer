#pragma once
#include <shared_mutex>

/** @file */

#include <cstddef>
#include <list>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "vc/core/types/Cache.hpp"

namespace volcart
{
/**
 * @class LRUCache
 * @brief Least Recently Used Cache
 * @author Sean Karlage
 *
 * A cache using a least recently used replacement policy. As elements are used,
 * they are moved to the front of the cache. When the cache exceeds capacity,
 * elements are popped from the end of the cache and replacement elements are
 * added to the front.
 *
 * Usage information is tracked in a std::unordered_map since its performance
 * will likely be the fastest of the STL classes. This should be profiled. Data
 * elements are stored in insertion order in a std::list and are pointed to
 * by the elements in the usage map.
 *
 * Design mostly taken from
 * <a href = "https://github.com/lamerman/cpp-lru-cache">here</a>.
 *
 * @ingroup Types
 */
template <typename TKey, typename TValue>
class LRUCache final : public Cache<TKey, TValue>
{
public:
    using BaseClass = Cache<TKey, TValue>;
    using BaseClass::capacity_;

    /**
     * @brief Templated Key/Value pair
     *
     * Stored in the data list.
     */
    using TPair = typename std::pair<TKey, TValue>;

    /**
     * @brief Templated Key/Value pair iterator
     *
     * Stored in the LRU map.
     */
    using TListIterator = typename std::list<TPair>::iterator;

    /** Shared pointer type */
    using Pointer = std::shared_ptr<LRUCache<TKey, TValue>>;

    /**@{*/
    /** @brief Default constructor */
    LRUCache() : BaseClass() {}

    /** @brief Constructor with cache capacity parameter */
    explicit LRUCache(std::size_t capacity) : BaseClass(capacity) {}

    /** @overload LRUCache() */
    static auto New() -> Pointer
    {
        return std::make_shared<LRUCache<TKey, TValue>>();
    }

    /** @overload LRUCache(std::size_t) */
    static auto New(std::size_t capacity) -> Pointer
    {
        return std::make_shared<LRUCache<TKey, TValue>>(capacity);
    }
    /**@}*/

    /**@{*/
    /** @brief Set the maximum number of elements in the cache */
    void setCapacity(std::size_t capacity) override
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        if (capacity <= 0) {
            throw std::invalid_argument(
                "Cannot create cache with capacity <= 0");
        }
        capacity_ = capacity;

        // Cleanup elements that exceed the capacity
        while (lookup_.size() > capacity_) {
            auto last = std::end(items_);
            last--;
            lookup_.erase(last->first);
            items_.pop_back();
        }
    }

    /** @brief Get the maximum number of elements in the cache */
    auto capacity() const -> std::size_t override { return capacity_; }

    /** @brief Get the current number of elements in the cache */
    auto size() const -> std::size_t override { return lookup_.size(); }
    /**@}*/

    /**@{*/
    /** @brief Get an item from the cache by key */
    auto get(const TKey& k) -> TValue override
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        auto lookupIter = lookup_.find(k);
        if (lookupIter == std::end(lookup_)) {
            throw std::invalid_argument("Key not in cache");
        } else {
            items_.splice(std::begin(items_), items_, lookupIter->second);
            return lookupIter->second->second;
        }
    }

    /** @brief Put an item into the cache */
    void put(const TKey& k, const TValue& v) override
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        // If already in cache, need to refresh it
        auto lookupIter = lookup_.find(k);
        if (lookupIter != std::end(lookup_)) {
            items_.erase(lookupIter->second);
            lookup_.erase(lookupIter);
        }

        items_.push_front(TPair(k, v));
        lookup_[k] = std::begin(items_);

        if (lookup_.size() > capacity_) {
            auto last = std::end(items_);
            last--;
            lookup_.erase(last->first);
            items_.pop_back();
        }
    }

    /** @brief Check if an item is already in the cache */
    auto contains(const TKey& k) -> bool override
    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        return lookup_.find(k) != std::end(lookup_);
    }

    /** @brief Clear the cache */
    void purge() override
    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex_);
        lookup_.clear();
        items_.clear();
    }
    /**@}*/

private:
    /** Cache data storage */
    std::list<TPair> items_;
    /** Cache usage information */
    std::unordered_map<TKey, TListIterator> lookup_;
    /** Shared mutex for thread-safe access */
    mutable std::shared_mutex cache_mutex_;
};
}  // namespace volcart
