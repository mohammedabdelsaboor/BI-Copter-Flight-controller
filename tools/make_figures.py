#!/usr/bin/env python3
"""
Builds the result figures in docs/figures/ from the recorded data and the simulations.

    pip install matplotlib
    python3 tools/make_figures.py

Figures are drawn in a plain academic style (serif text, panel labels, units on every axis, no titles inside
the image) so they can be used with captions in the PDF guide and the README.

  fig1  measured   roll vs pitch, console log of a throttle sweep           docs/data/throttle_sweep_*.txt
  fig2  simulated  attitude error, complementary vs Mahony (firmware code)  tools/sim/attitude_sim.cpp
  fig3  simulated  servo step response with and without phase lead        model below
  fig4  measured   attitude angles of the recorded flight                  docs/data/bicopter_*.csv
  fig5  measured   commanded vs measured rate, roll and pitch              same flight
  fig6  measured   motor and servo outputs                                 same flight
  fig7  measured   RMS rate tracking error per axis                        same flight

The script also prints the flight statistics that the PDF table uses.
"""

import csv
import io
import math
import os
import re
import statistics
import subprocess
import tempfile

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIG_DIR = os.path.join(ROOT, "docs", "figures")
SWEEP_LOG = os.path.join(ROOT, "docs", "data", "throttle_sweep_2026-10-24.txt")
FLIGHT_CSV = os.path.join(ROOT, "docs", "data", "bicopter_20261023_182347.csv")

# Two series colours (categorical slots 1 and 2 of the reference data-viz palette) plus a line style, so the
# figures still read in black-and-white print. Text and axes stay in neutral ink.
INK = "#1d1d1b"
INK_2 = "#55544f"
GRID = "#d9d8d2"
REF = "#7d7c77"
BLUE = "#2a78d6"
ORANGE = "#eb6834"
DASHED = (0, (5, 1.5))
TEXT_WIDTH_IN = 6.5  # matches the 165 mm text width of the PDF guide

plt.rcParams.update({
    "font.family": "serif",
    "font.serif": ["DejaVu Serif", "Times New Roman", "Times"],
    "mathtext.fontset": "dejavuserif",
    "font.size": 8.5,
    "axes.labelsize": 8.5,
    "axes.edgecolor": INK,
    "axes.labelcolor": INK,
    "axes.linewidth": 0.6,
    "xtick.color": INK,
    "ytick.color": INK,
    "xtick.direction": "in",
    "ytick.direction": "in",
    "xtick.top": True,
    "ytick.right": True,
    "xtick.major.size": 3,
    "ytick.major.size": 3,
    "xtick.major.width": 0.6,
    "ytick.major.width": 0.6,
    "legend.fontsize": 7.8,
    "legend.frameon": False,
    "lines.linewidth": 1.1,
    "savefig.dpi": 300,
    "savefig.bbox": "tight",
    "savefig.pad_inches": 0.03,
    "figure.facecolor": "white",
})


def grid(ax):
    ax.grid(True, color=GRID, linewidth=0.5, linestyle=(0, (1, 2)))
    ax.set_axisbelow(True)


def panel(ax, label):
    ax.text(0.012, 0.95, label, transform=ax.transAxes, va="top", ha="left", fontsize=9, color=INK,
            bbox=dict(facecolor="white", edgecolor="none", pad=1.0, alpha=0.85))


def save(fig, name):
    path = os.path.join(FIG_DIR, name)
    fig.savefig(path)
    plt.close(fig)
    return path


# ----------------------------------------------------------------------------------------------------------------
#  Data loading
# ----------------------------------------------------------------------------------------------------------------
def load_sweep_window():
    """20 s of the console log: the representative 10-30 s stretch of the longest armed run."""
    pat = re.compile(r"(\w+) thr ([\d.]+) \| R +(\S+) P +(\S+) \| rate .*?\| cmd (\S+) (\S+) (\S+)")
    rows = []
    for line in open(SWEEP_LOG):
        m = pat.search(line)
        if m:
            rows.append((m[1], float(m[2]), float(m[3]), float(m[4]), float(m[6])))
    start = next(i for i, r in enumerate(rows) if r[0] == "ARMED" and r[1] > 0.15)
    window = rows[start + 100:start + 300]
    t = [i / 10.0 for i in range(len(window))]  # console stream: 10 Hz
    return t, [r[2] for r in window], [r[3] for r in window], [r[4] for r in window]


