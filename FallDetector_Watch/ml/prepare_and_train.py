#!/usr/bin/env python3
"""
prepare_and_train.py — Descarga SisFall, procesa datos, y entrena el modelo.

Ejecutar:
    python prepare_and_train.py

Hace todo automáticamente:
    1. Descarga el dataset SisFall (4500+ muestras de caídas y actividades)
    2. Convierte de 200Hz a 50Hz y de ADC a g
    3. Genera ventanas de 3 segundos (150 muestras)
    5. Combina con datos locales del reloj si existen
    6. Entrena CNN 1D más potente
    7. Genera include/fall_model_data.h para el ESP32
"""

import os
import sys
import glob
import zipfile
import shutil
import urllib.request
import numpy as np
import pandas as pd
from pathlib import Path
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix

os.environ["TF_CPP_MIN_LOG_LEVEL"] = "2"
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers

# ─── Rutas ───────────────────────────────────────────────────────────────────
BASE_DIR     = Path(__file__).parent.resolve()
DATASET_DIR  = BASE_DIR / "dataset"
SISFALL_DIR  = BASE_DIR / "sisfall_raw"
MODEL_DIR    = BASE_DIR / "model"
INCLUDE_DIR  = BASE_DIR.parent / "include"

SISFALL_ZIP  = BASE_DIR / "SisFall.zip"
SISFALL_URL  = "https://github.com/BIng2325/SisFall/releases/download/dataset/SisFall.zip"

# ─── Config del modelo ───────────────────────────────────────────────────────
WINDOW_SIZE    = 150    # 3 segundos @ 50Hz
NUM_FEATURES   = 4      # x, y, z, magnitude
BATCH_SIZE     = 32
EPOCHS         = 150
TEST_SPLIT     = 0.15
VAL_SPLIT      = 0.2
RANDOM_SEED    = 42

# ─── SisFall: conversión ─────────────────────────────────────────────────────
SISFALL_SAMPLE_RATE  = 200   # Hz
TARGET_SAMPLE_RATE   = 50    # Hz
DOWNSAMPLE_FACTOR    = SISFALL_SAMPLE_RATE // TARGET_SAMPLE_RATE  # 4
ADXL345_SCALE        = 0.00390625  # g/LSB para ±16g 13-bit




# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 1: Descargar SisFall
# ═══════════════════════════════════════════════════════════════════════════════
def download_sisfall():
    """Descarga SisFall.zip desde GitHub."""
    if SISFALL_DIR.exists() and any(SISFALL_DIR.rglob("*.txt")):
        count = len(list(SISFALL_DIR.rglob("*.txt")))
        print(f"  SisFall ya descargado ({count} archivos)")
        return True

    if SISFALL_ZIP.exists():
        print(f"  ZIP ya existe, extrayendo...")
    else:
        print(f"  Descargando SisFall (~200MB)...")
        print(f"  URL: {SISFALL_URL}")
        try:
            def report_progress(block_num, block_size, total_size):
                downloaded = block_num * block_size
                if total_size > 0:
                    pct = min(downloaded * 100 / total_size, 100)
                    mb = downloaded / (1024 * 1024)
                    total_mb = total_size / (1024 * 1024)
                    print(f"\r  [{pct:5.1f}%] {mb:.1f}/{total_mb:.1f} MB", end="", flush=True)

            urllib.request.urlretrieve(SISFALL_URL, str(SISFALL_ZIP), report_progress)
            print()
        except Exception as e:
            print(f"\n  [ERROR] No se pudo descargar: {e}")
            return False

    # Extraer (puede haber ZIPs anidados)
    print(f"  Extrayendo ZIP...")
    SISFALL_DIR.mkdir(parents=True, exist_ok=True)
    try:
        with zipfile.ZipFile(str(SISFALL_ZIP), 'r') as zf:
            zf.extractall(str(SISFALL_DIR))

        # Buscar ZIPs internos y extraerlos también
        for inner_zip in SISFALL_DIR.rglob("*.zip"):
            print(f"  Extrayendo ZIP interno: {inner_zip.name}")
            with zipfile.ZipFile(str(inner_zip), 'r') as zf2:
                zf2.extractall(str(SISFALL_DIR))

        print(f"  Extraído en {SISFALL_DIR}")
        return True
    except Exception as e:
        print(f"  [ERROR] Error extrayendo: {e}")
        return False


# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 2: Parsear archivos SisFall
# ═══════════════════════════════════════════════════════════════════════════════
def parse_sisfall_file(filepath):
    """
    Lee un archivo SisFall y devuelve aceleración en g a 50Hz.
    Formato real: "  17,-179, -99, -18,-504,-352,  76,-697,-279;"
    Columnas 0-2 = ADXL345 (x,y,z). Usamos solo esas.
    """
    try:
        lines = []
        with open(filepath, 'r', errors='ignore') as f:
            for line in f:
                line = line.strip().rstrip(';').strip()
                if not line:
                    continue
                parts = [p.strip() for p in line.split(',')]
                if len(parts) >= 3:
                    try:
                        vals = [float(parts[0]), float(parts[1]), float(parts[2])]
                        lines.append(vals)
                    except ValueError:
                        continue

        if len(lines) < WINDOW_SIZE * DOWNSAMPLE_FACTOR:
            return None

        data = np.array(lines, dtype=np.float64)

        # Convertir ADC a g
        data_g = data * ADXL345_SCALE

        # Downsample de 200Hz a 50Hz
        data_50hz = data_g[::DOWNSAMPLE_FACTOR]

        # Calcular magnitud
        mag = np.sqrt(np.sum(data_50hz ** 2, axis=1, keepdims=True))
        data_with_mag = np.hstack([data_50hz, mag])  # (N, 4)

        return data_with_mag.astype(np.float32)

    except Exception:
        return None


def is_fall_file(filename):
    """Determina si un archivo SisFall es caída o ADL."""
    name = os.path.basename(filename).upper()
    if name.startswith('F') or name.startswith('D'):
        return True
    if name.startswith('A'):
        return False
    return None


def extract_windows(data, window_size=WINDOW_SIZE, stride=None):
    """Extrae ventanas de tamaño fijo de una secuencia larga."""
    if stride is None:
        stride = window_size // 2
    windows = []
    for start in range(0, len(data) - window_size + 1, stride):
        window = data[start:start + window_size]
        windows.append(window)
    return windows


def has_freefall_pattern(data_window):
    """
    Verifica si una ventana tiene el patrón de caída libre → impacto.
    Esto se usa para filtrar ventanas de caída de mejor calidad.
    """
    mag = data_window[:, 3]  # magnitud
    # Buscar si hay al menos 2 muestras consecutivas con mag < 0.6g
    low_count = 0
    has_freefall = False
    for m in mag:
        if m < 0.6:
            low_count += 1
            if low_count >= 2:
                has_freefall = True
                break
        else:
            low_count = 0
    return has_freefall


