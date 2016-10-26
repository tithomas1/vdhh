#include "../vlaunch.h"
#include <gtest/gtest.h>

TEST(vlaunch, serialize) {
    char tmpl[] = "/tmp/vtest.XXXX";
    int fd = mkstemps(tmpl, 4);
    EXPECT_NE(fd, -1);

    // read from empty file
    vlaunch_run(fd, fileno(stdout));

    close(fd);
    unlink(tmpl);
}
