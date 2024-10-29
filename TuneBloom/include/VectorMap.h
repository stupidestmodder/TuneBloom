#pragma once

#include <vector>
#include <unordered_map>

template <typename Key, typename T>
class VectorMap
{
public:
    using iterator                             = typename std::vector<T>::iterator;
    using const_iterator                       = typename std::vector<T>::const_iterator;
    iterator begin()                           { return theVector.begin(); }
    iterator end()                             { return theVector.end(); }
    const_iterator begin() const               { return theVector.begin(); }
    const_iterator end() const                 { return theVector.end(); }
    const T& front() const                     { return theVector.front(); }
    const T& back() const                      { return theVector.back(); }
    void insert(const Key& key, const T& item) { if (theMap.try_emplace(key, item).second) theVector.push_back(item); }
    size_t count(const T& item) const          { return theMap.count(item); }
    bool empty() const                         { return theMap.empty(); }
    size_t size() const                        { return theMap.size(); }
    auto& operator[](const Key& key)           { return theMap[key]; }
    bool contains(const Key& key) const        { return theMap.contains(key); }
    const auto find(const Key& key) const      { return theMap.find(key); }
    bool isInMap(const auto& it) const         { return it != theMap.end(); }
    void clear()                               { theVector.clear(); theMap.clear(); }

private:
    std::vector<T> theVector;
    std::unordered_map<Key, T> theMap;
};
