#!/usr/bin/env python3
"""Run PicoOS in a virtual machine (QEMU, Cortex-M33) -- no board needed.

  python3 run_vm.py                 interactive: type in this terminal
  python3 run_vm.py --screen        also open a window showing the 320x240 LCD
                                    (and you can type into that window)
  python3 run_vm.py --script "help;ls;cat readme.txt" --snapshot screen.png
                                    run commands automatically, save a screenshot
Quit with Ctrl-]  (or close the window).

The VM runs the real OS (scheduler, heap, FAT, shell, terminal).  The BIOS is
replaced by a small stand-in, so BIOS hardware drivers are NOT tested here.
"""
import argparse, os, shutil, socket, subprocess, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from vm_screen import Screen  # noqa: E402

ROOT = os.path.dirname(HERE)
DEFAULT_ELF = next((p for p in (os.path.join(ROOT, "prebuilt", "vm", "pico2os_vm.elf"),
                                os.path.join(ROOT, "firmware", "build", "vm_os.elf")) if os.path.exists(p)),
                   os.path.join(ROOT, "prebuilt", "vm", "pico2os_vm.elf"))
DEFAULT_DISK = os.path.join(ROOT, "prebuilt", "vm", "disk.img")


def find_qemu():
    exe = shutil.which("qemu-system-arm")
    if exe:
        return exe
    for p in (r"C:\Program Files\qemu\qemu-system-arm.exe", r"C:\Program Files (x86)\qemu\qemu-system-arm.exe",
              "/opt/homebrew/bin/qemu-system-arm", "/usr/local/bin/qemu-system-arm"):
        if os.path.exists(p):
            return p
    sys.exit("qemu-system-arm not found.\n"
             "  Windows: install QEMU from qemu.org (tick the ARM system emulator) and re-run\n"
             "  macOS:   brew install qemu\n"
             "  Linux / WSL: sudo apt install qemu-system-arm")


def free_port():
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", default=DEFAULT_ELF)
    ap.add_argument("--disk", default=DEFAULT_DISK, help="FAT16/FAT32 disk image used as the SD card (max 16 MB)")
    ap.add_argument("--screen", action="store_true", help="show the LCD in a window")
    ap.add_argument("--script", help="commands separated by ';' (non-interactive)")
    ap.add_argument("--snapshot", help="save a PNG of the LCD at the end (with --script)")
    a = ap.parse_args()

    if not os.path.exists(a.elf):
        sys.exit(f"missing {a.elf} (run 'make vm' in firmware/ or use the prebuilt folder)")
    qemu = find_qemu()
    screen = Screen()
    cport, sport = free_port(), free_port()

    # we listen for the "LCD" serial port; QEMU listens for the console port
    srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", sport)); srv.listen(1)
    cmd = [qemu, "-M", "mps2-an505", "-cpu", "cortex-m33", "-display", "none", "-monitor", "none",
           "-kernel", a.elf,
           "-serial", f"tcp:127.0.0.1:{cport},server=on,wait=off",
           "-serial", f"tcp:127.0.0.1:{sport}"]
    if os.path.exists(a.disk):
        if os.path.getsize(a.disk) > 16 * 1024 * 1024:
            sys.exit("disk image is bigger than 16 MB")
        cmd += ["-device", f"loader,file={a.disk},addr=0x80000000,force-raw=on"]
    else:
        print(f"(no disk image at {a.disk}: the SD card will be empty)")
    qp = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    def lcd_reader():
        try:
            c, _ = srv.accept()
            while True:
                d = c.recv(4096)
                if not d:
                    break
                screen.feed(d)
        except OSError:
            pass
    threading.Thread(target=lcd_reader, daemon=True).start()

    con = None
    for _ in range(100):
        if qp.poll() is not None:
            sys.exit("QEMU exited:\n" + qp.stderr.read().decode(errors="replace"))
        try:
            con = socket.create_connection(("127.0.0.1", cport), timeout=1); break
        except OSError:
            time.sleep(0.1)
    if con is None:
        qp.kill(); sys.exit("could not connect to QEMU")
    con.settimeout(None)
    stop = threading.Event()
    out_lock = threading.Lock()
    captured = bytearray()

    def con_reader():
        while not stop.is_set():
            try:
                d = con.recv(4096)
            except OSError:
                break
            if not d:
                break
            captured.extend(d)
            if not a.script:
                with out_lock:
                    sys.stdout.write(d.decode("latin-1")); sys.stdout.flush()
        stop.set()
    threading.Thread(target=con_reader, daemon=True).start()

    def send(s):
        try:
            con.sendall(s.encode("latin-1"))
        except OSError:
            stop.set()

    try:
        if a.script:
            time.sleep(1.5)
            for c in a.script.split(";"):
                send(c.strip() + "\r"); time.sleep(0.5)
            time.sleep(1.0)
            print(captured.decode("latin-1").replace("\r", ""))
            if a.snapshot:
                screen.write_png(a.snapshot, 3)
                print(f"[screenshot saved to {a.snapshot}]")
        elif a.screen:
            run_window(screen, send, stop)
        else:
            print("PicoOS VM -- type commands.  Quit: Ctrl-]\n")
            run_terminal(send, stop)
    finally:
        stop.set(); qp.kill()
        try:
            con.close()
        except OSError:
            pass


def run_terminal(send, stop):
    if os.name == "nt":
        import msvcrt
        while not stop.is_set():
            if msvcrt.kbhit():
                ch = msvcrt.getwch()
                if ch == "\x1d":
                    return
                send("\x7f" if ch == "\x08" else ch)
            else:
                time.sleep(0.01)
    else:
        import termios, tty, select
        fd = sys.stdin.fileno()
        if not sys.stdin.isatty():
            for line in sys.stdin:
                send(line.rstrip("\n") + "\r"); time.sleep(0.4)
            time.sleep(1.0); return
        old = termios.tcgetattr(fd)
        try:
            tty.setcbreak(fd)
            while not stop.is_set():
                if select.select([fd], [], [], 0.1)[0]:
                    ch = os.read(fd, 1).decode("latin-1")
                    if ch == "\x1d":
                        return
                    send(ch)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)


def run_window(screen, send, stop):
    try:
        import tkinter as tk
    except ImportError:
        sys.exit("tkinter is not installed (Linux: sudo apt install python3-tk). Run without --screen instead.")
    root = tk.Tk(); root.title("PicoOS VM  -  type here  (Ctrl-] or close to quit)")
    label = tk.Label(root, bd=0); label.pack()
    last = [-1]

    def refresh():
        if stop.is_set():
            root.destroy(); return
        if screen.version != last[0]:
            last[0] = screen.version
            w, h, rgb = screen.render(3)
            img = tk.PhotoImage(data=b"P6 %d %d 255\n" % (w, h) + rgb, format="PPM")
            label.configure(image=img); label.image = img
        root.after(60, refresh)

    def key(e):
        if e.char == "\x1d":
            stop.set(); return
        if e.keysym == "Return": send("\r")
        elif e.keysym == "BackSpace": send("\x7f")
        elif e.keysym == "Tab": send("\t")
        elif e.char: send(e.char)
    root.bind("<Key>", key)
    root.protocol("WM_DELETE_WINDOW", lambda: stop.set())
    refresh(); root.mainloop()


if __name__ == "__main__":
    main()
