#!/usr/bin/env python3
"""
collect_data.py — Descarga datos del T-Watch V3 y los guarda como CSVs.

Uso:
    python collect_data.py --port COM5 download    Descargar datos del reloj
    python collect_data.py --port COM5 status      Ver estado
    python collect_data.py --port COM5 clear       Borrar datos del reloj

Flujo:
    1. Sube firmware data-collector al reloj
    2. Lleva el reloj puesto. Botón lateral = marcar caída
    3. Conecta al PC: python collect_data.py --port COM5 download
    4. Entrena: python train_model.py
"""

import argparse
import csv
import os
import sys
import time
import math
from datetime import datetime

import serial

WINDOW_SIZE = 150
DATASET_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dataset")


def ensure_dataset_dir():
    os.makedirs(DATASET_DIR, exist_ok=True)


def save_window_csv(samples, label, session_id, index):
    """Guarda una ventana como CSV con valores en g."""
    filename = f"{label}_{session_id}_{index:04d}.csv"
    filepath = os.path.join(DATASET_DIR, filename)

    with open(filepath, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp_ms", "x", "y", "z", "magnitude"])
        for i, s in enumerate(samples):
            writer.writerow([i * 20, f"{s[0]:.4f}", f"{s[1]:.4f}", f"{s[2]:.4f}", f"{s[3]:.4f}"])

    return filepath


def read_line(ser, timeout=10, expect_prefix=None):
    """Lee una línea del Serial filtrando debug del ESP32."""
    ser.timeout = timeout
    deadline = time.time() + timeout
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", errors="ignore").strip()
        if not line:
            continue
        if line.startswith("[") and "]" in line:
            continue
        if expect_prefix and not line.startswith(expect_prefix):
            continue
        return line
    return ""


def cmd_download(args):
    """Descarga datos del reloj y guarda como CSVs."""
    ensure_dataset_dir()
    session_id = datetime.now().strftime("%Y%m%d_%H%M%S")

    print("=" * 60)
    print("  FallDetector — Descarga de datos")
    print("=" * 60)

    print(f"\n  Conectando a {args.port}...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=5)
    except serial.SerialException as e:
        print(f"  [ERROR] No se pudo conectar: {e}")
        sys.exit(1)

    time.sleep(1.5)
    ser.reset_input_buffer()

    print("  Solicitando descarga...\n")
    ser.write(b"DOWNLOAD\n")

    # BEGIN,<num_records>,<lsb_per_g>
    line = read_line(ser, timeout=10, expect_prefix="BEGIN,")
    if not line.startswith("BEGIN,"):
        print(f"  [ERROR] Respuesta inesperada: '{line}'")
        ser.close()
        sys.exit(1)

    parts = line.split(",")
    total_records = int(parts[1])
    lsb_per_g = int(parts[2]) if len(parts) > 2 else 512

    print(f"  Registros: {total_records} | Escala: {lsb_per_g} LSB/g")

    if total_records == 0:
        print("  No hay datos en el reloj.")
        read_line(ser)
        ser.close()
        return

    scale = 1.0 / lsb_per_g
    fall_count = 0
    nofall_count = 0

    for r in range(total_records):
        # R,<id>,<label>
        line = read_line(ser, timeout=10, expect_prefix="R,")
        if not line.startswith("R,"):
            continue

        label_num = int(line.split(",")[2])
        label = "fall" if label_num == 1 else "nofall"

        # 150 líneas: D,x,y,z (raw int16)
        samples = []
        for s in range(WINDOW_SIZE):
            line = read_line(ser, timeout=5, expect_prefix="D,")
            if line.startswith("D,"):
                vals = line.split(",")
                try:
                    rx, ry, rz = float(vals[1]), float(vals[2]), float(vals[3])
                    gx, gy, gz = rx * scale, ry * scale, rz * scale
                    mag = math.sqrt(gx*gx + gy*gy + gz*gz)
                    samples.append([gx, gy, gz, mag])
                except (ValueError, IndexError):
                    samples.append([0.0, 0.0, 0.0, 0.0])
            else:
                samples.append([0.0, 0.0, 0.0, 0.0])

        save_window_csv(samples, label, session_id, r)

        if label == "fall":
            fall_count += 1
        else:
            nofall_count += 1

        pct = (r + 1) * 100 // total_records
        print(f"\r  [{pct:3d}%] {r+1}/{total_records}  "
              f"caidas:{fall_count}  normal:{nofall_count}", end="", flush=True)

    read_line(ser, timeout=5)
    ser.close()

    print(f"\n\n  Descarga completada!")
    print(f"  Caidas: {fall_count}  |  Normal: {nofall_count}")
    print(f"  Datos en: {DATASET_DIR}")

    total_falls = len([f for f in os.listdir(DATASET_DIR) if f.startswith("fall_")])
    total_nofalls = len([f for f in os.listdir(DATASET_DIR) if f.startswith("nofall_")])
    print(f"\n  Dataset acumulado: {total_falls} caidas / {total_nofalls} normal")

    if total_falls >= 30:
        print(f"\n  Listo para entrenar: python train_model.py")
    else:
        print(f"\n  Necesitas al menos 30 caidas. Faltan {max(0, 30 - total_falls)}.")


def cmd_status(args):
    try:
        ser = serial.Serial(args.port, args.baud, timeout=5)
    except serial.SerialException as e:
        print(f"  [ERROR] {e}")
        sys.exit(1)

    time.sleep(1.5)
    ser.reset_input_buffer()
    ser.write(b"STATUS\n")

    line = read_line(ser, timeout=5, expect_prefix="STATUS,")
    ser.close()

    if line.startswith("STATUS,"):
        parts = line.split(",")
        print(f"\n  Ventanas: {parts[1]} (caidas:{parts[2]} normal:{parts[3]})")
        print(f"  SPIFFS: {int(parts[4])//1024}KB / {int(parts[5])//1024}KB")
    else:
        print(f"  Sin respuesta. Verifica que el firmware data-collector esta en el reloj.")


def cmd_clear(args):
    try:
        ser = serial.Serial(args.port, args.baud, timeout=5)
    except serial.SerialException as e:
        print(f"  [ERROR] {e}")
        sys.exit(1)

    time.sleep(1.5)
    ser.reset_input_buffer()
    ser.write(b"CLEAR\n")

    line = read_line(ser, timeout=5, expect_prefix="S,")
    ser.close()

    if "CLEARED" in line:
        print("  Datos borrados del reloj.")
    else:
        print(f"  Respuesta: {line}")


def main():
    parser = argparse.ArgumentParser(description="Descarga datos del T-Watch V3")
    parser.add_argument("command", choices=["download", "status", "clear"])
    parser.add_argument("--port", required=True, help="Puerto Serial (ej: COM5)")
    parser.add_argument("--baud", type=int, default=115200)

    args = parser.parse_args()

    if args.command == "download":
        cmd_download(args)
    elif args.command == "status":
        cmd_status(args)
    elif args.command == "clear":
        cmd_clear(args)


if __name__ == "__main__":
    main()
