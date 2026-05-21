import sys
import json
import socket

from PySide6.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QLineEdit, QFrame, QSizePolicy
)
from PySide6.QtGui import QColor, QPainter, QBrush, QPen, QFont, QFontDatabase
from PySide6.QtCore import Qt, QSize

LAMP_PORT = 4210


# ─── UDP send ─────────────────────────────────────────────────────────────────

def send_udp(ip: str, payload: dict) -> str:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.settimeout(1.0)
            s.sendto(json.dumps(payload).encode(), (ip, LAMP_PORT))
            try:
                ack, _ = s.recvfrom(256)
                return ack.decode()
            except socket.timeout:
                return "no ack"
    except Exception as e:
        return f"error: {e}"


# ─── Color swatch button ──────────────────────────────────────────────────────

class ColorSwatch(QWidget):
    """A rounded rectangle that shows a color and opens a color picker on click."""

    def __init__(self, color: QColor, parent=None):
        super().__init__(parent)
        self._color = color
        self.setFixedSize(64, 64)
        self.setCursor(Qt.PointingHandCursor)

    @property
    def color(self) -> QColor:
        return self._color

    def set_color(self, c: QColor):
        self._color = c
        self.update()

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        pen = QPen(QColor("#3a3a3a"), 2)
        p.setPen(pen)
        p.setBrush(QBrush(self._color))
        p.drawRoundedRect(self.rect().adjusted(1, 1, -1, -1), 12, 12)

    def mousePressEvent(self, event):
        from PySide6.QtWidgets import QColorDialog
        c = QColorDialog.getColor(self._color, self, "Pick color")
        if c.isValid():
            self.set_color(c)


# ─── Status bar ───────────────────────────────────────────────────────────────

class StatusBar(QLabel):
    def set_ok(self, msg: str):
        self.setStyleSheet("color: #6fcf97; font-size: 11px; padding: 4px 0;")
        self.setText(f"✓  {msg}")

    def set_err(self, msg: str):
        self.setStyleSheet("color: #eb5757; font-size: 11px; padding: 4px 0;")
        self.setText(f"✗  {msg}")

    def set_neutral(self, msg: str):
        self.setStyleSheet("color: #888; font-size: 11px; padding: 4px 0;")
        self.setText(msg)


# ─── Section label ────────────────────────────────────────────────────────────

def section_label(text: str) -> QLabel:
    lbl = QLabel(text)
    lbl.setStyleSheet(
        "color: #666; font-size: 10px; letter-spacing: 2px; text-transform: uppercase;"
    )
    return lbl


# ─── Divider ─────────────────────────────────────────────────────────────────

def divider() -> QFrame:
    f = QFrame()
    f.setFrameShape(QFrame.HLine)
    f.setStyleSheet("color: #2a2a2a;")
    return f


# ─── Main window ─────────────────────────────────────────────────────────────

