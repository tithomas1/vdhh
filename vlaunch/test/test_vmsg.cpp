#include "../vmsg.h"
#include <gtest/gtest.h>
#include <uuid/uuid.h>
#include <string>

TEST(vmsg, pipe) {
    int p[2] = {-1, -1};
    pipe(p);

    // write
    auto d1 = vobj_create();
    vobj_set_llong(d1, "1", 100500);
    vobj_set_str(d1, "2", "abc");

    uuid_t uid;
    uuid_generate_random(uid);
    vobj_set_blob(d1, "0", uid, sizeof(uid));

    auto l1 = vmsg_write(p[1], d1);
    EXPECT_GT(l1, 0);

    // read
    if (l1 > 0) {
        auto d2 = vobj_create();
        auto l2 = vmsg_read(p[0], d2);
        EXPECT_EQ(l1, l2);

        // check
        EXPECT_EQ(vobj_get_llong(d1, "1"), vobj_get_llong(d2, "1"));
        EXPECT_EQ(std::string(vobj_get_str(d1, "2")),
                  std::string(vobj_get_str(d2, "2")));
        EXPECT_EQ(memcmp(uid, vobj_get_blob_data(d2, "0"),
                              vobj_get_blob_size(d2, "0")), 0);
    }
}
