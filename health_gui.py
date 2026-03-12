import sys
import serial
import serial.tools.list_ports
import pyqtgraph as pg
from PyQt5.QtWidgets import *
from PyQt5.QtCore import *
from collections import deque

class HealthMonitorGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Health Monitor")
        self.setGeometry(100, 100, 1000, 700)
        
        self.max_history = 500
        self.ecg_data = deque([0]*500, maxlen=500)
        self.hr_data = deque([0]*500, maxlen=500)
        self.spo2_data = deque([0]*500, maxlen=500)
        self.temp_data = deque([0]*500, maxlen=500)
        
        self.serial_port = None
        self.setup_ui()
        
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_data)
        self.timer.start(100)
        
    def setup_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        
        control = QHBoxLayout()
        
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(200)
        self.refresh_ports()
        
        refresh_btn = QPushButton("Refresh")
        refresh_btn.clicked.connect(self.refresh_ports)
        
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.connect_esp32)
        self.connect_btn.setStyleSheet("background: #4CAF50; color: white;")
        
        control.addWidget(QLabel("COM Port:"))
        control.addWidget(self.port_combo)
        control.addWidget(refresh_btn)
        control.addWidget(self.connect_btn)
        control.addStretch()
        
        self.ecg_plot = pg.PlotWidget(title="ECG")
        self.ecg_plot.setYRange(0, 4095)
        self.ecg_curve = self.ecg_plot.plot(pen='y')
        
        self.hr_plot = pg.PlotWidget(title="Heart Rate")
        self.hr_plot.setYRange(40, 200)
        self.hr_curve = self.hr_plot.plot(pen='r')
        
        self.spo2_plot = pg.PlotWidget(title="SpO2")
        self.spo2_plot.setYRange(70, 100)
        self.spo2_curve = self.spo2_plot.plot(pen='g')
        
        self.temp_plot = pg.PlotWidget(title="Temperature")
        self.temp_plot.setYRange(20, 40)
        self.temp_curve = self.temp_plot.plot(pen='b')
        
        values = QHBoxLayout()
        self.hr_label = QLabel("HR: -- BPM")
        self.hr_label.setStyleSheet("font-size: 20px; color: red;")
        self.spo2_label = QLabel("SpO2: --%")
        self.spo2_label.setStyleSheet("font-size: 20px; color: green;")
        self.temp_label = QLabel("Temp: --°C")
        self.temp_label.setStyleSheet("font-size: 20px; color: blue;")
        
        values.addWidget(self.hr_label)
        values.addWidget(self.spo2_label)
        values.addWidget(self.temp_label)
        values.addStretch()
        
        layout.addLayout(control)
        layout.addWidget(self.ecg_plot)
        layout.addWidget(self.hr_plot)
        layout.addWidget(self.spo2_plot)
        layout.addWidget(self.temp_plot)
        layout.addLayout(values)
        
        self.status = QStatusBar()
        self.setStatusBar(self.status)
        self.status.showMessage("Not connected")
        
    def refresh_ports(self):
        self.port_combo.clear()
        for port in serial.tools.list_ports.comports():
            self.port_combo.addItem(port.device)
    
    def connect_esp32(self):
        port = self.port_combo.currentText()
        if not port:
            return
            
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            self.connect_btn.setEnabled(False)
            self.connect_btn.setText("Connected")
            self.status.showMessage(f"Connected to {port}")
        except:
            self.status.showMessage(f"Failed to connect to {port}")
    
    def update_data(self):
        if not self.serial_port or not self.serial_port.is_open:
            return
            
        try:
            if self.serial_port.in_waiting:
                line = self.serial_port.readline().decode().strip()
                if line and ',' in line:
                    parts = line.split(',')
                    if len(parts) >= 5:
                        hr = float(parts[0])
                        spo2 = float(parts[1])
                        ecg = float(parts[2])
                        temp = float(parts[3])
                        
                        self.hr_data.append(hr)
                        self.spo2_data.append(spo2)
                        self.ecg_data.append(ecg)
                        self.temp_data.append(temp)
                        
                        self.hr_curve.setData(list(self.hr_data))
                        self.spo2_curve.setData(list(self.spo2_data))
                        self.ecg_curve.setData(list(self.ecg_data))
                        self.temp_curve.setData(list(self.temp_data))
                        
                        self.hr_label.setText(f"HR: {hr:.0f} BPM")
                        self.spo2_label.setText(f"SpO2: {spo2:.0f}%")
                        self.temp_label.setText(f"Temp: {temp:.1f}°C")
        except:
            pass

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = HealthMonitorGUI()
    window.show()
    sys.exit(app.exec_())
