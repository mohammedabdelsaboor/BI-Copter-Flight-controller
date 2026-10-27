#!/usr/bin/env python3
"""
Builds docs/BiCopter_Project_Guide.pdf.

    pip install reportlab
    python3 tools/make_guide.py

The text lives in this file, so update it here when the firmware changes and run the script again.
"""

import datetime
import os

from reportlab.graphics.shapes import Circle, Drawing, Line, Polygon, Rect, String
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Image, KeepTogether, PageBreak, Paragraph, Preformatted, SimpleDocTemplate, Spacer, Table,
                                TableStyle)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT = os.path.join(ROOT, "docs", "BiCopter_Project_Guide.pdf")
AUTHOR = "Mohammed Abdelsabour"
TITLE = "BiCopter Flight Controller"

# ----------------------------------------------------------------------------------------------------------------
#  Fonts and styles
# ----------------------------------------------------------------------------------------------------------------
FONT_DIRS = ["/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/dejavu", "/Library/Fonts", "C:/Windows/Fonts"]


def register_fonts():
    """DejaVu has the degree, micro and arrow signs that the built-in PDF fonts lack."""
    wanted = {
        "Serif": "DejaVuSerif.ttf",
        "Serif-Bold": "DejaVuSerif-Bold.ttf",
        "Serif-Italic": "DejaVuSerif-Italic.ttf",
        "Sans": "DejaVuSans.ttf",
        "Sans-Bold": "DejaVuSans-Bold.ttf",
        "Mono": "DejaVuSansMono.ttf",
    }
    for directory in FONT_DIRS:
        if all(os.path.exists(os.path.join(directory, f)) for f in wanted.values()):
            for name, file in wanted.items():
                pdfmetrics.registerFont(TTFont(name, os.path.join(directory, file)))
            pdfmetrics.registerFontFamily("Serif", normal="Serif", bold="Serif-Bold", italic="Serif-Italic",
                                          boldItalic="Serif-Bold")
            return {"body": "Serif", "bold": "Serif-Bold", "italic": "Serif-Italic", "head": "Sans-Bold",
                    "sans": "Sans", "mono": "Mono"}
    print("DejaVu fonts not found, falling back to the built-in fonts (some symbols may be missing)")
    return {"body": "Times-Roman", "bold": "Times-Bold", "italic": "Times-Italic", "head": "Helvetica-Bold",
            "sans": "Helvetica", "mono": "Courier"}


F = register_fonts()

INK = colors.HexColor("#1d2733")
ACCENT = colors.HexColor("#2f5d8a")
MUTED = colors.HexColor("#5b6774")
RULE = colors.HexColor("#c9d1da")
SHADE = colors.HexColor("#eef2f6")
CODE_BG = colors.HexColor("#f4f5f7")
WARN_BG = colors.HexColor("#fbf3e4")
WARN_EDGE = colors.HexColor("#d9a441")

S = {
    "body": ParagraphStyle("body", fontName=F["body"], fontSize=10.2, leading=14.6, textColor=INK, spaceAfter=7),
    "lead": ParagraphStyle("lead", fontName=F["body"], fontSize=11.5, leading=16.5, textColor=INK, spaceAfter=10),
    "h1": ParagraphStyle("h1", fontName=F["head"], fontSize=17, leading=21, textColor=ACCENT, spaceBefore=4,
                         spaceAfter=10),
    "h2": ParagraphStyle("h2", fontName=F["head"], fontSize=12.2, leading=16, textColor=INK, spaceBefore=10,
                         spaceAfter=5),
    "bullet": ParagraphStyle("bullet", fontName=F["body"], fontSize=10.2, leading=14.4, textColor=INK, leftIndent=14,
                             bulletIndent=3, spaceAfter=3),
    "cell": ParagraphStyle("cell", fontName=F["body"], fontSize=9, leading=12, textColor=INK),
    "cellhead": ParagraphStyle("cellhead", fontName=F["head"], fontSize=8.8, leading=12, textColor=INK),
    "code": ParagraphStyle("code", fontName=F["mono"], fontSize=8.4, leading=11.2, textColor=INK, leftIndent=8,
                           rightIndent=8, backColor=CODE_BG, borderPadding=(6, 6, 6, 6), spaceBefore=4,
                           spaceAfter=10),
    "caption": ParagraphStyle("caption", fontName=F["italic"], fontSize=8.8, leading=12, textColor=MUTED,
                              alignment=TA_CENTER, spaceBefore=3, spaceAfter=12),
    "ref": ParagraphStyle("ref", fontName=F["body"], fontSize=9.3, leading=13, textColor=INK, leftIndent=18,
                          firstLineIndent=-18, spaceAfter=5),
    "note": ParagraphStyle("note", fontName=F["body"], fontSize=9.8, leading=14, textColor=INK, backColor=WARN_BG,
                           borderColor=WARN_EDGE, borderWidth=0.8, borderPadding=(7, 8, 7, 8), spaceBefore=6,
                           spaceAfter=12),
}


def keep_units(text):
    """Stop a line break from separating a number from its % or ° sign."""
    return text.replace(" %", "&nbsp;%").replace(" µs", "&nbsp;µs").replace(" ms", "&nbsp;ms").replace(" Hz", "&nbsp;Hz")


def p(text, style="body"):
    return Paragraph(keep_units(text), S[style])


def bullets(items):
    return [Paragraph(keep_units(item), S["bullet"], bulletText="•") for item in items]


def code(text):
    return Preformatted(text.strip("\n"), S["code"])


def note(text):
    return Paragraph(text, S["note"])