class LampController(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Lamp Controller")
        self.setFixedWidth(340)
        self.setStyleSheet("""
            QWidget {
                background-color: #111111;
                color: #f0ede6;
                font-family: 'Courier New', monospace;
            }
            QLineEdit {
                background: #1c1c1c;
                border: 1px solid #2e2e2e;
                border-radius: 8px;
                color: #f0ede6;
                padding: 8px 12px;
                font-family: 'Courier New', monospace;
                font-size: 13px;
            }
            QLineEdit:focus {
                border: 1px solid #f5a623;
            }
            QPushButton {
                background: #1c1c1c;
                border: 1px solid #2e2e2e;
                border-radius: 8px;
                color: #f0ede6;
                font-family: 'Courier New', monospace;
                font-size: 12px;
                letter-spacing: 1px;
                padding: 10px 0;
            }
            QPushButton:hover {
                border: 1px solid #f5a623;
                color: #f5a623;
            }
            QPushButton:pressed {
                background: #f5a623;
                color: #111;
                border: 1px solid #f5a623;
            }
            QPushButton#btnOn {
                border: 1px solid #6fcf97;
                color: #6fcf97;
            }
            QPushButton#btnOn:hover {
                background: #6fcf97;
                color: #111;
            }
            QPushButton#btnOff {
                border: 1px solid #eb5757;
                color: #eb5757;
            }
            QPushButton#btnOff:hover {
                background: #eb5757;
                color: #111;
            }
        """)

        root = QVBoxLayout(self)
        root.setContentsMargins(24, 24, 24, 24)
        root.setSpacing(16)

        # ── Title ──────────────────────────────────────────────────────────
        title = QLabel("◉  LAMP CTRL")
        title.setStyleSheet(
            "font-size: 18px; letter-spacing: 4px; color: #f5a623; font-weight: bold;"
        )
        root.addWidget(title)
        root.addWidget(divider())

        # ── IP ─────────────────────────────────────────────────────────────
        root.addWidget(section_label("device ip"))
        self.ip_field = QLineEdit("192.168.1.50")
        self.ip_field.setPlaceholderText("192.168.1.x")
        root.addWidget(self.ip_field)
        root.addWidget(divider())

        # ── On / Off buttons ───────────────────────────────────────────────
        root.addWidget(section_label("power"))
        row_power = QHBoxLayout()
        row_power.setSpacing(10)
        self.btn_on = QPushButton("ON")
        self.btn_on.setObjectName("btnOn")
        self.btn_off = QPushButton("OFF")
        self.btn_off.setObjectName("btnOff")
        self.btn_on.clicked.connect(self.send_on)
        self.btn_off.clicked.connect(self.send_off)
        row_power.addWidget(self.btn_on)
        row_power.addWidget(self.btn_off)
        root.addLayout(row_power)
        root.addWidget(divider())

        # ── ON color ───────────────────────────────────────────────────────
        root.addWidget(section_label("on color"))
        row_on = QHBoxLayout()
        row_on.setSpacing(14)
        self.swatch_on = ColorSwatch(QColor(250, 200, 200))
        btn_send_on = QPushButton("SEND ON COLOR")
        btn_send_on.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        btn_send_on.clicked.connect(self.send_on_color)
        row_on.addWidget(self.swatch_on)
        row_on.addWidget(btn_send_on)
        root.addLayout(row_on)
        root.addWidget(divider())

        # ── OFF color ──────────────────────────────────────────────────────
        root.addWidget(section_label("off color"))
        row_off = QHBoxLayout()
        row_off.setSpacing(14)
        self.swatch_off = ColorSwatch(QColor(0, 0, 20))
        btn_send_off = QPushButton("SEND OFF COLOR")
        btn_send_off.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        btn_send_off.clicked.connect(self.send_off_color)
        row_off.addWidget(self.swatch_off)
        row_off.addWidget(btn_send_off)
        root.addLayout(row_off)
        root.addWidget(divider())

        # ── Status ─────────────────────────────────────────────────────────
        self.status = StatusBar("—")
        self.status.set_neutral("ready")
        root.addWidget(self.status)

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _ip(self) -> str:
        return self.ip_field.text().strip()

    def _dispatch(self, payload: dict):
        ip = self._ip()
        if not ip:
            self.status.set_err("no IP address set")
            return
        ack = send_udp(ip, payload)
        if '"ok":true' in ack:
            self.status.set_ok(f"{payload['cmd']} → {ack}")
        else:
            self.status.set_err(f"{payload['cmd']} → {ack}")

    # ── Slots ─────────────────────────────────────────────────────────────────

    def send_on(self):
        self._dispatch({"cmd": "on"})

    def send_off(self):
        self._dispatch({"cmd": "off"})

    def send_on_color(self):
        c = self.swatch_on.color
        self._dispatch({
            "cmd": "set_on_color",
            "r": c.red(), "g": c.green(), "b": c.blue()
        })

    def send_off_color(self):
        c = self.swatch_off.color
        self._dispatch({
            "cmd": "set_off_color",
            "r": c.red(), "g": c.green(), "b": c.blue()
        })


# ─── Entry point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = LampController()
    win.show()
    sys.exit(app.exec())