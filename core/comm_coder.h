//
//  comm_coder.h
//

#ifndef COMM_CODER_H
#define COMM_CODER_H

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <boost/endian/conversion.hpp>

namespace gpw {
namespace coder {

// -----------------------------------------------------------------------------
//  Endian conversion functions
// -----------------------------------------------------------------------------

// If current host is big endian, this function returns true.
// Otherwise, false.
inline bool
is_host_big_endian () {
    std::uint32_t one{1};
    std::uint32_t one_in_big_endian = boost::endian::native_to_big (one);

    if (one == one_in_big_endian)  // This system is big endian
        return true;
    else  // This system is little endian
        return false;
}

// Swaps byte order in-place, i.e., the contents of the memory pointed by 'buf'
// is changed when this function returns.
inline void
swap_byte_order_inplace (uint8_t* buf, std::size_t& size) {
    std::unique_ptr<uint8_t[]> swapped_buffer = std::make_unique<uint8_t[]> (size);
    for (std::size_t i = 0; i != size; i++) {
        swapped_buffer.get()[size - 1 - i] = buf[i];
    }
    std::memcpy (buf, swapped_buffer.get(), size);
}

// Depending on the endian of current system, this function swaps the byte order
// to big endian which is used for data transmit using tcp/ip network.
// The contents of the memory buffer pointed by 'buf' is changed in-place.
inline void
host_to_network_inplace (uint8_t* buf, std::size_t size) {
    // Check whether this system is big or little endian.
    // If the host is big endian, nothing to do.
    // Othersize, reorder the buffer.
    if (is_host_big_endian()) return;
    else {
        swap_byte_order_inplace (buf, size);
    }
}

// This function swaps the byte order to the endian being used in current system.
// The contents of the memory buffer pointed by 'buf' is changed in-place.
inline void
network_to_host_inplace (uint8_t* buf, std::size_t size) {
    // Check whether this system is big or little endian.
    // If the host is big endian, nothing to do.
    // Othersize, reorder the buffer.
    if (is_host_big_endian()) return;
    else {
        swap_byte_order_inplace (buf, size);
    }
}

// -----------------------------------------------------------------------------
//  Functions to encode/decode (serialize/deserialize) data into a memory buffer
// -----------------------------------------------------------------------------

using buffer_t = std::vector<uint8_t>;

// Decode string from the buffer
std::string
decode_string (const buffer_t& buffer) {
    std::string decoded (buffer.size(), ' ');
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        char c;
        std::memcpy (&c, buffer.data() + i, 1);
        decoded[i] = c;
    }
    return decoded;
}

template <typename T>
std::vector<T>
decode (const buffer_t& buffer) {
    if constexpr (std::is_same_v<T, std::string>) return decode_string (buffer);

    constexpr std::size_t elem_size = sizeof (T);
    const std::size_t     count     = buffer.size() / elem_size;
    std::vector<T>        decoded (count);
    std::memcpy (decoded.data(), buffer.data(), count * elem_size);

    // If the system uses big-endian, no need to convert thee endianness.
    if (is_host_big_endian()) return decoded;

    // Convert from big-endian (network byte order) to native, i.e., little endian.
    if constexpr (sizeof (T) > 1 && (std::is_integral_v<T> || std::is_floating_point_v<T>)) {
        std::for_each (decoded.begin(), decoded.end(), [] (auto& elem) {
            uint8_t* ptr = reinterpret_cast<uint8_t*> (&elem);
            network_to_host_inplace (ptr, elem_size);
        });
    }
    return decoded;
}

// Append a new element
template <typename T>
void
append (buffer_t& buffer, const T& new_elem) {
    if constexpr (std::is_same_v<T, std::string>) {
        std::copy (new_elem.cbegin(), new_elem.cend(), std::back_inserter (buffer));
        return;
    }

    // Resize the buffer for additional element.
    constexpr size_t elem_size = sizeof (T);
    buffer.resize (buffer.size() + elem_size);

    uint8_t* ptr = buffer.data() + buffer.size() - elem_size;
    std::memcpy (ptr, &new_elem, elem_size);

    if (is_host_big_endian()) return;
    if constexpr (sizeof (T) == 1) return;

    host_to_network_inplace (ptr, elem_size);
}

template <typename T>
T
pop (buffer_t& buffer) {
    constexpr std::size_t elem_size = sizeof (T);

    T elem;
    std::memcpy (&elem, buffer.data() + buffer.size() - elem_size, elem_size);
    network_to_host_inplace (reinterpret_cast<uint8_t*> (&elem), elem_size);

    // Reduce buffer size
    buffer.resize (buffer.size() - elem_size);

    return elem;
}

template <typename T>
void
encode_to_buffer (buffer_t& buffer, const std::vector<T>& v) {
    constexpr std::size_t elem_size = sizeof (T);
    const std::size_t     count     = v.size();

    // Resize body
    const std::size_t offset = buffer.size();  // Previous size
    buffer.resize (offset + count * elem_size);

    // Copy data into buffer and convert to big endian
    std::memcpy (buffer.data() + offset, v.data(), elem_size * count);

    if (is_host_big_endian()) return;

    // This system uses little endian convention.
    // Convert the byte order.
    for (std::size_t i = 0; i < count; ++i) {
        uint8_t* ptr = reinterpret_cast<uint8_t*> (buffer.data() + offset + i * elem_size);
        host_to_network_inplace (ptr, elem_size);
    }
}

void
encode_to_buffer (buffer_t& buffer, const std::string& str) {
    // Set size
    const std::size_t offset = buffer.size();  // Previous size
    buffer.resize (offset + str.length());

    // Set data elements
    for (std::size_t i = 0; i < str.length(); ++i) {
        std::memcpy (buffer.data() + offset + i, &str[i], 1);
    }
}

template <typename T>
buffer_t
new_buffer (const std::vector<T>& vec) {
    buffer_t buffer;
    encode_to_buffer (buffer, vec);
    return buffer;
}

template <typename T>
buffer_t
new_buffer (const std::string& str) {
    buffer_t buffer;
    encode_to_buffer (buffer, str);
    return buffer;
}

}  // namespace coder
}  // namespace gpw

