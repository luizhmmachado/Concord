#!/usr/bin/env python3
"""Generate the stable downloads.json consumed by the future landing page."""

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
from urllib.parse import quote


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def describe(path: Path) -> dict:
    name = path.name
    if name.endswith("-setup.exe"):
        platform, arch, kind = "windows", "x64", "installer"
    elif name.endswith("-portable.zip"):
        platform, arch, kind = "windows", "x64", "portable"
    elif name.endswith(".deb"):
        platform, arch, kind = "linux", "amd64", "deb"
    else:
        platform, arch, kind = "unknown", "unknown", "file"

    return {
        "name": name,
        "platform": platform,
        "arch": arch,
        "kind": kind,
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", required=True, type=Path)
    parser.add_argument("--repo", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    output = args.output or args.dir / "downloads.json"
    files = sorted(
        path
        for path in args.dir.iterdir()
        if path.is_file() and path.name not in {output.name, "SHA256SUMS.txt"}
    )
    if not files:
        raise SystemExit(f"no release assets found in {args.dir}")

    base_url = f"https://github.com/{args.repo}/releases/download/{quote(args.tag)}"
    assets = []
    for path in files:
        asset = describe(path)
        asset["url"] = f"{base_url}/{quote(path.name)}"
        assets.append(asset)

    assets.sort(key=lambda item: (item["platform"] != "windows", item["kind"] != "installer", item["name"]))
    payload = {
        "schema_version": 1,
        "product": "Concord",
        "version": args.version,
        "tag": args.tag,
        "channel": "stable",
        "published_at": dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat(),
        "release_url": f"https://github.com/{args.repo}/releases/tag/{quote(args.tag)}",
        "latest_metadata_url": f"https://github.com/{args.repo}/releases/latest/download/downloads.json",
        "minimum_os": {"windows": "10 x64", "linux": "Ubuntu 22.04 / glibc 2.35"},
        "assets": assets,
    }
    output.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {output} with {len(assets)} assets")


if __name__ == "__main__":
    main()