def load_flight():
    rows = list(csv.DictReader(open(FLIGHT_CSV)))
    t0 = float(rows[0]["ms"])
    data = {k: [float(r[k]) for r in rows] for k in rows[0] if k != "state"}
    data["t"] = [(float(r["ms"]) - t0) / 1000.0 for r in rows]
    return data


def flight_stats(d):
    air = [i for i, v in enumerate(d["throttle"]) if v > 0.3]

    def sel(k):
        return [d[k][i] for i in air]

    def rms_error(axis):
        e = [d[f"set_{axis}_dps"][i] - d[f"gyro_{axis}_dps"][i] for i in air]
        return math.sqrt(sum(x * x for x in e) / len(e))

    return {
        "airborne_s": d["t"][air[-1]] - d["t"][air[0]],
        "throttle": statistics.mean(sel("throttle")),
        "roll_mean": statistics.mean(sel("roll_deg")), "roll_sd": statistics.pstdev(sel("roll_deg")),
        "pitch_mean": statistics.mean(sel("pitch_deg")), "pitch_sd": statistics.pstdev(sel("pitch_deg")),
        "rms": {a: rms_error(a) for a in ("roll", "pitch", "yaw")},
        "esc_diff": statistics.mean(sel("esc_l_us")) - statistics.mean(sel("esc_r_us")),
        "hover_tilt": statistics.mean(sel("hover_tilt_deg")),
        "accel_used": 100.0 * statistics.mean(sel("accel_used")),
        "land_t": d["t"][air[-1]],
    }


# ----------------------------------------------------------------------------------------------------------------
#  Figures 1-3
# ----------------------------------------------------------------------------------------------------------------
def figure1():
    t, roll, pitch, cmd = load_sweep_window()
    fig, (a, b) = plt.subplots(2, 1, figsize=(TEXT_WIDTH_IN, 3.6), sharex=True,
                               gridspec_kw={"height_ratios": [1.6, 1], "hspace": 0.08})
    a.plot(t, roll, color=BLUE, label="Roll (motors)")
    a.plot(t, pitch, color=ORANGE, linestyle=DASHED, label="Pitch (servos)")
    a.axhline(0, color=REF, linewidth=0.5)
    a.set_ylabel("Angle (deg)")
    a.legend(loc="lower right", ncol=2)
    b.plot(t, cmd, color=ORANGE, linestyle=DASHED)
    b.axhline(0, color=REF, linewidth=0.5)
    b.set_ylim(-1.05, 1.05)
    b.set_ylabel("Pitch command (-)")
    b.set_xlabel("Time (s)")
    for ax, label in ((a, "(a)"), (b, "(b)")):
        grid(ax)
        panel(ax, label)
    return save(fig, "fig1_roll_vs_pitch.png")


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


def figure2(rows):
    t = [float(r["t"]) for r in rows]
    old = [float(r["complementary_pitch"]) - float(r["true_pitch"]) for r in rows]
    new = [float(r["mahony_pitch"]) - float(r["true_pitch"]) for r in rows]

    def rms(errors):
        tail = [e for ti, e in zip(t, errors) if ti > 40]
        return math.sqrt(sum(e * e for e in tail) / len(tail))

    fig, ax = plt.subplots(figsize=(TEXT_WIDTH_IN, 2.5))
    ax.plot(t, old, color=ORANGE, linestyle=DASHED, linewidth=0.8,
            label=f"Complementary filter, RMS {rms(old):.2f} deg")
    ax.plot(t, new, color=BLUE, linewidth=0.8, label=f"Mahony filter (firmware code), RMS {rms(new):.2f} deg")
    ax.axhline(0, color=REF, linewidth=0.5)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Pitch estimation error (deg)")
    ax.legend(loc="center right", bbox_to_anchor=(1.0, 0.56))
    grid(ax)
    return save(fig, "fig2_attitude_error.png")


