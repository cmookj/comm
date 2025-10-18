//
//  net_message.h
//

#ifndef NET_MESSAGE_H
#define NET_MESSAGE_H

#include "core/comm_common.h"

#include <boost/endian/conversion.hpp>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>

namespace gpw {
namespace net {

inline bool
is_host_big_endian () {
    std::uint32_t one{1};
    std::uint32_t one_in_big_endian = boost::endian::native_to_big (one);

    if (one == one_in_big_endian)  // This system is big endian
        return true;
    else  // This system is little endian
        return false;
}

inline void
swap_byte_order_inplace (char* buf, std::size_t& size) {
    std::unique_ptr<char[]> swapped_buffer = std::make_unique<char[]> (size);
    for (std::size_t i = 0; i != size; i++) {
        swapped_buffer.get()[size - 1 - i] = buf[i];
    }
    std::memcpy (buf, swapped_buffer.get(), size);
}

inline void
host_to_network_inplace (char* buf, std::size_t size) {
    // Check whether this system is big or little endian.
    // If the host is big endian, nothing to do.
    // Othersize, reorder the buffer.
    if (is_host_big_endian()) return;
    else {
        swap_byte_order_inplace (buf, size);
    }
}

inline void
network_to_host_inplace (char* buf, std::size_t size) {
    // Check whether this system is big or little endian.
    // If the host is big endian, nothing to do.
    // Othersize, reorder the buffer.
    if (is_host_big_endian()) return;
    else {
        swap_byte_order_inplace (buf, size);
    }
}

// Message header is sent at start of all messages.  The template allows us
// to use "enum class" to ensure that the messages are valid at compile time.
// Note that the length of the message header should be the same regardless of
// the architecture of the system running this code.  (Especially for 32-bit and
// 64-bit systems)
// Also, the order of the byte matters, i.e., big-endian or little-endian.
// However, in this code, simply assume that all the system running this code
// is based on intel x86 architecture and ignore that byte-order issue.
template <typename T> struct message_header {
  public:
    T
    id () const {
        constexpr std::size_t data_size = sizeof (T);
        char                  buf[data_size];

        std::memcpy (buf, &_id, data_size);
        boost::endian::big_to_native_inplace (*buf);

        T the_id;
        std::memcpy (&the_id, buf, data_size);

        return the_id;
    }

    std::size_t
    size () const {
        uint32_t the_size;
        std::memcpy (&the_size, &_size, sizeof (uint32_t));
        boost::endian::big_to_native_inplace (the_size);
        return the_size;
    }

    void
    set_id (const T& the_id) {
        constexpr std::size_t data_size = sizeof (T);
        char                  buf[data_size];

        std::memcpy (buf, &the_id, data_size);
        boost::endian::native_to_big_inplace (*buf);

        std::memcpy (&_id, buf, data_size);
    }

    void
    set_size (const std::size_t& the_size) {
        uint32_t size_as_uint_32 = static_cast<uint32_t> (the_size);
        boost::endian::native_to_big_inplace (size_as_uint_32);
        std::memcpy (&_size, &size_as_uint_32, sizeof (uint32_t));
    }

  private:
    T        _id{};
    uint32_t _size = 0;
};

template <typename T> struct message {
  public:
    // Constructors & destructor
    message () {};
    virtual ~message () {};

    // Convenience constructor, given a verb only.
    message (const T& v) { _header.set_id (v); }

    // Convenience constructor for binary data given by a pointer with size.
    message (
        const T&            verb,
        const std::uint8_t* data,
        const std::size_t&  size  // Byte size of the binary data
    ) {
        _header.set_id (verb);
        _header.set_size (size);

        // Prepare memory
        _body.resize (size);

        // Copy binary data
        std::memcpy (_body.data(), data, size);
    }

    // Convenience constructor for 2 byte integers (with Verb or not).
    message (const T& verb, const std::vector<std::int16_t>& vec) {
        _header.set_id (verb);
        _header.set_size (sizeof (std::int16_t) * vec.size());
        _encode_integer_16 (vec);
    }

    // Convenience constructor for 4 byte integers (with Verb or not).
    message (const T& verb, const std::vector<std::int32_t>& vec) {
        _header.set_id (verb);
        _header.set_size (sizeof (std::int32_t) * vec.size());
        _encode_integer_32 (vec);
    }

    // Convenience constructor for 4 byte floats (with Verb or not).
    message (const T& verb, const std::vector<float>& vec) {
        _header.set_id (verb);
        _header.set_size (sizeof (float) * vec.size());
        _encode_float (vec);
    }

    // Convenience constructor for 8 byte doubles (with Verb or not).
    message (const T& verb, const std::vector<double>& vec) {
        _header.set_id (verb);
        _header.set_size (sizeof (double) * vec.size());
        _encode_double (vec);
    }

    // Convenience constructor for string (with Verb or not).
    message (const T& verb, const std::string& str) {
        _header.set_id (verb);
        _header.set_size (str.length());
        _encode_string (str);
    }

    const T
    id () const {
        return _header.id();
    }

    void
    set_id (T the_id) {
        _header.set_id (the_id);
    }

    // Replace the id in the message header, for message forwarding.
    message<T>&
    replace_id (const T& v) {
        _header.set_id (v);
        return *this;
    }

    // Access the header
    const message_header<T>&
    header () const {
        return _header;
    }
    message_header<T>&
    header () {
        return _header;
    }

    // Access the body
    std::vector<std::uint8_t>&
    body () {
        return _body;
    }

    // Access the data of the body directly
    const std::uint8_t*
    data () const {
        return _body.data();
    }
    std::uint8_t*
    data () {
        return _body.data();
    }

    // Returns the size of the body of the message in bytes.
    std::size_t
    size () const {
        return _header.size();
    }

    // message description
    std::string
    description () const {
        std::stringstream desc;
        desc << "[ " << sizeof (message_header<T>) << " + " << size() << " : ";
        desc << std::hex << "0x" << std::setw (8) << static_cast<uint32_t> (id()) << std::dec
             << " ]";

        return desc.str();
    }

    // Read (2 * the number of specified length) bytes from the body and
    // construct a vector of 2 byte integers.
    std::vector<std::int16_t>
    decode_integer_16 () const {
        std::size_t               elem_size = sizeof (std::int16_t);
        std::size_t               count     = _body.size() / elem_size;
        std::vector<std::int16_t> decoded (count);
        for (std::size_t i = 0; i != count; ++i) {
            std::int16_t v{0};
            std::memcpy (&v, _body.data() + elem_size * i, elem_size);
            boost::endian::big_to_native_inplace (v);
            decoded[i] = v;
        }
        return decoded;
    }

    // Read (4 * the number of specified length) bytes from the body and
    // construct a vector of 4 byte integers.
    std::vector<std::int32_t>
    decode_integer_32 () const {
        std::size_t               elem_size = sizeof (std::int32_t);
        std::size_t               count     = _body.size() / elem_size;
        std::vector<std::int32_t> decoded (count);
        for (std::size_t i = 0; i != count; ++i) {
            std::int32_t v{0};
            std::memcpy (&v, _body.data() + elem_size * i, elem_size);
            boost::endian::big_to_native_inplace (v);
            decoded[i] = v;
        }
        return decoded;
    }

    // Read (4 * the number of specified length) bytes from the body and
    // construct a vector of 4 byte floats.
    std::vector<float>
    decode_float () const {
        constexpr std::size_t elem_size = sizeof (float);
        std::size_t           count     = _body.size() / elem_size;
        std::vector<float>    decoded (count);
        for (std::size_t i = 0; i != count; ++i) {
            char buf[elem_size];
            std::memcpy (buf, _body.data() + elem_size * i, elem_size);
            // boost::endian::big_to_native_inplace(*buf);
            network_to_host_inplace (buf, elem_size);
            float v{0.};
            std::memcpy (&v, buf, elem_size);
            decoded[i] = v;
        }
        return decoded;
    }

    // Read (8 * the number of specified length) bytes from the body and
    // construct a vector of 8 byte doubles.
    std::vector<double>
    decode_double () const {
        constexpr std::size_t elem_size = sizeof (double);
        std::size_t           count     = _body.size() / elem_size;
        std::vector<double>   decoded (count);
        for (std::size_t i = 0; i != count; ++i) {
            char buf[elem_size];
            std::memcpy (buf, _body.data() + elem_size * i, elem_size);
            // boost::endian::big_to_native_inplace(*buf);
            network_to_host_inplace (buf, elem_size);
            double v{0.};
            std::memcpy (&v, buf, elem_size);
            decoded[i] = v;
        }
        return decoded;
    }

    // Read (the number of specified length) bytes from the body and
    // construct a string.
    std::string
    decode_string () const {
        std::string decoded (_body.size(), ' ');
        for (std::size_t i = 0; i != _body.size(); ++i) {
            char c;
            std::memcpy (&c, _body.data() + i, 1);
            decoded[i] = c;
        }
        return decoded;
    }

    // Resize the body of the message.
    void
    resize (const std::size_t& new_size) {
        // Resize the vector by the size of the data being pushed.
        _body.resize (new_size);

        // Recalculate the message size.
        _header.set_size (static_cast<uint32_t> (new_size));
    }

    std::vector<std::uint8_t>
    get_message_body () {
        return std::move (_body);
    }

  private:
    void
    _encode_integer_16 (const std::vector<std::int16_t>& v) {
        // Resize body
        _body.resize (v.size() * sizeof (int16_t));

        // Set data elements
        for (std::size_t i = 0; i != v.size(); i++) {
            std::int16_t val{v[i]};
            boost::endian::native_to_big_inplace (val);
            std::memcpy (_body.data() + i * sizeof (int16_t), &val, sizeof (int16_t));
        }
    }

    void
    _encode_integer_32 (const std::vector<std::int32_t>& v) {
        // Resize body
        _body.resize (v.size() * sizeof (int32_t));

        // Set data elements
        for (std::size_t i = 0; i != v.size(); i++) {
            std::int32_t val{v[i]};
            boost::endian::native_to_big_inplace (val);
            std::memcpy (_body.data() + i * sizeof (int32_t), &val, sizeof (int32_t));
        }
    }

    void
    _encode_float (const std::vector<float>& vec) {
        // Resize body
        _body.resize (vec.size() * sizeof (float));

        // Set data elements
        for (std::size_t i = 0; i != vec.size(); i++) {
            float v{vec[i]};
            char  buf[sizeof (float)];
            std::memcpy (buf, &v, sizeof (float));
            // boost::endian::native_to_big_inplace(*buf);
            host_to_network_inplace (buf, sizeof (float));
            std::memcpy (_body.data() + i * sizeof (float), buf, sizeof (float));
        }
    }

    void
    _encode_double (const std::vector<double>& vec) {
        // Resize body
        _body.resize (vec.size() * sizeof (double));

        // Set data elements
        for (std::size_t i = 0; i != vec.size(); i++) {
            double v{vec[i]};
            char   buf[sizeof (double)];
            std::memcpy (buf, &v, sizeof (double));
            // boost::endian::native_to_big_inplace(*buf);
            host_to_network_inplace (buf, sizeof (double));
            std::memcpy (_body.data() + i * sizeof (double), buf, sizeof (double));
        }
    }

    void
    _encode_string (const std::string& str) {
        // Set size
        _body.resize (str.length());

        // Set data elements
        for (std::size_t i = 0; i != str.length(); i++) {
            std::memcpy (_body.data() + i, &str[i], 1);
        }
    }

    // -------------------------------------------------------------------------
    //                                                            Data members
    // -------------------------------------------------------------------------
    message_header<T>         _header{};
    std::vector<std::uint8_t> _body;
};

// Forward declare the connection
template <typename T> class connection;

// Encapsulate the message type
template <typename T> struct owned_message {
    std::shared_ptr<connection<T>> remote = nullptr;
    message<T>                     msg;

    // Again, a friendly string maker
    friend std::ostream&
    operator<< (std::ostream& os, const owned_message<T>& msg) {
        os << msg.msg;
        return os;
    }
};

}  // namespace net
}  // namespace gpw

