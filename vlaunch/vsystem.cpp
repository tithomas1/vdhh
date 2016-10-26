//
//  vsystem.h
//  vlaunch
//
//  Created by Boris Remizov on 02/10/16.
//  Copyright © 2016 Veertu. All rights reserved.
//

#include "vlaunch.h"
#include "vmsg.h"
#include "vobj.h"
#include <string>
#include <sstream>
#include <vector>

namespace {

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) {
        if (item.size() > 0)
            elems.push_back(item);
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return std::move(elems);
}

} // namespace

extern "C" {

int vlaunchfd[2] = {-1, -1};

int vsystem(const char* command, int wait) {
    if (-1 == vlaunchfd[1]) {
        // no communucation pipe
        return -1;
    }

    // run the command
    vobj_t msg = vobj_create();
    vobj_set_llong(msg, VLAUNCH_KEY_CMD, VLAUNCH_CMD_LAUNCH);

    size_t cmdlen = strlen(command);

    // infer arguments from string,
    auto args = split(command, ' ');
    const char* path = args[0].c_str();
    vobj_set_str(msg, VLAUNCH_KEY_PATH, path);
    if (args.size() > 1) {
        vobj_t argv = vobj_create();
        for (auto const& arg : args)
            vobj_add_str(argv, arg.c_str());
        vobj_set_obj(msg, VLAUNCH_KEY_ARGV, argv);
        vobj_dispose(argv);
    }

    vobj_set_llong(msg, "wait", wait);

    if (vmsg_write(vlaunchfd[1], msg) <= 0) {
        vobj_dispose(msg);
        return -1;
    }

    // get status
    int status = 0;
    if (-1 != vlaunchfd[0]) {
        status = vmsg_read(vlaunchfd[0], msg) > 0 ?
            (int)vobj_get_llong(msg, "status") : 127;
    }

    vobj_dispose(msg);
    
    return status;
}

} // extern "C"
