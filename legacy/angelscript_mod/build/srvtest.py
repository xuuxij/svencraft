# Starts the Svencraft Coop dedicated server, runs console commands over RCON, prints output, quits.
# Temporarily swaps steam_appid.txt to the dedicated-server app id and always restores it.
import socket, subprocess, sys, time, os, re
DIR = r"C:\Program Files (x86)\Steam\steamapps\common\Svencraft Coop"
APPID = os.path.join(DIR, "steam_appid.txt")
PORT, PASS = 27099, "svtest"
MAP = sys.argv[1] if len(sys.argv) > 1 else "stadium4"
CMDS = sys.argv[2:] or ["as_listplugins"]

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
write_appid("276060")
proc = None
try:
    proc = subprocess.Popen([os.path.join(DIR, "svends.exe"), "-console", "-condebug", "-port", str(PORT), "+developer", "1", "+maxplayers", "2", "+sv_lan", "1",
                             "+rcon_password", PASS, "+map", MAP], cwd=DIR, creationflags=0x08000000)
    up = False
    for _ in range(60):
        pass
    time.sleep(8)
    for _ in range(60):
        time.sleep(1)
        try:
            if "plugin" in rcon("as_listplugins", 1.0).lower() or True:
                r = rcon("status", 1.0)
                if "map" in r.lower(): up = True; break
        except Exception:
            pass
    print("server up:", up)
    for c in CMDS:
        if c.startswith("sleep:"):
            time.sleep(float(c[6:])); continue
        print(f"===== {c}"); print(rcon(c, 4.0).rstrip())
finally:
    try: rcon("quit", 1.0)
    except Exception: pass
    if proc:
        try: proc.wait(10)
        except Exception: proc.kill()
    write_appid(orig)
    os.chmod(APPID, stat.S_IREAD)
    print("steam_appid.txt restored:", open(APPID).read(), "(read-only)")