def figure(filename, caption, width_mm=165):
    """A result figure from docs/figures, scaled to the text width, kept together with its caption."""
    path = os.path.join(ROOT, "docs", "figures", filename)
    img = Image(path)
    scale = (width_mm * mm) / img.imageWidth
    img.drawWidth = img.imageWidth * scale
    img.drawHeight = img.imageHeight * scale
    return KeepTogether([img, Paragraph(caption, S["caption"])])


def table(rows, widths, header=True):
    data = [[Paragraph(keep_units(str(c)), S["cellhead"] if (header and r == 0) else S["cell"]) for c in row]
            for r, row in enumerate(rows)]
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if header else 0)
    style = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEBELOW", (0, 0), (-1, -1), 0.4, RULE),
        ("TOPPADDING", (0, 0), (-1, -1), 3.5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3.5),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
    ]
    if header:
        style += [("BACKGROUND", (0, 0), (-1, 0), SHADE), ("LINEBELOW", (0, 0), (-1, 0), 0.8, MUTED)]
    t.setStyle(TableStyle(style))
    return t


# ----------------------------------------------------------------------------------------------------------------
#  Diagrams
# ----------------------------------------------------------------------------------------------------------------
def label(d, x, y, text, size=8.5, color=INK, anchor="middle", font=None):
    d.add(String(x, y, text, fontName=font or F["sans"], fontSize=size, fillColor=color, textAnchor=anchor))


def arrow(d, x1, y1, x2, y2, color=INK, width=1.1, head=5):
    d.add(Line(x1, y1, x2, y2, strokeColor=color, strokeWidth=width))
    import math
    a = math.atan2(y2 - y1, x2 - x1)
    left = (x2 - head * math.cos(a - 0.45), y2 - head * math.sin(a - 0.45))
    right = (x2 - head * math.cos(a + 0.45), y2 - head * math.sin(a + 0.45))
    d.add(Polygon([x2, y2, left[0], left[1], right[0], right[1]], fillColor=color, strokeColor=color,
                  strokeWidth=0.5))


def airframe_diagram():
    """Top view (left) and side view (right) of the bicopter with what each actuator does."""
    d = Drawing(170 * mm, 62 * mm)
    cx, cy = 40 * mm, 31 * mm
    # Top view: arm, body, two props
    d.add(Line(cx - 30 * mm, cy, cx + 30 * mm, cy, strokeColor=INK, strokeWidth=3))
    d.add(Rect(cx - 6 * mm, cy - 9 * mm, 12 * mm, 18 * mm, rx=2, ry=2, fillColor=SHADE, strokeColor=INK))
    for side, x in (("left", cx - 30 * mm), ("right", cx + 30 * mm)):
        d.add(Circle(x, cy, 10 * mm, fillColor=None, strokeColor=ACCENT, strokeWidth=0.9,
                     strokeDashArray=[2, 2]))
        d.add(Circle(x, cy, 2.2 * mm, fillColor=INK, strokeColor=INK))
        label(d, x, cy - 15 * mm, f"{side} motor + servo", 7.8, MUTED)
    arrow(d, cx, cy + 9 * mm, cx, cy + 22 * mm, ACCENT)
    label(d, cx + 1.5 * mm, cy + 23.5 * mm, "nose (+X)", 8, ACCENT, "start")
    arrow(d, cx - 6 * mm, cy, cx - 17 * mm, cy, ACCENT, 0.9, 4)
    label(d, cx - 12 * mm, cy + 2.5 * mm, "+Y", 7.5, ACCENT)
    label(d, cx, 2 * mm, "Top view", 8.5, MUTED, font=F["head"])

    # Side view of one motor on its tilt servo
    sx, sy = 122 * mm, 18 * mm
    d.add(Rect(sx - 22 * mm, sy - 2 * mm, 44 * mm, 4 * mm, fillColor=SHADE, strokeColor=INK))
    d.add(Circle(sx, sy + 2 * mm, 2 * mm, fillColor=INK, strokeColor=INK))
    for tilt, dash in ((0, None), (18, [2, 2]), (-18, [2, 2])):
        import math
        a = math.radians(90 - tilt)
        x2, y2 = sx + 26 * mm * math.cos(a), sy + 2 * mm + 26 * mm * math.sin(a)
        d.add(Line(sx, sy + 2 * mm, x2, y2, strokeColor=INK if tilt == 0 else MUTED, strokeWidth=1.4 if tilt == 0
                   else 0.8, strokeDashArray=dash))
    arrow(d, sx, sy + 30 * mm, sx, sy + 40 * mm, ACCENT)
    label(d, sx + 2 * mm, sy + 41 * mm, "thrust", 8, ACCENT, "start")
    label(d, sx + 14 * mm, sy + 22 * mm, "tilt fwd", 7.5, MUTED, "start")
    label(d, sx - 14 * mm, sy + 22 * mm, "tilt back", 7.5, MUTED, "end")
    arrow(d, sx - 26 * mm, sy - 8 * mm, sx + 26 * mm, sy - 8 * mm, MUTED, 0.6, 3)
    label(d, sx + 27 * mm, sy - 9 * mm, "nose", 7.5, MUTED, "start")
    label(d, sx, 2 * mm, "Side view, one motor", 8.5, MUTED, font=F["head"])
    return d


def box(d, x, y, w, h, title, sub=None, fill=SHADE):
    d.add(Rect(x, y, w, h, rx=3, ry=3, fillColor=fill, strokeColor=MUTED, strokeWidth=0.7))
    label(d, x + w / 2, y + h / 2 + (2.2 if sub else -2.5), title, 8.4, INK, font=F["head"])
    if sub:
        label(d, x + w / 2, y + h / 2 - 7.5, sub, 7, MUTED)


