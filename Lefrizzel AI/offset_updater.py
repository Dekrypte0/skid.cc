#!/usr/bin/env python3
"""Lefrizzel AI - auto offset updater.

Sources:
  * a2x/cs2-dumper  offsets.hpp / client_dll.hpp / buttons.hpp  (globals + schema fallbacks + buttons)
  * cspatterns.dev  /json                                       (signatures, for reference + verify)

Usage:
  python offset_updater.py --check            # fetch + diff report, no writes
  python offset_updater.py --apply            # backup offsets.h, then update in place
  python offset_updater.py --verify           # scan cspatterns sigs against on-disk game DLLs
  python offset_updater.py --save-sigs        # write cspatterns json next to offsets.h
  python offset_updater.py --selftest         # built-in checks

Exit codes: 0 ok, 1 error, 2 verify found misses / check found drift.
Python 3.9+, stdlib only.
"""

from __future__ import annotations

import argparse
import json
import logging
import re
import shutil
import struct
import sys
import urllib.request
from datetime import datetime
from pathlib import Path

log = logging.getLogger("offset_updater")

A2X_RAW = "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/%s"
CSPATTERNS_JSON = "https://cspatterns.dev/json"
CSPATTERNS_ROOT = "https://cspatterns.dev/"

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OFFSETS = SCRIPT_DIR / "source" / "lefrizzel Ai" / "offsets" / "offsets.h"
DEFAULT_GAME = Path(r"C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game")
USER_AGENT = "LefrizzelAiOffsetUpdater/1.0 (+python-urllib)"

# FB namespace: which schema classes own which names (resolution order).
FB_PRIORITY_CLASSES = [
    "C_CSPlayerPawn", "C_BasePlayerPawn", "C_BaseCombatCharacter", "C_BaseModelEntity",
    "C_BaseEntity",
    "CGameSceneNode", "CSkeletonInstance", "CModelState", "CEntityIdentity",
    "CEntityInstance", "C_CSWeaponBase", "C_CSWeaponBaseGun", "C_BasePlayerWeapon",
    "C_EconEntity", "CCSPlayerController", "CBasePlayerController",
    "CBasePlayerWeapon", "C_CSPlayerResource", "C_BaseGrenade", "CBaseGrenade", "C_C4",
]

RE_GENERATED = re.compile(r"^\s*//\s*(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})", re.M)
RE_NS = re.compile(r"^\s*namespace\s+(\w+)\s*\{")
RE_CONST = re.compile(
    r"^(?P<head>\s*constexpr std::(?:u?int32_t|ptrdiff_t)\s+(?P<name>\w+)\s*=\s*)"
    r"0x(?P<val>[0-9A-Fa-f]+)(?P<tail>;[^\r\n]*)$"
)
RE_COMMENT_CLASS = re.compile(r"^\s*//\s*([A-Za-z_]\w*)")


# ---------------------------------------------------------------- fetch

