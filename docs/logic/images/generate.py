"""Generate the figures used by docs/logic/*.md.

Usage (from the repository root):

    python docs/logic/images/generate.py

Requires matplotlib. Curves are computed from the same constants and
tables as the C++ code (e.g. the Threepeat hop path is parsed from
`FreeClimbAnimationInput/include/animation/ThreepeatMotion.h`), so the
pictures stay true to the implementation.
"""

from __future__ import annotations

import math
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle  # noqa: E402

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]

# Shared palette.
BLUE, GREEN, ORANGE, RED, GREY, PURPLE = (
    "#4C78A8",
    "#54A24B",
    "#F58518",
    "#E45756",
    "#9D9D9D",
    "#B279A2",
)
LIGHT = {
    "blue": "#DCE8F5",
    "green": "#DFF0DA",
    "orange": "#FDE6CC",
    "red": "#F9DADA",
    "grey": "#EEEEEE",
    "purple": "#EFE0EC",
}


# Small drawing helpers ------------------------------------------------------


def box(ax, x, y, w, h, text, face, edge=GREY, size=10, weight="normal"):
    """Rounded box centred at (x, y)."""
    ax.add_patch(
        FancyBboxPatch(
            (x - w / 2, y - h / 2),
            w,
            h,
            boxstyle="round,pad=0.02,rounding_size=0.06",
            facecolor=face,
            edgecolor=edge,
            linewidth=1.4,
        )
    )
    ax.text(
        x,
        y,
        text,
        ha="center",
        va="center",
        fontsize=size,
        fontweight=weight,
        wrap=True,
    )


def arrow(
    ax, a, b, text="", color="#444444", style="-|>", rad=0.0, size=9, offset=(0, 0)
):
    """Arrow from point a to point b with an optional label."""
    ax.add_patch(
        FancyArrowPatch(
            a,
            b,
            arrowstyle=style,
            mutation_scale=14,
            color=color,
            linewidth=1.4,
            connectionstyle=f"arc3,rad={rad}",
        )
    )
    if text:
        mx, my = (a[0] + b[0]) / 2 + offset[0], (a[1] + b[1]) / 2 + offset[1]
        ax.text(
            mx,
            my,
            text,
            ha="center",
            va="center",
            fontsize=size,
            color=color,
            backgroundcolor="white",
        )


def canvas(w, h, title):
    fig, ax = plt.subplots(figsize=(w, h))
    ax.set_xlim(0, w)
    ax.set_ylim(0, h)
    ax.axis("off")
    ax.set_title(title, fontsize=14, fontweight="bold", pad=12)
    return fig, ax


def save(fig, name):
    fig.tight_layout()
    fig.savefig(HERE / name, dpi=110, facecolor="white")
    plt.close(fig)
    print("wrote", name)


