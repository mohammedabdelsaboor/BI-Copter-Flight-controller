#!/usr/bin/env python3
"""
Builds the project report, docs/BiCopter_Report.pdf.

    pip install reportlab
    python3 tools/make_figures.py     # first, if the data or the simulations changed
    python3 tools/make_report.py

The report is laid out as an academic document: title page, abstract, contents, lists of figures and tables,
numbered chapters, numbered equations, references and appendices. The text lives in this file; update it here
when the firmware changes and run the script again. Figures and tables are numbered automatically, and the
text refers to them by key through fig() and tab().
"""

import datetime
import math
import os

from reportlab.graphics.shapes import Circle, Drawing, Line, Polygon, Rect, String
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT, TA_RIGHT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, Frame, Image, KeepTogether, NextPageTemplate,
                                PageBreak, PageTemplate, Paragraph, Preformatted, Spacer, Table, TableStyle)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT = os.path.join(ROOT, "docs", "BiCopter_Report.pdf")
AUTHOR = "Mohammed Abdelsabour"
TITLE = "Attitude Control of a Tilt-Rotor Bicopter on an ESP32-S3"
SUBTITLE = "Design, implementation and experimental evaluation of a flight controller"
SHORT_TITLE = "Attitude Control of a Tilt-Rotor Bicopter"
REPO = "https://github.com/mohammedabdelsaboor/BI-Copter-Flight-controller"

# ----------------------------------------------------------------------------------------------------------------
#  Fonts and styles
# ----------------------------------------------------------------------------------------------------------------
FONT_DIRS = ["/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/dejavu", "/Library/Fonts", "C:/Windows/Fonts"]


def register_fonts():
    """DejaVu has the Greek letters, degree and micro signs that the built-in PDF fonts lack."""
    wanted = {
        "Serif": "DejaVuSerif.ttf",
        "Serif-Bold": "DejaVuSerif-Bold.ttf",
        "Serif-Italic": "DejaVuSerif-Italic.ttf",
        "Serif-BoldItalic": "DejaVuSerif-BoldItalic.ttf",
        "Sans": "DejaVuSans.ttf",
        "Sans-Bold": "DejaVuSans-Bold.ttf",
        "Mono": "DejaVuSansMono.ttf",
    }
    for directory in FONT_DIRS:
        if all(os.path.exists(os.path.join(directory, f)) for f in wanted.values()):
            for name, file in wanted.items():
                pdfmetrics.registerFont(TTFont(name, os.path.join(directory, file)))
            pdfmetrics.registerFontFamily("Serif", normal="Serif", bold="Serif-Bold", italic="Serif-Italic",
                                          boldItalic="Serif-BoldItalic")
            return {"body": "Serif", "bold": "Serif-Bold", "italic": "Serif-Italic", "sans": "Sans",
                    "sans_bold": "Sans-Bold", "mono": "Mono"}
    print("DejaVu fonts not found, falling back to the built-in fonts (some symbols may be missing)")
    return {"body": "Times-Roman", "bold": "Times-Bold", "italic": "Times-Italic", "sans": "Helvetica",
            "sans_bold": "Helvetica-Bold", "mono": "Courier"}


F = register_fonts()

INK = colors.HexColor("#1a1a1a")
MUTED = colors.HexColor("#555555")
RULE = colors.HexColor("#1a1a1a")
LIGHT_RULE = colors.HexColor("#b5b5b5")
CODE_BG = colors.HexColor("#f5f5f3")
DIAGRAM_ACCENT = colors.HexColor("#2a5f8f")
DIAGRAM_FILL = colors.HexColor("#eef1f4")

TEXT_WIDTH = 160 * mm
S = {
    "body": ParagraphStyle("body", fontName=F["body"], fontSize=10.3, leading=14.8, textColor=INK, spaceAfter=7,
                           alignment=TA_JUSTIFY),
    "abstract": ParagraphStyle("abstract", fontName=F["body"], fontSize=10.3, leading=15, textColor=INK,
                               alignment=TA_JUSTIFY, leftIndent=8 * mm, rightIndent=8 * mm, spaceAfter=8),
    "chapter": ParagraphStyle("chapter", fontName=F["bold"], fontSize=16, leading=20, textColor=INK, spaceBefore=6,
                              spaceAfter=14),
    "section": ParagraphStyle("section", fontName=F["bold"], fontSize=12, leading=16, textColor=INK, spaceBefore=12,
                              spaceAfter=6),
    "frontmatter": ParagraphStyle("frontmatter", fontName=F["bold"], fontSize=16, leading=20, textColor=INK,
                                  spaceAfter=14),
    "bullet": ParagraphStyle("bullet", fontName=F["body"], fontSize=10.3, leading=14.6, textColor=INK,
                             leftIndent=14, bulletIndent=3, spaceAfter=3, alignment=TA_JUSTIFY),
    "cell": ParagraphStyle("cell", fontName=F["body"], fontSize=9, leading=12, textColor=INK),
    "cellhead": ParagraphStyle("cellhead", fontName=F["bold"], fontSize=9, leading=12, textColor=INK),
    "code": ParagraphStyle("code", fontName=F["mono"], fontSize=8.4, leading=11.2, textColor=INK, leftIndent=6,
                           rightIndent=6, backColor=CODE_BG, borderPadding=(5, 6, 5, 6), spaceBefore=4,
                           spaceAfter=10),
    "caption": ParagraphStyle("caption", fontName=F["body"], fontSize=9, leading=12.4, textColor=INK,
                              alignment=TA_JUSTIFY, spaceBefore=4, spaceAfter=12),
    "equation": ParagraphStyle("equation", fontName=F["body"], fontSize=10.5, leading=15, textColor=INK,
                               alignment=TA_CENTER),
    "eqnum": ParagraphStyle("eqnum", fontName=F["body"], fontSize=10.3, leading=15, textColor=INK,
                            alignment=TA_RIGHT),
    "ref": ParagraphStyle("ref", fontName=F["body"], fontSize=9.4, leading=13.2, textColor=INK, leftIndent=22,
                          firstLineIndent=-22, spaceAfter=5),
    "keywords": ParagraphStyle("keywords", fontName=F["body"], fontSize=9.8, leading=14, textColor=INK,
                               leftIndent=8 * mm, rightIndent=8 * mm, spaceBefore=4),
}

# ----------------------------------------------------------------------------------------------------------------
#  Automatic numbering of figures and tables. Keys are listed in order of appearance; the text uses fig("key").
# ----------------------------------------------------------------------------------------------------------------
FIGURE_ORDER = ["airframe", "photos", "cascade", "dataflow", "roll_pitch", "estimator", "servo", "flight_attitude",
                "rate_tracking", "actuators", "tracking_error"]
TABLE_ORDER = ["hardware", "safety", "libraries", "flight_stats", "pins", "console", "problems"]


def fig(key):
    return f"Figure {FIGURE_ORDER.index(key) + 1}"


def tab(key):
    return f"Table {TABLE_ORDER.index(key) + 1}"


_equation_counter = [0]


def scripts(text):
    """ReportLab's <sub>/<super> drop into the next line at this leading; use a smaller, gently shifted font."""
    # for <sub>, rise is the distance DOWN; for <super>, the distance up
    return text.replace("<sub>", "<sub rise='2' size='7.2'>").replace("<super>", "<super rise='3.6' size='7.2'>")


def keep_units(text):
    """Stops a line break from separating a number from its unit, and tidies sub/superscripts."""
    for unit in (" %", " µs", " ms", " Hz", " deg", " g ", " s ", " V", " A "):
        text = text.replace(unit, unit.replace(" ", "&nbsp;", 1))
    return scripts(text)