def fetch(url: str, timeout: float, tries: int = 3) -> str:
    last: Exception | None = None
    for attempt in range(1, tries + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                data = resp.read().decode("utf-8", errors="replace")
            log.debug("GET %s -> %d bytes", url, len(data))
            return data
        except Exception as exc:  # noqa: BLE001 - report and retry
            last = exc
            log.warning("GET %s failed (try %d/%d): %s", url, attempt, tries, exc)
    raise RuntimeError(f"fetch failed: {url}: {last}")


def parse_generated(text: str) -> str:
    m = RE_GENERATED.search(text)
    return m.group(1) if m else "unknown date"


# ------------------------------------------------------- namespace walker

class Entry:
    __slots__ = ("ns", "name", "value", "comment")

    def __init__(self, ns: tuple[str, ...], name: str, value: int, comment: str):
        self.ns = ns
        self.name = name
        self.value = value
        self.comment = comment


def walk_namespaces(text: str) -> list[Entry]:
    """Collect constexpr entries with their namespace path (brace-balanced)."""
    entries: list[Entry] = []
    stack: list[tuple[str, int]] = []  # (namespace, balance at its '{')
    balance = 0
    for line in text.splitlines():
        body = line.split("//", 1)[0]
        m = RE_NS.match(line)
        if m:
            stack.append((m.group(1), balance))
        else:
            cm = RE_CONST.match(line)
            if cm and stack:
                comment = cm.group("tail").split("//", 1)[1].strip() if "//" in cm.group("tail") else ""
                entries.append(Entry(tuple(n for n, _ in stack), cm.group("name"),
                                     int(cm.group("val"), 16), comment))
        balance += body.count("{") - body.count("}")
        while stack and balance == stack[-1][1]:
            stack.pop()
    return entries


def to_map(entries: list[Entry]) -> dict[tuple[str, ...], dict[str, Entry]]:
    out: dict[tuple[str, ...], dict[str, Entry]] = {}
    for e in entries:
        out.setdefault(e.ns, {})[e.name] = e
    return out


# ------------------------------------------------------------ a2x parsing

def parse_a2x(text: str) -> tuple[dict[str, dict[str, Entry]], str]:
    """Returns ({module_ns: {name: Entry}}, generated_date) for offsets.hpp / buttons.hpp."""
    entries = walk_namespaces(text)
    mods: dict[str, dict[str, Entry]] = {}
    for e in entries:
        ns = e.ns
        if len(ns) >= 3 and ns[0] == "cs2_dumper" and ns[1] == "offsets":
            mods.setdefault(ns[2], {})[e.name] = e          # offsets.hpp: cs2_dumper>offsets><module>
        elif len(ns) == 2 and ns[0] == "cs2_dumper" and ns[1] == "buttons":
            mods.setdefault("buttons", {})[e.name] = e      # buttons.hpp: cs2_dumper>buttons
    return mods, parse_generated(text)


def parse_schemas(text: str) -> tuple[dict[str, dict[str, Entry]], str]:
    """Returns ({class_name: {field: Entry}}, generated_date) from client_dll.hpp."""
    entries = walk_namespaces(text)
    classes: dict[str, dict[str, Entry]] = {}
    for e in entries:
        ns = e.ns
        if len(ns) >= 4 and ns[0] == "cs2_dumper" and ns[1] == "schemas" and ns[2] == "client_dll":
            classes.setdefault(ns[3], {})[e.name] = e
    return classes, parse_generated(text)


def fb_resolve(name: str, comment: str, schemas: dict[str, dict[str, Entry]],
               dump_globals: dict[str, dict[str, Entry]]) -> tuple[int | None, str]:
    """Resolve an FB name to (value, how). None = unresolvable (keep old)."""
    if name.startswith("dw"):
        g = dump_globals.get("client_dll", {}).get(name)
        if g:
            return g.value, "dump"
        return None, "missing"
    classes = schemas
    # 1) explicit class hint in the trailing comment (first token must be a known class)
    hint = comment.split()[0] if comment else ""
    if hint in classes and name in classes[hint]:
        return classes[hint][name].value, f"class hint {hint}"
    # 2) priority class scan
    for cls in FB_PRIORITY_CLASSES:
        if cls in classes and name in classes[cls]:
            return classes[cls][name].value, f"priority {cls}"
    # 3) unique / unanimous across all classes
    cands = [(cls, fields[name].value) for cls, fields in classes.items() if name in fields]
    if cands:
        vals = {v for _, v in cands}
        if len(vals) == 1:
            return cands[0][1], "unique"
        return None, f"ambiguous ({len(cands)} classes differ)"
    return None, "missing from dump"


# ------------------------------------------------------- offsets.h update

def update_offsets_h(offsets_path: Path, a2x_offsets: dict[str, dict[str, Entry]],
                     schemas: dict[str, dict[str, Entry]], a2x_buttons: dict[str, Entry],
                     apply: bool, add_new: bool) -> tuple[int, list[str]]:
    """Returns (changed_count, report_lines)."""
    with open(offsets_path, "r", encoding="utf-8", errors="replace", newline="") as f:
        text = f.read()
    lines = text.splitlines(keepends=True)

    # index lines by (ns_tail, name) -> line number
    stack: list[tuple[str, int]] = []
    balance = 0
    targets: dict[tuple[str, str], int] = {}
    ns_tails: set[str] = set()
    for i, line in enumerate(lines):
        m = RE_NS.match(line)
        if m:
            stack.append((m.group(1), balance))
            balance += line.count("{") - line.count("}")
            continue
        if RE_CONST.match(line) and stack:
            name = RE_CONST.match(line).group("name")
            targets[(stack[-1][0], name)] = i
            ns_tails.add(stack[-1][0])
        balance += line.count("{") - line.count("}")
        while stack and balance == stack[-1][1]:
            stack.pop()

    report: list[str] = []
    changed = 0

    def new_value_for(ns_tail: str, name: str, comment: str) -> tuple[int | None, str]:
        if ns_tail == "buttons":
            if name in a2x_buttons:
                return a2x_buttons[name].value, "buttons"
            return None, "missing from buttons.hpp"
        if ns_tail == "FB":
            return fb_resolve(name, comment, schemas, a2x_offsets)
        if ns_tail in a2x_offsets:
            if name in a2x_offsets[ns_tail]:
                return a2x_offsets[ns_tail][name].value, "dump"
            return None, "missing from offsets.hpp"
        return None, f"no a2x namespace '{ns_tail}'"

    for (ns_tail, name), li in sorted(targets.items(), key=lambda kv: kv[1]):
        line = lines[li]
        m = RE_CONST.match(line)
        comment = m.group("tail").split("//", 1)[1].strip() if "//" in m.group("tail") else ""
        val, how = new_value_for(ns_tail, name, comment)
        if val is None:
            report.append(f"KEEP {ns_tail}::{name}  ({how})")
            continue
        old = int(m.group("val"), 16)
        if old == val:
            log.debug("same %s::%s", ns_tail, name)
            continue
        hexs = f"0x{val:X}"
        lines[li] = re.sub(r"(=\s*)0x[0-9A-Fa-f]+", lambda mm: mm.group(1) + hexs, line, count=1)
        report.append(f"SET  {ns_tail}::{name:<30} 0x{old:X} -> {hexs}  ({how})")
        changed += 1

    if add_new:
        pass  # appended names are not needed by the cheat; --add-new is a no-op guard for now

    # FYI: names the dump tracks that offsets.h does not (documentation only)
    for ns_tail in sorted(ns_tails & set(a2x_offsets) | {"buttons"}):
        src = a2x_buttons if ns_tail == "buttons" else a2x_offsets.get(ns_tail, {})
        extra = sorted(set(src) - {n for (t, n) in targets if t == ns_tail})
        if extra:
            report.append(f"NOTE {ns_tail}: {len(extra)} new in dump (not tracked): {', '.join(extra[:6])}...")

    if apply and changed:
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup = offsets_path.with_name(f"{offsets_path.name}.bak_{stamp}")
        shutil.copy2(offsets_path, backup)
        log.info("backup: %s", backup)
        with open(offsets_path, "w", encoding="utf-8", newline="") as f:
            f.write("".join(lines))
        report.insert(0, f"APPLIED {changed} change(s); backup {backup.name}")
    elif apply:
        report.insert(0, "APPLY: nothing to change - offsets.h already current")
    else:
        report.insert(0, f"CHECK: {changed} change(s) pending (run with --apply)")

    return changed, report


# --------------------------------------------------------------- verify

def pe_sections(data: bytes) -> list[tuple[int, int, int, int]]:
    """[(va, vsize, rawptr, rawsize)] for a PE64 image."""
    if len(data) < 0x400 or data[:2] != b"MZ":
        raise ValueError("not an MZ image")
    off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[off:off + 4] != b"PE\0\0":
        raise ValueError("bad PE signature")
    machine = struct.unpack_from("<H", data, off + 4)[0]
    if machine != 0x8664:
        raise ValueError(f"not x64 (machine=0x{machine:X})")
    nsec = struct.unpack_from("<H", data, off + 6)[0]
    opt_size = struct.unpack_from("<H", data, off + 20)[0]
    table = off + 24 + opt_size
    sections = []
    for i in range(nsec):
        base = table + i * 40
        vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, base + 8)
        if vsize or rawsize:
            sections.append((va, vsize, rawptr, rawsize))
    return sections