def ease(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * x * (10 + x * (-15 + 6 * x))


def smoothstep(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


# Figures --------------------------------------------------------------------


def architecture():
    fig, ax = canvas(12, 7.2, "How the three FreeClimb projects fit together")
    box(
        ax,
        6,
        6.3,
        6.5,
        0.8,
        "Skyrim SE / AE  +  SKSE (script extender)",
        LIGHT["grey"],
        size=11,
        weight="bold",
    )
    box(
        ax,
        6,
        4.6,
        5.2,
        1.4,
        "FreeClimb  (FreeClimb.dll)\nhooks, per-frame update, pose output,\n"
        "audio, settings menu",
        LIGHT["blue"],
        BLUE,
        11,
        "bold",
    )
    box(
        ax,
        2.6,
        2.2,
        4.4,
        1.5,
        "FreeClimbSettings\nFreeClimb.ini, key bindings,\ngamepad, menu translations",
        LIGHT["green"],
        GREEN,
        10.5,
        "bold",
    )
    box(
        ax,
        9.4,
        2.2,
        4.4,
        1.5,
        "FreeClimbAnimationInput\nclimbing brain (Core), body poses,\nHKX + animation pack loader",
        LIGHT["orange"],
        ORANGE,
        10.5,
        "bold",
    )
    arrow(ax, (6, 5.9), (6, 5.3), "loads & calls every frame")
    arrow(ax, (4.6, 3.95), (3.4, 2.95), "uses")
    arrow(ax, (7.4, 3.95), (8.6, 2.95), "uses")
    arrow(ax, (4.8, 2.2), (7.2, 2.2), "uses climbing types")
    box(
        ax,
        2.6,
        0.55,
        4.6,
        0.7,
        "Data/SKSE/Plugins/FreeClimb.ini\nInterface/Translations/*.txt",
        "white",
        GREEN,
        9,
    )
    box(
        ax,
        9.4,
        0.55,
        4.6,
        0.7,
        "meshes/.../animations/FreeClimb/\npack.json, skeleton.json, *.hkx",
        "white",
        ORANGE,
        9,
    )
    arrow(ax, (2.6, 0.9), (2.6, 1.45), "reads")
    arrow(ax, (9.4, 0.9), (9.4, 1.45), "reads")
    save(fig, "architecture.png")


def state_machine():
    fig, ax = canvas(12, 7.5, "The climbing state machine (Traversal::state)")
    pos = {
        "idle": (1.6, 3.8),
        "approach": (4.6, 6.2),
        "wall": (7.4, 3.8),
        "ledge": (10.4, 6.2),
        "mantle": (10.4, 1.4),
        "action": (4.6, 1.4),
    }
    text = {
        "idle": "idle\n(not climbing)",
        "approach": "approach\n(moving onto wall)",
        "wall": "wall\n(holding on)",
        "ledge": "ledge\n(hanging on a lip)",
        "mantle": "mantle\n(climbing over top)",
        "action": "action\n(hop, drop, flip,\nkick, wall run jump)",
    }
    colors = {
        "idle": LIGHT["grey"],
        "approach": LIGHT["blue"],
        "wall": LIGHT["green"],
        "ledge": LIGHT["green"],
        "mantle": LIGHT["orange"],
        "action": LIGHT["purple"],
    }
    for key, (x, y) in pos.items():
        box(ax, x, y, 2.5, 1.1, text[key], colors[key], size=10, weight="bold")
    arrow(ax, (2.3, 4.4), (3.6, 5.7), "attach()\nfound a wall", rad=-0.15)
    arrow(ax, (5.6, 5.7), (6.7, 4.4), "arrived", rad=-0.15)
    arrow(ax, (8.6, 4.2), (9.3, 5.7), "top edge\nwithin reach", rad=-0.2)
    arrow(ax, (9.3, 5.6), (8.5, 4.3), "", rad=-0.2)
    arrow(ax, (8.4, 3.3), (9.6, 1.95), "W pressed\nat the top", rad=0.15)
    # Route under every box: mantle -> down -> left -> up into idle.
    ax.plot([10.4, 10.4, 0.6, 0.6], [0.85, 0.3, 0.3, 2.9], color="#444444", lw=1.4)
    arrow(ax, (0.6, 2.9), (0.6, 3.25))
    ax.text(
        6.0,
        0.3,
        "landed on top (released)",
        ha="center",
        va="center",
        fontsize=9,
        color="#444444",
        backgroundcolor="white",
    )
    arrow(
        ax,
        (6.5, 3.3),
        (5.4, 1.95),
        "Space / S+Space /\nautomatic action",
        rad=0.15,
        offset=(-0.6, 0),
    )
    arrow(ax, (5.6, 1.6), (6.7, 3.3), "caught next hold", rad=0.25, offset=(0.9, -0.1))
    arrow(ax, (3.5, 1.2), (1.9, 3.2), "dropped off", rad=-0.25, offset=(-0.6, -0.2))
    arrow(
        ax,
        (6.2, 3.9),
        (2.9, 3.9),
        "stamina empty / support\nlost / reached ground",
        offset=(0, 0.35),
    )
    save(fig, "state_machine.png")


def frame_loop():
    fig, ax = canvas(10, 11, "One game frame inside FreeClimb (about 1/60 s)")
    steps = [
        ("1. Game runs its own player update", LIGHT["grey"]),
        (
            "2. Apply pending menu changes;\ncheck menus, controller, teleports",
            LIGHT["green"],
        ),
        (
            "3. Read keys / gamepad -> climbing input\n(W/A/S/D, Shift, Space, drop)",
            LIGHT["green"],
        ),
        (
            "4a. Not climbing: watch for the entry chord\nor a jump grab -> attach()",
            LIGHT["blue"],
        ),
        (
            "4b. Climbing: Traversal.update()\nmoves the body along the wall",
            LIGHT["blue"],
        ),
        (
            "5. Solve the body pose\n(SurfacePose: hands & feet on real geometry)",
            LIGHT["orange"],
        ),
        (
            "6. Publish the pose; engine hooks write it\ninto the skeleton during rendering",
            LIGHT["orange"],
        ),
        ("7. Play step / grip / push sounds", LIGHT["purple"]),
        (
            "8. Turn the player toward the wall, spend\nstamina, or release when something fails",
            LIGHT["red"],
        ),
    ]
    y = 10.0
    for i, (label, face) in enumerate(steps):
        box(ax, 5, y, 7.6, 0.85, label, face, size=10)
        if i < len(steps) - 1:
            arrow(ax, (5, y - 0.45), (5, y - 0.72))
        y -= 1.15
    save(fig, "frame_loop.png")


def wall_geometry():
    fig, ax = plt.subplots(figsize=(10, 7))
    ax.set_title(
        "What the climbing brain measures (side view, default settings)",
        fontsize=13,
        fontweight="bold",
    )
    gap, radius, height, chest, grip, reach = 30, 22, 125, 70, 112, 110
    ax.add_patch(Rectangle((0, -10), 40, 220, color="#BDBDBD"))
    ax.text(20, 200, "WALL", ha="center", fontsize=12, fontweight="bold")
    feet_x = 40 + gap
    ax.add_patch(
        Rectangle(
            (feet_x, 0),
            radius * 1.2,
            height,
            fill=False,
            edgecolor=BLUE,
            linewidth=2,
            linestyle="--",
        )
    )
    ax.text(
        feet_x + radius * 0.6,
        height + 6,
        "body capsule\nheight 125, radius 22",
        ha="center",
        color=BLUE,
    )
    for h, name, color in [
        (grip, "grip probe (hands) 112", GREEN),
        (chest, "chest probe 70", ORANGE),
        (6, "feet 6", PURPLE),
    ]:
        ax.annotate(
            "",
            xy=(40, h),
            xytext=(feet_x + 40, h),
            arrowprops=dict(arrowstyle="->", color=color, lw=2),
        )
        ax.text(feet_x + 44, h, name, va="center", color=color)
    ax.annotate(
        "",
        xy=(40, -4),
        xytext=(feet_x, -4),
        arrowprops=dict(arrowstyle="<->", color=RED, lw=2),
    )
    ax.text(40 + gap / 2, -14, "gap 30", ha="center", color=RED)
    ax.annotate(
        "",
        xy=(feet_x + reach + 40, 40),
        xytext=(40, 40),
        arrowprops=dict(arrowstyle="<->", color=GREY, lw=1.5),
    )
    ax.text(
        feet_x + 60,
        30,
        "reach 110: how far a wall may be to start climbing",
        color="#555555",
    )
    ax.set_xlim(-5, 260)
    ax.set_ylim(-25, 215)
    ax.set_aspect("equal")
    ax.set_xlabel("game units (1 unit ~ 1.4 cm)")
    ax.grid(alpha=0.2)
    save(fig, "wall_geometry.png")


def blend_envelope():
    fig, axes = plt.subplots(1, 2, figsize=(11, 4))
    t = [i / 200 for i in range(201)]
    entry = [0.01 + 0.99 * ease(x / 0.18) for x in [v * 0.3 for v in t]]
    axes[0].plot([v * 0.3 for v in t], entry, color=BLUE, lw=2.5)
    axes[0].set_title("Blending IN when a climb starts (0.18 s)")
    axes[0].set_xlabel("seconds after attach")
    axes[0].set_ylabel("FreeClimb's share of the pose")
    for seconds, name, color in [
        (0.28, "normal exit 0.28 s", GREEN),
        (0.16, "falling exit 0.16 s", RED),
        (0.12, "top handover 0.12 s", ORANGE),
    ]:
        xs = [v * 0.3 for v in t]
        axes[1].plot(
            xs, [1 - ease(x / seconds) for x in xs], lw=2.5, color=color, label=name
        )
    axes[1].set_title("Blending OUT when a climb ends")
    axes[1].set_xlabel("seconds after release")
    axes[1].legend()
    for ax in axes:
        ax.set_ylim(-0.05, 1.05)
        ax.grid(alpha=0.3)
    save(fig, "blend_envelope.png")


def threepeat_tables():
    text = (
        ROOT / "FreeClimbAnimationInput/include/animation/ThreepeatMotion.h"
    ).read_text(encoding="utf-8")
    tables = {}
    for side in ("Left", "Right"):
        start = text.index(f"threepeat{side}Path{{{{")
        body = text[start : text.index("}};", start)]
        rows = re.findall(
            r"\{\s*([-\d.]+)f?,\s*([-\d.]+)f?,\s*([-\d.]+)f?,\s*([-\d.]+)f?\s*\}", body
        )
        tables[side] = [tuple(float(v) for v in row) for row in rows]
    return tables


def threepeat_paths():
    tables = threepeat_tables()
    fig, axes = plt.subplots(1, 2, figsize=(12.5, 4.6))
    share = axes[0].twinx()
    for side, color in (("Left", BLUE), ("Right", ORANGE)):
        rows = tables[side]
        phase = [r[0] for r in rows]
        axes[0].plot(
            phase, [r[2] for r in rows], color=color, lw=2.5, label=f"{side} hop: lift"
        )
        axes[0].plot(
            phase,
            [r[3] for r in rows],
            color=color,
            lw=1.4,
            ls=":",
            label=f"{side} hop: away from wall",
        )
        share.plot(
            phase,
            [r[1] for r in rows],
            color=color,
            lw=1.8,
            ls="--",
            label=f"{side} hop: sideways progress",
        )
    axes[0].set_title("Threepeat sideways hop: body path (built-in table)")
    axes[0].set_xlabel("hop phase (0 = start, 1 = landed)")
    axes[0].set_ylabel("lift / away from wall (game units)")
    share.set_ylabel("sideways progress (0..1 of the hop distance)")
    share.set_ylim(-0.05, 1.6)
    lines = axes[0].get_legend_handles_labels()
    more = share.get_legend_handles_labels()
    axes[0].legend(
        lines[0] + more[0], lines[1] + more[1], fontsize=8, loc="upper right"
    )
    axes[0].grid(alpha=0.3)

    hop = 1.3
    windows = {
        "left hand lets go": (0.03 / hop, 0.10 / hop, BLUE, False),
        "right hand lets go": (0.13 / hop, 0.23 / hop, ORANGE, False),
        "left hand catches": (0.50 / hop, 0.66 / hop, BLUE, True),
        "right hand catches": (0.61 / hop, 0.71 / hop, ORANGE, True),
    }
    xs = [i / 300 for i in range(301)]
    for name, (a, b, color, rising) in windows.items():
        ys = [smoothstep((x - a) / (b - a)) for x in xs]
        if not rising:
            ys = [1 - y for y in ys]
        axes[1].plot(
            xs, ys, color=color, lw=2.3, ls="-" if rising else "--", label=name
        )
    axes[1].set_title("Leftward hop: when each hand holds (1 = gripping)")
    axes[1].set_xlabel("hop phase")
    axes[1].legend(fontsize=8)
    axes[1].grid(alpha=0.3)
    save(fig, "threepeat_hop.png")


def pack_loading():
    fig, ax = canvas(12, 6.2, "Loading the animation pack: all or nothing")
    steps = [
        (1.2, "pack.json\n(list of slots)"),
        (3.4, "skeleton.json\n(99 bones)"),
        (5.6, "configs/<slot>.json\n(stride, contacts,\npaths, windows)"),
        (7.8, "<slot>.hkx\n(Havok animation)"),
        (10.2, "validate every\nslot + limits"),
    ]
    for x, label in steps:
        box(ax, x, 4.4, 1.95, 1.3, label, LIGHT["orange"], ORANGE, 9.5)
    for a, b in zip(steps, steps[1:]):
        arrow(ax, (a[0] + 1.0, 4.4), (b[0] - 1.0, 4.4))
    box(
        ax,
        7.8,
        1.5,
        3.6,
        1.1,
        "all 35 slots OK\n-> swap in the new library",
        LIGHT["green"],
        GREEN,
        10,
    )
    box(
        ax,
        3.2,
        1.5,
        4.2,
        1.1,
        "anything wrong\n-> keep the previous library,\nreport why",
        LIGHT["red"],
        RED,
        10,
    )
    arrow(ax, (10.2, 3.7), (8.9, 2.1), "pass")
    arrow(ax, (9.6, 3.7), (4.6, 2.1), "fail", rad=-0.15)
    ax.text(
        6,
        0.35,
        "Built in a separate 'staged' library first, so a broken file can never "
        "leave the game half-loaded.",
        ha="center",
        fontsize=9.5,
        style="italic",
    )
    save(fig, "pack_loading.png")


def contacts_curve():
    fig, ax = plt.subplots(figsize=(10, 4))
    phase = [i / 100 for i in range(101)]
    # Illustrative climb-up cycle: hands and feet alternate.
    curves = {
        "left hand": lambda p: 0.5 + 0.5 * math.cos(2 * math.pi * p),
        "right hand": lambda p: 0.5 - 0.5 * math.cos(2 * math.pi * p),
        "left foot": lambda p: 0.5 - 0.5 * math.cos(2 * math.pi * p),
        "right foot": lambda p: 0.5 + 0.5 * math.cos(2 * math.pi * p),
    }
    styles = {
        "left hand": (BLUE, "-"),
        "right hand": (ORANGE, "-"),
        "left foot": (BLUE, ":"),
        "right foot": (ORANGE, ":"),
    }
    for name, f in curves.items():
        color, ls = styles[name]
        ax.plot(phase, [f(p) for p in phase], color=color, ls=ls, lw=2.3, label=name)
    ax.axhline(0.65, color=GREY, lw=1, ls="--")
    ax.text(
        1.01,
        0.65,
        "sound plays when a limb\nnewly passes 0.65",
        va="center",
        fontsize=8,
    )
    ax.set_title("Example 'contacts' curve of one climbing cycle (illustrative)")
    ax.set_xlabel("cycle phase")
    ax.set_ylabel("contact weight (1 = planted)")
    ax.set_xlim(0, 1.25)
    ax.legend(loc="lower left", fontsize=8)
    ax.grid(alpha=0.3)
    save(fig, "contacts.png")


def settings_flow():
    fig, ax = canvas(12, 6.5, "Settings: from the INI file to the running game")
    box(ax, 1.6, 5.0, 2.6, 1.0, "FreeClimb.ini\n(text file)", "white", GREEN, 10)
    box(ax, 4.7, 5.0, 2.6, 1.0, "parse\n[Section] Key=Value", LIGHT["green"], GREEN, 10)
    box(
        ax,
        7.8,
        5.0,
        2.6,
        1.0,
        "sanitize\nclamp ranges,\nvalidate keys",
        LIGHT["green"],
        GREEN,
        10,
    )
    box(ax, 10.6, 5.0, 2.2, 1.0, "UserSettings\n(in memory)", LIGHT["blue"], BLUE, 10)
    arrow(ax, (2.9, 5.0), (3.4, 5.0))
    arrow(ax, (6.0, 5.0), (6.5, 5.0))
    arrow(ax, (9.1, 5.0), (9.5, 5.0))
    box(
        ax,
        10.6,
        2.6,
        2.4,
        1.0,
        "climbing, audio,\ninput use them",
        LIGHT["blue"],
        BLUE,
        10,
    )
    arrow(ax, (10.6, 4.5), (10.6, 3.1), "apply")
    box(
        ax,
        6.2,
        2.6,
        3.0,
        1.0,
        "in-game menu\n(SKSE Menu Framework)",
        LIGHT["purple"],
        PURPLE,
        10,
    )
    arrow(ax, (7.7, 2.6), (9.4, 2.6), "Save")
    box(
        ax,
        2.4,
        2.6,
        3.4,
        1.0,
        "write FreeClimb.ini.freeclimb.tmp\nthen replace the INI",
        LIGHT["grey"],
        GREY,
        9,
    )
    arrow(ax, (4.7, 2.6), (4.1, 2.6), "")
    ax.annotate(
        "",
        xy=(1.6, 4.5),
        xytext=(2.0, 3.1),
        arrowprops=dict(arrowstyle="->", color="#444444", lw=1.4),
    )
    ax.text(
        6,
        0.7,
        "Bad values never crash the game: out-of-range numbers are clamped, "
        "broken key combinations fall back to the defaults,\nand every repair is "
        "written to the log as a warning.",
        ha="center",
        fontsize=9.5,
        style="italic",
    )
    save(fig, "settings_flow.png")


def translation_lookup():
    fig, ax = canvas(12, 3.6, "Finding the text for a menu label")
    labels = [
        ("selected language file\nFreeClimb_chinese.txt", LIGHT["blue"]),
        ("English file\nFreeClimb_english.txt", LIGHT["green"]),
        ("built-in English\n(inside the DLL)", LIGHT["orange"]),
        ('"[Missing translation]"', LIGHT["red"]),
    ]
    xs = [1.6, 4.6, 7.6, 10.5]
    for x, (label, face) in zip(xs, labels):
        box(ax, x, 1.8, 2.6, 1.1, label, face, size=10)
    for a, b in zip(xs, xs[1:]):
        arrow(ax, (a + 1.3, 1.8), (b - 1.3, 1.8), "not found", offset=(0, 0.45))
    save(fig, "translation_lookup.png")


def input_ownership():
    fig, ax = plt.subplots(figsize=(11, 3.8))
    ax.set_title(
        "Who receives a key: the game or FreeClimb (entry chord W+A+D+Space)",
        fontsize=12,
        fontweight="bold",
    )
    keys = ["W", "A", "D", "Space"]
    presses = {"W": (0.5, 6.5), "A": (1.0, 6.0), "D": (1.4, 6.2), "Space": (2.0, 2.6)}
    for i, key in enumerate(keys):
        a, b = presses[key]
        ax.add_patch(Rectangle((a, i - 0.3), min(b, 2.0) - a, 0.6, color=GREY))
        if b > 2.0:
            ax.add_patch(Rectangle((2.0, i - 0.3), b - 2.0, 0.6, color=ORANGE))
    ax.axvline(2.0, color=RED, lw=2)
    ax.text(2.05, 3.55, "chord complete -> climb starts", color=RED, fontsize=9)
    ax.set_yticks(range(len(keys)))
    ax.set_yticklabels(keys)
    ax.set_xlabel("time (s)")
    ax.set_xlim(0, 7)
    ax.set_ylim(-0.7, 3.9)
    ax.add_patch(Rectangle((4.6, -0.6), 0.25, 0.25, color=GREY))
    ax.text(4.9, -0.48, "game sees it (walking)", fontsize=8.5)
    ax.add_patch(Rectangle((5.9, -0.6), 0.25, 0.25, color=ORANGE))
    ax.text(6.2, -0.48, "hidden from game", fontsize=8.5)
    save(fig, "input_ownership.png")


def main():
    for figure in (
        architecture,
        state_machine,
        frame_loop,
        wall_geometry,
        blend_envelope,
        threepeat_paths,
        pack_loading,
        contacts_curve,
        settings_flow,
        translation_lookup,
        input_ownership,
    ):
        figure()


if __name__ == "__main__":
    main()
