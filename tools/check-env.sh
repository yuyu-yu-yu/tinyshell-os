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

printf 'TinyShell OS toolchain: OK\n'
printf '  gcc:  %s\n' "$(gcc -dumpfullversion)"
printf '  qemu: %s\n' "$(qemu-system-i386 --version | head -n 1)"
printf '  grub: %s\n' "$(grub-mkrescue --version | head -n 1)"