def p(text, style="body"):
    return Paragraph(keep_units(text), S[style])


def bullets(items):
    return [Paragraph(keep_units(item), S["bullet"], bulletText="•") for item in items]


def code(text):
    return Preformatted(text.strip("\n"), S["code"])


def mono(text):
    return f"<font face='{F['mono']}' size='9.2'>{text}</font>"


def equation(text):
    """A displayed equation with its number on the right, as in a paper."""
    _equation_counter[0] += 1
    t = Table([[Paragraph(scripts(text), S["equation"]), Paragraph(f"({_equation_counter[0]})", S["eqnum"])]],
              colWidths=[TEXT_WIDTH - 16 * mm, 16 * mm])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "MIDDLE"), ("LEFTPADDING", (0, 0), (-1, -1), 0),
                           ("RIGHTPADDING", (0, 0), (-1, -1), 0), ("TOPPADDING", (0, 0), (-1, -1), 2),
                           ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    return t


class Caption(Paragraph):
    """A caption that also reports itself to the list of figures or tables."""

    def __init__(self, kind, label, text):
        self.kind, self.label, self.short = kind, label, text.split(". ")[0].rstrip(".")
        super().__init__(f"<b>{label}.</b> {keep_units(text)}", S["caption"])


def figure(key, path_or_flowable, caption, width_mm=150):
    if isinstance(path_or_flowable, str):
        img = Image(os.path.join(ROOT, "docs", "figures", path_or_flowable))
        scale = (width_mm * mm) / img.imageWidth
        img.drawWidth, img.drawHeight = img.imageWidth * scale, img.imageHeight * scale
        body = img
    else:
        body = path_or_flowable
    return KeepTogether([Spacer(1, 2), body, Caption("figure", fig(key), caption)])


def table(key, caption, rows, widths, header=True, font_size=None):
    """Booktabs-style table: caption above, rules only at the top, under the header and at the bottom."""
    cell, head = S["cell"], S["cellhead"]
    if font_size:
        cell = ParagraphStyle("c2", parent=cell, fontSize=font_size, leading=font_size * 1.35)
        head = ParagraphStyle("h2", parent=head, fontSize=font_size, leading=font_size * 1.35)
    data = [[Paragraph(keep_units(str(c)), head if (header and r == 0) else cell) for c in row]
            for r, row in enumerate(rows)]
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if header else 0, hAlign="CENTER")
    style = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LINEABOVE", (0, 0), (-1, 0), 1.0, RULE),
        ("LINEBELOW", (0, -1), (-1, -1), 1.0, RULE),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
        ("LEFTPADDING", (0, 0), (-1, -1), 4),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
    ]
    if header:
        style.append(("LINEBELOW", (0, 0), (-1, 0), 0.6, RULE))
    t.setStyle(TableStyle(style))
    cap = Caption("table", tab(key), caption)
    cap.style = ParagraphStyle("tcap", parent=S["caption"], spaceBefore=6, spaceAfter=5)
    return KeepTogether([cap, t, Spacer(1, 10)])


# ----------------------------------------------------------------------------------------------------------------
#  Document template: headings and captions feed the table of contents and the lists of figures and tables
# ----------------------------------------------------------------------------------------------------------------
class ListOf(TableOfContents):
    """A table of contents that collects one kind of caption (figure or table) instead of headings."""

    def __init__(self, kind):
        super().__init__()
        self.kind = kind
        self.levelStyles = [ParagraphStyle("lo", fontName=F["body"], fontSize=9.6, leading=13.5, leftIndent=0,
                                           firstLineIndent=0, spaceAfter=1)]
        self.dotsMinLevel = 0

    def notify(self, kind, stuff):
        if kind == "ListEntry" and stuff[0] == self.kind:
            self.addEntry(0, stuff[1], stuff[2], stuff[3])


class Report(BaseDocTemplate):
    def __init__(self, filename, **kw):
        super().__init__(filename, **kw)
        frame = Frame(25 * mm, 22 * mm, TEXT_WIDTH, 250 * mm, id="body", leftPadding=0, rightPadding=0,
                      topPadding=0, bottomPadding=0)
        self.addPageTemplates([PageTemplate("title", [frame], onPage=lambda c, d: None),
                               PageTemplate("front", [frame], onPage=page_front),
                               PageTemplate("main", [frame], onPage=page_main)])
        self._bookmark = 0

    def beforeDocument(self):
        # multiBuild lays the document out several times; the entries must be identical in every pass
        self._bookmark = 0

    def afterFlowable(self, flowable):
        if isinstance(flowable, Caption):
            self._bookmark += 1
            key = f"cap{self._bookmark}"
            self.canv.bookmarkPage(key)
            self.notify("ListEntry", (flowable.kind, f"{flowable.label}. {flowable.short}",
                                      self.page - FRONT_PAGES[0], key))
            return
        if not isinstance(flowable, Paragraph):
            return
        level = {"chapter": 0, "section": 1}.get(flowable.style.name)
        if level is None:
            return
        self._bookmark += 1
        key = f"h{self._bookmark}"
        self.canv.bookmarkPage(key)
        text = flowable.getPlainText()
        self.canv.addOutlineEntry(text, key, level=level, closed=level > 0)
        self.notify("TOCEntry", (level, text, self.page - FRONT_PAGES[0], key))  # printed page number


def page_front(canvas, doc):
    canvas.saveState()
    canvas.setFont(F["body"], 9)
    canvas.setFillColor(MUTED)
    canvas.drawCentredString(105 * mm, 12 * mm, _roman(doc.page))
    canvas.restoreState()