def file_off_to_rva(sections: list[tuple[int, int, int, int]], off: int) -> int | None:
    for va, vsize, rawptr, rawsize in sections:
        if rawptr <= off < rawptr + rawsize:
            return va + (off - rawptr)
    return None


def compile_pattern(sig: str) -> tuple[bytes, bytes, int] | None:
    """-> (bytes, mask, window_len); '?' / '??' wildcard. None if no literal anchor."""
    tokens = sig.split()
    if not tokens:
        return None
    byts = bytearray()
    mask = bytearray()
    for tok in tokens:
        if tok in ("?", "??"):
            byts.append(0)
            mask.append(0)
        else:
            try:
                byts.append(int(tok, 16) & 0xFF)
            except ValueError:
                raise ValueError(f"bad pattern token '{tok}' in '{sig}'") from None
            mask.append(1)
    if 1 not in mask:
        return None
    return bytes(byts), bytes(mask), len(tokens)


def scan_pattern(data: bytes, compiled: tuple[bytes, bytes, int], cap: int = 8) -> list[int]:
    """Return file offsets of matches (capped)."""
    byts, mask, _ = compiled
    first = mask.find(b"\x01")
    anchor_start = first
    anchor_end = first
    while anchor_end < len(mask) and mask[anchor_end] == 1:
        anchor_end += 1
    anchor = byts[anchor_start:anchor_end]
    hits: list[int] = []
    pos = 0
    win = len(mask)
    while len(hits) < cap:
        pos = data.find(anchor, pos)
        if pos < 0:
            break
        start = pos - anchor_start
        if start >= 0:
            seg = data[start:start + win]
            if len(seg) == win and all(seg[i] == byts[i] for i in range(win) if mask[i]):
                hits.append(start)
        pos += 1
    return hits


