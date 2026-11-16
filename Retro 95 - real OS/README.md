# Retro 95 bootable kernel starter

This folder was empty when inspected, so there was no existing OS architecture, kernel, or Limine installation to preserve. This starter now contains a real Limine boot configuration and a small freestanding x86-64 kernel that requests Limine framebuffer, memory-map, and command-line data. It is not a finished operating system or desktop yet.

## Limine and supported appearance

The build pins the official Limine **v12.10.0** release (latest stable release checked on 2026-11-16) and creates a hybrid BIOS/UEFI ISO. The menu entries boot the ELF kernel using Limine's native protocol. The menu includes normal, safe-mode, recovery, diagnostics, and advanced startup choices, with an 8-second default timeout. `limine.conf` is intended to remain user-editable.

Limine natively supports a boot-menu branding line and color, wallpaper, menu/interface resolution, terminal palette/colors, and a terminal font. Its public configuration does **not** expose arbitrary menu layout, button rendering, title-bar widgets, border bevels, selection geometry, or custom per-entry graphics. The project uses Limine's real menu and native keyboard navigation with a clean solid backdrop and classic palette; no wallpaper is loaded. Arrow keys navigate, Enter selects, and the configured countdown/default are handled by Limine itself.

The boot menu's available keyboard help is left enabled. The native interface will not look pixel-for-pixel identical to Windows 95; that would require replacing/modifying Limine's own UI rather than using a supported theme mechanism.

## Desktop, mouse, and clock

The kernel hands directly from Limine to a small interactive desktop drawn into the Limine-provided framebuffer. It has a solid teal workspace, label-only icon selection, the RETRO 95 Start menu, stable-order app buttons, a readable RTC clock, and narrower app-sized movable windows. All windows except the fixed-size Calculator can be resized from the lower-right corner; resizing keeps the top-left corner anchored and each app has a content-safe minimum size. The title-bar square or Alt+O maximizes/restores the foreground app; other open windows remain available, and clicking a visible background window brings it forward. Default app dimensions are fitted to content to reduce excess side and bottom space. Buttons recess while pressed and rise on release. A click activates only after release inside the button; holding longer than 500 ms cancels the action. Quick clicks remain visibly pressed for at least 60 ms. The display backbuffer reduces flicker; mouse and clock changes update only small screen regions. PS/2 mouse polling is rate-limited to 250 Hz. Single-click selects desktop icon labels; double-click opens. Drag the desktop to select icons or a title bar to move a window. Multiple apps remain open; focusing one does not reorder taskbar buttons. The `_` title-bar button minimizes, the taskbar restores or hides, and `X` closes only that app. The clock reads the PC CMOS/RTC. Built-ins are My Computer, a multiline Notepad, a mouse-operated integer Calculator, Control Panel, and Help. Notepad supports Enter, four-space Tab, backspace across line breaks, and a Save RAM/Discard/Cancel close prompt. Saved notes are volatile RAM copies and are lost at power-off. Shift and Caps Lock control letter case; Notepad input goes only to the foreground app. Storage/networking and full recovery tools remain unimplemented.

There are no required kernel modules in this starter, so the configuration does not request non-existent modules. Add `module_path` entries only when the kernel actually needs a module and can consume its Limine module response.

## Build and test in QEMU (UEFI)

The project can be built and tested entirely on Windows; WSL is not used. This machine has MSYS2 UCRT64, native Clang/LLD, GNU make, curl, unzip, xorriso, Python 3, QEMU, and EDK2 firmware installed. The build downloads Limine's official v12.10.0 binary release and writes generated files only inside `build/`.

1. Open an MSYS2 UCRT64 shell and ensure `/ucrt64/bin` and `/usr/bin` are on `PATH`.
2. From this project directory, run `make iso` to compile the ELF kernel and create `build/retro95.iso`.
3. Run `make qemu-uefi` to boot the ISO in native Windows QEMU using EDK2 pflash firmware. The target creates a disposable UEFI variable file at `build/edk2-vars.fd`; it does not touch host firmware variables. Override `QEMU`, `OVMF_CODE`, or `OVMF_VARS` if your MSYS2 paths differ.
4. Optionally test BIOS with QEMU's `-cdrom build/retro95.iso -boot d` options.

Native Windows build and UEFI QEMU boot were performed. The first boot exposed a Limine ELF page-permission panic; the kernel linker script now emits page-aligned RX, R, and RW program headers. The rebuilt ISO boots into the interactive desktop in QEMU. No host firmware settings or existing bootloader were changed.

## USB and physical hardware

Do not install Limine onto an internal disk during development. After the ISO has booted successfully in QEMU, write `build/retro95.iso` to a disposable USB drive with a trusted image-writing utility, then choose that USB device from the firmware's one-time boot menu. Writing an image erases the selected USB device—verify the target carefully. This starter has not been validated on physical hardware and should not be used as a daily-driver OS.

For UEFI Secure Boot, firmware must trust a signed Limine EFI executable and the configuration/kernel integrity policy must be deliberately configured. This development image is not signed and Secure Boot compatibility is not claimed.

## Files

- `limine.conf` — actual Limine menu configuration and boot entries.
- `kernel/kernel.c`, `kernel/limine.h` — freestanding kernel and the minimal upstream protocol declarations it consumes.
- `linker.ld` — x86-64 high-half ELF link layout.
- `scripts/build-image.sh` — pinned dependency fetch, kernel build, and hybrid ISO creation.

Upstream references: [Limine v12.10.0](https://github.com/Limine-Bootloader/Limine/tree/v12.10.0), [Limine configuration reference](https://github.com/Limine-Bootloader/Limine/blob/v12.10.0/CONFIG.md), [Limine BIOS/UEFI ISO instructions](https://github.com/Limine-Bootloader/Limine/blob/v12.10.0/USAGE.md#biosuefi-hybrid-iso-creation), and [Limine boot protocol](https://github.com/Limine-Bootloader/limine-protocol/blob/trunk/PROTOCOL.md).