def servo_response(step_deg, lead_s, dt=0.002, duration=0.3):
    """MG996R-like servo: 100 Hz frame, internal P loop ~50 ms, 400 deg/s top speed, 0.5 deg dead band."""
    pos, prev, rate_f, held = 0.0, 0.0, 0.0, 0.0
    k = dt / (dt + 1.0 / (2 * math.pi * 8.0))  # 8 Hz filter on the rate of change, as in the firmware
    t_out, p_out = [], []
    for i in range(int(duration / dt)):
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


def figure3():
    step = 10.0
    t, plain = servo_response(step, 0.0)
    _, lead = servo_response(step, 0.04)

    def t90(p):
        return next(tt for tt, pp in zip(t, p) if pp >= 0.9 * step) - 20.0

    fig, ax = plt.subplots(figsize=(TEXT_WIDTH_IN, 2.5))
    ax.step([0, 20, 20, t[-1]], [0, 0, step, step], color=REF, linewidth=0.8, linestyle=(0, (2, 2)),
            where="post", label="Command")
    ax.plot(t, plain, color=ORANGE, linestyle=DASHED, label=f"No lead, 90 % after {t90(plain):.0f} ms")
    ax.plot(t, lead, color=BLUE, label=f"40 ms lead, 90 % after {t90(lead):.0f} ms")
    ax.set_ylim(-0.5, 12.5)
    ax.set_xlabel("Time (ms)")
    ax.set_ylabel("Servo angle (deg)")
    ax.legend(loc="lower right")
    grid(ax)
    return save(fig, "fig3_servo_lead.png")


# ----------------------------------------------------------------------------------------------------------------
#  Figures 4-7: the recorded flight
# ----------------------------------------------------------------------------------------------------------------
def mark_landing(ax, t_land):
    ax.axvspan(t_land, ax.get_xlim()[1], color="#efeee9", zorder=0)


def figure4(d, s):
    fig, (a, b) = plt.subplots(2, 1, figsize=(TEXT_WIDTH_IN, 3.6), sharex=True, gridspec_kw={"hspace": 0.08})
    a.plot(d["t"], d["roll_deg"], color=BLUE)
    a.axhline(0, color=REF, linewidth=0.5)
    a.set_ylabel("Roll angle (deg)")
    b.plot(d["t"], d["pitch_deg"], color=ORANGE, linestyle=DASHED, label="Pitch angle")
    b.plot(d["t"], d["hover_trim_deg"], color=REF, linewidth=0.9, linestyle=(0, (2, 2)),
           label="Hover trim (pitch target)")
    b.axhline(0, color=REF, linewidth=0.5)
    b.set_ylabel("Pitch angle (deg)")
    b.set_xlabel("Time (s)")
    low = min(d["pitch_deg"])
    b.set_ylim(low - 18, 12)  # room below the deepest swing for the legend
    b.legend(loc="lower left", ncol=2)
    for ax, label in ((a, "(a)"), (b, "(b)")):
        ax.set_xlim(0, d["t"][-1])
        mark_landing(ax, s["land_t"])
        grid(ax)
        panel(ax, label)
    a.text(s["land_t"] + 0.12, 0.95, "landed", transform=a.get_xaxis_transform(), va="top", fontsize=7.5,
           color=INK_2)
    return save(fig, "fig4_flight_attitude.png")


def figure5(d):
    t0, t1 = 5.0, 10.0  # 5 s window, long enough to show several swings and short enough to read
    idx = [i for i, t in enumerate(d["t"]) if t0 <= t <= t1]
    t = [d["t"][i] for i in idx]
    fig, (a, b) = plt.subplots(2, 1, figsize=(TEXT_WIDTH_IN, 3.6), sharex=True, gridspec_kw={"hspace": 0.08})
    for ax, axis, label in ((a, "roll", "(a)"), (b, "pitch", "(b)")):
        ax.plot(t, [d[f"set_{axis}_dps"][i] for i in idx], color=ORANGE, linestyle=DASHED, label="Commanded rate")
        ax.plot(t, [d[f"gyro_{axis}_dps"][i] for i in idx], color=BLUE, label="Measured rate (gyro)")
        ax.axhline(0, color=REF, linewidth=0.5)
        ax.set_ylabel(f"{axis.capitalize()} rate (deg/s)")
        grid(ax)
        panel(ax, label)
    a.legend(loc="lower right", ncol=2)
    lim = max(abs(v) for i in idx for v in (d["set_pitch_dps"][i], d["gyro_pitch_dps"][i])) * 1.08
    a.set_ylim(-lim, lim)
    b.set_ylim(-lim, lim)  # same scale on both panels, so the difference between the axes is visible
    b.set_xlabel("Time (s)")
    return save(fig, "fig5_rate_tracking.png")


