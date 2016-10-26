#include "../vobj.h"
#include <gtest/gtest.h>
#include <uuid/uuid.h>
#include <string>

TEST(vobj, alloc) {
    // create dispose
    vobj_dispose(vobj_create());
}

TEST(vobj, dict) {
    auto v = vobj_create();
    vobj_set_llong(v, "1", 1);
    vobj_set_llong(v, "3", 2);
    vobj_set_llong(v, "4", 3);

    EXPECT_EQ(vobj_get_llong(v, "1"), 1);
    EXPECT_EQ(vobj_get_llong(v, "3"), 2);
    EXPECT_EQ(vobj_get_llong(v, "4"), 3);

    {
        std::string s1("1");
        vobj_set_str(v, "1", s1.c_str());
    }
    EXPECT_EQ(std::string(vobj_get_str(v, "1")), std::string("1"));

    uuid_t uid;
    uuid_generate_random(uid);
    {
        auto v2 = vobj_create();
        vobj_set_str(v2, "hello", "world");

        vobj_set_blob(v2, "blob", uid, sizeof(uid));
        vobj_set_obj(v, "2", v2);
        vobj_dispose(v2);

        EXPECT_TRUE(NULL != vobj_get_obj(v, "2"));
    }

    // serialization
    {
        char buf[1024];
        auto len = sizeof(buf);
        EXPECT_EQ(vobj_get_data(v, buf, &len), 0);
        EXPECT_EQ(vobj_set_data(v, buf, len), 0);
    }

    EXPECT_EQ(std::string(vobj_get_str(v, "1")), std::string("1"));

    auto v2 = vobj_get_obj(v, "2");

    EXPECT_EQ(strcmp(vobj_get_str(v2, "hello"), "world"), 0);
    EXPECT_EQ(memcmp(uid, vobj_get_blob_data(v2, "blob"), vobj_get_blob_size(v2, "blob")), 0);

    vobj_dispose(v);
}

TEST(vobj, array) {
    auto v = vobj_create();
    vobj_add_str(v, "hello");
    vobj_add_str(v, "world");

    EXPECT_EQ(strcmp(vobj_iget_str(v, 0), "hello"), 0);
    EXPECT_EQ(strcmp(vobj_iget_str(v, 1), "world"), 0);

    // serialization
    {
        char buf[1024];
        auto len = sizeof(buf);
        EXPECT_EQ(vobj_get_data(v, buf, &len), 0);
        EXPECT_EQ(vobj_set_data(v, buf, len), 0);
    }

    EXPECT_EQ(strcmp(vobj_iget_str(v, 0), "hello"), 0);
    EXPECT_EQ(strcmp(vobj_iget_str(v, 1), "world"), 0);
}
