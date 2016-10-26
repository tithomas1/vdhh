#pragma once

#include <sys/cdefs.h>

__BEGIN_DECLS

#define VLAUNCH_KEY_CMD     "cmd"

#define VLAUNCH_CMD_NOPE    0       // do nothing but send OK back
#define VLAUNCH_CMD_LAUNCH  1       // launch tool provided in arguments
#define VLAUNCH_CMD_STOP    2       // stop the tool, by sending SIGSTOP/SIGKILL pair

#define VLAUNCH_KEY_PATH    "path"  // str
#define VLAUNCH_KEY_ARGV    "argv"  // vobj(array)
#define VLAUNCH_KEY_ENVP    "envp"  // vobj(array)

/// \brief run main loop of vlaunch, receiving messages from
//         ifd, and sending responses to ofd file descriptors
int vlaunch_run(int ifd, int ofd);

/// \brief single run of processing circle
int vlaunch_run_once(int ifd, int ofd);

__END_DECLS