def dataflow_diagram():
    """How data moves through the libraries in one loop."""
    d = Drawing(170 * mm, 78 * mm)
    W, H = 34 * mm, 13 * mm
    rx_y, imu_y = 58 * mm, 24 * mm
    box(d, 0, rx_y, W, H, "RcPwmInput", "4 pulse widths")
    box(d, 0, imu_y, W, H, "Mpu6050", "gyro + accel")
    box(d, 45 * mm, rx_y, W, H, "StickInput", "sticks, gestures")
    box(d, 45 * mm, imu_y, W, H, "AttitudeEstimator", "roll, pitch")
    box(d, 90 * mm, 41 * mm, W, H, "FlightController", "states + PIDs", colors.HexColor("#dfe8f1"))
    box(d, 135 * mm, 41 * mm, W, H, "BiCopterMixer", "4 outputs")
    box(d, 135 * mm, 8 * mm, W, H, "PwmOutput × 4", "ESC L/R, servo L/R")
    box(d, 90 * mm, 8 * mm, W, H, "Settings", "trims, level, gains")
    box(d, 45 * mm, 2 * mm, W, 10 * mm, "SerialConsole")
    arrow(d, W, rx_y + H / 2, 45 * mm, rx_y + H / 2)
    arrow(d, W, imu_y + H / 2, 45 * mm, imu_y + H / 2)
    arrow(d, 45 * mm + W, rx_y + H / 2, 90 * mm, 41 * mm + H - 2)
    arrow(d, 45 * mm + W, imu_y + H / 2, 90 * mm, 41 * mm + 2)
    arrow(d, 90 * mm + W, 41 * mm + H / 2, 135 * mm, 41 * mm + H / 2)
    arrow(d, 135 * mm + W / 2, 41 * mm, 135 * mm + W / 2, 8 * mm + H)
    arrow(d, 90 * mm + W / 2, 8 * mm + H, 90 * mm + W / 2, 41 * mm)
    arrow(d, 45 * mm + W, 7 * mm, 90 * mm, 12 * mm, MUTED, 0.7, 3.5)
    return d


def cascade_diagram():
    """Outer angle loop feeding the inner rate loop, for one axis."""
    d = Drawing(170 * mm, 34 * mm)
    y, h = 12 * mm, 12 * mm
    label(d, 0, y + h / 2 - 3, "stick", 8.5, INK, "start")
    arrow(d, 10 * mm, y + h / 2, 18 * mm, y + h / 2)
    box(d, 18 * mm, y, 30 * mm, h, "angle P", "outer loop")
    arrow(d, 48 * mm, y + h / 2, 60 * mm, y + h / 2)
    label(d, 54 * mm, y + h / 2 + 3, "rate", 7, MUTED)
    box(d, 60 * mm, y, 30 * mm, h, "rate PID", "inner loop", colors.HexColor("#dfe8f1"))
    arrow(d, 90 * mm, y + h / 2, 102 * mm, y + h / 2)
    box(d, 102 * mm, y, 26 * mm, h, "mixer")
    arrow(d, 128 * mm, y + h / 2, 140 * mm, y + h / 2)
    box(d, 140 * mm, y, 28 * mm, h, "motors/servos")
    # feedback paths
    d.add(Line(154 * mm, y, 154 * mm, 3 * mm, strokeColor=MUTED, strokeWidth=0.7))
    d.add(Line(154 * mm, 3 * mm, 33 * mm, 3 * mm, strokeColor=MUTED, strokeWidth=0.7))
    arrow(d, 33 * mm, 3 * mm, 33 * mm, y, MUTED, 0.7, 3.5)
    arrow(d, 75 * mm, 3 * mm, 75 * mm, y, MUTED, 0.7, 3.5)
    label(d, 34 * mm, 5 * mm, "angle (filter)", 7, MUTED, "start")
    label(d, 76 * mm, 5 * mm, "rate (gyro)", 7, MUTED, "start")
    return d


