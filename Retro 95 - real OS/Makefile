SHELL := /bin/bash
OVMF_CODE ?= /ucrt64/share/qemu/edk2-x86_64-code.fd
OVMF_VARS ?= build/edk2-vars.fd
QEMU ?= /ucrt64/bin/qemu-system-x86_64.exe
.PHONY: iso qemu-uefi clean

iso:
	bash scripts/build-image.sh

qemu-uefi: iso
	@test -f "$(OVMF_CODE)" || (echo "UEFI firmware not found: $(OVMF_CODE)"; exit 1)
	@test -f "$(OVMF_VARS)" || qemu-img create -f raw "$(OVMF_VARS)" 540672
	$(QEMU) -machine q35 -m 512M -smp 2 \
		-drive if=pflash,format=raw,unit=0,readonly=on,file="$(OVMF_CODE)" \
		-drive if=pflash,format=raw,unit=1,file="$(OVMF_VARS)" \
		-netdev user,id=net0,hostfwd=tcp::8080-:80 \
		-device e1000,netdev=net0 \
		-cdrom build/retro95.iso -boot d -no-reboot

clean:
	rm -rf build
