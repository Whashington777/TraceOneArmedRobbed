import importlib.util, tempfile
from pathlib import Path
spec = importlib.util.spec_from_file_location('installer', Path(__file__).with_name('install_runtime.py'))
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as temp:
    root = Path(temp); source = root/'source'; game = root/'game'; binary = game/'OAR/Binaries/Win64'
    binary.mkdir(parents=True); exe = binary/'OAR-Win64-Shipping.exe'; exe.write_bytes(b'fixture game')
    m.EXPECTED = m.digest(exe)
    runtime = source/'Mods/TraceOAR/TraceRuntime.dll'; runtime.parent.mkdir(parents=True); runtime.write_bytes(b'fixture runtime')
    helper = source/'UE4SS.dll'; helper.write_bytes(b'fixture helper')
    existing_map = game/'OAR/Content/Paks/OAR-LocalMaps_P.pak'; existing_map.parent.mkdir(parents=True); existing_map.write_bytes(b'existing maps')
    (binary/'UE4SS.dll').write_bytes(b'user file')
    try: m.install(source, game)
    except RuntimeError: pass
    else: raise AssertionError('collision accepted')
    assert not (binary/'Mods').exists()
    (binary/'UE4SS.dll').write_bytes(helper.read_bytes()) 
    m.install(source, game)
    assert (binary/'Mods/TraceOAR/TraceRuntime.dll').read_bytes()==b'fixture runtime'
    m.remove(game)
    assert not (binary/'Mods/TraceOAR/TraceRuntime.dll').exists()
    assert (binary/'UE4SS.dll').read_bytes()==b'fixture helper'
    assert existing_map.read_bytes()==b'existing maps'
    m.install(source, game)
    (binary/'Mods/TraceOAR/TraceRuntime.dll').write_bytes(b'user change')
    m.remove(game)
    assert (binary/'Mods/TraceOAR/TraceRuntime.dll').read_bytes()==b'user change'
    assert (binary/'trace-install.json').exists()
print('Installer collision preflight, ownership, uninstall and preservation passed')
