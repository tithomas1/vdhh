#include "vobj.h"
#include <cctype>
#include <memory>
#include <sstream>
#include <cassert>
#include <string>
#include <vector>
#include <map>

// internal types

struct vobj_entry {
    enum type_t {
        integral = 1,
        string = 2,
        object = 3,
        blob = 4
    } type;

    long long int_value;
    std::string str_value;
    std::vector<uint8_t> blob_value;
    std::map<std::string, vobj_entry> obj_value;

    vobj_entry() : type(object) {
    }

    template<typename T, class = std::enable_if<std::is_integral<T>::value>>
    vobj_entry(T val) :
        type(integral),
        int_value(val) {
    }

    vobj_entry(const char* val) :
        type(string),
        str_value(val) {
    }

    vobj_entry(const void* data, size_t len) :
        type(blob),
        blob_value(reinterpret_cast<const uint8_t*>(data),
                   reinterpret_cast<const uint8_t*>(data) + len) {
    }

    template<typename T, class = std::enable_if<std::is_integral<T>::value>>
    operator T() const {
        assert(integral == type);
        return int_value;
    }

    operator const char*() const {
        assert(string == type);
        return str_value.c_str();
    }
};

namespace {

void trim(std::string& s) {
    // trim heading space
    auto it = s.begin();
    while (s.end() != it && !isspace(*it))
        ++it;
    if (s.end() == it)
        return;
    s.erase(s.begin(), it);

    // trim trailing space
    it = s.end();
    while (it != s.begin() && isspace(*(--it)));
    s.erase(it, s.end());
}

template<typename T>
size_t ser_write(void*& pptr, size_t& available, const T* value) {
    auto len = sizeof(typename std::remove_reference<T>::type);
    auto elen = std::min(available, len);
    if (elen > 0) {
        memmove(pptr, value, elen);
        pptr = reinterpret_cast<uint8_t*>(pptr) + elen;
        available -= elen;
    }
    return len;
}

size_t ser_write(void*& pptr, size_t& available, std::string const& value) {
    auto len = value.size();
    auto elen = std::min(available, len);
    if (elen > 0) {
        memmove(pptr, value.c_str(), elen);
        pptr = reinterpret_cast<uint8_t*>(pptr) + elen;
        available -= elen;
    }
    return len;
}

size_t ser_write(void*& pptr, size_t& available, const void* data, size_t len) {
    auto elen = std::min(available, len);
    if (elen > 0) {
        memmove(pptr, data, elen);
        pptr = reinterpret_cast<uint8_t*>(pptr) + elen;
        available -= elen;
    }
    return len;
}

template<typename T>
bool ser_read(const void*& gptr, size_t& available, T* value) {
    auto elen = std::min(available, sizeof(T));
    if (elen > 0) {
        memmove(value, gptr, elen);
        gptr = reinterpret_cast<const uint8_t*>(gptr) + elen;
        available -= elen;
    }
    return sizeof(T) == elen;
}

bool ser_read(const void*& gptr, size_t& available, void* buf, size_t len) {
    auto elen = std::min(available, len);
    if (elen > 0) {
        memmove(buf, gptr, elen);
        gptr = reinterpret_cast<const uint8_t*>(gptr) + elen;
        available -= elen;
    }
    return len == elen;
}

} // namespace

extern "C" {

vobj_t vobj_create() {
    return reinterpret_cast<vobj_t>(new vobj_entry);
}

void vobj_dispose(vobj_t dict) {
    delete reinterpret_cast<vobj_entry*>(dict);
}

void vobj_clear(vobj_t d) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    dict->obj_value.clear();
}

int vobj_get_count(const vobj_t d) {
    auto dict = reinterpret_cast<const vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    return dict->obj_value.size();
}

const char* get_key(const vobj_t d, int idx) {
    auto dict = reinterpret_cast<const vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);

    std::vector<std::string> keys;
    for (auto& pair : dict->obj_value) {
        keys.push_back(pair.first);
    }

    // sort keys alphabetically
    std::sort(keys.begin(), keys.end());

    return keys[idx].c_str();
}

