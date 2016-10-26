#include "vlaunch.h"
#include "vmsg.h"
#include "vobj.h"
#include "log.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <unistd.h>
#include <spawn.h>
#include <vector>
#include <memory>
#include <cassert>
#include <cerrno>
#include <string>

// -----------------------------------------------------------------------------

namespace {

int process_msg_nope(const vobj_t cmd, vobj_t rsp) {
    // send OK status back
    vobj_set_llong(rsp, "status", 0);
    return 0;
}

// launch tool with path and arguments provided
// in the cmd
// TODO: support launchd.plist??
// TODO: create pipe and route it to the tool
// TODO: control pid and signal it exited or restart silently
int process_msg_launch(const vobj_t msg, vobj_t rsp) {
    std::string path(vobj_get_str(msg, VLAUNCH_KEY_PATH));

    // prepare argv vector
    std::unique_ptr<char*[]> argv;
    if (vobj_t args = vobj_get_obj(msg, VLAUNCH_KEY_ARGV)) {
        auto count = vobj_get_count(args);
        if (count > 0) {
            argv = std::unique_ptr<char*[]>(new char*[count + 1]);
            for (int idx = 0; idx < count; ++idx)
                argv.get()[idx] = const_cast<char*>(vobj_iget_str(args, idx));
            argv.get()[count] = NULL;
        }
    }

    // prepare envp vector
    std::unique_ptr<char*[]> envp;
    if (vobj_t env = vobj_get_obj(msg, VLAUNCH_KEY_ENVP)) {
        auto count = vobj_get_count(env);
        if (count > 0) {
            envp = std::unique_ptr<char*[]>(new char*[count + 1]);
            for (int idx = 0; idx < count; ++idx)
                envp.get()[idx] = const_cast<char*>(vobj_iget_str(env, idx));
            envp.get()[count] = NULL;
        }
    }

    // configure the environment (prevent inheritance of descriptors)
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_CLOEXEC_DEFAULT);

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);

    // configure stdin & stdout descriptors
    if (vobj_get_str(msg, "stdin"))
        posix_spawn_file_actions_addopen(&fa, fileno(stdin),
            vobj_get_str(msg, "stdin"), O_RDONLY, 0);
    if (vobj_get_str(msg, "stdout"))
     	posix_spawn_file_actions_addopen(&fa, fileno(stdout),
     	    vobj_get_str(msg, "stdout"), O_WRONLY, 0);

    // spawn
    pid_t pid = 0;
    int res = posix_spawn(&pid, path.c_str(), &fa, &attr, argv.get(), envp.get());

    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&fa);

    if (0 != res) {
        LOG("Failed to spawn %s, status %d", path.c_str(), res);
        // send status back
        vobj_set_llong(rsp, "status", -1);
        vobj_set_llong(rsp, "errno", errno);
        return 0;
    }

    LOG("Spawned %s, pid %d", path.c_str(), pid);
    if (vobj_get_llong(msg, "wait")) {
        int status = 0;
        int ret = 0;
        while ((ret = waitpid(pid, &status, 0)) == -1 && EINTR == errno);
        if (ret == pid) {
            LOG("Pid %d exited with status %u", pid, WEXITSTATUS(status));
            vobj_set_llong(rsp, "status", WEXITSTATUS(status));
        } else {
            ERR("Failed to waitpid(%d), error %d", pid, errno);
            vobj_set_llong(rsp, "status", -1);
            vobj_set_llong(rsp, "errno", errno);
        }
    } else {
        vobj_set_llong(rsp, "pid", pid);
        vobj_set_llong(rsp, "status", 0);
    }

    return 0;
}

int process_msg(const vobj_t msg, vobj_t rsp) {
    switch(vobj_get_llong(msg, VLAUNCH_KEY_CMD)) {
        default:
        case VLAUNCH_CMD_NOPE:
            // also incomplete msgs will be handled here :/
            return process_msg_nope(msg, rsp);

        case VLAUNCH_CMD_LAUNCH:
            // also incomplete msgs will be handled here :/
            return process_msg_launch(msg, rsp);

        case VLAUNCH_CMD_STOP:
            // also incomplete msgs will be handled here :/
            return process_msg_nope(msg, rsp);
    }
}

} // namespace

extern "C" {

int vlaunch_run_once(int ifd, int ofd) {
    vobj_t msg = vobj_create();
    ssize_t len = vmsg_read(ifd, msg);
    if (len > 0) {
        // prepare response
        auto rsp = vobj_create();
        vobj_set_llong(rsp, "msgid", vobj_get_llong(msg, "msgid"));
        len = (0 == process_msg(msg, rsp)) ? vmsg_write(ofd, rsp) : -1;
        vobj_dispose(rsp);
    }
    vobj_dispose(msg);
    return len;
}

// receive and process commands from fd
int vlaunch_run(int ifd, int ofd) {
    ssize_t len = 0;
    vobj_t msg = vobj_create();
    while((len = vmsg_read(ifd, msg)) > 0) {
        // process msg
        auto rsp = vobj_create();
        vobj_set_llong(rsp, "msgid", vobj_get_llong(msg, "msgid"));
        if (0 == process_msg(msg, rsp)) {
            vmsg_write(ofd, rsp);
        }
        vobj_dispose(rsp);
    }
    vobj_dispose(msg);
    return len;
}

} // extern "C"
