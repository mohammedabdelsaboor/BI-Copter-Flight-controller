#!/usr/bin/env python3
"""
Renders the 3D-printed mechanism parts in hardware/3d-parts/ to an image for the README and the PDF guide.

    pip install matplotlib numpy
    python3 tools/render_parts.py

Reads the STL files (binary or ASCII), shades every triangle with a simple directional light and draws the
parts side by side as panels (a) and (b): docs/figures/fig8_mechanism_parts.png.
"""

import os
import struct

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from mpl_toolkits.mplot3d.art3d import Poly3DCollection  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PARTS = [
    # path, caption, (elevation, azimuth), zoom (1 = whole part with the default 3D-axes margin)
    ("hardware/3d-parts/brushless-motor-mount/files/brushless_motor_mount.stl", "(a) Motor mount, two variants",
     (24, -58), 0.80),
    ("hardware/3d-parts/servo-mount/files/servo-mount.stl", "(b) Servo mount", (28, -50), 0.62),
]
OUT = os.path.join(ROOT, "docs", "figures", "fig8_mechanism_parts.png")

BASE = np.array([0.55, 0.62, 0.70])  # neutral grey-blue, like printed PLA under daylight
LIGHT = np.array([0.4, -0.6, 0.8])


def load_stl(path):
    """Returns an (n, 3, 3) array of triangles. Handles binary and ASCII STL."""
    data = open(path, "rb").read()
    count = struct.unpack("<I", data[80:84])[0] if len(data) >= 84 else 0
    if len(data) == 84 + 50 * count:
        raw = np.frombuffer(data, dtype=np.uint8, offset=84, count=50 * count).reshape(count, 50)
        return raw[:, 12:48].copy().view("<f4").reshape(count, 3, 3).astype(float)
    vertices = [list(map(float, line.split()[1:4])) for line in data.decode(errors="ignore").splitlines()
                if line.strip().startswith("vertex")]
    return np.array(vertices, dtype=float).reshape(-1, 3, 3)


def shade(triangles):
    normals = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
    length = np.linalg.norm(normals, axis=1, keepdims=True)
    normals = normals / np.where(length == 0, 1, length)
    light = LIGHT / np.linalg.norm(LIGHT)
    intensity = 0.35 + 0.65 * np.abs(normals @ light)  # ambient + diffuse, two-sided
    return np.clip(BASE[None, :] * intensity[:, None] + 0.08, 0, 1)


def draw(ax, triangles, title, view, zoom):
    centre = (triangles.reshape(-1, 3).max(axis=0) + triangles.reshape(-1, 3).min(axis=0)) / 2
    triangles = triangles - centre
    span = np.ptp(triangles.reshape(-1, 3), axis=0).max() / 2 * zoom
    mesh = Poly3DCollection(triangles, facecolors=shade(triangles), edgecolors="none", linewidths=0)
    ax.add_collection3d(mesh)
    for setter in (ax.set_xlim, ax.set_ylim, ax.set_zlim):
        setter(-span, span)
    ax.set_box_aspect((1, 1, 1))
    ax.view_init(elev=view[0], azim=view[1])
    ax.set_axis_off()
    ax.set_title(title, y=0.0, fontsize=9, family="serif", color="#1d1d1b")


def main():
    fig = plt.figure(figsize=(6.5, 2.7), dpi=300, facecolor="white")
    for i, (path, title, view, zoom) in enumerate(PARTS):
        ax = fig.add_subplot(1, 2, i + 1, projection="3d")
        triangles = load_stl(os.path.join(ROOT, path))
        draw(ax, triangles, title, view, zoom)
        print(f"{path}: {len(triangles)} triangles")
    fig.subplots_adjust(left=0, right=1, top=1, bottom=0.06, wspace=0)
    fig.savefig(OUT, facecolor="white", bbox_inches="tight", pad_inches=0.05)
    print("wrote", os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()
