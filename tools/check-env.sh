#!/usr/bin/env bash

set -euo pipefail

required_commands=(
    gcc
    ld
    make
    grub-file
    grub-mkrescue
    qemu-system-i386
    xorriso
    gdb
    nasm
    python3
)

missing=0
for command_name in "${required_commands[@]}"; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        printf 'MISSING: %s\n' "${command_name}" >&2
        missing=1
    fi
done

if [[ "${missing}" -ne 0 ]]; then
    exit 1
fi

if ! printf 'int main(void) { return 0; }\n' \
    | gcc -m32 -x c -c -o /tmp/tinyos-env-check.o - 2>/dev/null; then
    printf 'ERROR: gcc cannot compile 32-bit objects with -m32\n' >&2
    exit 1
fi
rm -f /tmp/tinyos-env-check.o

gcc_version="$(gcc -dumpfullversion)"
qemu_version="$(qemu-system-i386 --version | head -n 1)"
grub_version="$(grub-mkrescue --version | head -n 1)"

if [[ "${gcc_version}" != 13.* ]]; then
    printf 'ERROR: expected GCC 13.x, found %s\n' "${gcc_version}" >&2
    exit 1
fi

if [[ "${qemu_version}" != *"version 8."* ]]; then
    printf 'ERROR: expected QEMU 8.x, found %s\n' "${qemu_version}" >&2
    exit 1
fi

if [[ "${grub_version}" != *" 2.12"* ]]; then
    printf 'ERROR: expected GRUB 2.12, found %s\n' "${grub_version}" >&2
    exit 1
fi

printf 'TinyShell OS toolchain: OK\n'
printf '  gcc:  %s\n' "${gcc_version}"
printf '  qemu: %s\n' "${qemu_version}"
printf '  grub: %s\n' "${grub_version}"