def module_candidates(game: Path, module: str) -> list[Path]:
    name = module.lower() if module.lower().endswith(".dll") else module.lower() + ".dll"
    roots = [
        game,
        game / "csgo" / "bin" / "win64",
        game / "bin" / "win64",
        game.parent,                                # Counter-Strike Global Offensive root
        game.parents[2] if len(game.parents) >= 3 else game,  # steamapps (rare)
        game.parents[3] if len(game.parents) >= 4 else game,  # Steam root (GameOverlayRenderer64)
    ]
    seen: set[Path] = set()
    out: list[Path] = []
    for root in roots:
        cand = root / name
        if cand not in seen:
            seen.add(cand)
            if cand.is_file():
                out.append(cand)
    return out


def run_verify(cspatterns: dict, game: Path) -> tuple[int, int, int, list[str]]:
    lines: list[str] = []
    ok = multi = miss = 0
    loaded: dict[Path, bytes] = {}
    secs: dict[Path, list] = {}

    def get_pe(path: Path):
        if path not in loaded:
            loaded[path] = path.read_bytes()
            secs[path] = pe_sections(loaded[path])
        return loaded[path], secs[path]

    for module, sigs in sorted(cspatterns.items()):
        for label, sig in sigs.items():
            # mangled export names (cspatterns tier0 entries) are not byte patterns:
            # prove existence by searching for the name string inside the image
            if "@" in sig and not re.fullmatch(r"[\dA-F? ]+", sig):
                found = False
                for path in module_candidates(game, module):
                    try:
                        data, _ = get_pe(path)
                    except (OSError, ValueError):
                        continue
                    if sig.encode("ascii", errors="ignore") in data:
                        ok += 1
                        lines.append(f"NAME  {module} {label}")
                        found = True
                        break
                if not found:
                    miss += 1
                    lines.append(f"MISS  {module} {label} (export name)")
                continue
            try:
                compiled = compile_pattern(sig)
            except ValueError as exc:
                lines.append(f"ERR  {module}::{label}  {exc}")
                miss += 1
                continue
            if compiled is None:
                lines.append(f"ERR  {module}::{label}  no literal tokens")
                miss += 1
                continue
            found = False
            for path in module_candidates(game, module):
                try:
                    data, sections = get_pe(path)
                except (OSError, ValueError) as exc:
                    log.debug("skip %s: %s", path, exc)
                    continue
                hits = scan_pattern(data, compiled)
                rvas = [file_off_to_rva(sections, h) for h in hits]
                rvas = [r for r in rvas if r is not None]
                if rvas:
                    found = True
                    if len(rvas) == 1:
                        ok += 1
                        lines.append(f"OK    1 {rvas[0]:08X} {module} {label}")
                    else:
                        multi += 1
                        listing = " ".join(f"{r:08X}" for r in rvas[:3])
                        lines.append(f"MULTI {len(rvas)} {listing} {module} {label}")
                    break
            if not found:
                miss += 1
                lines.append(f"MISS  {module} {label}")
    return ok, multi, miss, lines


