# Cycle 04 integration record

## Scope and boundary

Cycle 04 adds the user-visible Ring 0 shell loop without changing the existing kernel architecture. The real path is:

```text
PS/2 IRQ1 -> bounded keyboard queue -> foreground main loop
          -> line editor -> command parser -> RAMFS / system status
```

The shell and RAMFS still run in Ring 0. This cycle does not add TSS, Ring 3, system calls, user address spaces, preemption, blocking IPC, disk persistence, directories, pipes, redirects, quoting, variables, history, or completion.

## Integration order

The branch was created from `origin/main` at `5bbbda9`. Day 1 modules were merged with independent merge commits in the fixed order, and the original Docker/QEMU matrix passed after every merge:

| Order | Module branch | Module commit | Merge commit | Result |
|---|---|---|---|---|
| 1 | `feature/cycle-04-b-ramfs` | `fa83103` | `b86ea21` | 16/64/128 MiB PASS |
| 2 | `feature/cycle-04-c-shell-parser` | `4802308` | `fcb3046` | 16/64/128 MiB PASS |
| 3 | `feature/cycle-04-a-shell-input` | `bd44a4e` | `316a31e` | 16/64/128 MiB PASS |
| 4 | `feature/cycle-04-d-defense-qa` | `cc3076b` | `5b267b2` | 16/64/128 MiB PASS |

## Runtime initialization and boot order

`shell_runtime_init()` runs with interrupts disabled after the existing PMM, paging, heap and IPC tests. It performs synthetic RAMFS, line-editor and parser tests. On success it resets RAMFS and the live editor again, so the interactive shell starts with an empty filesystem and no sample files.

The existing real PIT/task/IPC flow remains after `sti`. Once that succeeds, `kernel_main` reads and checks a public `system_status` snapshot, emits the new markers, emits the unique `BOOT_OK`, starts the runtime once, and enters the existing `hlt` foreground loop. IRQ1 still only reads one scan code and enqueues a translated character; command parsing, output and RAMFS access occur only after `keyboard_pop_char()` in foreground context.

New markers, each emitted once:

```text
RAMFS_OK
SHELL_INPUT_OK
SHELL_PARSE_OK
SYSTEM_STATUS_OK
SHELL_READY
```

## Command behavior

The prompt is exactly `tiny> `. Runtime implements the frozen command set:

```text
help
clear
echo [text ...]
ls
cat <name>
touch <name>
write <name> <text ...>
append <name> <text ...>
rm <name>
status
about
```

All `echo`, `write`, and `append` text is joined in a fixed 256-byte buffer with a NUL reserved after every copied byte. A non-empty file receives one leading space plus joined text through one non-empty `ramfs_append()` call. The low-level RAMFS remains a raw-byte append API.

`status` prints the fixed five-line PMM, heap, timer, keyboard and task format. `about` explicitly says that Shell and RAMFS currently run in Ring 0.

## Validation evidence

- Original Docker/QEMU boot matrix after Runtime integration: PASS at 16, 64 and 128 MiB.
- PMM free pages: `3565 < 15853 < 32237`.
- All original third-round markers remain, all five new markers appear once, `BOOT_OK` appears once, and no `BOOT_FAIL:` or `PAGE_FAULT` appears.
- Real 64 MiB QEMU monitor `sendkey` path: help, Backspace, echo, touch, write, cat, append, ls, two status snapshots, rm, missing-file error, empty listing, unknown command and Ring 0 about output all PASS.
- The Day 1 interaction script currently performs one incorrect raw-log assertion for Backspace: it searches for contiguous `echo hello` even though the serial terminal correctly emits `echoo\\b \\b hello`. A temporary ignored wrapper skipped only that assertion; all remaining stages passed. D must normalize terminal Backspace bytes or assert the edited command through its output before enabling the script in the standard Makefile target.

## Known limits

RAMFS is volatile and fixed at 16 files of 512 bytes. The frontend is a single foreground Ring 0 loop. There is no user/kernel isolation, persistent storage, concurrent filesystem access, shell process model, or external program execution. These are explicit post-cycle decisions rather than completed features.
