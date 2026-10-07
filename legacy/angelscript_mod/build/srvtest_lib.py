# Starts the Svencraft Coop dedicated server, runs console commands over RCON, prints output, quits.
# Temporarily swaps steam_appid.txt to the dedicated-server app id and always restores it.
import socket, subprocess, sys, time, os, re
DIR = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop"
APPID = os.path.join(DIR, "steam_appid.txt")
PORT, PASS = 27099, "svtest"
MAP = "svencraft_sandbox"
CMDS = []

def rcon(cmd, timeout=3.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(timeout)
    try:
        s.sendto(b"\xff\xff\xff\xffchallenge rcon\n", ("127.0.0.1", PORT))
        data, _ = s.recvfrom(4096)
        ch = re.search(rb"challenge rcon (\d+)", data).group(1).decode()
        s.sendto(b"\xff\xff\xff\xff" + f'rcon {ch} "{PASS}" {cmd}\n'.encode(), ("127.0.0.1", PORT))
        out = b""
        end = time.time() + timeout
        while time.time() < end:
            try:
                d, _ = s.recvfrom(65535)
                out += d[5:] if d.startswith(b"\xff\xff\xff\xffl") else d
                s.settimeout(0.6)
            except socket.timeout:
                break
        return out.decode("latin-1")
    finally:
        s.close()

import stat
def write_appid(v):
    if os.path.exists(APPID):
        os.chmod(APPID, stat.S_IWRITE)
    open(APPID, "w").write(v)
orig = "225840"   # the game's own id; the file is kept read-only so the game can't delete it on exit
