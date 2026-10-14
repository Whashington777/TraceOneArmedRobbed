from pathlib import Path
import shutil, subprocess, tempfile
base=Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory() as root:
    (Path(root)/"runtime").mkdir()
    subprocess.run([shutil.which('lua') or 'lua',str(base/'tools/test_bootstrap.lua'),str(base/'packaging/TraceOAR/Scripts/main.lua'),root],check=True)