def figure6(d, s):
    fig, (a, b) = plt.subplots(2, 1, figsize=(TEXT_WIDTH_IN, 3.6), sharex=True, gridspec_kw={"hspace": 0.08})
    a.plot(d["t"], d["esc_l_us"], color=BLUE, label="Left motor")
    a.plot(d["t"], d["esc_r_us"], color=ORANGE, linestyle=DASHED, label="Right motor")
    a.set_ylabel("ESC pulse (us)")
    a.legend(loc="lower left", ncol=2)
    b.plot(d["t"], d["servo_l_us"], color=BLUE, label="Left servo")
    b.plot(d["t"], d["servo_r_us"], color=ORANGE, linestyle=DASHED, label="Right servo (mounted reversed)")
    b.axhline(1500, color=REF, linewidth=0.6, linestyle=(0, (2, 2)))
    b.set_ylabel("Servo pulse (us)")
    b.set_xlabel("Time (s)")
    lo = min(min(d["servo_l_us"]), min(d["servo_r_us"]))
    hi = max(max(d["servo_l_us"]), max(d["servo_r_us"]))
    b.set_ylim(lo - 20, hi + 0.35 * (hi - lo))  # headroom so the legend sits above the lines
    b.legend(loc="upper right", ncol=2)
    for ax, label in ((a, "(a)"), (b, "(b)")):
        ax.set_xlim(0, d["t"][-1])
        mark_landing(ax, s["land_t"])
        grid(ax)
        panel(ax, label)
    return save(fig, "fig6_actuators.png")


def figure7(s):
    axes = ["Roll", "Pitch", "Yaw"]
    values = [s["rms"]["roll"], s["rms"]["pitch"], s["rms"]["yaw"]]
    actuator = ["motors", "servos", "servos"]
    fig, ax = plt.subplots(figsize=(TEXT_WIDTH_IN * 0.62, 2.3))
    bars = ax.bar(axes, values, width=0.5, color=BLUE, edgecolor="white", linewidth=0.8)
    for bar, v, act in zip(bars, values, actuator):
        ax.text(bar.get_x() + bar.get_width() / 2, v + 2, f"{v:.0f}", ha="center", va="bottom", fontsize=8,
                color=INK)
        ax.text(bar.get_x() + bar.get_width() / 2, -0.12, f"({act})", ha="center", va="top", fontsize=7.5,
                color=INK_2, transform=ax.get_xaxis_transform())
    ax.set_ylabel("RMS rate error (deg/s)")
    ax.set_ylim(0, max(values) * 1.18)
    grid(ax)
    ax.grid(False, axis="x")
    return save(fig, "fig7_tracking_error.png")


def main():
    os.makedirs(FIG_DIR, exist_ok=True)
    flight = load_flight()
    stats = flight_stats(flight)
    for path in (figure1(), figure2(run_attitude_sim()), figure3(), figure4(flight, stats), figure5(flight),
                 figure6(flight, stats), figure7(stats)):
        print("wrote", os.path.relpath(path, ROOT))
    print("\nflight statistics (airborne, throttle > 0.3):")
    print(f"  airborne {stats['airborne_s']:.1f} s at throttle {stats['throttle']:.2f}")
    print(f"  roll  {stats['roll_mean']:+.1f} +- {stats['roll_sd']:.1f} deg, rate RMS error {stats['rms']['roll']:.1f}")
    print(f"  pitch {stats['pitch_mean']:+.1f} +- {stats['pitch_sd']:.1f} deg, rate RMS error "
          f"{stats['rms']['pitch']:.1f}")
    print(f"  yaw   rate RMS error {stats['rms']['yaw']:.1f}")
    print(f"  ESC left - right {stats['esc_diff']:+.0f} us, hover tilt {stats['hover_tilt']:+.1f} deg, "
          f"accelerometer used {stats['accel_used']:.0f} %")


if __name__ == "__main__":
    main()
