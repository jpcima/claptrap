#pragma once
#include <clap/clap.h>
#include <unordered_map>
#include <bitset>
#include <memory>
#include <cstdint>

// Mapping of CLAP identifiers to some other type
// low identifiers have a faster table-based lookup

template <class T, uint32_t NumLowIDs>
class clap_id_map {
public:
    clap_id_map() = default;
    template <class U> void set(clap_id id, U &&value);
    void unset(clap_id id);
    void clear();
    const T *find(clap_id id) const;
    T *find(clap_id id);

private:
    std::unique_ptr<T[]> m_low{new T[NumLowIDs]};
    std::bitset<NumLowIDs> m_low_valid;
    std::unordered_map<clap_id, T> m_high;
};

//------------------------------------------------------------------------------
template <class T, uint32_t NumLowIDs>
const T *clap_id_map<T, NumLowIDs>::find(clap_id id) const
{
    if (id < NumLowIDs) {
        return m_low_valid.test(id) ? &m_low[id] : nullptr;
    }
    else {
        auto it = m_high.find(id);
        return (it != m_high.end()) ? &it->second : nullptr;
    }
}

template <class T, uint32_t NumLowIDs>
T *clap_id_map<T, NumLowIDs>::find(clap_id id)
{
    return const_cast<T *>(
        const_cast<const clap_id_map<T, NumLowIDs> *>(this)->find(id));
}

template <class T, uint32_t NumLowIDs>
template <class U>
void clap_id_map<T, NumLowIDs>::set(clap_id id, U &&value)
{
    if (id < NumLowIDs) {
        m_low[id] = std::forward<U>(value);
        m_low_valid.set(id);
    }
    else {
        m_high[id] = std::forward<U>(value);
    }
}

template <class T, uint32_t NumLowIDs>
void clap_id_map<T, NumLowIDs>::unset(clap_id id)
{
    if (id < NumLowIDs) {
        m_low[id] = T{};
        m_low_valid.reset(id);
    }
    else {
        m_high.erase(id);
    }
}

template <class T, uint32_t NumLowIDs>
void clap_id_map<T, NumLowIDs>::clear()
{
    for (uint32_t i = 0; i < NumLowIDs; ++i)
        m_low[i] = T{};
    m_low_valid.reset();
    m_high.clear();
}