// Pushes any POD-like data into the buffer
template <typename T>
gpw::coder::buffer_t&
operator<< (gpw::coder::buffer_t& bfr, T data) {
    // Check that the type of the data being pushed is trivially copyable.
    //
    // Note
    //  1. static_assert performs compile-time assertion checking.
    //  2. std::is_standard_layout is defined in header <type_traits> as
    //       template<class T> struct is_standard_layout;
    //     and if T is a standard layout type (that is, a scalar type, a standard-layout class,
    //     or an array of such type/class, possibly cv-qualified), provides the member constant
    //     `value` equal to `true`.  For any other type, `value` is `false`.
    static_assert (
        std::is_standard_layout<T>::value, "Data is too complex to be pushed into vector"
    );

    gpw::coder::append (bfr, data);

    // Return the target coder so it can be "chained".
    return bfr;
}

// Pops data from the buffer
// This function pops the data from the end of the buffer, which does
// not have memory re-allocation overhead.  Otherwise, i.e., if we pops the
// data from the beginning of the buffer, it will require a lot of overhead
// from memory re-allocation and data copy.
template <typename T>
gpw::coder::buffer_t&
operator>> (gpw::coder::buffer_t& bfr, T& data) {
    // Check that the type of the data being pushed is trivially copyable.
    static_assert (
        std::is_standard_layout<T>::value, "Data is too complex to be pushed into vector"
    );

    data = gpw::coder::pop<T> (bfr);

    return bfr;
}

// Override for std::cout compatibility - produces friendly description
template <typename T>
std::ostream&
operator<< (std::ostream& os, const gpw::coder::buffer_t& bfr) {
    os << " Size: " << bfr.size();
    return os;
}

#endif
