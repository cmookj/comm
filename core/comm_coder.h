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
//  Class and interface to serialize/deserialize data into a memory buffer
// -----------------------------------------------------------------------------

//// class: coder_t
class coder_t {
    using buffer_t = std::vector<uint8_t>;

  public:
    //// Core Functionalities

    // Append (push) a new element
    template <typename T>
    void
    push (const T& new_elem) {
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

        if constexpr (std::is_same_v<T, std::string>) {
            // To serialize a variable length string, we insert the string
            // in reverse order, beginning with a null (0x00) character first.
            // This enables to decode the string trivial.
            const std::size_t offset = _buffer.size();  // Offset to the very next byte.
            _buffer.resize (_buffer.size() + new_elem.length() + 1);
            _buffer[offset] = 0x00;
            std::copy (new_elem.crbegin(), new_elem.crend(), _buffer.end() - new_elem.length());
            return;
        }

        // Resize the buffer for additional element.
        constexpr size_t elem_size = sizeof (T);
        _buffer.resize (_buffer.size() + elem_size);

        uint8_t* ptr = _buffer.data() + _buffer.size() - elem_size;
        std::memcpy (ptr, &new_elem, elem_size);

        if (is_host_big_endian()) return;
        if constexpr (sizeof (T) == 1) return;

        host_to_network_inplace (ptr, elem_size);
    }

    // Remove (pop) an element at the end of the buffer
    template <typename T>
    T
    pop () {
        // Check that the type of the data being pushed is trivially copyable.
        static_assert (
            std::is_standard_layout<T>::value, "Data is too complex to be pushed into vector"
        );

        if constexpr (std::is_same_v<T, std::string>) {
            // A string is serialized in reverse order beginning a null (0x00) character.
            std::string str;
            std::size_t idx = _buffer.size() - 1;
            while (_buffer[idx] != 0x00)
                str.append (std::string{static_cast<char> (_buffer[idx--])});

            _buffer.resize (idx);

            return str;
        }

        constexpr std::size_t elem_size = sizeof (T);

        T elem;
        std::memcpy (&elem, _buffer.data() + _buffer.size() - elem_size, elem_size);
        network_to_host_inplace (reinterpret_cast<uint8_t*> (&elem), elem_size);

        // Reduce buffer size
        _buffer.resize (_buffer.size() - elem_size);

        return elem;
    }

    //// Convenience Functions

    // Convert the contents of the buffer into a vector.
    // Note: The buffer should contain homogeneous types, obviously.
    template <typename T>
    std::vector<T>
    to_vector () {
        // Note that if we push and pop the elements of a vector, the order of the
        // elements is flipped.  Hence this function flips the elements for user's
        // convenience.

        // It is impossible to estimate the number of strings in a buffer.
        if constexpr (std::is_same_v<T, std::string>) {
            std::vector<std::string> rdecoded;

            while (_buffer.size() > 0)
                rdecoded.push_back (pop<std::string>());

            std::vector<std::string> decoded;
            std::copy (rdecoded.crbegin(), rdecoded.crend(), std::back_inserter (decoded));

            return decoded;
        }

        const std::size_t count_elements = _buffer.size() / sizeof (T);
        std::vector<T>    decoded (count_elements, T{});

        for (std::size_t i = 0; i < count_elements; ++i)
            decoded[count_elements - 1 - i] = std::move (pop<T>());

        return decoded;
    }

    template <typename T>
    void
    to_buffer (const std::vector<T>& vec) {
        for (const T& elem : vec)
            push (elem);
    }

    // Pushes any POD-like data into the buffer
    template <typename T>
    coder_t&
    operator<< (T data) {
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

        push (data);

        // Return the target coder so it can be "chained".
        return *this;
    }

    // Pops data from the buffer
    // This function pops the data from the end of the buffer, which does
    // not have memory re-allocation overhead.  Otherwise, i.e., if we pops the
    // data from the beginning of the buffer, it will require a lot of overhead
    // from memory re-allocation and data copy.
    template <typename T>
    coder_t&
    operator>> (T& data) {
        // Check that the type of the data being pushed is trivially copyable.
        static_assert (
            std::is_standard_layout<T>::value, "Data is too complex to be pushed into vector"
        );

        data = pop<T>();

        return *this;
    }

  private:
    buffer_t _buffer;
};

}  // namespace coder
}  // namespace gpw

#endif