# ----------------------------------------------------------------------------------------------------------------
#  Page decoration
# ----------------------------------------------------------------------------------------------------------------
def on_page(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(RULE)
    canvas.setLineWidth(0.5)
    canvas.line(20 * mm, 283 * mm, 190 * mm, 283 * mm)
    canvas.setFont(F["sans"], 7.8)
    canvas.setFillColor(MUTED)
    canvas.drawString(20 * mm, 285 * mm, TITLE + " · project guide")
    canvas.drawRightString(190 * mm, 285 * mm, AUTHOR)
    canvas.drawCentredString(105 * mm, 12 * mm, str(doc.page))
    canvas.restoreState()


def on_cover(canvas, doc):
    canvas.saveState()
    canvas.setFillColor(ACCENT)
    canvas.rect(0, 262 * mm, 210 * mm, 35 * mm, stroke=0, fill=1)
    canvas.restoreState()


# ----------------------------------------------------------------------------------------------------------------
#  Content
# ----------------------------------------------------------------------------------------------------------------
def cover():
    today = datetime.date.today().strftime("%B %Y")
    title = ParagraphStyle("t", fontName=F["head"], fontSize=28, leading=34, textColor=INK, alignment=TA_LEFT)
    sub = ParagraphStyle("s", fontName=F["sans"], fontSize=13, leading=18, textColor=MUTED)
    meta = ParagraphStyle("m", fontName=F["sans"], fontSize=10, leading=15, textColor=INK)
    return [
        Spacer(1, 40 * mm),
        Paragraph(TITLE, title),
        Spacer(1, 4 * mm),
        Paragraph("Firmware for a tilt-rotor bicopter on the ESP32-S3 PowerFeather", sub),
        Spacer(1, 14 * mm),
        airframe_diagram(),
        Spacer(1, 14 * mm),
        p("This guide explains what the project is, how the controller works and how to rebuild it step by step: "
          "from wiring and the first bench checks to tuning the PID gains on the real model. It finishes with the "
          "problems that came up during testing and the sources the design draws on.", "lead"),
        Spacer(1, 20 * mm),
        Paragraph(f"{AUTHOR}<br/>{today}", meta),
        PageBreak(),
    ]


def section_overview():
    return [
        p("1. What this project is", "h1"),
        p("A bicopter is a multicopter with only two rotors. Quadcopters can steer purely by changing motor "
          "speeds, but two rotors are not enough for that, so each motor is mounted on a servo that tilts it "
          "forward and back. The result is a compact, efficient airframe that is also harder to control: one "
          "axis is driven by fast motors, the other two by much slower servos."),
        p("This project is the flight controller for such a model. It runs on an ESP32-S3 PowerFeather, reads a "
          "standard hobby radio receiver and an MPU6050 motion sensor, and drives two ESCs and two servos. The "
          "firmware was written from scratch for this airframe, with three priorities in this order: that it is "
          "safe to test with, that it can be understood by reading it, and that it flies well."),
        p("What it does", "h2"),
        *bullets([
            "Self-levelling flight: roll and pitch sticks set an angle, and the model returns to level when they "
            "are released. With the yaw stick centred it also holds its heading.",
            "A 500 Hz control loop with a cascaded angle and rate controller on each axis.",
            "Arming, disarming and level calibration from the sticks, because a 4-channel radio has no switches.",
            "Failsafe, crash detection, automatic disarm and a hardware watchdog.",
            "A USB console with live data streams, servo trims and PID tuning without re-flashing.",
        ]),
        p("What it does not do", "h2"),
        p("It holds attitude and heading, not position. There is no GPS, optical flow or altitude sensor, so the "
          "pilot controls height with the throttle and corrects slow drift by hand. Section 9 suggests how that "
          "could be added."),
        p("Hardware used", "h2"),
        table([
            ["Part", "Model", "Role"],
            ["Flight controller", "ESP32-S3 PowerFeather", "dual core 240 MHz, native USB, battery management"],
            ["Motion sensor", "MPU6050 on a GY-521 board", "3-axis gyro and 3-axis accelerometer over I2C"],
            ["Radio", "FlySky FS-i6 with FS-iA6B", "4 channels, one PWM signal per channel"],
            ["Tilt servos", "2 × MG996R", "tilt each motor forward and back"],
            ["Propulsion", "2 × brushless motor + ESC", "standard PWM ESCs at 400 Hz"],
            ["Servo supply", "5–6 V UBEC", "keeps servo current away from the controller"],
        ], [36, 50, 84]),
        PageBreak(),
    ]


def section_how_it_flies():
    return [
        p("2. How a bicopter flies", "h1"),
        p("Each of the four actuators has a clear job, but most of them do two jobs at once. The firmware's "
          "mixer combines the controller's three corrections and the throttle into the four outputs:"),
        code("""
left motor  = throttle + roll          left servo  = pitch - yaw
right motor = throttle - roll          right servo = pitch + yaw
"""),
        *bullets([
            "<b>Roll</b> (tipping left or right) comes from running one motor faster than the other. This is the "
            "fastest axis because motors respond within a few milliseconds.",
            "<b>Pitch</b> (nose up or down) comes from tilting both motors the same way. Tilting the thrust forward "
            "pushes the top of the model forward, which lowers the nose.",
            "<b>Yaw</b> (turning) comes from tilting the motors in opposite directions, so one pushes forward and "
            "the other back.",
        ]),
        airframe_diagram(),
        p("Figure 1. The airframe. Roll is controlled by motor speed, pitch and yaw by servo tilt.", "caption"),
        p("Axes and signs", "h2"),
        p("The whole firmware uses one body frame: X points to the nose, Y to the left motor and Z up. Positive "
          "roll means the right side goes down, positive pitch means the nose goes down, and positive yaw means "
          "the nose turns left. Getting a single one of these signs wrong is the most common reason a new build "
          "flips the moment it leaves the ground, which is why the setup procedure checks each one separately."),
        p("Why it is harder than a quadcopter", "h2"),
        p("The MG996R servos take about 0.17 s to move 60°, and their gears have some free play. Pitch and yaw "
          "therefore react far more slowly than roll, and the gains on those axes have to stay modest. The two "
          "axes are also coupled: when the roll controller runs one motor faster, that motor's reaction torque "
          "tries to turn the model, and the yaw controller has to tilt the servos to cancel it."),
        PageBreak(),
    ]


def section_firmware():
    return [
        p("3. Firmware design", "h1"),
        p("The firmware is a PlatformIO project for the Arduino framework. The sketch itself only connects the "
          "pieces and runs the loop. Each job lives in its own small library under <font face='%s'>lib/</font>, "
          "with one header and one source file. None of the libraries read the project configuration directly: "
          "each takes a small settings struct, which the sketch fills in from <font face='%s'>Config.hpp</font>. "
          "That makes every part reusable and testable on its own." % (F["mono"], F["mono"])),
        dataflow_diagram(),
        p("Figure 2. Data flow in one pass of the control loop.", "caption"),
        table([
            ["Library", "What it does"],
            ["Mpu6050", "Reads gyro, accelerometer and temperature in one 14-byte burst. Calibrates the gyro at "
                        "start-up and remaps the sensor axes to the body frame."],
            ["RcPwmInput", "Measures the four receiver pulses with the ESP32-S3's MCPWM capture hardware, so the "
                           "timing does not depend on interrupt delays. A median of three pulses removes glitches."],
            ["StickInput", "Converts pulses to stick values, learns where each stick rests, detects the receiver "
                           "failsafe and recognises the arm, disarm and level gestures."],
            ["AttitudeEstimator", "Estimates roll and pitch with a Mahony filter: the gyro for fast changes, the "
                                  "accelerometer to remove slow drift, and an integral term that learns the gyro's "
                                  "zero error in flight."],
            ["Pid / Pt1Filter / BiquadFilter", "The rate controller and the low pass filters used on the gyro, the "
                                               "accelerometer, the D term and the servo commands."],
            ["LeadCompensator", "Optional phase lead for the servo commands, so a slow servo starts moving sooner. "
                                "Off by default."],
            ["BiCopterMixer / PwmOutput", "Turn the corrections into four pulse widths and generate them on the LEDC "
                                          "peripheral: 400 Hz for the ESCs, 100 Hz for the servos."],
            ["FlightController", "The state machine (disarmed, armed, failsafe, fault), the safety checks and the "
                                 "control law."],
            ["Settings / SerialConsole", "Keep trims, level and tuned gains in flash, and provide the USB console."],
        ], [42, 128]),
        Spacer(1, 6),
        p("The control loop", "h2"),
        p("The loop runs every 2 ms. Each pass reads the sensor, updates the attitude estimate, reads the sticks, "
          "runs the state machine and the controllers, and writes the outputs. Reading the sensor takes about "
          "0.4 ms of the 2 ms, and the console reports the longest pass so that a slow change shows up straight away."),
        p("Each axis uses two loops in series. The outer loop compares the angle the pilot wants with the "
          "measured angle and asks for a rotation rate. The inner loop compares that rate with the gyro and "
          "drives the actuators. The inner loop does most of the work against disturbances; the outer loop only "
          "has to decide where level is."),
        cascade_diagram(),
        p("Figure 3. Cascaded control on one axis.", "caption"),
        p("Attitude estimate", "h2"),
        p("The gyro measures rotation rate very accurately but slowly drifts. The accelerometer knows where down "
          "is, but every vibration and manoeuvre disturbs it, and on a bicopter it mostly sees the direction of "
          "the motor thrust, which moves with every servo correction. It can only be trusted slowly."),
        p("The firmware uses a Mahony filter on a quaternion. The gyro is integrated as the main source. The "
          "accelerometer, after a 10 Hz second order low pass, steers the estimate slowly towards \"down\" "
          "(a 3 s time constant). An integral term learns the gyro's zero error in flight, so the slow correction "
          "does not leave a steady angle error behind:"),
        code("""
error   = measured_down x estimated_down               (cross product)
bias   += ki * trust * error * dt                       (learns the gyro zero error)
rate    = gyro + kp * trust * error + bias              kp = 1 / 3 s, ki = kp / 15 s
q      += 0.5 * q * (0, rate) * dt                      (then normalise)
"""),
        p("How much the accelerometer is trusted fades smoothly from full at exactly 1 g to nothing at 0.8 g or "
          "1.2 g, because outside that range it is measuring the model's motion as well as gravity. In a "
          "simulated hover with a 0.5 deg/s gyro error and prop vibration, this cut the angle error from about "
          "1.5 deg to 0.1 deg compared with the simple complementary filter used before."),
        p("Filters", "h2"),
        p("The sensor's own low pass filter is set to 44 Hz. On top of that, the gyro signal is filtered at 80 Hz "
          "for roll, 40 Hz for pitch and 25 Hz for yaw, and the servo commands at 30 Hz. The servos are updated at "
          "100 Hz. Every filter adds a little delay, so the servo axes use the lightest filtering that still keeps "
          "vibration from making the servos buzz inside their gear play."),
        p("Take-off detection", "h2"),
        p("Sitting on the ground with the motors spooling, the frame cannot respond to corrections. An I term "
          "running there keeps adding correction against the ground, and in testing it tilted the servos 20-25 deg "
          "before lift-off, so the model shot forward as it left the ground. The I terms, heading hold and roll on "
          "the motors therefore only switch on once the throttle reaches the lift-off throttle, and switch off again "
          "only near idle."),
        Spacer(1, 10),
    ]


def section_safety():
    return [
        p("4. Safety design", "h1"),
        p("A bicopter on the bench can hurt someone, so the firmware starts from \"motors off\" and only leaves "
          "that state when every condition is right."),
        table([
            ["Situation", "What the firmware does"],
            ["Power-up", "ESC outputs go to 1000 µs (motor off) before anything else runs."],
            ["Arming", "Refused unless the radio is valid, the throttle is low, the model is within 25° of level, "
                       "the gyro is calibrated and the stick centres have been learned. The console prints the "
                       "reason."],
            ["Radio lost", "Motors stop. To fly again the radio must come back with the throttle low, and the pilot "
                           "has to arm again."],
            ["Crash", "Tilted past 70° for half a second: disarm."],
            ["Forgotten model", "Armed at idle for 15 s: disarm."],
            ["Sensor failure", "25 failed reads in a row (50 ms): motors stop until reboot."],
            ["Software hang", "A 300 ms watchdog resets the chip. All outputs go low, so the ESCs stop."],
            ["On the ground", "At idle, roll corrections are not sent to the motors, so the model cannot tip "
                              "itself over before take-off. The I terms are held at zero until the throttle is up."],
        ], [36, 134]),
        Spacer(1, 8),
        note("<b>Receiver failsafe.</b> When the FS-iA6B loses the transmitter it keeps repeating the last pulses "
             "it received. Without a failsafe set on the transmitter the firmware cannot tell that the link is "
             "gone. Section 5.2 explains how to make the receiver send an out-of-range throttle value instead, and "
             "how to test it. Do not fly before that test passes."),
        PageBreak(),
    ]


def section_follow():
    return [
        p("5. Following the project step by step", "h1"),
        p("Keep the propellers off until step 5.6. Each step checks one thing, so that when something is wrong you "
          "know exactly where to look."),
        p("5.1 Build and flash", "h2"),
        p("Install PlatformIO (the VS Code extension is the easiest way), open the project folder, connect the "
          "board over USB and run:"),
        code("""
pio run -e powerfeather -t upload      # build and flash
pio device monitor                     # console: type h and Enter for help
"""),
        p("Leave the model still for the first couple of seconds: the gyro is calibrated then. Keep your hands off "
          "the roll, pitch and yaw sticks for two more seconds while the firmware learns their resting positions."),
        p("5.2 Radio", "h2"),
        p("Type <font face='%s'>r</font>. Roll right, pitch forward and yaw right must all read positive, and the "
          "throttle must run from 0.00 to 1.00. Reverse any channel that is wrong. Then set the receiver "
          "failsafe:" % F["mono"]),
        *bullets([
            "<i>Functions setup → End points</i>: set the CH3 low end to 120 %.",
            "<i>Functions setup → Failsafe → Channel 3</i>: switch it on, hold the throttle fully down and "
            "long-press OK.",
            "Set the CH3 end point back to 100 %.",
            "With <font face='%s'>r</font> running, switch the transmitter off. The line must show FAILSAFE "
            "within a second." % F["mono"],
        ]),
        p("5.3 Sensor direction and level", "h2"),
        p("Type <font face='%s'>i</font> and tilt the frame by hand: right side down must give positive roll, nose "
          "down positive pitch, and turning the nose left a positive yaw rate. If the readings are swapped or "
          "reversed, change <font face='%s'>IMU_AXIS_MAP</font>; the setup guide in the repository lists the common "
          "mountings. Then put the model on a level surface and type <font face='%s'>l</font> to store that as "
          "level." % (F["mono"], F["mono"], F["mono"])),
        p("5.4 Servos", "h2"),
        p("Fit the servo horns so both motors point straight up at centre and fine-tune with "
          "<font face='%s'>trim l</font> and <font face='%s'>trim r</font>. Then, still disarmed:" % (F["mono"],
                                                                                                    F["mono"])),
        *bullets([
            "Tilt the nose down by hand: both motors must tilt back.",
            "Turn the nose left by hand: the left motor must tilt forward and the right one back.",
            "Push the pitch stick forward: both motors tilt forward.",
        ]),
        p("If the hand tests fail, swap the servo reverse settings. If only a stick test fails, reverse that stick "
          "instead."),
        p("5.5 Motors", "h2"),
        p("Calibrate the ESCs once with the <font face='%s'>esc_calibration</font> build, check that the two "
          "propellers spin in opposite directions, and use the <font face='%s'>o</font> stream to confirm that "
          "tipping the right side down raises the right motor's command." % (F["mono"], F["mono"])),
        p("5.6 First hover", "h2"),
        p("Fit the propellers, choose an open space and raise the throttle slowly. A well-matched setup hovers "
          "between 40 % and 60 % throttle. If it needs much more, the controller has no thrust left to correct "
          "with, and no amount of tuning will make it steady."),
        p("5.7 Tuning", "h2"),
        p("Gains can be changed from the console while disarmed, take effect immediately and are kept after "
          "<font face='%s'>save</font>:" % F["mono"]),
        code("""
pid                          show all gains
pid r 0.0016 0.003 0.00004   roll rate P I D
pid p 0.0025 0.004 0         pitch rate P I D
pid y 0.0025 0.003 0         yaw rate P I D
ang r 3.0 / ang p 3.0        self-level strength
ang y 1.5                    heading hold (0 = off)
save                         keep them
"""),
        p("Tune one axis at a time: roll, then pitch, then yaw. Raise P until the axis starts to wobble and go "
          "back to about 70 %% of that. Raise I until the model stops leaning or creeping. Use a little D on roll "
          "only. To see what the controller is doing, record a log with "
          "<font face='%s'>tools/flight_log.py</font>: each line shows the measured and the wanted rate on every "
          "axis, the commands and all four outputs." % F["mono"]),
        Spacer(1, 10),
    ]


def section_console():
    return [
        p("6. Console reference", "h1"),
        table([
            ["Command", "Meaning"],
            ["h", "help"],
            ["s", "status line on or off (once a second)"],
            ["i / r / o / f", "stream the IMU, the radio, the outputs or the full flight log (10 per second)"],
            ["x", "stop all output"],
            ["g", "recalibrate the gyro (keep the model still)"],
            ["l", "store the current attitude as level"],
            ["trim l|r N", "servo centre trim in microseconds"],
            ["pid, pid r|p|y P I D", "show or set the rate gains"],
            ["ang r|p|y P", "self-level gain (roll, pitch) or heading hold gain (yaw)"],
            ["save", "write trims, level and gains to flash"],
            ["defaults", "go back to the values in Config.hpp"],
            ["reboot", "restart the board"],
        ], [44, 126]),
        Spacer(1, 6),
        p("Every setting that changes the model's behaviour only works while disarmed."),
        p("7. Test results", "h1"),
        p("Figure 4 shows 20 seconds of a logged throttle sweep with an earlier version of the controller, and it "
          "contains the central result of the project. Roll, driven by the motors, stays within 6 deg of level. "
          "Pitch, driven by the servos, swings by up to 25 deg, and its command stays positive for long periods "
          "because the centre of gravity sat behind the rotors and the servos had to hold them tilted forward."),
        figure("fig1_roll_vs_pitch.png",
               "Figure 4. Measured roll and pitch angle and the pitch servo command during a throttle sweep."),
        p("Figure 5 compares the complementary filter used earlier with the Mahony filter now in the firmware, on "
          "the same simulated hover. The simulation calls the firmware's own estimator. The complementary filter "
          "keeps an error of about 1.5 deg caused by the gyro offset; the Mahony filter learns the offset within "
          "about 30 s and then stays near zero."),
        figure("fig2_attitude_error.png",
               "Figure 5. Simulated pitch estimation error, complementary filter and Mahony filter."),
        p("Figure 6 shows the effect of the optional phase lead on a simple servo model: 90 % of a 10 deg step is "
          "reached after about 30 ms instead of 110 ms. On the real model the lead, together with a higher pitch "
          "gain, made the frame vibrate, so it is off by default."),
        figure("fig3_servo_lead.png",
               "Figure 6. Simulated servo step response with and without the 40 ms phase lead (a model, not a "
               "measurement)."),
        p("Recorded flight of 23 October 2026", "h2"),
        p("Figures 7 to 10 come from a flight recorded with the web page's flight recorder: 22 s at 25 samples per "
          "second, of which 20 s airborne at 63 % throttle. The full record is included as "
          "docs/data/bicopter_20261023_182347.csv. It was flown with an earlier firmware version in which the hover "
          "trim was active, so the pitch target was about 9 deg nose-up rather than level. Table 1 summarises the "
          "airborne part."),
        table([
            ["Quantity (airborne, throttle above 30 %)", "Value"],
            ["Airborne time and throttle", "20.2 s at 63 %"],
            ["Roll angle, mean and standard deviation", "+1.1 deg, 5.3 deg"],
            ["Pitch angle, mean and standard deviation", "-18.6 deg, 14.1 deg"],
            ["RMS rate tracking error: roll / pitch / yaw", "16 / 80 / 22 deg/s"],
            ["Mean ESC pulse, left minus right", "+90 us"],
            ["Motor tilt learned in hover", "+9.3 deg (forward)"],
            ["Samples in which the accelerometer was trusted", "44 %"],
        ], [110, 55]),
        p("Table 1. Statistics of the recorded flight.", "caption"),
        p("Figure 7 shows the attitude over the whole flight. Roll stays within a few degrees of level apart from "
          "short excursions, while pitch repeatedly drops 40 to 55 deg nose-up and recovers, roughly every 2.5 s."),
        figure("fig4_flight_attitude.png",
               "Figure 7. Roll angle (a) and pitch angle with its hover-trim target (b) during the recorded flight."),
        p("Figure 8 compares the rate each axis was asked for with the rate the gyro measured, on the same scale. "
          "On roll the two curves follow each other closely. On pitch the commanded rate reaches its 90 deg/s limit "
          "and stays there for almost two seconds while the model barely responds, and afterwards the measured rate "
          "oscillates around the command: the servos cannot produce the pitch moment fast enough."),
        figure("fig5_rate_tracking.png",
               "Figure 8. Commanded and measured rate, roll (a) and pitch (b), for 5 s of the flight."),
        p("Figure 9 shows the actuator outputs. The left motor runs about 90 us above the right one for most of "
          "the flight, so either the centre of gravity was offset towards the left or the left motor and propeller give less thrust; weighing the two sides would tell which. Both servos stay away from "
          "their 1500 us centre, holding the rotors tilted forward: the centre of gravity was also behind the rotor "
          "axis, which matches the +9.3 deg hover tilt in Table 1."),
        figure("fig6_actuators.png",
               "Figure 9. ESC pulses (a) and servo pulses (b) during the recorded flight. The right servo is mounted "
               "mirrored, so it moves the opposite way for the same tilt."),
        p("Figure 10 condenses the comparison into one number per axis. The pitch axis, driven by the servos, "
          "tracks its commanded rate five times worse than the roll axis, driven by the motors."),
        figure("fig7_tracking_error.png",
               "Figure 10. RMS rate tracking error per axis during the airborne part of the recorded flight.",
               width_mm=105),
        p("All figures are generated by tools/make_figures.py from the data in docs/data and from the simulations."),
        PageBreak(),
        p("Problems found during testing", "h2"),
        p("These came up while the firmware was being brought up on the real model. They are worth reading before "
          "your own first test, because most of them look like a tuning problem but are not."),
        table([
            ["Symptom", "Cause", "What fixed it"],
            ["Console printed almost nothing", "The PowerFeather's USB buffer holds only 64 bytes, and longer lines "
                                               "were being dropped.",
             "Console output now goes through a 2 KB queue."],
            ["Angle never settled, drifted", "The accelerometer read 1.42 g at rest, so the filter rejected it.",
             "The Z offset is measured at every start-up and removed."],
            ["Roll and pitch swapped", "The sensor was mounted rotated 90° from the assumed orientation.",
             "New IMU_AXIS_MAP; a compile-time check stops mirror-image maps."],
            ["Model kept turning", "The yaw stick rested at 1438 µs because of a transmitter trim.",
             "Stick centres are learned at start-up; yaw has a wider deadband."],
            ["Servos slowly moved apart", "Raising the throttle nudged the yaw stick, and the yaw I term built up.",
             "Wider yaw deadband and a yaw-I readout in the flight log."],
            ["Both servos crept forward", "Level was never calibrated (the board read -8° when level).",
             "Level calibration with the l command."],
            ["Board stuck in FAULT at boot", "The frame was moved during gyro calibration.",
             "Calibration now retries on its own until the model is still."],
            ["Upload failed every first time", "The USB port reconnects when the chip enters its bootloader.",
             "1200-baud reset in platformio.ini."],
            ["Shot forward at take-off", "The pitch I term wound up against the ground before lift-off.",
             "Take-off detection: I terms only switch on at the lift-off throttle."],
            ["Forward/back swing, needs a lot of room", "Slow servos on the pitch axis, and extra controller "
                                                         "layers that made fast servo starts worse.",
             "Simple pitch PID, CG low under the rotors, front-to-back balance."],
        ], [42, 66, 62]),
        PageBreak(),
    ]


def section_limits():
    return [
        p("8. Limitations", "h1"),
        *bullets([
            "<b>No position hold.</b> The model holds angle and heading. Wind or a small level error makes it slide "
            "slowly, and the pilot has to correct.",
            "<b>Servo quality.</b> The MG996R is slow and has free play. It sets the limit on how precisely pitch "
            "and yaw can be held. A digital metal-gear servo running at 200 Hz would be a clear improvement.",
            "<b>Thrust margin.</b> During testing the model needed close to full throttle. Below about 60 % hover "
            "throttle the controller has the headroom it needs; above that, every correction fights saturation.",
            "<b>Yaw reference.</b> Without a magnetometer the heading slowly wanders with gyro drift. The firmware "
            "re-zeroes the gyro whenever the model sits still, which keeps this small.",
        ]),
        p("9. Where it could go next", "h1"),
        *bullets([
            "An optical-flow sensor (for example the PMW3901) and a laser rangefinder (for example the VL53L1X) for "
            "position and height hold indoors.",
            "Battery voltage measurement through a divider on a spare ADC pin, to keep the throttle response the "
            "same as the battery runs down and to warn before it is empty.",
            "Logging to the PowerFeather's flash for later analysis, instead of over USB.",
            "Feed-forward from the roll motor command into yaw, to cancel the torque coupling before the gyro sees it.",
        ]),
    ]


def section_references():
    refs = [
        "Espressif Systems, <i>ESP32-S3 Technical Reference Manual</i> (chapters on MCPWM and LED PWM "
        "controller). https://www.espressif.com/en/support/documents/technical-documents",
        "Espressif Systems, <i>Arduino-ESP32 documentation</i>. https://docs.espressif.com/projects/arduino-esp32",
        "PowerFeather, <i>ESP32-S3 PowerFeather documentation and pinout</i>. https://powerfeather.dev",
        "InvenSense (TDK), <i>MPU-6000 and MPU-6050 Product Specification</i> (PS-MPU-6000A-00) and "
        "<i>Register Map and Descriptions</i> (RM-MPU-6000A-00).",
        "PlatformIO, <i>PlatformIO documentation</i>. https://docs.platformio.org",
        "FlySky, <i>FS-i6 Digital Proportional Radio Control System, user manual</i>.",
        "R. Mahony, T. Hamel and J.-M. Pflimlin, \"Nonlinear complementary filters on the special orthogonal "
        "group\", <i>IEEE Transactions on Automatic Control</i>, vol. 53, no. 5, 2008.",
        "S. O. H. Madgwick, \"An efficient orientation filter for inertial and inertial/magnetic sensor arrays\", "
        "technical report, University of Bristol, 2010.",
        "K. J. Åström and R. M. Murray, <i>Feedback Systems: An Introduction for Scientists and Engineers</i>, "
        "Princeton University Press, 2008.",
        "Q. Quan, <i>Introduction to Multicopter Design and Control</i>, Springer, 2017.",
        "Y. Qin, W. Xu, A. Lee and F. Zhang, \"Gemini: A compact yet efficient bi-copter UAV for indoor "
        "applications\", <i>IEEE Robotics and Automation Letters</i>, vol. 5, no. 2, 2020.",
        "ArduPilot, <i>Tilt rotor planes</i> (bicopter frame class). https://ardupilot.org/plane/docs/"
        "guide-tilt-rotor.html",
        "P. Cook, <i>PlainFlightController</i>. https://github.com/plainFlight/plainFlightController",
        "N. Rehm, <i>dRehmFlight VTOL flight controller</i>. https://github.com/nickrehm/dRehmFlight",
        "R. Lopez, <i>ESP-FC, a flight controller for ESP32</i>. https://github.com/rtlopez/esp-fc",
        "Espressif Systems, <i>ESP-Drone</i>. https://github.com/espressif/esp-drone",
        "Betaflight, <i>Betaflight documentation</i> (PID tuning and filtering). https://betaflight.com/docs",
    ]
    out = [p("10. Resources and references", "h1"),
           p("The firmware draws on the datasheets and documentation for the parts used, on standard references for "
             "attitude estimation and feedback control, and on several open source flight controllers whose "
             "approaches were studied while designing this one.")]
    for i, ref in enumerate(refs, 1):
        out.append(Paragraph(f"[{i}]&nbsp;&nbsp;{ref}", S["ref"]))
    return out


def build():
    doc = SimpleDocTemplate(OUTPUT, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=20 * mm,
                            bottomMargin=20 * mm, title=TITLE + " - Project Guide", author=AUTHOR,
                            subject="Tilt-rotor bicopter flight controller for the ESP32-S3 PowerFeather")
    story = []
    story += cover()
    story += section_overview()
    story += section_how_it_flies()
    story += section_firmware()
    story += section_safety()
    story += section_follow()
    story += section_console()
    story += section_limits()
    story += [Spacer(1, 6)]
    story += section_references()
    doc.build(story, onFirstPage=on_cover, onLaterPages=on_page)
    print("wrote", OUTPUT)


if __name__ == "__main__":
    build()
