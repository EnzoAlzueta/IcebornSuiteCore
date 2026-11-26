"""Run the DX11 fixture with copies of optional DLLs and fresh settings per process."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("game", type=Path)
parser.add_argument("--case", choices=["core", "hp", "buff", "crown", "meter", "all"])
parser.add_argument("--host", default="render_dx11.exe")
parser.add_argument("--regression", action="store_true")
parser.add_argument("--rounds", type=int, default=3)
args = parser.parse_args()
build = args.build.resolve()
native = args.game.resolve() / "nativePC"
modules = native / "IcebornSuite/modules"
dlls = {
    "hp.dll": modules / "monster-info/0.1.0-dev.4/module.dll",
    "buff.dll": modules / "buff-tracker/0.1.0-dev.1/module.dll",
    "crown.dll": modules / "crown-tracker/0.1.0-dev.2/module.dll",
    "iceborne_damage_meter.dll": native / "plugins/iceborne_damage_meter.dll",
}
root = Path(tempfile.mkdtemp(prefix="dx11-", dir=build))
print(root, flush=True)
cases = ["core", "hp", "buff", "crown", "meter", "all"]
if args.case:
    cases = [args.case]
if args.regression:
    cases = ["crowns", "core-first", "meter-first"]
for repeat in range(1 if args.regression else args.rounds):
    for mode in cases:
        work = root / f"{repeat}-{mode}"
        work.mkdir()
        for filename, source in dlls.items():
            if args.regression or mode == "all" or filename == {
                "hp": "hp.dll", "buff": "buff.dll", "crown": "crown.dll",
                "meter": "iceborne_damage_meter.dll",
            }.get(mode):
                shutil.copyfile(source, work / filename)
        command = ["--bench", mode]
        if mode == "crowns":
            command = ["hp.dll", "crown.dll"]
        if mode in ("core-first", "meter-first"):
            command = ["hp.dll", mode, "iceborne_damage_meter.dll", "both", "buff.dll"]
        try:
            result = subprocess.run([build / args.host, *command], cwd=work,
                                    capture_output=True, timeout=90)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"Timeout in {work}") from error
        log = result.stdout + result.stderr
        (work / "result.txt").write_bytes(log)
        print(repeat, mode, result.returncode, log.decode(errors="replace").splitlines()[-3:], flush=True)
        result.check_returncode()