def page_main(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(LIGHT_RULE)
    canvas.setLineWidth(0.4)
    canvas.line(25 * mm, 280 * mm, 185 * mm, 280 * mm)
    canvas.setFont(F["italic"], 8.5)
    canvas.setFillColor(MUTED)
    canvas.drawString(25 * mm, 282 * mm, SHORT_TITLE)
    canvas.drawRightString(185 * mm, 282 * mm, AUTHOR)
    canvas.setFont(F["body"], 9)
    canvas.drawCentredString(105 * mm, 12 * mm, str(doc.page - FRONT_PAGES[0]))
    canvas.restoreState()


FRONT_PAGES = [0]  # number of front-matter pages, filled in during the first pass


def _roman(n):
    out = ""
    for value, numeral in ((10, "x"), (9, "ix"), (5, "v"), (4, "iv"), (1, "i")):
        while n >= value:
            out += numeral
            n -= value
    return out


class MarkFrontEnd(Spacer):
    """Placed on the last front-matter page: records how many pages the front matter has, so the main text
    starts at page 1. It runs before the first main page is started, so that page's footer is already right."""

    def __init__(self):
        super().__init__(0, 0)

    def drawOn(self, canvas, x, y, _sW=0):
        FRONT_PAGES[0] = canvas.getPageNumber()
        super().drawOn(canvas, x, y, _sW)


# ----------------------------------------------------------------------------------------------------------------
#  Diagrams
# ----------------------------------------------------------------------------------------------------------------
def label(d, x, y, text, size=8.5, color=INK, anchor="middle", font=None):
    d.add(String(x, y, text, fontName=font or F["sans"], fontSize=size, fillColor=color, textAnchor=anchor))


def arrow(d, x1, y1, x2, y2, color=INK, width=1.0, head=5):
    d.add(Line(x1, y1, x2, y2, strokeColor=color, strokeWidth=width))
    a = math.atan2(y2 - y1, x2 - x1)
    left = (x2 - head * math.cos(a - 0.45), y2 - head * math.sin(a - 0.45))
    right = (x2 - head * math.cos(a + 0.45), y2 - head * math.sin(a + 0.45))
    d.add(Polygon([x2, y2, left[0], left[1], right[0], right[1]], fillColor=color, strokeColor=color,
                  strokeWidth=0.5))


def airframe_diagram():
    """Top view (left) and side view (right) of the bicopter with what each actuator does."""
    d = Drawing(160 * mm, 60 * mm)
    cx, cy = 38 * mm, 30 * mm
    d.add(Line(cx - 30 * mm, cy, cx + 30 * mm, cy, strokeColor=INK, strokeWidth=3))
    d.add(Rect(cx - 6 * mm, cy - 9 * mm, 12 * mm, 18 * mm, rx=2, ry=2, fillColor=DIAGRAM_FILL, strokeColor=INK))
    for side, x in (("left", cx - 30 * mm), ("right", cx + 30 * mm)):
        d.add(Circle(x, cy, 10 * mm, fillColor=None, strokeColor=DIAGRAM_ACCENT, strokeWidth=0.9,
                     strokeDashArray=[2, 2]))
        d.add(Circle(x, cy, 2.2 * mm, fillColor=INK, strokeColor=INK))
        label(d, x, cy - 15 * mm, f"{side} motor + servo", 7.8, MUTED)
    arrow(d, cx, cy + 9 * mm, cx, cy + 22 * mm, DIAGRAM_ACCENT)
    label(d, cx + 1.5 * mm, cy + 23.5 * mm, "nose (+X)", 8, DIAGRAM_ACCENT, "start")
    arrow(d, cx - 6 * mm, cy, cx - 17 * mm, cy, DIAGRAM_ACCENT, 0.9, 4)
    label(d, cx - 12 * mm, cy + 2.5 * mm, "+Y", 7.5, DIAGRAM_ACCENT)
    label(d, cx, 1 * mm, "(a) Top view", 8.5, INK, font=F["body"])

    sx, sy = 116 * mm, 17 * mm
    d.add(Rect(sx - 22 * mm, sy - 2 * mm, 44 * mm, 4 * mm, fillColor=DIAGRAM_FILL, strokeColor=INK))
    d.add(Circle(sx, sy + 2 * mm, 2 * mm, fillColor=INK, strokeColor=INK))
    for tilt, dash in ((0, None), (18, [2, 2]), (-18, [2, 2])):
        a = math.radians(90 - tilt)
        x2, y2 = sx + 26 * mm * math.cos(a), sy + 2 * mm + 26 * mm * math.sin(a)
        d.add(Line(sx, sy + 2 * mm, x2, y2, strokeColor=INK if tilt == 0 else MUTED,
                   strokeWidth=1.4 if tilt == 0 else 0.8, strokeDashArray=dash))
    arrow(d, sx, sy + 30 * mm, sx, sy + 40 * mm, DIAGRAM_ACCENT)
    label(d, sx + 2 * mm, sy + 41 * mm, "thrust", 8, DIAGRAM_ACCENT, "start")
    label(d, sx + 14 * mm, sy + 22 * mm, "tilt forward", 7.5, MUTED, "start")
    label(d, sx - 14 * mm, sy + 22 * mm, "tilt back", 7.5, MUTED, "end")
    arrow(d, sx - 26 * mm, sy - 8 * mm, sx + 26 * mm, sy - 8 * mm, MUTED, 0.6, 3)
    label(d, sx + 27 * mm, sy - 9 * mm, "nose", 7.5, MUTED, "start")
    label(d, sx, 1 * mm, "(b) Side view of one tilt unit", 8.5, INK, font=F["body"])
    return d


def box(d, x, y, w, h, title, sub=None, fill=DIAGRAM_FILL):
    d.add(Rect(x, y, w, h, rx=2, ry=2, fillColor=fill, strokeColor=INK, strokeWidth=0.6))
    label(d, x + w / 2, y + h / 2 + (2.2 if sub else -2.5), title, 8.2, INK, font=F["sans_bold"])
    if sub:
        label(d, x + w / 2, y + h / 2 - 7.5, sub, 7, MUTED)


def dataflow_diagram():
    d = Drawing(160 * mm, 76 * mm)
    W, H = 32 * mm, 13 * mm
    rx_y, imu_y = 57 * mm, 24 * mm
    box(d, 0, rx_y, W, H, "RcPwmInput", "4 pulse widths")
    box(d, 0, imu_y, W, H, "Mpu6050", "gyro + accel")
    box(d, 42 * mm, rx_y, W, H, "StickInput", "sticks, gestures")
    box(d, 42 * mm, imu_y, W, H, "AttitudeEstimator", "roll, pitch")
    box(d, 85 * mm, 40 * mm, W, H, "FlightController", "states + PIDs", colors.HexColor("#dde5ee"))
    box(d, 128 * mm, 40 * mm, W, H, "BiCopterMixer", "4 outputs")
    box(d, 128 * mm, 8 * mm, W, H, "PwmOutput × 4", "ESC L/R, servo L/R")
    box(d, 85 * mm, 8 * mm, W, H, "Settings", "trims, level, gains")
    box(d, 42 * mm, 2 * mm, W, 10 * mm, "SerialConsole")
    arrow(d, W, rx_y + H / 2, 42 * mm, rx_y + H / 2)
    arrow(d, W, imu_y + H / 2, 42 * mm, imu_y + H / 2)
    arrow(d, 42 * mm + W, rx_y + H / 2, 85 * mm, 40 * mm + H - 2)
    arrow(d, 42 * mm + W, imu_y + H / 2, 85 * mm, 40 * mm + 2)
    arrow(d, 85 * mm + W, 40 * mm + H / 2, 128 * mm, 40 * mm + H / 2)
    arrow(d, 128 * mm + W / 2, 40 * mm, 128 * mm + W / 2, 8 * mm + H)
    arrow(d, 85 * mm + W / 2, 8 * mm + H, 85 * mm + W / 2, 40 * mm)
    arrow(d, 42 * mm + W, 7 * mm, 85 * mm, 12 * mm, MUTED, 0.7, 3.5)
    return d


def cascade_diagram():
    d = Drawing(160 * mm, 34 * mm)
    y, h = 12 * mm, 12 * mm
    label(d, 0, y + h / 2 - 3, "θ*", 9, INK, "start", font=F["body"])
    arrow(d, 6 * mm, y + h / 2, 14 * mm, y + h / 2)
    box(d, 14 * mm, y, 30 * mm, h, "Angle P", "outer loop")
    arrow(d, 44 * mm, y + h / 2, 56 * mm, y + h / 2)
    label(d, 50 * mm, y + h / 2 + 3, "ω*", 8, MUTED, font=F["body"])
    box(d, 56 * mm, y, 30 * mm, h, "Rate PID", "inner loop", colors.HexColor("#dde5ee"))
    arrow(d, 86 * mm, y + h / 2, 98 * mm, y + h / 2)
    label(d, 92 * mm, y + h / 2 + 3, "u", 8, MUTED, font=F["body"])
    box(d, 98 * mm, y, 26 * mm, h, "Mixer")
    arrow(d, 124 * mm, y + h / 2, 134 * mm, y + h / 2)
    box(d, 134 * mm, y, 26 * mm, h, "Airframe")
    d.add(Line(147 * mm, y, 147 * mm, 3 * mm, strokeColor=MUTED, strokeWidth=0.7))
    d.add(Line(147 * mm, 3 * mm, 29 * mm, 3 * mm, strokeColor=MUTED, strokeWidth=0.7))
    arrow(d, 29 * mm, 3 * mm, 29 * mm, y, MUTED, 0.7, 3.5)
    arrow(d, 71 * mm, 3 * mm, 71 * mm, y, MUTED, 0.7, 3.5)
    label(d, 30 * mm, 5 * mm, "θ (estimator)", 7.5, MUTED, "start", font=F["body"])
    label(d, 72 * mm, 5 * mm, "ω (gyro)", 7.5, MUTED, "start", font=F["body"])
    return d


# ----------------------------------------------------------------------------------------------------------------
#  Front matter
# ----------------------------------------------------------------------------------------------------------------
def title_page():
    month = datetime.date.today().strftime("%B %Y")
    title = ParagraphStyle("t", fontName=F["bold"], fontSize=24, leading=31, textColor=INK, alignment=TA_CENTER)
    sub = ParagraphStyle("s", fontName=F["italic"], fontSize=13, leading=18, textColor=MUTED, alignment=TA_CENTER)
    meta = ParagraphStyle("m", fontName=F["body"], fontSize=11.5, leading=17, textColor=INK, alignment=TA_CENTER)
    small = ParagraphStyle("sm", fontName=F["body"], fontSize=9, leading=13, textColor=MUTED, alignment=TA_CENTER)
    photo = Image(os.path.join(ROOT, "docs", "media", "bicopter.jpg"))
    scale = (120 * mm) / photo.imageWidth
    photo.drawWidth, photo.drawHeight = photo.imageWidth * scale, photo.imageHeight * scale
    return [
        NextPageTemplate("front"),
        Spacer(1, 32 * mm),
        Paragraph(TITLE, title),
        Spacer(1, 6 * mm),
        Paragraph(SUBTITLE, sub),
        Spacer(1, 16 * mm),
        photo,
        Spacer(1, 18 * mm),
        Paragraph("Project report", meta),
        Spacer(1, 3 * mm),
        Paragraph(f"<b>{AUTHOR}</b>", meta),
        Spacer(1, 3 * mm),
        Paragraph(month, meta),
        Spacer(1, 24 * mm),
        Paragraph(f"Source code, data and figures: {REPO}", small),
        PageBreak(),
    ]


def abstract_page():
    return [
        Paragraph("Abstract", S["frontmatter"]),
        p("A bicopter is a multirotor with only two rotors. Because two rotors cannot steer by speed alone, each is "
          "mounted on a servo that tilts it forward and back: roll is controlled by differential thrust, while "
          "pitch and yaw are controlled by tilting the rotors. This report describes the design, implementation "
          "and experimental evaluation of a flight controller for such an aircraft, running on an ESP32-S3 "
          "microcontroller with an MPU6050 inertial sensor, two brushless motors and two MG996R servos.", "abstract"),
        p("Attitude is estimated with a Mahony complementary filter on a quaternion, extended with a smooth "
          "accelerometer weighting and an integral term that learns the gyroscope bias in flight. Each axis is "
          "stabilised by a cascaded controller, an outer proportional angle loop feeding an inner PID rate loop, "
          "executed at 500 Hz. Safety logic covers arming, radio failsafe, crash detection, take-off detection and "
          "a hardware watchdog. The firmware is organised as small, independent C++ libraries and was tuned against "
          "flight data logged on the real aircraft.", "abstract"),
        p("The experiments show a clear asymmetry between the axes. The roll axis, actuated by the motors, tracked "
          "its commanded rate with an RMS error of 16 deg/s, whereas the pitch axis, actuated by the servos, showed "
          "an error of 80 deg/s and oscillated by up to 25 deg. The analysis attributes this mainly to the "
          "mechanics of the tilt axis: the slow response and free play of the servos, the short moment arm between "
          "the rotors and the centre of gravity, and a centre of gravity located behind the rotor axis. Firmware "
          "measures, in particular take-off detection and a simplified pitch controller, removed controller-induced "
          "contributions; the remaining limitation is mechanical.", "abstract"),
        Paragraph("<b>Keywords:</b> bicopter, tilt-rotor, attitude estimation, Mahony filter, cascaded PID "
                  "control, ESP32-S3, embedded flight control", S["keywords"]),
        PageBreak(),
    ]


def contents_pages():
    toc = TableOfContents()
    toc.levelStyles = [
        ParagraphStyle("toc0", fontName=F["bold"], fontSize=10.2, leading=13.5, leftIndent=0, firstLineIndent=0,
                       spaceBefore=4),
        ParagraphStyle("toc1", fontName=F["body"], fontSize=9.7, leading=12.4, leftIndent=8 * mm,
                       firstLineIndent=0),
    ]
    toc.dotsMinLevel = 0
    return [
        Paragraph("Contents", S["frontmatter"]), toc, PageBreak(),
        Paragraph("List of Figures", S["frontmatter"]), ListOf("figure"),
        Spacer(1, 10 * mm),
        Paragraph("List of Tables", S["frontmatter"]), ListOf("table"),
        MarkFrontEnd(),
        NextPageTemplate("main"),
        PageBreak(),
    ]


# ----------------------------------------------------------------------------------------------------------------
#  Chapters
# ----------------------------------------------------------------------------------------------------------------
def chapter(title, new_page=True):
    """Chapters start on a new page, as in a report."""
    return [PageBreak() if new_page else Spacer(1, 0), Paragraph(title, S["chapter"])]


def section(title):
    return Paragraph(title, S["section"])


def ch_introduction():
    return [
        *chapter("1  Introduction", new_page=False),
        p("Multirotors with four or more rotors control all three rotational axes by varying rotor speeds. A "
          "bicopter, with only two rotors, has enough thrust to fly but not enough independent speed changes to "
          "steer. The usual solution is to tilt the rotors: tilting both rotors forward lowers the nose, tilting "
          "them in opposite directions yaws the aircraft, and roll is still produced by running one rotor faster "
          "than the other [5]. The resulting airframe is compact and efficient, but its pitch and yaw axes depend on "
          "servos whose response is an order of magnitude slower than that of an electronic speed controller."),
        p("This project develops a flight controller for a home-built tilt-rotor bicopter. Its objectives are:"),
        *bullets([
            "a controller that is safe to test with and small enough to be read and modified without a large "
            "autopilot code base;",
            "an analysis, based on logged flight data, of where the instability on the servo-actuated axes comes "
            "from;",
            "firmware measures that reduce that instability, and an honest separation of what remains a mechanical "
            "limitation.",
        ]),
        p("Chapter 2 reviews related work. Chapter 3 describes the aircraft and its hardware, and Chapter 4 the "
          "estimation and control design. Chapter 5 covers the implementation, Chapter 6 the experimental results, "
          "and Chapters 7 and 8 discuss the findings and conclude. The appendices contain the wiring, the "
          "step-by-step setup and tuning procedure, the console reference and the problems encountered during "
          "testing."),
    ]


def ch_background():
    return [
        *chapter("2  Background and Related Work"),
        section("2.1  Bicopters and tilt-rotor control"),
        p("Tilt-rotor bicopters have been studied as an efficient alternative to quadrotors for indoor flight. "
          "Qin et al. [5] presented Gemini, a compact bi-copter whose two rotors are tilted by servos, and showed "
          "that the configuration can fly efficiently but places high demands on the tilt actuators. In the open "
          "source domain, ArduPilot supports the bicopter as a frame class with tilt servos [6], while hobby "
          "flight controllers such as PlainFlightController [7], dRehmFlight [8], ESP-FC [9] and ESP-Drone [10] "
          "provide reference implementations of multirotor control on small microcontrollers; most of them "
          "target quadrotors."),
        section("2.2  Attitude estimation"),
        p("Low-cost inertial sensors combine a gyroscope, which measures angular rate precisely but drifts, with "
          "an accelerometer, which measures the direction of gravity but is disturbed by vibration and by the "
          "aircraft's own accelerations. Complementary filters fuse the two by trusting the gyroscope at high "
          "frequencies and the accelerometer at low frequencies. Mahony et al. [1] formulated this as a nonlinear "
          "complementary filter on the rotation group with proportional and integral correction, and Madgwick [2] "
          "proposed a gradient-descent variant. The filter used here follows Mahony's formulation."),
        section("2.3  Cascaded attitude control"),
        p("Multirotor attitude control is commonly implemented as a cascade of an outer angle loop and an inner "
          "angular-rate loop [3], [4]. The inner loop rejects disturbances quickly using the gyroscope, while the "
          "outer loop only has to define the desired attitude. Practical flight controllers add derivative "
          "filtering, integrator anti-windup and gain scheduling with throttle; Betaflight's documentation [11] "
          "summarises these techniques for small multirotors."),
    ]


def ch_system():
    return [
        *chapter("3  System Description"),
        section("3.1  Airframe and tilt mechanism"),
        p(f"The aircraft has a single arm with a rotor at each end ({fig('airframe')}). Each rotor is driven by a "
          "brushless motor sitting on a 3D-printed mount, which is turned about the arm by an MG996R servo. The "
          "flight controller, the inertial sensor and the battery are mounted at the centre of the arm. "
          f"{fig('photos')} shows the aircraft built for this project."),
        figure("airframe", airframe_diagram(),
               "Schematic of the airframe. (a) Top view with the body axes. (b) Side view of one tilt unit: the "
               "servo tilts the rotor forward or back, which changes the direction of its thrust."),
        figure("photos", "fig8_mechanism_photos.jpg",
               "The bicopter built in this project. (a) Complete aircraft on its test stand. (b) Left and (c) right "
               "tilt unit: brushless motor on a printed mount, turned by an MG996R servo. (d) Aircraft held by hand "
               "with both motors running. (e) Aircraft held up during a flight test, seen from below.",
               width_mm=140),
        section("3.2  Hardware components"),
        p(f"The components are listed in {tab('hardware')}. The servos are supplied by a separate regulator so that "
          "their current peaks do not disturb the flight controller."),
        table("hardware", "Hardware components.", [
            ["Function", "Component", "Notes"],
            ["Flight controller", "ESP32-S3 PowerFeather", "240 MHz dual core, native USB, WiFi"],
            ["Inertial sensor", "MPU6050 (GY-521 board)", "3-axis gyroscope and accelerometer, I2C"],
            ["Radio link", "FlySky FS-i6 with FS-iA6B", "four channels, one PWM signal each"],
            ["Tilt actuators", "2 × MG996R servo", "about 0.17 s per 60 deg, updated at 100 Hz"],
            ["Propulsion", "2 × brushless motor with ESC", "PWM at 400 Hz"],
            ["Servo supply", "5-6 V UBEC", "separate from the controller supply"],
        ], [36, 56, 68]),
        section("3.3  Coordinate frame and sign conventions"),
        p("A single body frame is used throughout: X points to the nose, Y to the left rotor and Z upwards. "
          "Positive roll <i>φ</i> lowers the right side, positive pitch <i>θ</i> lowers the nose and positive yaw "
          "<i>ψ</i> turns the "
          "nose to the left. The sensor axes are mapped onto this frame by a rotation set in the configuration; "
          "the build rejects mappings that are mirror images, which is the most frequent mounting error."),
    ]


def ch_design():
    return [
        *chapter("4  Modelling and Control Design"),
        section("4.1  Actuation and mixing"),
        p("Each actuator serves more than one axis. With the collective thrust command <i>T</i> and the controller "
          "outputs <i>u</i><sub>φ</sub>, <i>u</i><sub>θ</sub> and <i>u</i><sub>ψ</sub> for roll, pitch and yaw, "
          "the motor commands <i>m</i> and the servo tilt commands <i>s</i> are"),
        equation("<i>m</i><sub>L</sub> = <i>T</i> + <i>u</i><sub>φ</sub>,&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<i>m</i><sub>R</sub> = <i>T</i> − <i>u</i><sub>φ</sub>"),
        equation("<i>s</i><sub>L</sub> = <i>u</i><sub>θ</sub> − <i>u</i><sub>ψ</sub>,&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<i>s</i><sub>R</sub> = <i>u</i><sub>θ</sub> + <i>u</i><sub>ψ</sub>"),
        p("When a motor command would leave its range, both motor commands are shifted together so that the roll "
          "difference is preserved. Propeller thrust grows approximately with the square of the command, so the "
          "thrust demand <i>τ</i> is converted into an actuator command <i>a</i> using the thrust curve"),
        equation("τ = (1 − <i>e</i>)<i>a</i> + <i>e a</i><super>2</super>&nbsp;&nbsp;⇒&nbsp;&nbsp;"
                 "<i>a</i> = [<i>e</i> − 1 + √((1 − <i>e</i>)<super>2</super> + 4<i>e</i>τ)] / 2<i>e</i>"),
        p("with <i>e</i> = 0.65, the same model as ArduPilot's thrust exponent. A roll command therefore produces the "
          "same torque at any throttle."),
        section("4.2  Attitude estimation"),
        p("On a bicopter the accelerometer measures mainly the direction of the rotor thrust, which changes with "
          "every servo correction, so it can only be trusted on long time scales. The estimator is a Mahony filter "
          "[1] on the unit quaternion <i>q</i>. Let <b>a</b><sub>n</sub> be the normalised, low-pass filtered "
          "accelerometer vector (second-order Butterworth, 10 Hz) and <b>v</b> the direction of gravity predicted "
          "by <i>q</i>. The correction is"),
        equation("<b>e</b> = <b>a</b><sub>n</sub> × <b>v</b>"),
        equation("<b>b</b> ← <b>b</b> + <i>k</i><sub>i</sub> <i>w</i> <b>e</b> Δ<i>t</i>"),
        equation("<b>ω</b>′ = <b>ω</b> + <i>k</i><sub>p</sub> <i>w</i> <b>e</b> + <b>b</b>,&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<i>q̇</i> = ½ <i>q</i> ⊗ (0, <b>ω</b>′)"),
        p("with <i>k</i><sub>p</sub> = 1/3 s<super>−1</super> and <i>k</i><sub>i</sub> = <i>k</i><sub>p</sub>/15 s. The integral state "
          "<b>b</b> converges to the gyroscope bias, so the slow correction leaves no steady angle error. The "
          "accelerometer weight <i>w</i> falls linearly from 1 at exactly 1 g to 0 at 0.8 g and 1.2 g, because outside "
          "that range the measurement contains the aircraft's own acceleration:"),
        equation("<i>w</i> = max(0, 1 − | ‖<b>a</b>‖ − 1 | / 0.2)"),
        section("4.3  Cascaded attitude control"),
        p(f"Each axis uses two loops in series ({fig('cascade')}). The outer loop converts the angle error into a "
          "desired angular rate, and the inner loop drives the actuators so that the measured rate <i>ω</i> follows "
          "it:"),
        equation("<i>ω</i>* = <i>K</i><sub>θ</sub> (<i>θ</i>* − <i>θ</i>)"),
        equation("<i>u</i> = <i>K</i><sub>p</sub> <i>e</i><sub>ω</sub> + <i>K</i><sub>i</sub> ∫ <i>e</i><sub>ω</sub>"
                 " d<i>t</i> − <i>K</i><sub>d</sub> d<i>ω</i>/d<i>t</i>,&nbsp;&nbsp;&nbsp;&nbsp;"
                 "<i>e</i><sub>ω</sub> = <i>ω</i>* − <i>ω</i>"),
        p("The derivative acts on the measurement rather than the error, so fast stick movements do not cause a "
          "kick, and the integrator stops while the output is saturated. With the yaw stick centred, an outer "
          "heading loop of the same form holds the current heading."),
        figure("cascade", cascade_diagram(),
               "Cascaded attitude control for one axis. The outer loop uses the estimated angle θ, the inner loop "
               "the gyroscope rate ω.", width_mm=160),
        section("4.4  Filtering"),
        p("The sensor's internal low-pass filter is set to 44 Hz. In addition the gyroscope signal is filtered at "
          "80 Hz for roll, 40 Hz for pitch and 25 Hz for yaw, and the servo commands at 30 Hz. Every filter adds "
          "phase lag, so the servo axes use the lightest filtering that still prevents vibration from making the "
          "servos chatter in their gear play."),
        section("4.5  Take-off detection and safety"),
        p("On the ground the airframe cannot respond to corrections, and an active integrator accumulates "
          "correction against the ground. In testing this tilted the servos by 20-25 deg before lift-off and made "
          "the aircraft lunge forward as it left the ground. The integrators, heading hold and roll actuation are "
          "therefore enabled only when the throttle reaches a lift-off value (35 %), and disabled again only near "
          f"idle. The remaining safety functions are summarised in {tab('safety')}."),
        table("safety", "Safety functions of the flight controller.", [
            ["Situation", "Response"],
            ["Power-up", "ESC outputs set to 1000 µs (motor off) before any other initialisation."],
            ["Arming request", "Refused unless the radio is valid, the throttle is low, the aircraft is within "
                               "25 deg of level, the gyroscope is calibrated and the stick centres are learned."],
            ["Radio link lost", "Motors stop; re-arming requires the link to return with the throttle low."],
            ["Crash", "Tilted beyond 60 deg for 0.5 s: disarm."],
            ["Idle while armed", "15 s at idle: disarm."],
            ["Sensor failure", "25 consecutive failed reads (50 ms): motors stop until reboot."],
            ["Software stall", "300 ms watchdog resets the processor; all outputs go low."],
        ], [36, 124]),
    ]


def ch_implementation():
    return [
        *chapter("5  Implementation"),
        section("5.1  Software architecture"),
        p("The firmware is a PlatformIO project for the Arduino framework on the ESP32-S3. The main sketch only "
          "connects the components and runs the control loop; every function lives in a small library with one "
          "header and one source file. No library reads the project configuration directly: each receives a "
          "settings structure from the sketch, which keeps the libraries independent and allows the estimator, "
          f"the filters and the PID to be compiled and tested on a PC. {fig('dataflow')} shows the data flow and "
          f"{tab('libraries')} the role of each library."),
        figure("dataflow", dataflow_diagram(), "Data flow through the libraries in one pass of the control loop.",
               width_mm=160),
        table("libraries", "Firmware libraries.", [
            ["Library", "Function"],
            ["Mpu6050", "Burst read of gyroscope, accelerometer and temperature; gyroscope calibration at start-up; "
                        "mapping of the sensor axes to the body frame."],
            ["RcPwmInput", "Measurement of the four receiver pulses with the MCPWM capture hardware, independent of "
                           "interrupt latency; median-of-three glitch filter."],
            ["StickInput", "Scaling of the sticks, learning of the stick centres, receiver failsafe detection, "
                           "arming and calibration gestures."],
            ["AttitudeEstimator", "Mahony filter with smooth accelerometer weighting and gyroscope bias learning."],
            ["Pid, Pt1Filter, BiquadFilter", "Rate controller and the low-pass filters."],
            ["LeadCompensator", "Optional phase lead for the servo commands (disabled by default)."],
            ["BiCopterMixer, PwmOutput", "Mixing to four pulse widths, generated by the LEDC peripheral at 400 Hz "
                                         "(ESCs) and 100 Hz (servos)."],
            ["FlightController", "State machine, safety logic and control laws."],
            ["Settings, SerialConsole, TuningWeb", "Non-volatile parameters, USB console, WiFi tuning page and "
                                                   "flight recorder."],
        ], [44, 116]),
        section("5.2  Real-time loop"),
        p("The control loop runs every 2 ms. Each pass reads the sensor, updates the attitude estimate, reads the "
          "sticks, runs the state machine and the controllers, and writes the outputs. Reading the sensor over "
          "I2C takes about 0.4 ms. The longest pass is reported continuously so that timing regressions are "
          "visible immediately."),
        section("5.3  Development and test tools"),
        p("Two tools supported the development. A USB console streams the controller's internal state ten times "
          "per second and accepts commands for calibration and gain changes. A web page served by the ESP32 over "
          "its own WiFi network shows the attitude live, allows gains to be changed from a phone and records "
          "flights at 25 samples per second to a CSV file. Most design decisions in this report were taken after "
          "analysing such recordings."),
    ]


def ch_results():
    return [
        *chapter("6  Experimental Results"),
        section("6.1  Test method"),
        p("Tests were carried out with the aircraft held by hand, on a stand and on a tether. Two data sources "
          "were used: the console stream at 10 Hz and the web recorder at 25 Hz. Two results were obtained by "
          "simulation; they are labelled as such. All figures are generated by tools/make_figures.py from the data "
          "in docs/data and from the simulations."),
        section("6.2  Roll and pitch behaviour"),
        p(f"{fig('roll_pitch')} shows 20 s of a logged throttle sweep with an earlier version of the controller. "
          "The roll axis, actuated by the motors, stays within 6 deg of level. The pitch axis, actuated by the "
          "servos, swings by up to 25 deg, and its command remains positive for long periods because the centre "
          "of gravity lay behind the rotor axis, so the servos had to hold the rotors tilted forward."),
        figure("roll_pitch", "fig1_roll_vs_pitch.png",
               "Measured (a) roll and pitch angle and (b) pitch servo command during a throttle sweep at 25 % "
               "throttle (console log, earlier controller version)."),
        section("6.3  Attitude estimation"),
        p(f"{fig('estimator')} compares the complementary filter used initially with the Mahony filter of Section "
          "4.2 on the same simulated hover, including a gyroscope bias, propeller vibration and the accelerations "
          "caused by servo corrections. The simulation calls the firmware's own estimator code. The complementary "
          "filter retains a constant error of about 1.5 deg, the bias multiplied by its time constant, while the "
          "Mahony filter learns the bias within about 30 s (RMS error 0.13 deg against 1.45 deg)."),
        figure("estimator", "fig2_attitude_error.png",
               "Simulated pitch estimation error of the complementary filter and the Mahony filter."),
        section("6.4  Servo response"),
        p(f"{fig('servo')} shows the response of a simple MG996R-like servo model to a 10 deg step. A phase lead of "
          "40 ms reduces the time to 90 % from 112 ms to 30 ms, at the cost of a small overshoot. On the real "
          "aircraft, combined with a higher pitch gain, the lead caused vibration, possibly through the reaction "
          "torque of the motor mounts when the servos accelerate; it is therefore disabled by default."),
        figure("servo", "fig3_servo_lead.png",
               "Simulated servo step response with and without a 40 ms phase lead (model, not a measurement)."),
        section("6.5  Recorded flight"),
        p("A 22 s flight was recorded on 23 October 2026 with the web recorder, of which 20 s were airborne at "
          "63 % throttle. The complete record is available as docs/data/bicopter_20261023_182347.csv. It was flown "
          "with an earlier firmware version in which a hover trim was active, so the pitch target was about 9 deg "
          f"nose-up. {tab('flight_stats')} summarises the airborne part."),
        table("flight_stats", "Statistics of the recorded flight (airborne part, throttle above 30 %).", [
            ["Quantity", "Value"],
            ["Airborne time and throttle", "20.2 s at 63 %"],
            ["Roll angle, mean and standard deviation", "+1.1 deg, 5.3 deg"],
            ["Pitch angle, mean and standard deviation", "−18.6 deg, 14.1 deg"],
            ["RMS rate tracking error: roll / pitch / yaw", "16 / 80 / 22 deg/s"],
            ["Mean ESC pulse, left minus right", "+90 µs"],
            ["Motor tilt learned in hover", "+9.3 deg (forward)"],
            ["Samples in which the accelerometer was trusted", "44 %"],
        ], [105, 55]),
        p(f"{fig('flight_attitude')} shows the attitude over the whole flight. Roll remains close to level apart "
          "from short excursions, whereas pitch repeatedly drops 40-55 deg nose-up and recovers, with a period of "
          "about 2.5 s."),
        figure("flight_attitude", "fig4_flight_attitude.png",
               "Recorded flight: (a) roll angle; (b) pitch angle and its hover-trim target."),
        p(f"{fig('rate_tracking')} compares commanded and measured rates on a common scale. On roll the two follow "
          "each other closely. On pitch the commanded rate saturates at its 90 deg/s limit for almost two seconds "
          "while the aircraft barely responds, after which the measured rate oscillates around the command: the "
          "servos cannot generate the pitch moment fast enough."),
        figure("rate_tracking", "fig5_rate_tracking.png",
               "Commanded and measured angular rate for (a) roll and (b) pitch, 5 s of the recorded flight."),
        p(f"{fig('actuators')} shows the actuator outputs. The left motor runs about 90 µs above the right one for "
          "most of the flight, indicating either a centre of gravity offset to the left or less thrust from the "
          "left rotor. Both servos remain away from their 1500 µs centre, holding the rotors tilted forward, which "
          "is consistent with a centre of gravity behind the rotor axis and with the learned hover tilt of "
          "+9.3 deg."),
        figure("actuators", "fig6_actuators.png",
               "Recorded flight: (a) ESC pulses; (b) servo pulses. The right servo is mounted mirrored and moves "
               "in the opposite direction for the same tilt."),
        p(f"{fig('tracking_error')} condenses the comparison: the servo-actuated pitch axis tracks its commanded "
          "rate five times worse than the motor-actuated roll axis."),
        figure("tracking_error", "fig7_tracking_error.png",
               "RMS rate tracking error per axis over the airborne part of the recorded flight.", width_mm=100),
    ]


def ch_discussion():
    return [
        *chapter("7  Discussion"),
        p("The results point to a single dominant cause: on this tilt-rotor bicopter the pitch axis is limited "
          "by its mechanics rather than by the controller. The control moment of the pitch axis is the thrust "
          "multiplied by the tilt angle and by the height of the rotors above the centre of gravity, a far shorter "
          "lever than the arm through which roll acts. The MG996R servos add roughly 100 ms of response time and "
          "some free play, and every pitch correction also produces a horizontal force that moves the aircraft "
          "forward or back. Where the centre of gravity lies behind the rotor axis, the servos must additionally "
          "hold a constant tilt, which consumes the travel needed for corrections and drives the pitch command "
          "into saturation."),
        p("Several controller-induced contributions were identified and removed: integrator wind-up on the "
          "ground, a stick offset integrated by the yaw loop, an accelerometer bias that caused the estimator to "
          "reject the accelerometer, and additional layers on the pitch controller that amplified fast servo "
          "movements. After these corrections the remaining behaviour is consistent with the mechanical "
          "explanation above."),
        section("7.1  Limitations"),
        *bullets([
            "The controller stabilises attitude and heading but not position; without an optical-flow or "
            "position sensor the aircraft drifts slowly with air currents and small level errors.",
            "The tilt servos limit the achievable pitch and yaw bandwidth.",
            "During testing the aircraft required a high hover throttle, which leaves little thrust margin for "
            "corrections.",
            "Without a magnetometer the heading reference drifts with the gyroscope bias.",
            "The recorded flight data were obtained with an earlier firmware version; the effect of the final "
            "configuration has not yet been quantified in the same way.",
        ]),
    ]


def ch_conclusion():
    return [
        *chapter("8  Conclusions and Future Work"),
        p("A flight controller for a tilt-rotor bicopter was designed, implemented on an ESP32-S3 and evaluated "
          "with logged flight data. The roll axis, actuated by differential thrust, proved easy to stabilise. The "
          "pitch axis, actuated by servos, tracked its commanded rate five times worse, and the analysis showed "
          "this to be mainly a mechanical limitation. A Mahony estimator with bias learning, take-off detection "
          "and a simplified pitch controller removed the contributions of the firmware itself."),
        p("The following steps are expected to improve the aircraft most:"),
        *bullets([
            "a centre of gravity placed low below the rotor axis and balanced front to back, so that the rotors are "
            "vertical in hover;",
            "faster digital servos (0.06-0.10 s per 60 deg) driven at 200 Hz or more, and a stiffer linkage "
            "without play;",
            "an optical-flow sensor and a laser rangefinder for position and height hold indoors;",
            "battery voltage measurement to keep the throttle response constant during discharge;",
            "feed-forward from the roll motor command to yaw, to cancel the rotor torque coupling.",
        ]),
    ]


def references():
    refs = [
        "R. Mahony, T. Hamel and J.-M. Pflimlin, \"Nonlinear complementary filters on the special orthogonal "
        "group,\" <i>IEEE Transactions on Automatic Control</i>, vol. 53, no. 5, 2008.",
        "S. O. H. Madgwick, \"An efficient orientation filter for inertial and inertial/magnetic sensor arrays,\" "
        "technical report, University of Bristol, 2010.",
        "K. J. Åström and R. M. Murray, <i>Feedback Systems: An Introduction for Scientists and Engineers</i>. "
        "Princeton University Press, 2008.",
        "Q. Quan, <i>Introduction to Multicopter Design and Control</i>. Springer, 2017.",
        "Y. Qin, W. Xu, A. Lee and F. Zhang, \"Gemini: A compact yet efficient bi-copter UAV for indoor "
        "applications,\" <i>IEEE Robotics and Automation Letters</i>, vol. 5, no. 2, 2020.",
        "ArduPilot, \"Tilt rotor planes (bicopter frame class).\" https://ardupilot.org/plane/docs/"
        "guide-tilt-rotor.html",
        "P. Cook, \"PlainFlightController.\" https://github.com/plainFlight/plainFlightController",
        "N. Rehm, \"dRehmFlight VTOL flight controller.\" https://github.com/nickrehm/dRehmFlight",
        "R. Lopez, \"ESP-FC: a flight controller for ESP32.\" https://github.com/rtlopez/esp-fc",
        "Espressif Systems, \"ESP-Drone.\" https://github.com/espressif/esp-drone",
        "Betaflight, \"Betaflight documentation: PID tuning and filtering.\" https://betaflight.com/docs",
        "Espressif Systems, <i>ESP32-S3 Technical Reference Manual</i>, chapters on MCPWM and the LED PWM "
        "controller.",
        "Espressif Systems, \"Arduino-ESP32 documentation.\" https://docs.espressif.com/projects/arduino-esp32",
        "InvenSense (TDK), <i>MPU-6000 and MPU-6050 Product Specification</i> (PS-MPU-6000A-00) and <i>Register "
        "Map and Descriptions</i> (RM-MPU-6000A-00).",
        "PowerFeather, \"ESP32-S3 PowerFeather documentation.\" https://powerfeather.dev",
        "PlatformIO, \"PlatformIO documentation.\" https://docs.platformio.org",
        "FlySky, <i>FS-i6 Digital Proportional Radio Control System: User Manual</i>.",
    ]
    out = [*chapter("References")]
    for i, ref in enumerate(refs, 1):
        out.append(Paragraph(f"[{i}]&nbsp;&nbsp;{ref}", S["ref"]))
    return out


# ----------------------------------------------------------------------------------------------------------------
#  Appendices
# ----------------------------------------------------------------------------------------------------------------
def appendix_wiring():
    return [
        *chapter("Appendix A  Hardware Wiring"),
        table("pins", "Pin assignment of the ESP32-S3 PowerFeather.", [
            ["Signal", "GPIO", "Board label", "Connected to"],
            ["MPU6050 SDA / SCL", "35 / 36", "SDA / SCL", "GY-521 SDA / SCL"],
            ["Receiver CH1 (roll)", "39", "SCK", "receiver CH1 signal"],
            ["Receiver CH2 (pitch)", "40", "MO", "receiver CH2 signal"],
            ["Receiver CH3 (throttle)", "41", "MI", "receiver CH3 signal"],
            ["Receiver CH4 (yaw)", "42", "RX", "receiver CH4 signal"],
            ["Left / right tilt servo", "15 / 16", "D5 / D6", "servo signal"],
            ["Left / right ESC", "6 / 9", "D8 / A1", "ESC signal"],
            ["Status LED", "46", "on board", "-"],
        ], [44, 22, 32, 62]),
        p("The servos and the receiver are supplied by a separate 5-6 V regulator rated for at least 5 A, with a "
          "470-1000 µF capacitor across the servo supply. All grounds are connected. The firmware enables the "
          "3V3 rail (GPIO 4), which powers the inertial sensor, during start-up. The sensor is mounted near the "
          "centre of the frame on soft foam, with its X axis towards the nose and its Y axis towards the left "
          "rotor; other orientations are configured in include/Config.hpp."),
    ]


def appendix_setup():
    return [
        *chapter("Appendix B  Setup and Tuning Procedure"),
        p("All steps up to the first hover are carried out with the propellers removed. Each step verifies one "
          "item, so that a fault is found where it occurs. The detailed procedure is maintained in "
          "docs/SETUP_AND_TUNING.md; the essential steps are:"),
        *bullets([
            "<b>Build and flash</b> with PlatformIO (" + mono("pio run -e powerfeather -t upload") + ") and open "
            "the console (" + mono("pio device monitor") + "). Keep the aircraft still while the gyroscope is "
            "calibrated and the stick centres are learned.",
            "<b>Radio.</b> Roll right, pitch forward and yaw right must read positive. Set the receiver failsafe "
            "on the transmitter so that a lost link produces an out-of-range throttle value, and verify it by "
            "switching the transmitter off.",
            "<b>Sensor direction and level.</b> Right side down must give positive roll and nose down positive "
            "pitch; then store the level attitude on a flat surface.",
            "<b>Servos.</b> Both rotors vertical at centre (trims). Tilting the nose down by hand must tilt both "
            "rotors back; turning the nose left must tilt the left rotor forward.",
            "<b>Motors.</b> Calibrate the ESCs once, verify opposite propeller directions and the sign of the roll "
            "correction.",
            "<b>First hover and tuning.</b> Tune one axis at a time: increase P until the axis begins to "
            "oscillate and reduce it to about 70 %; increase I until slow drift disappears; use D on roll only.",
        ]),
        p("The pitch axis is started from a plain PID (rate P 0.0030, I 0.0015, D 0, angle P 2.5). Gains can be "
          "changed live from the console or the web page and are stored with the save command."),
    ]


def appendix_console():
    return [
        *chapter("Appendix C  Console Commands"),
        table("console", "USB console commands. Commands that change parameters are accepted only when disarmed.", [
            ["Command", "Function"],
            ["h", "help"],
            ["s", "status line on or off (once per second)"],
            ["i / r / o / f", "stream IMU, radio, outputs or the full flight log (10 Hz)"],
            ["x", "stop all output"],
            ["g", "recalibrate the gyroscope (aircraft still)"],
            ["l", "store the current attitude as level"],
            ["trim l|r N", "servo centre trim in microseconds"],
            ["pid, pid r|p|y P I D", "show or set the rate gains"],
            ["ang r|p|y P", "self-levelling gain (roll, pitch) or heading-hold gain (yaw)"],
            ["lead S", "servo phase lead in seconds, 0 = off (until reboot)"],
            ["save / defaults", "store parameters / restore the configured defaults"],
            ["reboot", "restart the processor"],
        ], [44, 116]),
    ]


def appendix_problems():
    return [
        *chapter("Appendix D  Problems Encountered During Testing"),
        p("The following problems occurred while the firmware was brought up on the aircraft. Most of them first "
          "appeared to be tuning problems."),
        table("problems", "Problems encountered, their causes and the corrective measures.", [
            ["Symptom", "Cause", "Measure"],
            ["Console output almost absent", "USB transmit buffer of 64 bytes dropped longer lines.",
             "Output queued in a 2 KB buffer."],
            ["Attitude drifted, never settled", "Accelerometer read 1.42 g at rest and was rejected.",
             "Z offset measured at each start-up and removed."],
            ["Roll and pitch swapped", "Sensor mounted rotated by 90 deg.",
             "Axis mapping corrected; mirror images rejected at compile time."],
            ["Continuous yaw rotation", "Yaw stick rested at 1438 µs due to a transmitter trim.",
             "Stick centres learned at start-up; wider yaw deadband."],
            ["Servos slowly diverged", "Throttle movement nudged the yaw stick; yaw integrator wound up.",
             "Wider yaw deadband; yaw integrator shown in the flight log."],
            ["Both servos crept forward", "Level attitude never calibrated (−8 deg offset).",
             "Level calibration."],
            ["Fault state at start-up", "Aircraft moved during gyroscope calibration.",
             "Calibration repeats until the aircraft is still."],
            ["First upload always failed", "USB port re-enumerates when entering the bootloader.",
             "1200-baud reset before upload."],
            ["Lunge forward at take-off", "Pitch integrator wound up against the ground.",
             "Take-off detection (Section 4.5)."],
            ["Forward/back oscillation", "Slow servos; controller layers amplified fast servo starts.",
             "Plain pitch PID; low and balanced centre of gravity."],
        ], [42, 64, 54], font_size=8.6),
    ]


# ----------------------------------------------------------------------------------------------------------------
#  Build
# ----------------------------------------------------------------------------------------------------------------
def story():
    _equation_counter[0] = 0
    out = []
    out += title_page()
    out += abstract_page()
    out += contents_pages()
    for part in (ch_introduction, ch_background, ch_system, ch_design, ch_implementation, ch_results, ch_discussion,
                 ch_conclusion, references, appendix_wiring, appendix_setup, appendix_console, appendix_problems):
        out += part()
    return out


def build():
    doc = Report(OUTPUT, pagesize=A4, title=TITLE, author=AUTHOR, subject=SUBTITLE,
                 keywords="bicopter, tilt-rotor, attitude estimation, Mahony filter, PID, ESP32-S3")
    doc.multiBuild(story())
    print("wrote", os.path.relpath(OUTPUT, ROOT))


if __name__ == "__main__":
    build()
