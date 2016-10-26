#include "hw.h"

/* Stub functions for binaries that never call vmx_devices_reset(),
 * and don't need to keep track of the reset handler list.
 */

void vmx_register_reset(QEMUResetHandler *func, void *opaque)
{
}

void vmx_unregister_reset(QEMUResetHandler *func, void *opaque)
{
}
