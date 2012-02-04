#pragma once

#include <vector>
#include <string>
#include <boost/shared_ptr.hpp>

namespace pbc {

class buffer {
public:
    enum buffer_type {
        BT_BINARY,
        BT_ANSI,
        BT_ANSI16,
        BT_UTF16
    };
    buffer();
    buffer(buffer_type type, size_t size);
    buffer(const std::string& ansi_str);
    buffer(buffer_type type, const char* src, size_t size);
    buffer(const std::wstring& wide_str);
    buffer(buffer_type type, const wchar_t* src, size_t size);
    // intentionally no copy ctor and assignment operator 

    char* make_writable(buffer_type type, size_t size);
    const char* make_ansi();
    operator bool () const { return m_data; }
    char* buf() const { return m_data ? &m_data->buf[0] : 0; }
    size_t size() const { return m_data ? m_data->size : 0;}
    char* ansi_buf() const { return (char*)buf(); }
    wchar_t* wide_buf() const { return (wchar_t*)buf(); }
    buffer_type type() const { return m_data ? m_data->type : BT_BINARY; }
    size_t use_count() const { return m_data ? m_data.use_count() : 0; }
    void make_writable();
private:
    struct data {
        buffer_type type;
        size_t size; // in chars, not bytes, not incl trailing zero
        std::vector<char> buf;
        data() : type(BT_BINARY), size(0), buf(2, 0) {}
    };
    typedef boost::shared_ptr<data> data_ptr;
    data_ptr copy_ansi16_to_ansi(data_ptr src);
    void copy_ansi16_to_ansi(char* dest, wchar_t* src, size_t size);
    void copy_ansi_to_ansi16(wchar_t* dest, char* src, size_t size);
private:
    data_ptr m_data;
};

} // namespace pbc
