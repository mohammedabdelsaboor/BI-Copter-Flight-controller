#!/usr/bin/env python3
"""
Builds the result figures in docs/figures/ from the recorded data and the simulations.

    pip install matplotlib
    python3 tools/make_figures.py

Figure 1 is measured: a 20 s window of the logged throttle sweep in docs/data/.
Figure 2 is simulated with the firmware's own AttitudeEstimator (tools/sim/attitude_sim.cpp, needs g++).
Figure 3 is simulated with a simple MG996R-like servo model (defined below).
"""

import csv
import io
import math
import os
import re
import subprocess
import tempfile

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIG_DIR = os.path.join(ROOT, "docs", "figures")
LOG = os.path.join(ROOT, "docs", "data", "throttle_sweep_2026-10-24.txt")

# Colours: categorical slots 1 and 2 of the reference data-viz palette (validated light-mode pair),
# text and grid in neutral inks. Colour follows the entity: pitch is always orange, roll and the new
# filter blue.
SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3de"
REFERENCE = "#8a8984"
BLUE = "#2a78d6"
ORANGE = "#eb6834"


def style(ax, ylabel, xlabel=None):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRID)
    ax.tick_params(colors=TEXT_2, labelsize=9, length=0)
    ax.grid(True, axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    ax.set_ylabel(ylabel, color=TEXT_2, fontsize=9.5)
    if xlabel:
        ax.set_xlabel(xlabel, color=TEXT_2, fontsize=9.5)


def end_label(ax, x, y, text):
    """Direct label at the end of a line, in text ink (the line next to it carries the colour)."""
    ax.annotate(text, (x, y), xytext=(6, 0), textcoords="offset points", va="center", fontsize=9, color=TEXT)


def new_figure(height):
    fig = plt.figure(figsize=(7.6, height), dpi=160, facecolor=SURFACE)
    return fig


def title(fig, main, sub):
    fig.text(0.075, 0.965, main, fontsize=12, fontweight="bold", color=TEXT, va="top")
    fig.text(0.075, 0.915, sub, fontsize=9.5, color=TEXT_2, va="top")


# ----------------------------------------------------------------------------------------------------------------
#  Figure 1: measured roll vs pitch
# ----------------------------------------------------------------------------------------------------------------
def figure_flight_log():
    pat = re.compile(r"(\w+) thr ([\d.]+) \| R +(\S+) P +(\S+) \| rate .*?\| cmd (\S+) (\S+) (\S+)")
    rows = []
    for line in open(LOG):
        m = pat.search(line)
        if m:
            rows.append((m[1], float(m[2]), float(m[3]), float(m[4]), float(m[6])))
    # Longest armed stretch above 15 % throttle, then the representative 10-30 s window inside it
    start = next(i for i, r in enumerate(rows) if r[0] == "ARMED" and r[1] > 0.15)
    window = rows[start + 100:start + 300]
    t = [i / 10.0 for i in range(len(window))]  # the console stream runs at 10 Hz
    roll = [r[2] for r in window]
    pitch = [r[3] for r in window]
    cmd = [r[4] for r in window]

    fig = new_figure(5.4)
    title(fig, "Measured: roll holds level, pitch swings",
          "20 s of a logged throttle sweep at 25 % throttle, earlier controller version; command 1 = full tilt")
    ax1 = fig.add_axes([0.075, 0.47, 0.80, 0.36])
    ax2 = fig.add_axes([0.075, 0.10, 0.80, 0.27], sharex=ax1)
    style(ax1, "Angle (deg)")
    style(ax2, "Pitch command", "Time (s)")

    ax1.plot(t, roll, color=BLUE, linewidth=2, label="Roll (motors)")
    ax1.plot(t, pitch, color=ORANGE, linewidth=2, label="Pitch (servos)")
    ax1.axhline(0, color=REFERENCE, linewidth=0.8)
    end_label(ax1, t[-1], roll[-1], "Roll")
    end_label(ax1, t[-1], pitch[-1], "Pitch")
    ax1.legend(loc="upper left", frameon=False, fontsize=9, ncol=2, labelcolor=TEXT)
    plt.setp(ax1.get_xticklabels(), visible=False)

    ax2.plot(t, cmd, color=ORANGE, linewidth=2)
    ax2.axhline(0, color=REFERENCE, linewidth=0.8)
    ax2.set_ylim(-1.05, 1.05)
    out = os.path.join(FIG_DIR, "fig1_roll_vs_pitch.png")
    fig.savefig(out, facecolor=SURFACE)
    plt.close(fig)
    return out


# ----------------------------------------------------------------------------------------------------------------
#  Figure 2: simulated attitude error, complementary vs Mahony
# ----------------------------------------------------------------------------------------------------------------
def run_attitude_sim():
    src = [os.path.join(ROOT, p) for p in ("tools/sim/attitude_sim.cpp",
                                           "lib/AttitudeEstimator/AttitudeEstimator.cpp",
                                           "lib/BiquadFilter/BiquadFilter.cpp")]
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "attitude_sim")
        subprocess.run(["g++", "-O2", "-std=c++17", "-I" + os.path.join(ROOT, "lib/AttitudeEstimator"),
                        "-I" + os.path.join(ROOT, "lib/BiquadFilter"), *src, "-o", exe], check=True)
        out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout
    return list(csv.DictReader(io.StringIO(out)))


