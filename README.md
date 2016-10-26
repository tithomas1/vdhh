# Veertu Desktop Hosted Hypervisor

## Overview

Veertu Desktop Hosted Hypervisor(VDHH) is the core hypervisor platform for
Veertu Desktop product. Veertu Desktop is native virtualization for macOS
platforms to run Linux and Windows VMS built by Veertu Inc. VDHH is a Type 2
hypervisor built on top of Apple macOS Hypervisor.Framework. It uses APIs
provided by the framework for resource scheduling and power management, without
the need to install kernel level extensions (KEXTs). As a result, it's extremely
lightweight, responsive and less resource intensive.

Veertu Desktop supports Vagrant and also has API interface for custom automation.
It supports shared folders and Copy and Paste for Windows VMs. We have also
implemented support for Level 2 Bridged networking and USB Passthrough using
privileged helpers that performs few configuration tasks, without compromising
all the system resources.

## Download

To Install Veertu Desktop on Mac, please visit [veertu.com](https://veertu.com)

## Building

Sources are being tracked in Xcode projects for now. To build VDHH with
corresponding dependencies use __vmx.xcworkspace__ from the project root.

```
xcodebuild -workspace vmx.xcworkspace -scheme vmx
```

## Environment

VDHH could be launched as standalone application from command line

```
vmx -vm <path_to_vm_folder>
```

but to achieve full set of features, it have to be launched by Veertu Desktop
app. Veertu Desktop configures environment of vmx process and contains number of
helper tools, used by vdhh. To start custom version of vdhh the `vdlaunch` tool
(shipped with Veertu Desktop) could be used

```
vdlaunch [-vmx <path_to_custom_vdhh/Contents/MacOS/vmx>] [-vm vm_name_or_path]
```

If __-vmx__ argument is omitted, default (being shipped with Veertu Desktop)
hypervisor will be used to run VM.

## License

VDHH is licensed under the terms of the [GPL v2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).
