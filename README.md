Retro-95

RETRO 95 – An experimental, AI-assisted custom operating system written in C and Assembly, inspired by the classic Windows 95 desktop experience. Built to push AI-assisted development in Visual Studio Code to its limits and explore whether AI could help create an entire operating system from scratch.

<img width="1028" height="772" alt="image" src="https://github.com/user-attachments/assets/35a62655-ba2b-4c3f-b833-066950af3096" />

Demo

https://github.com/user-attachments/assets/a3d81e1a-5fe4-4c2c-8155-4b84accd09d8

Description

An experiment to push AI-assisted programming to its limits.

RETRO 95 is an experimental operating system developed with the help of AI inside Visual Studio Code.

The original goal was simple: push AI as far as possible and see whether it could help develop an entire operating system, rather than just small applications or scripts.

What started as an experiment turned into a custom operating system with its own kernel, hardware drivers, graphical desktop environment, window manager and built-in applications.

RETRO 95 Versions

RETRO 95 currently has two versions: V1 (Original) and V2 (Expanded Edition).

Both versions are available as separate releases and represent different stages of the project's development.

RETRO 95 V1 – Original Edition

The original experimental release.

V1 established the foundation of RETRO 95, featuring its custom kernel, bootloader integration, graphical desktop environment, window manager and built-in applications.

What's currently working?

Custom kernel written in C and Assembly
Bootloader integration
Multitasking
Custom interrupt handlers
Keyboard and mouse drivers
Mouse movement and clicking
Window dragging, resizing and minimising
Custom Start menu and taskbar
Interactive desktop environment
Working Calculator
Working Notepad
Control Panel
My Computer
BIOS RTC-based clock
Memory detection
Button interaction animations

V1 limitations:

Networking functionality is not included.
Storage drivers and persistent disk storage are not implemented.
Files and data currently reside in RAM.

V1 remains available as the original release of RETRO 95.

RETRO 95 V2 – Expanded Edition

V2 builds upon the original V1 foundation and introduces additional operating-system functionality, hardware support and improvements.

What's new in V2?

Built-in networking driver support
Network connectivity and internet access
Storage driver support
Persistent disk storage capabilities
Expanded hardware support
Additional operating-system functionality
Further improvements to the graphical desktop environment

V2 represents the next stage of RETRO 95's development, moving beyond the original experimental foundation towards a more complete operating system.

Note: Networking and storage functionality depend on supported hardware, drivers and virtual machine configuration.

Development Environment
C
Assembly
Make
Visual Studio Code
AI-assisted development
Current Development Status & Known Issues

Both RETRO 95 V1 and V2 are still works in progress and remain under active development.

Although both versions feature a functional graphical desktop environment, users may encounter small GUI bugs, visual glitches, unexpected behaviour and incomplete features.

Some known areas that may require further improvements include:

Minor graphical glitches and desktop rendering issues
Window dragging, resizing and minimising inconsistencies
Occasional button interaction issues
Desktop elements that may not always behave as expected
Incomplete application functionality
Hardware compatibility limitations

V2 introduces additional functionality, including networking and storage support, but these features may also have limitations depending on the hardware and virtual machine configuration.

Please keep in mind:

Neither V1 nor V2 is considered a finished operating system.
Minor GUI bugs and glitches are expected.
Features may change or improve between releases.
Development, testing, debugging and optimisation are ongoing.

The project will continue to evolve as new features are implemented and existing issues are addressed.

Why this project exists

This project explores how far AI-assisted software development can go when applied to something as complex as operating-system development.

It is also an experiment in collaboration between human direction, AI-generated code, debugging and iterative development.

Disclaimer

RETRO 95 is an independent experimental project. It is not affiliated with, endorsed by, or sponsored by Microsoft.

Windows 95 is referenced solely as inspiration for the retro desktop aesthetic.

🚀 How to Run RETRO 95

RETRO 95 is provided as a bootable ISO image.

Finding the ISO
Open the downloaded project folder named Retro 95 - real OS.
Open the folder named build.
Locate the ISO image for the version you want to run.
Select this ISO file when creating or configuring your virtual machine.

Make sure you select the correct ISO for V1 or V2.

You can use the ISO with:

QEMU
Oracle VirtualBox
Compatible physical hardware