def process_sisfall():
    """Procesa todos los archivos SisFall y genera ventanas."""
    print("\n  Procesando archivos SisFall...")

    all_files = list(SISFALL_DIR.rglob("*.txt"))
    if not all_files:
        all_files = list(SISFALL_DIR.rglob("*.*"))
        all_files = [f for f in all_files if f.suffix.lower() in ('.txt', '.csv')]

    print(f"  Encontrados {len(all_files)} archivos")

    fall_windows = []
    adl_windows = []
    errors = 0
    processed = 0

    for i, filepath in enumerate(all_files):
        if (i + 1) % 100 == 0 or i == len(all_files) - 1:
            print(f"\r  Procesando: {i+1}/{len(all_files)}  "
                  f"(caídas: {len(fall_windows)}, ADL: {len(adl_windows)})",
                  end="", flush=True)

        is_fall = is_fall_file(str(filepath))
        if is_fall is None:
            continue

        data = parse_sisfall_file(str(filepath))
        if data is None:
            errors += 1
            continue

        processed += 1

        if is_fall:
            # Para caídas: extraer ventanas centradas en el pico de impacto
            mag = data[:, 3]
            peak_idx = np.argmax(mag)

            # Ventana centrada en el pico
            start = max(0, peak_idx - WINDOW_SIZE // 2)
            end = start + WINDOW_SIZE
            if end > len(data):
                end = len(data)
                start = max(0, end - WINDOW_SIZE)

            if end - start == WINDOW_SIZE:
                fall_windows.append(data[start:end])

            # Ventanas desplazadas para más variedad
            for shift in [-WINDOW_SIZE // 4, WINDOW_SIZE // 4, -WINDOW_SIZE // 3]:
                s2 = max(0, start + shift)
                e2 = s2 + WINDOW_SIZE
                if e2 <= len(data):
                    fall_windows.append(data[s2:e2])
        else:
            # Para ADL: extraer múltiples ventanas
            windows = extract_windows(data, stride=WINDOW_SIZE // 2)
            adl_windows.extend(windows)

    print(f"\n\n  Resultado SisFall:")
    print(f"    Archivos procesados: {processed}")
    print(f"    Errores/ignorados:   {errors}")
    print(f"    Ventanas de caída:   {len(fall_windows)}")
    print(f"    Ventanas ADL:        {len(adl_windows)}")

    return fall_windows, adl_windows


# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 3: Cargar datos locales del reloj (si existen)
# ═══════════════════════════════════════════════════════════════════════════════
def load_local_dataset():
    """Carga CSVs locales del dataset del reloj."""
    DATASET_DIR.mkdir(parents=True, exist_ok=True)
    fall_files = list(DATASET_DIR.glob("fall_*.csv"))
    nofall_files = list(DATASET_DIR.glob("nofall_*.csv"))

    if not fall_files and not nofall_files:
        print("  No hay datos locales del reloj")
        return [], []

    print(f"  Datos locales: {len(fall_files)} caídas, {len(nofall_files)} no-caídas")

    falls = []
    nofalls = []

    for fp in fall_files:
        w = load_csv_window(fp)
        if w is not None:
            falls.append(w)

    for fp in nofall_files:
        w = load_csv_window(fp)
        if w is not None:
            nofalls.append(w)

    return falls, nofalls


def load_csv_window(filepath):
    """Carga un CSV del reloj como ventana numpy."""
    try:
        df = pd.read_csv(filepath)
        features = df[["x", "y", "z", "magnitude"]].values

        if len(features) >= WINDOW_SIZE:
            features = features[:WINDOW_SIZE]
        else:
            padding = np.zeros((WINDOW_SIZE - len(features), NUM_FEATURES))
            features = np.vstack([features, padding])

        return features.astype(np.float32)
    except Exception:
        return None


# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 4: Generar negativos difíciles (hard negatives)
# ═══════════════════════════════════════════════════════════════════════════════
def generate_hard_negatives(adl_windows, num_extra=500):
    """
    Genera ventanas de movimientos bruscos que NO son caídas.
    Simula agitar la mano, gestos rápidos, etc.
    Esto enseña al modelo a distinguir impactos repetitivos de caídas reales.
    """
    hard_negs = []
    np.random.seed(RANDOM_SEED)

    for _ in range(num_extra):
        t = np.linspace(0, 3, WINDOW_SIZE)

        # Movimiento oscilatorio (agitar mano)
        freq = np.random.uniform(3, 12)  # 3-12 Hz
        amp = np.random.uniform(1.5, 5.0)  # amplitud alta en g
        phase = np.random.uniform(0, 2 * np.pi, 3)

        x = amp * np.sin(2 * np.pi * freq * t + phase[0]) + np.random.normal(0, 0.2, WINDOW_SIZE)
        y = amp * 0.5 * np.sin(2 * np.pi * freq * t + phase[1]) + np.random.normal(0, 0.2, WINDOW_SIZE)
        z = amp * 0.3 * np.sin(2 * np.pi * freq * t + phase[2]) + np.random.normal(0, 0.1, WINDOW_SIZE) + 1.0

        # Clave: la magnitud NUNCA baja de ~0.5g (no hay caída libre)
        mag = np.sqrt(x**2 + y**2 + z**2)

        window = np.stack([x, y, z, mag], axis=1).astype(np.float32)
        hard_negs.append(window)

    # También generar "golpes" puntuales sin caída libre
    for _ in range(num_extra // 2):
        # Base: movimiento normal ~1g
        x = np.random.normal(0, 0.3, WINDOW_SIZE)
        y = np.random.normal(0, 0.3, WINDOW_SIZE)
        z = np.ones(WINDOW_SIZE) + np.random.normal(0, 0.2, WINDOW_SIZE)

        # Añadir 1-3 picos de impacto aleatorios
        num_peaks = np.random.randint(1, 4)
        for _ in range(num_peaks):
            idx = np.random.randint(10, WINDOW_SIZE - 10)
            peak_amp = np.random.uniform(3, 8)
            width = np.random.randint(2, 6)
            for ax_data in [x, y, z]:
                ax_data[idx:idx+width] += peak_amp * np.random.choice([-1, 1])

        mag = np.sqrt(x**2 + y**2 + z**2)
        window = np.stack([x, y, z, mag], axis=1).astype(np.float32)
        hard_negs.append(window)

    print(f"  Hard negatives generados: {len(hard_negs)}")
    return hard_negs


# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 5: Combinar datasets
# ═══════════════════════════════════════════════════════════════════════════════
def combine_datasets(sisfall_falls, sisfall_adl, local_falls, local_nofalls, hard_negs):
    """Combina SisFall + datos locales + hard negatives en un dataset balanceado."""
    print("\n── Combinando datasets ──")

    all_falls = sisfall_falls + local_falls
    all_nofalls = sisfall_adl + local_nofalls + hard_negs

    print(f"  Total caídas:     {len(all_falls)} ({len(sisfall_falls)} SisFall + {len(local_falls)} local)")
    print(f"  Total no-caídas:  {len(all_nofalls)} ({len(sisfall_adl)} SisFall + {len(local_nofalls)} local + {len(hard_negs)} hard neg)")

    # Balancear: no más de 3x no-caídas vs caídas
    max_nofalls = len(all_falls) * 3
    if len(all_nofalls) > max_nofalls:
        print(f"  Balanceando: recortando no-caídas a {max_nofalls}")
        np.random.seed(RANDOM_SEED)
        indices = np.random.choice(len(all_nofalls), max_nofalls, replace=False)
        all_nofalls = [all_nofalls[i] for i in indices]

    # Construir arrays (datos en g directamente, sin normalización)
    X = np.array(all_falls + all_nofalls, dtype=np.float32)
    y = np.array([1.0] * len(all_falls) + [0.0] * len(all_nofalls), dtype=np.float32)

    print(f"  Dataset final: {len(X)} ventanas ({int(y.sum())} caídas, {int(len(y) - y.sum())} no-caídas)")
    return X, y


# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 6: Augmentation
# ═══════════════════════════════════════════════════════════════════════════════
def augment_data(X, y):
    """Data augmentation mejorada."""
    X_aug, y_aug = list(X), list(y)
    np.random.seed(RANDOM_SEED)

    for i in range(len(X)):
        # Ruido gaussiano (suave)
        noise = X[i] + np.random.normal(0, 0.03, X[i].shape).astype(np.float32)
        X_aug.append(noise)
        y_aug.append(y[i])

        # Escalar ±15%
        scale = np.random.uniform(0.85, 1.15)
        X_aug.append((X[i] * scale).astype(np.float32))
        y_aug.append(y[i])

        # Shift temporal ±15 muestras
        shift = np.random.randint(-15, 16)
        shifted = np.roll(X[i], shift, axis=0)
        X_aug.append(shifted.astype(np.float32))
        y_aug.append(y[i])

        # Invertir ejes X/Y (simular diferentes orientaciones de muñeca)
        flipped = X[i].copy()
        if np.random.random() > 0.5:
            flipped[:, 0] *= -1  # invertir X
        if np.random.random() > 0.5:
            flipped[:, 1] *= -1  # invertir Y
        # Recalcular magnitud tras flip (ya normalizado, no cambia mucho)
        X_aug.append(flipped.astype(np.float32))
        y_aug.append(y[i])

    return np.array(X_aug, dtype=np.float32), np.array(y_aug, dtype=np.float32)


# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 7: Modelo CNN más potente
# ═══════════════════════════════════════════════════════════════════════════════
def build_model():
    """CNN 1D mejorada con más capacidad y regularización."""
    model = keras.Sequential([
        layers.Input(shape=(WINDOW_SIZE, NUM_FEATURES)),

        # Bloque 1: captura patrones locales (20-140ms)
        layers.Conv1D(16, kernel_size=7, activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.MaxPooling1D(pool_size=2),    # 150 → 75

        # Bloque 2: patrones medios (100-500ms)
        layers.Conv1D(32, kernel_size=5, activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.MaxPooling1D(pool_size=2),    # 75 → 37

        # Bloque 3: patrones largos (caída libre → impacto)
        layers.Conv1D(32, kernel_size=5, activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.MaxPooling1D(pool_size=2),    # 37 → 18

        # Bloque 4: contexto global
        layers.Conv1D(16, kernel_size=3, activation="relu", padding="same"),
        layers.GlobalAveragePooling1D(),

        # Clasificador
        layers.Dropout(0.4),
        layers.Dense(16, activation="relu"),
        layers.Dropout(0.3),
        layers.Dense(1, activation="sigmoid"),
    ])
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=0.001),
        loss="binary_crossentropy",
        metrics=["accuracy"],
    )
    return model


# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 8: Convertir a TFLite + header C
# ═══════════════════════════════════════════════════════════════════════════════
def convert_to_tflite(model, X_train):
    MODEL_DIR.mkdir(parents=True, exist_ok=True)

    # Float32
    converter_float = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_float = converter_float.convert()
    float_path = MODEL_DIR / "fall_model_float.tflite"
    float_path.write_bytes(tflite_float)
    print(f"  Modelo float32: {len(tflite_float)} bytes")

    # INT8
    def representative_dataset():
        for i in range(min(300, len(X_train))):
            yield [X_train[i:i+1]]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_int8 = converter.convert()
    int8_path = MODEL_DIR / "fall_model.tflite"
    int8_path.write_bytes(tflite_int8)
    print(f"  Modelo INT8:    {len(tflite_int8)} bytes")

    return tflite_int8


def generate_c_header(tflite_model):
    INCLUDE_DIR.mkdir(parents=True, exist_ok=True)
    header_path = INCLUDE_DIR / "fall_model_data.h"

    with open(header_path, "w") as f:
        f.write("/*******************************************************************************\n")
        f.write(" * @file    fall_model_data.h\n")
        f.write(" * @brief   Modelo TFLite de detección de caídas (generado automáticamente)\n")
        f.write(f" * @details Tamaño: {len(tflite_model)} bytes | Cuantización: INT8\n")
        f.write(f" *          Input: ({WINDOW_SIZE}, {NUM_FEATURES}) | Output: (1,)\n")
        f.write(f" *          Datos en g directamente (sin normalización)\n")
        f.write(f" *          Entrenado con SisFall + hard negatives + datos locales\n")
        f.write(" * NO EDITAR — Generado por ml/prepare_and_train.py\n")
        f.write(" ******************************************************************************/\n")
        f.write("#pragma once\n\n")
        f.write("#include <cstdint>\n\n")
        f.write(f"static constexpr uint32_t FALL_MODEL_SIZE = {len(tflite_model)};\n\n")
        f.write(f"alignas(16) static const uint8_t fall_model_data[{len(tflite_model)}] = {{\n")

        for i in range(0, len(tflite_model), 12):
            chunk = tflite_model[i:i+12]
            hex_values = ", ".join(f"0x{b:02X}" for b in chunk)
            f.write(f"    {hex_values},\n")

        f.write("};\n")

    print(f"  Header C: {header_path}")


# ═══════════════════════════════════════════════════════════════════════════════
#  PASO 9: Evaluar modelo TFLite
# ═══════════════════════════════════════════════════════════════════════════════
def evaluate_tflite(X_test, y_test):
    int8_path = str(MODEL_DIR / "fall_model.tflite")
    interpreter = tf.lite.Interpreter(model_path=int8_path)
    interpreter.allocate_tensors()

    inp = interpreter.get_input_details()[0]
    out = interpreter.get_output_details()[0]

    in_scale  = inp["quantization_parameters"]["scales"][0]
    in_zero   = inp["quantization_parameters"]["zero_points"][0]
    out_scale = out["quantization_parameters"]["scales"][0]
    out_zero  = out["quantization_parameters"]["zero_points"][0]

    predictions = []
    for i in range(len(X_test)):
        input_data = (X_test[i:i+1] / in_scale + in_zero).astype(np.int8)
        interpreter.set_tensor(inp["index"], input_data)
        interpreter.invoke()
        output_data = interpreter.get_tensor(out["index"])
        prob = (output_data[0][0] - out_zero) * out_scale
        predictions.append(1 if prob > 0.5 else 0)

    predictions = np.array(predictions)
    print(f"\n  TFLite INT8 — Classification Report:")
    print(classification_report(y_test, predictions, target_names=["No caída", "Caída"]))
    cm = confusion_matrix(y_test, predictions)
    print(f"  Confusion Matrix:\n{cm}")

    acc = np.mean(predictions == y_test)
    return acc


# ═══════════════════════════════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════════════════════════════
def main():
    print("=" * 64)
    print("  FallDetector — Entrenamiento V2 (Normalizado + Hard Negatives)")
    print("  Dataset: SisFall + hard negatives + datos locales")
    print("=" * 64)

    # 1. Descargar SisFall
    print("\n── Paso 1: Descargar SisFall ──")
    if not download_sisfall():
        print("[ERROR] No se pudo descargar SisFall.")
        sys.exit(1)

    # 2. Procesar SisFall
    print("\n── Paso 2: Procesar SisFall ──")
    sisfall_falls, sisfall_adl = process_sisfall()

    if len(sisfall_falls) == 0:
        print("[ERROR] No se encontraron caídas en SisFall.")
        sys.exit(1)

    # 3. Cargar datos locales
    print("\n── Paso 3: Datos locales del reloj ──")
    local_falls, local_nofalls = load_local_dataset()

    # 4. Generar hard negatives (movimientos bruscos que NO son caídas)
    print("\n── Paso 4: Generar hard negatives ──")
    hard_negs = generate_hard_negatives(sisfall_adl, num_extra=len(sisfall_falls))

    # 5. Combinar
    X, y = combine_datasets(sisfall_falls, sisfall_adl, local_falls, local_nofalls, hard_negs)

    # 6. Split
    print("\n── Paso 5: Split train/test ──")
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=TEST_SPLIT, random_state=RANDOM_SEED, stratify=y
    )
    print(f"  Train: {len(X_train)} | Test: {len(X_test)}")

    # 7. Augmentation
    print("\n── Paso 6: Data Augmentation ──")
    X_train_aug, y_train_aug = augment_data(X_train, y_train)
    print(f"  Augmented: {len(X_train_aug)} ventanas")

    # 8. Modelo
    print("\n── Paso 7: Construir modelo ──")
    model = build_model()
    model.summary()

    # 9. Entrenar
    print("\n── Paso 8: Entrenamiento ──")
    callbacks = [
        keras.callbacks.EarlyStopping(
            monitor="val_loss", patience=20, restore_best_weights=True
        ),
        keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss", factor=0.5, patience=7, min_lr=1e-6
        ),
    ]

    history = model.fit(
        X_train_aug, y_train_aug,
        validation_split=VAL_SPLIT,
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        callbacks=callbacks,
        verbose=1,
    )

    # 10. Evaluar float
    print("\n── Paso 9: Evaluación Float32 ──")
    y_pred_prob = model.predict(X_test).flatten()
    y_pred = (y_pred_prob > 0.5).astype(int)
    print(classification_report(y_test, y_pred, target_names=["No caída", "Caída"]))

    # 11. Convertir a TFLite
    print("\n── Paso 10: Conversión TFLite ──")
    tflite_int8 = convert_to_tflite(model, X_train)

    # 12. Header C
    print("\n── Paso 11: Header C para ESP32 ──")
    generate_c_header(tflite_int8)

    # 13. Evaluar TFLite INT8
    print("\n── Paso 12: Evaluación TFLite INT8 ──")
    acc = evaluate_tflite(X_test, y_test)

    # 14. Reporte
    report_path = MODEL_DIR / "training_report.txt"
    best_val_acc = max(history.history['val_accuracy'])
    best_val_loss = min(history.history['val_loss'])
    with open(report_path, "w") as f:
        f.write(f"Fecha: {pd.Timestamp.now()}\n")
        f.write(f"Dataset: SisFall + hard negatives + datos locales\n")
        f.write(f"  SisFall caídas:     {len(sisfall_falls)}\n")
        f.write(f"  SisFall ADL:        {len(sisfall_adl)}\n")
        f.write(f"  Hard negatives:     {len(hard_negs)}\n")
        f.write(f"  Local caídas:       {len(local_falls)}\n")
        f.write(f"  Local no-caídas:    {len(local_nofalls)}\n")
        f.write(f"Total ventanas:       {len(X)}\n")
        f.write(f"Augmented train:      {len(X_train_aug)}\n")
        f.write(f"Test:                 {len(X_test)}\n")
        f.write(f"Epochs:               {len(history.history['loss'])}\n")
        f.write(f"Best val_loss:        {best_val_loss:.4f}\n")
        f.write(f"Best val_accuracy:    {best_val_acc:.4f}\n")
        f.write(f"TFLite INT8 accuracy: {acc:.4f}\n")
        f.write(f"Modelo INT8 bytes:    {len(tflite_int8)}\n")
        f.write(f"NOTA: Datos en g directamente, sin normalización\n")

    print(f"\n  Reporte: {report_path}")

    print("\n" + "=" * 64)
    print("  COMPLETADO!")
    print(f"  Accuracy: {acc*100:.1f}%")
    print(f"  Modelo:   {len(tflite_int8)} bytes")
    print(f"  Header:   include/fall_model_data.h")
    print()
    print("  Datos en g directamente (sin normalización)")
    print("=" * 64)


if __name__ == "__main__":
    main()
