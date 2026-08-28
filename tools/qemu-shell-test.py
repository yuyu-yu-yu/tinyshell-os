#!/usr/bin/env python3
"""Drive TinyShell through QEMU monitor sendkey (real IRQ1), not C handlers.

Serial Backspace is emitted as the terminal sequence BS-space-BS. Assertions
that inspect typed commands must reconstruct visible text; they must not
require the raw log to contain a contiguous edited command string.
"""

from __future__ import annotations

import os
import re
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional


KEY_HOLD_MS = 5
KEY_GAP_S = 0.020
BOOT_TIMEOUT_S = 30.0
COMMAND_TIMEOUT_S = 20.0
OVERALL_TIMEOUT_S = 120.0
POLL_S = 0.05
PROMPT = "tiny> "

HELP_COMMANDS = (
    "help",
    "clear",
    "echo",
    "ls",
    "touch",
    "write",
    "append",
    "cat",
    "rm",
    "status",
    "about",
)

STATUS_RE = re.compile(
    r"PMM pages: free=(\d+) total=(\d+)\n"
    r"Heap bytes: free=(\d+) total=(\d+) largest=(\d+) blocks=(\d+)\n"
    r"Timer: ticks=(\d+) irq0=(\d+)\n"
    r"Keyboard: dropped=(\d+)\n"
    r"Tasks: switches=(\d+) finished=(\d+)",
    re.MULTILINE,
)

SPECIAL_KEYS = {
    " ": "spc",
    "\n": "ret",
    "\r": "ret",
    "\b": "backspace",
}


class StageError(Exception):
    def __init__(self, stage: str, message: str, excerpt: str = "") -> None:
        super().__init__(f"{stage}: {message}")
        self.stage = stage
        self.excerpt = excerpt


def visible_serial(text: str) -> str:
    """Replay BS and the console BS-space-BS erase sequence."""
    visible: list[str] = []
    index = 0
    length = len(text)
    while index < length:
        if text.startswith("\b \b", index):
            if visible:
                visible.pop()
            index += 3
            continue
        if text[index] == "\b":
            if visible:
                visible.pop()
            index += 1
            continue
        visible.append(text[index])
        index += 1
    return "".join(visible)


def visible_serial_selfcheck() -> None:
    sample = "echoo\b \b hello\nhello\n" + PROMPT
    expected = "echo hello\nhello\n" + PROMPT
    recovered = visible_serial(sample)
    if recovered != expected:
        raise StageError(
            "selfcheck",
            f"backspace normalize failed: {recovered!r}",
            sample,
        )