// Pushes any POD-like data into the message buffer
template <typename T, typename data_type>
gpw::net::message<T>&
operator<< (gpw::net::message<T>& msg, data_type data) {
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
        std::is_standard_layout<data_type>::value, "Data is too complex to be pushed into vector"
    );

    constexpr std::size_t data_size = sizeof (data_type);

    // Cache current size of vector, as this will be the point we insert the data.
    std::size_t i = msg.size();

    // Resize the vector by the size of the data being pushed.
    msg.resize (i + data_size);

    // Change the byte order
    char buf[data_size];
    std::memcpy (buf, &data, data_size);
    // boost::endian::native_to_big_inplace(*buf);
    gpw::net::host_to_network_inplace (buf, data_size);

    // Physically copy the data into the newly allocated vector space.
    std::memcpy (msg.body().data() + i, buf, data_size);

    // Return the target message so it can be "chained".
    return msg;
}

// Pops data from the message buffer
// This function pops the data from the end of the message buffer, which does
// not have memory re-allocation overhead.  Otherwise, i.e., if we pops the
// data from the beginning of the message buffer, it will require a lot of overhead
// from memory re-allocation and data copy.
template <typename T, typename data_type>
gpw::net::message<T>&
operator>> (gpw::net::message<T>& msg, data_type& data) {
    // Check that the type of the data being pushed is trivially copyable.
    static_assert (
        std::is_standard_layout<data_type>::value, "Data is too complex to be pushed into vector"
    );

    constexpr std::size_t data_size = sizeof (data_type);

    // Cache the location towards the end of the vector where the pulled data starts.
    std::size_t i = msg.size() - data_size;

    // Physically copy the data from the vector into the user variable.
    char buf[data_size];
    std::memcpy (buf, msg.body().data() + i, data_size);
    // boost::endian::big_to_native_inplace(*buf);
    gpw::net::network_to_host_inplace (buf, data_size);
    std::memcpy (&data, buf, data_size);

    // Shrink the vector to remove read bytes, and reset end position.
    // Note that it does not have any performance overhead, because reducing the
    // size of a vector does not reallocate memory.
    msg.resize (i);

    // Return the target message so it can be "chained".
    return msg;
}

// Override for std::cout compatibility - produces friendly description of message
template <typename T>
std::ostream&
operator<< (std::ostream& os, const gpw::net::message<T>& msg) {
    os << "ID: " << int (msg.id()) << " Size: " << msg.size();
    return os;
}

// Again, a friendly string maker
template <typename T>
std::ostream&
operator<< (std::ostream& os, const gpw::net::owned_message<T>& msg) {
    os << msg.msg;
    return os;
}

#endif
