from pathlib import Path
import os
import subprocess
import sys
import tempfile
import time

loader = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    game = root / 'game/OAR/Binaries/Win64'
    mod = game / 'Mods/TraceOAR'
    mod.mkdir(parents=True)
    (game / 'UE4SS.dll').write_text('test')
    (game / 'Mods/mods.txt').write_text('OtherMod : 1\nTraceOAR : 0\n')
    dll = root / 'runtime/TraceRuntime.dll'
    dll.parent.mkdir()
    dll.write_text('test')
    injector = dll.with_name('TraceInject.exe')
    injector.write_text('test')
    wine = root / 'wine'
    wine.write_text('''#!/usr/bin/env python3
from pathlib import Path
import os
root=Path(os.environ['TRACE_TEST_ROOT'])
for name in ['WINESERVERSOCKET','WINELOADERNOEXEC','WINEPRELOADRESERVE','WINE_LD_PRELOAD']:
    assert name not in os.environ, name
if not (root/'no-ready').exists():
    (root/'game/OAR/Binaries/Win64/Mods/TraceOAR/attach.ready').write_text('live-token')
''')
    wine.chmod(0o700)
    environment = os.environ.copy()
    environment.update(WINEPREFIX=str(root/'prefix'), WINELOADER=str(wine),
                       STEAM_COMPAT_INSTALL_PATH=str(root/'game'), TRACE_TEST_ROOT=str(root),
                       WINESERVERSOCKET='16', WINELOADERNOEXEC='1',
                       WINEPRELOADRESERVE='test', WINE_LD_PRELOAD='test')
    process = subprocess.Popen(['OAR-Win64-Shipping.exe','-c','import time;time.sleep(90)'],
                               executable=sys.executable, env=environment)
    arguments = [str(loader), '--pid', str(process.pid), '--dll', str(dll), '--injector', str(injector)]
    def run(expected, *extra):
        result = subprocess.run(arguments + list(extra), text=True, capture_output=True, timeout=20)
        assert result.returncode == expected, (result.returncode, result.stdout, result.stderr)
        return result
    try:
        time.sleep(0.2)
        run(3)
        assert 'OtherMod : 1' in (game/'Mods/mods.txt').read_text()
        assert 'TraceOAR : 1' in (game/'Mods/mods.txt').read_text()
        assert (mod/'Scripts/main.lua').is_file()
        assert not (mod/'attach.ready').exists()
        request=mod/'attach.request'
        request.write_text('stale-token')
        os.utime(request, (1, 1))
        run(3)
        request.write_text('live-token')
        run(0, '--check')
        result=run(0)
        assert 'Gameplay bridge connected' in result.stdout
        (root/'no-ready').write_text('1')
        ready=mod/'attach.ready'
        os.utime(ready, (1, 1))
        run(4)
    finally:
        process.terminate()
        process.wait(timeout=5)
print('Loader: bootstrap provisioning, mod preservation, stale-session rejection, Wine environment and gameplay readiness passed')