class QemuShellSession:
    def __init__(
        self,
        iso_path: Path,
        serial_path: Path,
        qemu_bin: str,
        memory: str,
    ) -> None:
        self.iso_path = iso_path
        self.serial_path = serial_path
        self.qemu_bin = qemu_bin
        self.memory = memory
        self.pid = os.getpid()
        self.monitor_path = Path(f"/tmp/tinyshell-qemu-monitor-{self.pid}.sock")
        self.process: Optional[subprocess.Popen[bytes]] = None
        self.monitor: Optional[socket.socket] = None
        self.deadline = time.monotonic() + OVERALL_TIMEOUT_S

    def remaining(self) -> float:
        return max(0.0, self.deadline - time.monotonic())

    def start(self) -> None:
        if self.monitor_path.exists():
            self.monitor_path.unlink()
        if self.serial_path.exists():
            self.serial_path.unlink()
        self.serial_path.parent.mkdir(parents=True, exist_ok=True)

        args = [
            self.qemu_bin,
            "-cdrom",
            str(self.iso_path),
            "-m",
            self.memory,
            "-display",
            "none",
            "-serial",
            f"file:{self.serial_path}",
            "-monitor",
            f"unix:{self.monitor_path},server,nowait",
            "-no-reboot",
            "-no-shutdown",
        ]
        self.process = subprocess.Popen(
            args,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self._connect_monitor("qemu-start")

    def _connect_monitor(self, stage: str) -> None:
        timeout = min(BOOT_TIMEOUT_S, self.remaining())
        end = time.monotonic() + timeout
        last_error = "monitor socket missing"
        while time.monotonic() < end:
            self._check_alive(stage)
            if self.monitor_path.exists():
                try:
                    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    sock.settimeout(1.0)
                    sock.connect(str(self.monitor_path))
                    self.monitor = sock
                    self._drain_monitor()
                    return
                except OSError as exc:
                    last_error = str(exc)
                    if self.monitor is not None:
                        self.monitor.close()
                        self.monitor = None
            time.sleep(POLL_S)
        raise StageError(stage, f"cannot connect to QEMU monitor ({last_error})")

    def _drain_monitor(self) -> None:
        if self.monitor is None:
            return
        self.monitor.settimeout(0.2)
        try:
            while True:
                chunk = self.monitor.recv(4096)
                if not chunk:
                    break
        except (socket.timeout, BlockingIOError, OSError):
            pass
        self.monitor.settimeout(1.0)

    def _check_alive(self, stage: str) -> None:
        if self.process is None:
            raise StageError(stage, "QEMU process was not started")
        code = self.process.poll()
        if code is not None:
            raise StageError(stage, f"QEMU exited early with status {code}")

    def serial_offset(self) -> int:
        if not self.serial_path.exists():
            return 0
        return self.serial_path.stat().st_size

    def read_since(self, offset: int) -> str:
        if not self.serial_path.exists():
            return ""
        data = self.serial_path.read_bytes()[offset:]
        return data.replace(b"\r", b"").decode("latin-1", errors="replace")

    def wait_for(self, needle: str, stage: str, offset: int, timeout: float) -> str:
        limit = min(timeout, self.remaining())
        end = time.monotonic() + limit
        excerpt = ""
        while time.monotonic() < end:
            self._check_alive(stage)
            excerpt = self.read_since(offset)
            if needle in excerpt:
                return excerpt
            time.sleep(POLL_S)
        raise StageError(
            stage,
            f"timeout waiting for {needle!r}",
            excerpt,
        )

    def monitor_command(self, command: str, stage: str) -> None:
        if self.monitor is None:
            raise StageError(stage, "monitor socket is closed")
        payload = (command + "\n").encode("ascii")
        try:
            self.monitor.sendall(payload)
        except OSError as exc:
            raise StageError(stage, f"monitor write failed: {exc}") from exc
        self._drain_monitor()

    def qemu_key(self, name: str) -> str:
        if len(name) == 1 and ("a" <= name <= "z" or "0" <= name <= "9"):
            return name
        if name in SPECIAL_KEYS:
            return SPECIAL_KEYS[name]
        raise StageError("sendkey", f"unsupported character {name!r}")

    def send_text(self, text: str, stage: str) -> None:
        if KEY_GAP_S * 1000.0 <= KEY_HOLD_MS:
            raise StageError(stage, "key gap must be greater than hold")
        for character in text:
            key = self.qemu_key(character)
            self.monitor_command(f"sendkey {key} {KEY_HOLD_MS}", stage)
            time.sleep(KEY_GAP_S)

    def run_line(self, text: str, stage: str) -> tuple[int, str]:
        cursor = self.serial_offset()
        self.send_text(text + "\n", stage)
        output = self.wait_for(PROMPT, stage, cursor, COMMAND_TIMEOUT_S)
        return cursor, output

    def expect_contains(self, stage: str, output: str, *needles: str) -> None:
        for needle in needles:
            if needle not in output:
                raise StageError(stage, f"missing {needle!r}", output)

    def expect_line(self, stage: str, output: str, line: str) -> None:
        if f"\n{line}\n" not in output and not output.startswith(f"{line}\n"):
            raise StageError(stage, f"missing line {line!r}", output)

    def parse_status(self, stage: str, output: str) -> dict[str, int]:
        match = STATUS_RE.search(output)
        if match is None:
            raise StageError(stage, "status block not found", output)
        keys = (
            "pmm_free",
            "pmm_total",
            "heap_free",
            "heap_total",
            "heap_largest",
            "heap_blocks",
            "ticks",
            "irq0",
            "dropped",
            "switches",
            "finished",
        )
        return {key: int(value) for key, value in zip(keys, match.groups())}

    def quit(self) -> None:
        if self.monitor is not None and self.process is not None:
            if self.process.poll() is None:
                try:
                    self.monitor_command("quit", "qemu-quit")
                except StageError:
                    pass
                try:
                    self.process.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    pass

    def close(self) -> None:
        try:
            self.quit()
        finally:
            if self.process is not None and self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=2.0)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=2.0)
            if self.monitor is not None:
                try:
                    self.monitor.close()
                except OSError:
                    pass
                self.monitor = None
            if self.monitor_path.exists():
                try:
                    self.monitor_path.unlink()
                except OSError:
                    pass


