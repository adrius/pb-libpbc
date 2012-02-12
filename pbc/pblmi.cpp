#include "StdAfx.h"

#include "pblmi.h"

#include <logger.h>
#include <pblmi/pblsdk.h>

namespace pbc {

using namespace std;

pblmi::ptr pblmi::m_instance;

struct library {
    library()
        : m_library(0)
    {
    }
    library(IPBLMI_PBL* lib)
        : m_library(lib)
    {
    }
    ~library()
    {
        destroy();
    }
    library& operator = (IPBLMI_PBL* lib)
    {
        destroy();
        m_library = lib;
    }
    IPBLMI_PBL* operator ->() { return m_library; }
    operator bool() { return m_library != 0; }
private:
    void destroy()
    {
        if (m_library) {
            m_library->Close();
            m_library = 0;
        }
    }
private:
    IPBLMI_PBL* m_library;
};



pblmi::pblmi()
: m_impl(0)
{
    m_impl = PBLMI_GetInterface();
}

pblmi::~pblmi()
{
    if(m_impl) { 
        m_impl->Release();
        m_impl = 0;
    }
}

pblmi::ptr pblmi::instance()
{
    if (!m_instance)
        m_instance.reset(new pblmi());
    return m_instance;
}

template <class Str>
pblmi::entry pblmi::export_entry_impl( const Str& lib_name, const Str& entry_name )
{
    trace_log << "pblmi::export_entry lib_name=" << lib_name << " entry_name=" << entry_name << endl;
    library lib = m_impl->OpenLibrary(lib_name.c_str(), FALSE /*bReadWrite*/);
    if (!lib) {
        debug_log << "OpenLibrary failed for '" << lib_name << "'" << endl;
        throw pbl_open_error("Missing or invalid library: " + logger::string_cast<string>(lib_name));
    }
    PBL_ENTRYINFO entry;
    PBLMI_Result ret = lib->SeekEntry(entry_name.c_str(), &entry, FALSE /*bCreate*/);   
    if (ret != PBLMI_OK) {
        debug_log << "SeekEntry failed for '" << entry_name << "' code=" << (int)ret << endl;
        throw pbl_entry_not_found("Entry not found: " + logger::string_cast<string>(entry_name));
    }

    trace_log << "entry.comment_len=" << entry.comment_len << endl;
    trace_log << "entry.data_len=" << entry.data_len << endl;
    pblmi::entry result;
    result.mod_time = entry.mod_time;
    result.is_unicode = lib->isUnicode() != 0;
    result.comment.make_writable(BT_BINARY, entry.comment_len);
    result.data.make_writable(BT_BINARY, entry.data_len /*incl comment len*/);
    ret = lib->ReadEntryData(&entry, result.data.buf());
    if (ret != PBLMI_OK) {
        debug_log << "ReadEntryData failed for '" << entry_name << "' code=" << (int)ret << endl;
        throw pblmi_error("Read entry failed: " + logger::string_cast<string>(entry_name));
    }
    if (entry.comment_len != 0) {
        memcpy(result.comment.buf(), result.data.buf(), entry.comment_len);
        result.data.erase(0, entry.comment_len);
    }
    result.comment.make(result.is_unicode ? BT_UTF16 : BT_ANSI);
    if (is_source_entry(entry_name)) {
        result.data.make(result.is_unicode ? BT_UTF16 : BT_ANSI);
    }
    return result;
}


pblmi::ptr pblmi::create()
{
    return ptr(new pblmi());
}

template <class Str> 
bool pblmi::is_source_entry_impl( const Str& entry_name )
{
    if (entry_name.size() < 5)  // min: "x.sr?"
        return false;
    size_t pos = entry_name.size() - 4;
    if (entry_name[pos + 0] != '.' 
        || entry_name[pos + 1] != 's'
        || entry_name[pos + 2] != 'r')
        return false;
    switch (entry_name[pos + 3]) {
        case 'a': 
        case 'd':
        case 'f':
        case 'm':
        case 'q':
        case 's':
        case 'u':
        case 'w':
        case 'p':
        case 'j':
        case 'x':
            return true;
        default:
            return false;
    }
}

pblmi::entry pblmi::export_entry( const std::string& lib_name, const std::string& entry_name )
{
    return export_entry_impl(lib_name, entry_name);
}

pblmi::entry pblmi::export_entry( const std::wstring& lib_name, const std::wstring& entry_name )
{
    return export_entry_impl(lib_name, entry_name);
}

bool pblmi::is_source_entry( const std::string& entry_name )
{
    return is_source_entry_impl(entry_name);
}

bool pblmi::is_source_entry( const std::wstring& entry_name )
{
    return is_source_entry_impl(entry_name);
}


template <class Str>
void pblmi::import_entry_impl( const Str& lib_name, const Str& entry_name, pbc::buffer data, pbc::buffer comment /*= pbc::buffer()*/, time_t mod_time /*= 0*/ )
{
    trace_log << "pblmi::import_entry lib_name=" << lib_name << " entry_name=" << entry_name << endl;
    library lib = m_impl->OpenLibrary(lib_name.c_str(), TRUE /*bReadWrite*/);
    if (!lib) {
        debug_log << "OpenLibrary failed for '" << lib_name << "'" << endl;
        throw pbl_open_error("Missing or invalid library: " + logger::string_cast<string>(lib_name));
    }
    PBL_ENTRYINFO entry;
    PBLMI_Result ret = lib->SeekEntry(entry_name.c_str(), &entry, TRUE /*bCreate*/);   
    if (ret != PBLMI_OK) {
        debug_log << "SeekEntry failed for '" << entry_name << "' code=" << (int)ret << endl;
        throw pblmi_error("Entry could not be created: " + logger::string_cast<string>(entry_name));
    }

    bool is_unicode = lib->isUnicode() != 0;
    if (!comment) {
        comment.make_writable(BT_BINARY, 0);
    }
    else {
        assert_throw(comment.size() < 32768);
        comment.make(is_unicode ? BT_UTF16 : BT_ANSI);
        comment.make(BT_BINARY);
    }

    if (!data) {
        data.make_writable(BT_BINARY, 0);
    }
    else if (is_source_entry(entry_name)) {
        data.make(is_unicode ? BT_UTF16 : BT_ANSI);
        data.make(BT_BINARY);
    }
    else {
        data.make(BT_BINARY);
    }
    data.insert(0, comment, 0, comment.size());
    entry.mod_time = (DWORD)mod_time;
    ret = lib->UpdateEntryData(&entry, data.buf(), data.size() - comment.size(), comment.size());
    if (ret != PBLMI_OK) {
        debug_log << "UpdateEntryData failed for '" << entry_name << "' code=" << (int)ret << endl;
        throw pblmi_error("Entry could not be written: " + logger::string_cast<string>(entry_name));
    }
}

void pblmi::import_entry( const std::string& lib_name, const std::string& entry_name, pbc::buffer data, pbc::buffer comment /*= pbc::buffer()*/, time_t mod_time /*= 0*/ )
{
    return import_entry_impl(lib_name, entry_name, data, comment);
}

void pblmi::import_entry( const std::wstring& lib_name, const std::wstring& entry_name, pbc::buffer data, pbc::buffer comment /*= pbc::buffer()*/, time_t mod_time /*= 0*/ )
{
    return import_entry_impl(lib_name, entry_name, data, comment);
}


struct list_entries_handler: IPBLMI_Callback {
    pblmi::list_callback_t handler;
    bool is_unicode;
    virtual BOOL DirCallback(PBL_ENTRYINFO *pEntry) 
    { 
        trace_log << "DirCallback"<< endl;
        if (!handler)
            return true;
        pblmi::dir_entry e;
        e.mod_time = pEntry->mod_time;
        trace_log << "name_len=" << pEntry->name_len << endl;
        e.name = pbc::buffer(is_unicode ?  BT_UTF16 : BT_ANSI, pEntry->entry_name);
        return handler(e);
    }
};

template <class Str>
void pblmi::list_entries_impl( const Str& lib_name, list_callback_t handler )
{
    trace_log << "pblmi::list_entries lib_name=" << lib_name << endl;
    library lib = m_impl->OpenLibrary(lib_name.c_str(), FALSE /*bReadWrite*/);
    if (!lib) {
        debug_log << "OpenLibrary failed for '" << lib_name << "'" << endl;
        throw pbl_open_error("Missing or invalid library: " + logger::string_cast<string>(lib_name));
    }
    list_entries_handler h;
    h.handler = handler;
    h.is_unicode = lib->isUnicode() != 0;
    lib->Dir(&h, FALSE);
}

void pblmi::list_entries( const std::string& lib_name, list_callback_t handler )
{
    list_entries_impl(lib_name, handler);
}

void pblmi::list_entries( const std::wstring& lib_name, list_callback_t handler )
{
    list_entries_impl(lib_name, handler);
}


template <class Str>
void pblmi::delete_entry_impl( const Str& lib_name, const Str& entry_name )
{
    trace_log << "pblmi::delete_entry lib_name=" << lib_name << " entry_name=" << entry_name << endl;
    library lib = m_impl->OpenLibrary(lib_name.c_str(), TRUE /*bReadWrite*/);
    if (!lib) {
        debug_log << "OpenLibrary failed for '" << lib_name << "'" << endl;
        throw pbl_open_error("Missing or invalid library: " + logger::string_cast<string>(lib_name));
    }
    PBL_ENTRYINFO entry;
    PBLMI_Result ret = lib->SeekEntry(entry_name.c_str(), &entry, FALSE /*bCreate*/);   
    if (ret != PBLMI_OK) {
        debug_log << "SeekEntry failed for '" << entry_name << "' code=" << (int)ret << endl;
        throw pbl_entry_not_found("Entry not found: " + logger::string_cast<string>(entry_name));
    }
    ret = lib->DeleteEntry(&entry);
    if (ret != PBLMI_OK) {
        debug_log << "DeleteEntry failed for '" << entry_name << "' code=" << (int)ret << endl;
        throw pblmi_error("Entry could not be deleted: " + logger::string_cast<string>(entry_name));
    }
}

void pblmi::delete_entry( const std::string& lib_name, const std::string& entry_name )
{
    delete_entry_impl(lib_name, entry_name);
}

void pblmi::delete_entry( const std::wstring& lib_name, const std::wstring& entry_name )
{
    delete_entry_impl(lib_name, entry_name);
}


} // namespace pbc
