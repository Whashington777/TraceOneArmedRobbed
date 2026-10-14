
"""Install/remove the test build without touching original game assets or saves."""
import argparse, hashlib, json, shutil
from pathlib import Path
EXPECTED = '66ee255697a8167525c6e78b744525fd38aa643f2878e4c03a63330584248280'
def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()
def install(source, game):
    if not (source / 'Mods/TraceOAR/TraceRuntime.dll').is_file():
        raise RuntimeError('Runtime source package is missing; nothing installed')
    binary = game / 'OAR/Binaries/Win64'
    exe = binary / 'OAR-Win64-Shipping.exe'
    if not exe.is_file() or digest(exe) != EXPECTED:
        raise RuntimeError('Game build does not match the tested static analysis; nothing installed')
    manifest_path = binary / 'trace-install.json'
    previous = json.loads(manifest_path.read_text()) if manifest_path.exists() else {'files': {}}
    pending = [(path, binary / path.relative_to(source)) for path in source.rglob('*') if path.is_file()]
    
    for src, dest in pending:
        key = str(dest.relative_to(binary))
        if dest.exists() and digest(dest) != digest(src):
            old = previous['files'].get(key)
            if old != digest(dest):
                raise RuntimeError(f'Existing unmanaged/changed file: {dest}; nothing installed')
    records = dict(previous['files'])
    preexisting = {str(dest.relative_to(binary)) for _, dest in pending if dest.exists()}
    for src, dest in pending:
        dest.parent.mkdir(parents=True, exist_ok=True)
        if not dest.exists() or digest(dest) != digest(src):
            temp = dest.with_name(dest.name + '.trace-tmp')
            shutil.copy2(src, temp)
            temp.replace(dest)
        key = str(dest.relative_to(binary))
        if key in previous['files'] or key not in preexisting:
            records[key] = digest(dest)
    manifest_path.write_text(json.dumps({'build_sha256': EXPECTED, 'files': records}, indent=2))
    print(f'Installed {len(pending)} files into {binary}')
    print('Proton launch option: WINEDLLOVERRIDES="dwmapi=n,b" %command%')
    print('INSERT: menu. F10: reset changes. Restart the game to load the bootstrap.')
def remove(game):
    binary = game / 'OAR/Binaries/Win64'
    manifest_path = binary / 'trace-install.json'
    if not manifest_path.exists():
        raise RuntimeError('No installer manifest found')
    manifest = json.loads(manifest_path.read_text())
    retained = {}
    for key, checksum in manifest['files'].items():
        dest = binary / key
        if not dest.resolve().is_relative_to(binary.resolve()):
            raise RuntimeError('Invalid path in manifest')
        if dest.is_file():
            if digest(dest) == checksum:
                dest.unlink()
            else:
                retained[key] = checksum
                print('Kept modified file:', dest)
    if retained:
        manifest['files'] = retained
        manifest_path.write_text(json.dumps(manifest, indent=2))
    else:
        manifest_path.unlink()
    print('Removed unchanged installed files. Profiles/logs and the existing maps pak were kept.')
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=Path(__file__).parent / 'Win64')
    parser.add_argument('--remove', action='store_true')
    args = parser.parse_args()
    try:
        if args.remove: remove(args.game)
        else: install(args.source, args.game)
    except (RuntimeError, OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')