def run_script(session: QemuShellSession) -> None:
    session.start()
    boot = session.wait_for("SHELL_READY", "wait-shell-ready", 0, BOOT_TIMEOUT_S)
    session.wait_for(PROMPT, "wait-first-prompt", 0, COMMAND_TIMEOUT_S)
    if boot.count("SHELL_READY") < 1:
        raise StageError("wait-shell-ready", "SHELL_READY missing", boot)

    _, help_out = session.run_line("help", "help")
    for name in HELP_COMMANDS:
        session.expect_contains("help", help_out, name)

    cursor = session.serial_offset()
    session.send_text("echoo", "echo-backspace")
    session.send_text("\b", "echo-backspace")
    session.send_text(" hello\n", "echo-backspace")
    echo_out = session.wait_for(PROMPT, "echo-backspace", cursor, COMMAND_TIMEOUT_S)
    visible = visible_serial(echo_out)
    session.expect_contains("echo-backspace", visible, "echo hello")
    session.expect_line("echo-backspace", visible, "hello")

    _, touch_out = session.run_line("touch note", "touch")
    session.expect_line("touch", touch_out, "ok")

    _, write_out = session.run_line("write note hello tiny", "write")
    session.expect_line("write", write_out, "ok")

    _, cat1_out = session.run_line("cat note", "cat-initial")
    session.expect_line("cat-initial", cat1_out, "hello tiny")

    _, append_out = session.run_line("append note again", "append")
    session.expect_line("append", append_out, "ok")

    _, cat2_out = session.run_line("cat note", "cat-appended")
    session.expect_line("cat-appended", cat2_out, "hello tiny again")

    _, ls_out = session.run_line("ls", "ls-note")
    session.expect_line("ls-note", ls_out, "note 16")

    _, status1_out = session.run_line("status", "status-1")
    first = session.parse_status("status-1", status1_out)
    _, status2_out = session.run_line("status", "status-2")
    second = session.parse_status("status-2", status2_out)
    if second["ticks"] <= first["ticks"]:
        raise StageError("status-2", "PIT ticks did not advance", status2_out)
    if second["irq0"] <= first["irq0"]:
        raise StageError("status-2", "IRQ0 count did not advance", status2_out)
    if first["dropped"] != 0 or second["dropped"] != 0:
        raise StageError("status-2", "keyboard dropped is not 0", status2_out)

    _, rm_out = session.run_line("rm note", "rm")
    session.expect_line("rm", rm_out, "ok")
    _, cat_missing = session.run_line("cat note", "cat-missing")
    session.expect_contains("cat-missing", cat_missing, "error: file not found")
    _, ls_empty = session.run_line("ls", "ls-empty")
    session.expect_contains("ls-empty", ls_empty, "(empty)")

    _, unknown_out = session.run_line("nosuch", "unknown")
    session.expect_contains("unknown", unknown_out, "error: unknown command")

    _, about_out = session.run_line("about", "about")
    session.expect_contains("about", about_out, "Ring 0")


def repository_root() -> Path:
    return Path(__file__).resolve().parent.parent


def main(argv: list[str]) -> int:
    visible_serial_selfcheck()

    root = repository_root()
    iso_path = Path(os.environ.get("TINYOS_ISO", root / "build" / "tinyshell.iso"))
    serial_path = Path(
        os.environ.get("TINYOS_SHELL_SERIAL", root / "build" / "qemu-shell-serial.log")
    )
    qemu_bin = os.environ.get("TINYOS_QEMU", "qemu-system-i386")
    memory = os.environ.get("TINYOS_QEMU_MEMORY", "64M")

    if not iso_path.is_file():
        print(f"qemu-shell-test: ISO not found: {iso_path}", file=sys.stderr)
        return 1

    session = QemuShellSession(iso_path, serial_path, qemu_bin, memory)
    try:
        run_script(session)
        print("QEMU shell interaction: PASS")
        return 0
    except StageError as exc:
        print(f"QEMU shell interaction: FAIL ({exc.stage}: {exc})", file=sys.stderr)
        if exc.excerpt:
            print("--- serial excerpt ---", file=sys.stderr)
            print(exc.excerpt[-4000:], file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"QEMU shell interaction: FAIL (internal: {exc})", file=sys.stderr)
        return 1
    finally:
        session.close()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