def figure_attitude(rows):
    t = [float(r["t"]) for r in rows]
    err_old = [float(r["complementary_pitch"]) - float(r["true_pitch"]) for r in rows]
    err_new = [float(r["mahony_pitch"]) - float(r["true_pitch"]) for r in rows]

    def rms(errors):
        tail = [e for ti, e in zip(t, errors) if ti > 40]
        return math.sqrt(sum(e * e for e in tail) / len(tail))

    fig = new_figure(4.2)
    title(fig, "Simulated: the Mahony filter removes the gyro offset error",
          "Pitch estimation error in hover, 0.4 deg/s gyro offset, prop vibration and servo pushes")
    ax = fig.add_axes([0.075, 0.13, 0.80, 0.68])
    style(ax, "Pitch error (deg)", "Time (s)")
    ax.plot(t, err_old, color=ORANGE, linewidth=2, label=f"Complementary filter (RMS {rms(err_old):.2f} deg)")
    ax.plot(t, err_new, color=BLUE, linewidth=2, label=f"Mahony filter, firmware code (RMS {rms(err_new):.2f} deg)")
    ax.axhline(0, color=REFERENCE, linewidth=0.8)
    end_label(ax, t[-1], err_old[-1], "Complementary")
    end_label(ax, t[-1], err_new[-1], "Mahony")
    ax.legend(loc="center right", bbox_to_anchor=(1.0, 0.45), frameon=False, fontsize=9, labelcolor=TEXT)
    out = os.path.join(FIG_DIR, "fig2_attitude_error.png")
    fig.savefig(out, facecolor=SURFACE)
    plt.close(fig)
    return out


# ----------------------------------------------------------------------------------------------------------------
#  Figure 3: simulated servo step response with and without lead
# ----------------------------------------------------------------------------------------------------------------
def servo_response(step_deg, lead_s, dt=0.002, duration=0.3):
    """MG996R-like servo: 100 Hz frame, internal P loop ~50 ms, 400 deg/s top speed, 0.5 deg dead band."""
    pos, prev, rate_f, held = 0.0, 0.0, 0.0, 0.0
    k = dt / (dt + 1.0 / (2 * math.pi * 8.0))  # 8 Hz filter on the rate of change, as in the firmware
    t_out, p_out = [], []
    n = int(duration / dt)
    for i in range(n):
        t = i * dt
        cmd = step_deg if t >= 0.02 else 0.0
        rate_f += k * ((cmd - prev) / dt - rate_f)
        prev = cmd
        out = cmd + max(-12.0, min(12.0, lead_s * rate_f))
        if i % 5 == 0:  # new pulse every 10 ms
            held = out
        err = held - pos
        if abs(err) > 0.5:
            pos += max(-400.0, min(400.0, err / 0.05)) * dt
        t_out.append(t * 1000.0)
        p_out.append(pos)
    return t_out, p_out


def figure_servo():
    step = 10.0
    t, plain = servo_response(step, 0.0)
    _, lead = servo_response(step, 0.04)

    def t90(p):
        return next(tt for tt, pp in zip(t, p) if pp >= 0.9 * step) - 20.0

    fig = new_figure(4.2)
    title(fig, "Simulated: a phase lead makes a slow servo start sooner",
          "Servo position after a 10 deg command step, MG996R-like model (not a measurement)")
    ax = fig.add_axes([0.075, 0.13, 0.80, 0.68])
    style(ax, "Servo angle (deg)", "Time (ms)")
    ax.step([0, 20, 20, t[-1]], [0, 0, step, step], color=REFERENCE, linewidth=1, linestyle=(0, (4, 3)),
            where="post")
    ax.plot(t, plain, color=ORANGE, linewidth=2, label=f"No lead (90 % after {t90(plain):.0f} ms)")
    ax.plot(t, lead, color=BLUE, linewidth=2, label=f"40 ms lead (90 % after {t90(lead):.0f} ms)")
    ax.annotate("command", (t[-1], step), xytext=(6, 4), textcoords="offset points", fontsize=8.5,
                color=TEXT_2)
    ax.set_ylim(-0.5, 12.5)
    ax.legend(loc="lower right", frameon=False, fontsize=9, labelcolor=TEXT)
    out = os.path.join(FIG_DIR, "fig3_servo_lead.png")
    fig.savefig(out, facecolor=SURFACE)
    plt.close(fig)
    return out


def main():
    os.makedirs(FIG_DIR, exist_ok=True)
    print("wrote", figure_flight_log())
    print("wrote", figure_attitude(run_attitude_sim()))
    print("wrote", figure_servo())


if __name__ == "__main__":
    main()