# ------------------------------------------------------------------ cli

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Lefrizzel AI offsets updater (a2x + cspatterns.dev)")
    ap.add_argument("--apply", action="store_true", help="write updates (backup first)")
    ap.add_argument("--check", action="store_true", help="report drift only (default when no --apply)")
    ap.add_argument("--verify", action="store_true", help="scan cspatterns sigs against on-disk DLLs")
    ap.add_argument("--save-sigs", nargs="?", const="", metavar="PATH",
                    help="save cspatterns json (default: offsets dir/patterns_cspatterns.json)")
    ap.add_argument("--offsets", type=Path, default=DEFAULT_OFFSETS, help="offsets.h path")
    ap.add_argument("--game", type=Path, default=DEFAULT_GAME, help="CS2 'game' directory")
    ap.add_argument("--timeout", type=float, default=30.0, help="http timeout seconds")
    ap.add_argument("--no-cspatterns", action="store_true", help="skip cspatterns.dev fetch")
    ap.add_argument("--selftest", action="store_true", help="run built-in checks")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args(argv)

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(levelname)s %(message)s")

    if args.selftest:
        return selftest()

    rc = 0
    cspatterns: dict = {}
    if not args.no_cspatterns:
        try:
            cspatterns = json.loads(fetch(CSPATTERNS_JSON, args.timeout))
            log.info("cspatterns.dev: %d modules, %d signatures",
                     len(cspatterns), sum(len(v) for v in cspatterns.values()))
        except Exception as exc:  # noqa: BLE001
            log.warning("cspatterns.dev fetch failed: %s", exc)

    if args.save_sigs is not None:
        dest = Path(args.save_sigs) if args.save_sigs else args.offsets.parent / "patterns_cspatterns.json"
        if cspatterns:
            dest.write_text(json.dumps(cspatterns, indent=2), encoding="utf-8")
            log.info("signatures saved: %s", dest)
        else:
            log.error("no cspatterns data to save")
            rc = 1

    if args.verify:
        if not cspatterns:
            log.error("--verify needs cspatterns.dev (or drop --no-cspatterns)")
            return 1
        ok, multi, miss, lines = run_verify(cspatterns, args.game)
        print("\n".join(lines))
        print(f"\nverify: OK {ok}  MULTI {multi}  MISS {miss}  total {ok + multi + miss}")
        verdict = "PATTERNS CURRENT" if miss == 0 else f"{miss} MISS - sources may be stale"
        print(verdict)
        if miss:
            rc = 2
        if not (args.apply or args.check or args.save_sigs is not None):
            return rc

    if args.apply or args.check:
        if not args.apply and not args.check:
            args.check = True
        try:
            offs_text_raw = fetch(A2X_RAW % "offsets.hpp", args.timeout)
            client_text = fetch(A2X_RAW % "client_dll.hpp", args.timeout)
            buttons_text = fetch(A2X_RAW % "buttons.hpp", args.timeout)
        except RuntimeError as exc:
            log.error("%s", exc)
            return 1

        a2x, gen_date = parse_a2x(offs_text_raw)
        schemas, schemas_date = parse_schemas(client_text)
        btn_map, _ = parse_a2x(buttons_text)
        a2x_buttons = btn_map.get("buttons", {})
        log.info("a2x generated: offsets %s / schemas %s (buttons %s)",
                 gen_date, schemas_date, parse_generated(buttons_text))
        log.info("a2x modules: %s", ", ".join(sorted(a2x)))
        log.info("schema fields: %d in %d classes",
                 sum(len(v) for v in schemas.values()), len(schemas))

        if not args.offsets.is_file():
            log.error("offsets.h not found: %s", args.offsets)
            return 1
        changed, report = update_offsets_h(args.offsets, a2x, schemas, a2x_buttons,
                                           apply=args.apply, add_new=False)
        print("\n".join(report))
        if args.check and changed:
            rc = 2

    return rc


