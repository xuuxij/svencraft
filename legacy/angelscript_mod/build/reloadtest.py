# Does a map change recompile a modified plugin? Start the server, change a script, changelevel, read the log.
import os, sys, time, stat, subprocess
sys.argv = ['srvtest.py']
import srvtest_lib as T

DIR = T.DIR
F = os.path.join(DIR, r"svencoop_addon\scripts\plugins\svencraft\svencraft.as")
LOG = os.path.join(DIR, "console-2026-10-06.log")
start = os.path.getsize(LOG) if os.path.exists(LOG) else 0
T.write_appid("276060")
proc = subprocess.Popen([os.path.join(DIR, "svends.exe"), "-console", "-condebug", "-port", str(T.PORT), "+developer", "1", "+maxplayers", "2",
                         "+sv_lan", "1", "+rcon_password", T.PASS, "+map", "svencraft_sandbox"], cwd=DIR, creationflags=0x08000000)
try:
    for _ in range(40):
        time.sleep(1)
        try:
            if 'map' in T.rcon('status', 1.0).lower(): break
        except Exception: pass
    time.sleep(3)
    os.utime(F, None)                     # "modify" the plugin
    with open(F, 'a', newline='') as fh: fh.write('\n')
    T.rcon('map svencraft_sandbox', 2.0)
    time.sleep(20)
finally:
    try: T.rcon('quit', 1.0)
    except Exception: pass
    try: proc.wait(15)
    except Exception: proc.kill()
    T.write_appid("225840"); os.chmod(T.APPID, stat.S_IREAD)
    s = open(F, newline='').read()
    if s.endswith('\n\n'): open(F, 'w', newline='').write(s[:-1])   # undo the test edit
log = open(LOG, 'rb').read()[start:].decode('latin-1')
print('plugin compilations in this run:', log.count('Beginning plugin "Svencraft" compilation'))
print('maps started:', log.count('Started map'))
