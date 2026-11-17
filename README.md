# Retro-95
RETRO 95 – An experimental, AI-assisted custom operating system written in C and Assembly, inspired by the classic Windows 95 desktop experience. Built to push AI-assisted development in Visual Studio Code to its limits and explore whether AI could help create an entire operating system from scratch.

<img width="1028" height="772" alt="image" src="https://github.com/user-attachments/assets/35a62655-ba2b-4c3f-b833-066950af3096" />

## Demo

https://github.com/user-attachments/assets/a3d81e1a-5fe4-4c2c-8155-4b84accd09d8

## Description

An experiment to push AI-assisted programming to its limits.

RETRO 95 is an experimental operating system developed with the help of AI inside Visual Studio Code.

The original goal was simple: push AI as far as possible and see whether it could help develop an entire operating system, rather than just small applications or scripts.

What started as an experiment turned into a custom operating system with its own kernel, hardware drivers, graphical desktop environment, window manager and built-in applications.

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
Development environment
C
Assembly
Make
Visual Studio Code
AI-assisted development
Current limitations

RETRO 95 is still a work in progress.

Storage drivers and persistent disk storage have not yet been implemented. Files and data currently reside in RAM.

Why this project exists

This project explores how far AI-assisted software development can go when applied to something as complex as operating-system development.

It is also an experiment in collaboration between human direction, AI-generated code, debugging and iterative development.

Disclaimer

RETRO 95 is an independent experimental project. It is not affiliated with, endorsed by, or sponsored by Microsoft.

Windows 95 is referenced solely as inspiration for the retro desktop aesthetic.

-------------------------------------------------------------------------------------------------------------------------------------------------------

🚀 How to Run RETRO 95

RETRO 95 is provided as a bootable ISO image.

Finding the ISO
Open the downloaded project folder named Retro 95 - real OS.
Open the folder named build.
Locate retro95.iso.
Select this ISO file when creating or configuring your virtual machine.

You can use the ISO with:

QEMU
Oracle VirtualBox
Compatible physical hardware

For physical hardware, write the ISO to a suitable bootable USB drive and boot from it. Make sure you understand that booting on real hardware can carry risks, so back up important data first.

⚠️ Troubleshooting: VirtualBox 64-bit Mode

If you cannot boot RETRO 95 in VirtualBox and receive an error such as:

64-bit mode is not supported for this CPU

Don't worry! QEMU is an alternative emulator you can try.

VirtualBox workaround (Windows)

You can try enabling x86 long mode for your virtual machine.

1. Open Command Prompt as Administrator.

2. Navigate to the VirtualBox installation directory:

cmd
cd "C:\Program Files\Oracle\VirtualBox"


3. Enable x86 long mode:

Replace Your-VM-Name with the actual name of your virtual machine.

cmd
VBoxManage modifyvm "Your-VM-Name" --x86-long-mode on


4. Restart VirtualBox and try booting RETRO 95 again.

Important: This setting cannot add 64-bit CPU capabilities to an unsupported processor. Your CPU and virtualization configuration must support the required operating mode. If the issue persists, check whether Intel VT-x or AMD-V is enabled and available.

If VirtualBox still doesn't work, try running RETRO 95 using QEMU instead.

Thank you for checking out RETRO 95!

This project is an ongoing experiment in AI-assisted operating-system development. Bugs, missing features and limitations are expected while development continues.

Enjoy exploring RETRO 95! ❤️