int vobj_set_data(vobj_t dict, const void* buff, size_t len) {
    // clear first
    vobj_clear(dict);

    // read sequences [len16]-[key]-[len32]-[type1]-[value] one by one to build
    // dictionary key-pair values
    do {
        uint16_t keylen = 0;
        if (!ser_read(buff, len, &keylen)) {
            // no more data
            break;
        }

        keylen = ntohs(keylen);
        std::vector<char> key(keylen);
        if (!ser_read(buff, len, &key.front(), keylen))
            return -1;
        key.push_back(0);       // make asciiz

        uint32_t valen = 0;
        if (!ser_read(buff, len, &valen))
            return -1;
        valen = ntohl(valen);

        uint8_t type = 0;
        if (!ser_read(buff, len, &type))
            return -1;

        switch(type) {
            case vobj_entry::integral: {
                int64_t value = 0;
                if (valen != sizeof(value) || !ser_read(buff, len, &value))
                    return -1;
                vobj_set_llong(dict, &key.front(), value);
                break;
            }

            case vobj_entry::string: {
                std::vector<char> value(valen);
                if (!ser_read(buff, len, &value.front(), valen))
                    return -1;
                value.push_back(0);     // make asciiz
                vobj_set_str(dict, &key.front(), &value.front());
                break;
            }

            case vobj_entry::object: {
                // deserialize vobj
                auto value = vobj_create();
                if (valen > len || -1 == vobj_set_data(value, buff, valen))
                    return -1;
                vobj_set_obj(dict, &key.front(), value);
                vobj_dispose(value);
                // adjust pointers
                len -= valen;
                buff = reinterpret_cast<const uint8_t*>(buff) + valen;
                break;
            }

            case vobj_entry::blob: {
                std::vector<char> value(valen);
                if (!ser_read(buff, len, &value.front(), valen))
                    return -1;
                vobj_set_blob(dict, &key.front(), &value.front(), value.size());
                break;
            }

            default: {
                // just pass the value and goto next entry record
                std::vector<char> value(valen);
                if (!ser_read(buff, len, &value.front(), valen))
                    return -1;
                break;
            }
        }
    } while(true);

    return 0;
}

int vobj_get_data(const vobj_t d, void* data, size_t* len) {
    if (NULL == data || NULL == len) {
        errno = EINVAL;
        return -1;
    }

    auto dict = reinterpret_cast<const vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);

    size_t available = *len, total = 0;
    bool result = true;
    for (auto& pair : dict->obj_value) {
        // write the pair in binary sequence [len]-[key]-[len]-[type]-[value]
        // len
        uint16_t keylen = htons(pair.first.size());
        total += ser_write(data, available, &keylen);
        // key
        total += ser_write(data, available, pair.first);

        uint8_t type = pair.second.type;
        switch(type) {
            case vobj_entry::integral: {
                // len
                uint32_t len = htonl(sizeof(pair.second.int_value));
                total += ser_write(data, available, &len);
                // type
                total += ser_write(data, available, &type);
                // value
                total += ser_write(data, available, &pair.second.int_value);
                break;
            }

            case vobj_entry::string: {
                // len
                auto const& str = pair.second.str_value;
                uint32_t len = htonl(str.size());
                total += ser_write(data, available, &len);
                // type
                total += ser_write(data, available, &type);
                // value
                total += ser_write(data, available, str);
                break;
            }

            case vobj_entry::object: {
                // len, save location of the 'len' field to patch later
                auto lenptr = available >= sizeof(uint32_t) ?
                    reinterpret_cast<uint32_t*>(data) : NULL;
                uint32_t len = 0;
                total += ser_write(data, available, &len);
                // type
                total += ser_write(data, available, &type);
                // value
                size_t buflen = available;
                result &= (0 == vobj_get_data((const vobj_t)&pair.second, data, &buflen));
                // patch the len field we reserved space for
                if (lenptr)
                    *lenptr = htonl(buflen);
                total += buflen;
                // adjust pointers
                buflen = std::min(available, buflen);
                available -= buflen;
                data = reinterpret_cast<uint8_t*>(data) + buflen;
                break;
            }

            case vobj_entry::blob: {
                // len
                auto const& blob = pair.second.blob_value;
                uint32_t len = htonl(blob.size());
                total += ser_write(data, available, &len);
                // type
                total += ser_write(data, available, &type);
                // value
                total += ser_write(data, available, &blob.front(), blob.size());
                break;
            }
        }
    }

    if (total > *len) {
        errno = ENOMEM;
        result &= false;
    }

    *len = total;
    return result ? 0 : -1;
}