# ------------------------------------------------------------- selftest

def selftest() -> int:
    sample = """#pragma once
namespace Offset {
    namespace FB {
        constexpr std::uint32_t m_iHealth = 0x100; // C_BaseEntity
        constexpr std::ptrdiff_t dwFoo = 0x200; // note
    }
    namespace client_dll {
        constexpr std::ptrdiff_t dwEntityList = 0x111;
    }
    namespace buttons {
        constexpr std::ptrdiff_t attack = 0x222;
    }
}
"""
    entries = walk_namespaces(sample)
    by = {(e.ns[-1], e.name): e for e in entries}
    assert by[("FB", "m_iHealth")].value == 0x100, "fb walk"
    assert by[("FB", "m_iHealth")].comment == "C_BaseEntity", "comment kept"
    assert by[("FB", "dwFoo")].value == 0x200, "fb dw walk"
    assert by[("client_dll", "dwEntityList")].value == 0x111, "dump walk"
    assert by[("buttons", "attack")].value == 0x222, "buttons walk"

    a2x = """namespace cs2_dumper {
    namespace offsets {
        namespace client_dll {
            constexpr std::ptrdiff_t dwEntityList = 0x999;
        }
        namespace engine2_dll {
            constexpr std::ptrdiff_t dwBuildNumber = 0xAAA;
        }
    }
    namespace schemas {
        namespace client_dll {
            namespace C_BaseEntity {
                constexpr std::ptrdiff_t m_iHealth = 0x34C; // int32
            }
            namespace CGameSceneNode {
                constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8; // VectorWS
            }
        }
    }
    namespace buttons {
        constexpr std::ptrdiff_t attack = 0x333;
    }
}"""
    mods, date = parse_a2x(a2x)
    assert mods["client_dll"]["dwEntityList"].value == 0x999, "a2x offsets"
    assert mods["engine2_dll"]["dwBuildNumber"].value == 0xAAA, "a2x engine2"
    assert mods["buttons"]["attack"].value == 0x333, "a2x buttons"
    assert date.startswith("unknown") or date, "date parse"
    schemas, _ = parse_schemas(a2x)
    assert schemas["C_BaseEntity"]["m_iHealth"].value == 0x34C, "schemas parse"

    assert fb_resolve("m_iHealth", "", schemas, mods) == (0x34C, "priority C_BaseEntity"), "fb priority"
    assert fb_resolve("m_vecAbsOrigin", "CGameSceneNode", schemas, mods) == (0xC8, "class hint CGameSceneNode"), "fb hint"
    assert fb_resolve("dwFoo", "note", schemas, mods) == (None, "missing"), "fb dw miss"
    assert fb_resolve("dwEntityList", "", schemas, mods) == (0x999, "dump"), "fb dw from dump"

    pat = compile_pattern("48 8B 05 ? ? ? ? 48 85")
    data = b"\xcc" * 8 + b"\x48\x8b\x05\x00\x00\x00\x00\x48\x85" + b"\xcc" * 8
    hits = scan_pattern(data, pat)
    assert hits == [8], f"pattern scan {hits}"
    assert compile_pattern("? ? ?") is None, "no anchor"

    full = b"MZ" + b"\0" * 0x3A + struct.pack("<I", 0x80) + b"\0" * 0x40
    full += b"PE\0\0" + struct.pack("<HHIIIHH", 0x8664, 1, 0, 0, 0, 0xF0, 0x22B)
    full += b"\0" * 0xF0                                    # optional header stub
    full += b".text\0\0\0" + struct.pack("<IIII", 0x10, 0x1000, 0x10, 0x200) + b"\0" * 16
    full += b"\0" * (0x200 - len(full))                     # pad to the raw pointer
    full += b"\x48\x8b\x05\x01\x02\x03\x04" + b"\0" * (0x400 - 0x200 - 7)
    sections = pe_sections(full)
    assert scan_pattern(full, compile_pattern("48 8B 05 01 02 03 04")) == [0x200], "scan e2e"
    assert file_off_to_rva(sections, 0x200) == 0x1000, "rva map"

    print("selftest: all passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