For physical hardware, write the ISO to a suitable bootable USB drive and boot from it. Make sure you understand that booting on real hardware can carry risks, so back up important data first.

🌐 Getting Internet Working (V2 Only)

RETRO 95 V2 includes networking driver support, allowing it to connect to a network when used with compatible virtual network hardware.

Important: Internet connectivity instructions apply to V2. V1 does not include the networking functionality introduced in V2.

QEMU – Internet Configuration

QEMU supports user-mode networking, which provides NAT-based internet connectivity through your host computer.

When launching RETRO 95 V2, configure the virtual network adapter using:

bash
-netdev user,id=net0 \
-device e1000,netdev=net0


This configuration:

Enables QEMU user-mode NAT networking.
Emulates an Intel E1000 Ethernet adapter.
Allows the guest operating system to access external networks through the host.

Make sure your host computer has an active internet connection.

Oracle VirtualBox 7.2.20 – Internet Configuration

VirtualBox can emulate an Intel E1000 network adapter, which can be used with RETRO 95 V2.

Step-by-step instructions
Shut down your RETRO 95 V2 virtual machine.
Open Oracle VirtualBox Manager.
Select your RETRO 95 V2 virtual machine.
Open Settings.
Navigate to Network → Adapter 1.
Enable Enable Network Adapter.
Under Attached to, select NAT.
Expand Advanced.
Under Adapter Type, select:

Intel PRO/1000 MT Desktop (82540EM)

Make sure Cable Connected is enabled.
Click OK.
Start RETRO 95 V2 and allow the network interface time to initialise.
Recommended VirtualBox Network Settings
Setting	Value
Attached to	NAT
Adapter Type	Intel PRO/1000 MT Desktop (82540EM)
Promiscuous Mode	Deny
Cable Connected	Enabled
Adapter Enabled	Yes
Alternative: Configure Using VBoxManage

On Windows, open Command Prompt and run:

cmd
VBoxManage modifyvm "YourVM" --nic1 nat --nictype1 82540EM


Replace YourVM with the actual name of your virtual machine.

Networking Troubleshooting

If internet connectivity does not work:

Check that the network adapter is enabled.
Confirm that Cable Connected is enabled.
Make sure you selected the correct E1000 adapter model.
Check that your host computer has an active internet connection.
Allow time for the network driver to initialise.
Restart the virtual machine after changing network settings.

Important: Selecting an E1000 adapter and enabling NAT does not automatically guarantee internet connectivity. RETRO 95 V2 must successfully initialise its networking driver and configure its network connection.

💾 Storage Support (V2)

RETRO 95 V2 introduces storage driver support and persistent disk storage capabilities.

This is an expansion beyond the original V1 release, which relies on RAM for its files and data.

Storage functionality depends on the supported virtual disk controller and hardware configuration.

⚠️ Troubleshooting: VirtualBox 64-bit Mode

If you cannot boot RETRO 95 in VirtualBox and receive an error such as:

64-bit mode is not supported for this CPU

Don't worry! QEMU is an alternative emulator you can try.

VirtualBox workaround (Windows)

You can try enabling x86 long mode for your virtual machine.

Open Command Prompt as Administrator.
Navigate to the VirtualBox installation directory:
cmd
cd "C:\Program Files\Oracle\VirtualBox"

Enable x86 long mode:

Replace Your-VM-Name with the actual name of your virtual machine.

cmd
VBoxManage modifyvm "Your-VM-Name" --x86-long-mode on

Restart VirtualBox and try booting RETRO 95 again.

Important: This setting cannot add 64-bit CPU capabilities to an unsupported processor. Your CPU and virtualization configuration must support the required operating mode. If the issue persists, check whether Intel VT-x or AMD-V is enabled and available.

If VirtualBox still doesn't work, try running RETRO 95 using QEMU instead.

Thank You!

Thank you for checking out RETRO 95!

Whether you're exploring the original V1 release or trying the expanded V2 edition, you're taking part in an ongoing experiment in AI-assisted operating-system development.

Both versions are still being developed, and bugs, minor GUI issues, missing features and limitations are expected while development continues.

Two versions. One experiment. Pushing AI-assisted operating-system development further.

Enjoy exploring RETRO 95! ❤️