void vobj_set_llong(vobj_t d, const char* key, long long val) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    dict->obj_value[key] = vobj_entry(val);
}

long long vobj_get_llong(const vobj_t d, const char* key) {
    auto dict = reinterpret_cast<const vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);

    auto it = dict->obj_value.find(key);
    return (dict->obj_value.end() == it) ?
        0 :
        static_cast<long long>(it->second);
}

void vobj_set_str(vobj_t d, const char* key, const char* val) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    dict->obj_value[key] = vobj_entry(val);
}

const char* vobj_get_str(const vobj_t d, const char* key) {
    auto dict = reinterpret_cast<const vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    auto it = dict->obj_value.find(key);
    return (dict->obj_value.end() == it) ?
        NULL :
        static_cast<const char*>(it->second);
}

void vobj_set_obj(vobj_t d, const char* key, const vobj_t object) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    auto value = reinterpret_cast<const vobj_entry*>(object);
    assert(vobj_entry::object == dict->type);
    assert(vobj_entry::object == value->type);
    dict->obj_value[key] = *value;
}

vobj_t vobj_get_obj(const vobj_t d, const char* key) {
    auto dict = reinterpret_cast<const vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    auto it = dict->obj_value.find(key);
    return dict->obj_value.end() == it ?
        NULL :
        (vobj_t)(&it->second);
}

void vobj_set_blob(vobj_t d, const char* key, const void* data, size_t len) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    dict->obj_value[key] = vobj_entry(data, len);
}

void* vobj_get_blob_data(const vobj_t d, const char* key) {
    auto dict = reinterpret_cast<const vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);

    auto it = dict->obj_value.find(key);
    if (dict->obj_value.end() == it)
        return NULL;

    assert(vobj_entry::blob == it->second.type);
    return (void*)(&it->second.blob_value.front());
}

size_t vobj_get_blob_size(const vobj_t d, const char* key) {
    auto dict = reinterpret_cast<const vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);

    auto it = dict->obj_value.find(key);
    if (dict->obj_value.end() == it)
        return 0;

    assert(vobj_entry::blob == it->second.type);
    return it->second.blob_value.size();
}

// array api
void vobj_add_llong(const vobj_t d, long long val) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", int(dict->obj_value.size()));
    vobj_set_llong(d, key, val);
}

long long vobj_iget_llong(const vobj_t d, int idx) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    return vobj_get_llong(d, key);
}

void vobj_iset_llong(const vobj_t d, int idx, long long val) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    return vobj_set_llong(d, key, val);
}

void vobj_add_str(vobj_t d, const char* val) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", int(dict->obj_value.size()));
    vobj_set_str(d, key, val);
}

const char* vobj_iget_str(const vobj_t d, int idx) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    return vobj_get_str(d, key);
}

void vobj_iset_str(const vobj_t d, int idx, const char* str) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    return vobj_set_str(d, key, str);
}

void vobj_add_obj(vobj_t d, const vobj_t val) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", int(dict->obj_value.size()));
    vobj_set_obj(d, key, val);
}

vobj_t vobj_iget_obj(const vobj_t d, int idx) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    return vobj_get_obj(d, key);
}

void vobj_iset_obj(const vobj_t d, int idx, vobj_t obj) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    return vobj_set_obj(d, key, obj);
}

void vobj_add_blob(vobj_t d, const void* data, size_t len) {
    auto dict = reinterpret_cast<vobj_entry*>(d);
    assert(vobj_entry::object == dict->type);
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", int(dict->obj_value.size()));
    vobj_set_blob(d, key, data, len);
}

void vobj_iset_blob(const vobj_t d, int idx, const void* data, size_t len) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    vobj_set_blob(d, key, data, len);
}

void* vobj_iget_blob_data(vobj_t d, int idx) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    return vobj_get_blob_data(d, key);

}

size_t vobj_iget_blob_size(vobj_t d, int idx) {
    char key[vobj_KEYMAXLEN];
    snprintf(key, vobj_KEYMAXLEN, "%02x", idx);
    return vobj_get_blob_size(d, key);
}

} // extern "C"
