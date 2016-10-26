#pragma once

#include <stdint.h>
#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

typedef struct vobj* vobj_t;

#define vobj_KEYMAXLEN 32
#define vobj_VALMAXLEN PATH_MAX

vobj_t vobj_create();
void vobj_dispose(vobj_t);

int vobj_get_count(const vobj_t);
const char* get_key(const vobj_t, int idx);

// dict api
void vobj_set_llong(vobj_t, const char* key, long long val);
long long vobj_get_llong(const vobj_t, const char* key);

void vobj_set_str(vobj_t, const char* key, const char* val);
const char* vobj_get_str(const vobj_t, const char* key);

void vobj_set_obj(vobj_t, const char* key, const vobj_t val);
vobj_t vobj_get_obj(const vobj_t, const char* key);

void vobj_set_blob(vobj_t, const char* key, const void* data, size_t len);
void* vobj_get_blob_data(vobj_t, const char* key);
size_t vobj_get_blob_size(vobj_t, const char* key);

// array api
void vobj_add_llong(const vobj_t, long long val);
long long vobj_iget_llong(const vobj_t, int idx);

void vobj_add_str(vobj_t, const char* val);
void vobj_iset_str(vobj_t, int idx, const char* val);
const char* vobj_iget_str(const vobj_t, int idx);

void vobj_add_obj(vobj_t d, const vobj_t val);
void vobj_iset_obj(vobj_t d, int idx, const vobj_t val);
vobj_t vobj_iget_obj(const vobj_t d, int idx);

void vobj_add_blob(vobj_t, const void* data, size_t len);
void vobj_iset_blob(vobj_t, int idx, const void* data, size_t len);
void* vobj_iget_blob_data(vobj_t, int idx);
size_t vobj_iget_blob_size(vobj_t, int idx);

// serialization
int vobj_set_data(vobj_t, const void* data, size_t len);
int vobj_get_data(const vobj_t, void* data, size_t* len);

__END_DECLS
