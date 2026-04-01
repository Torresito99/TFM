#!/usr/bin/env python3
"""
train_model.py — Entrena el modelo de detección de caídas con TUS datos.

Uso:
    python train_model.py

Requisitos:
    - CSVs en ml/dataset/ generados por collect_data.py
    - Minimo ~30 caidas y ~100 no-caidas
"""

import os
import sys
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
BASE_DIR    = Path(__file__).parent.resolve()
DATASET_DIR = BASE_DIR / "dataset"
MODEL_DIR   = BASE_DIR / "model"
INCLUDE_DIR = BASE_DIR.parent / "include"

# ─── Config ──────────────────────────────────────────────────────────────────
WINDOW_SIZE  = 150   # 3s @ 50Hz
NUM_FEATURES = 4     # x, y, z, magnitude (en g)
BATCH_SIZE   = 32
EPOCHS       = 150
RANDOM_SEED  = 42


# ═══════════════════════════════════════════════════════════════════════════════
#  1. Cargar datos
# ═══════════════════════════════════════════════════════════════════════════════
def load_dataset():
    """Carga todos los CSVs del dataset."""
    fall_files = sorted(DATASET_DIR.glob("fall_*.csv"))
    nofall_files = sorted(DATASET_DIR.glob("nofall_*.csv"))

    print(f"  Archivos: {len(fall_files)} caidas, {len(nofall_files)} no-caidas")

    if len(fall_files) < 10:
        print(f"\n  [ERROR] Muy pocas caidas ({len(fall_files)}). Necesitas al menos 30.")
        print(f"  Recoge mas datos con el reloj y descarga con collect_data.py")
        sys.exit(1)

    windows = []
    labels = []

    for fp in fall_files:
        w = load_csv(fp)
        if w is not None:
            windows.append(w)
            labels.append(1.0)

    for fp in nofall_files:
        w = load_csv(fp)
        if w is not None:
            windows.append(w)
            labels.append(0.0)

    X = np.array(windows, dtype=np.float32)
    y = np.array(labels, dtype=np.float32)

    print(f"  Cargadas: {int(y.sum())} caidas, {int(len(y) - y.sum())} no-caidas")
    return X, y


def load_csv(filepath):
    """Carga un CSV como ventana numpy (150, 4)."""
    try:
        df = pd.read_csv(filepath)
        features = df[["x", "y", "z", "magnitude"]].values.astype(np.float32)

        if len(features) >= WINDOW_SIZE:
            return features[:WINDOW_SIZE]
        else:
            padding = np.zeros((WINDOW_SIZE - len(features), NUM_FEATURES), dtype=np.float32)
            return np.vstack([features, padding])
    except Exception as e:
        print(f"  [WARN] Error leyendo {filepath.name}: {e}")
        return None


# ═══════════════════════════════════════════════════════════════════════════════
#  2. Data Augmentation
# ═══════════════════════════════════════════════════════════════════════════════
def augment_data(X, y):
    """Aumenta datos para mejorar generalizacion."""
    X_aug, y_aug = list(X), list(y)
    np.random.seed(RANDOM_SEED)

    for i in range(len(X)):
        # Ruido gaussiano suave
        noise = X[i] + np.random.normal(0, 0.05, X[i].shape).astype(np.float32)
        noise[:, 3] = np.sqrt(noise[:, 0]**2 + noise[:, 1]**2 + noise[:, 2]**2)
        X_aug.append(noise)
        y_aug.append(y[i])

        # Escalar +/-10%
        scale = np.random.uniform(0.9, 1.1)
        scaled = (X[i] * scale).astype(np.float32)
        X_aug.append(scaled)
        y_aug.append(y[i])

        # Shift temporal +/-10 muestras
        shift = np.random.randint(-10, 11)
        shifted = np.roll(X[i], shift, axis=0).astype(np.float32)
        X_aug.append(shifted)
        y_aug.append(y[i])

    return np.array(X_aug, dtype=np.float32), np.array(y_aug, dtype=np.float32)


# ═══════════════════════════════════════════════════════════════════════════════
#  3. Modelo CNN
# ═══════════════════════════════════════════════════════════════════════════════
def build_model():
    """CNN 1D para deteccion de caidas desde datos de muneca."""
    model = keras.Sequential([
        layers.Input(shape=(WINDOW_SIZE, NUM_FEATURES)),

        layers.Conv1D(16, kernel_size=7, activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.MaxPooling1D(2),

        layers.Conv1D(32, kernel_size=5, activation="relu", padding="same"),
        layers.BatchNormalization(),
        layers.MaxPooling1D(2),

        layers.Conv1D(16, kernel_size=3, activation="relu", padding="same"),
        layers.GlobalAveragePooling1D(),

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
#  4. Convertir a TFLite INT8
# ═══════════════════════════════════════════════════════════════════════════════
def convert_to_tflite(model, X_cal):
    MODEL_DIR.mkdir(parents=True, exist_ok=True)

    # Float32
    converter_f = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_f = converter_f.convert()
    (MODEL_DIR / "fall_model_float.tflite").write_bytes(tflite_f)
    print(f"  Float32: {len(tflite_f)} bytes")

    # INT8
    def representative_dataset():
        for i in range(min(200, len(X_cal))):
            yield [X_cal[i:i+1]]

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_int8 = converter.convert()
    (MODEL_DIR / "fall_model.tflite").write_bytes(tflite_int8)
    print(f"  INT8:    {len(tflite_int8)} bytes")

    return tflite_int8


def generate_c_header(tflite_model):
    INCLUDE_DIR.mkdir(parents=True, exist_ok=True)
    header_path = INCLUDE_DIR / "fall_model_data.h"

    with open(header_path, "w") as f:
        f.write("// Modelo TFLite generado por ml/train_model.py\n")
        f.write("// NO EDITAR\n")
        f.write(f"// Tamano: {len(tflite_model)} bytes | Input: ({WINDOW_SIZE}, {NUM_FEATURES}) en g\n")
        f.write("#pragma once\n\n#include <cstdint>\n\n")
        f.write(f"static constexpr uint32_t FALL_MODEL_SIZE = {len(tflite_model)};\n\n")
        f.write(f"alignas(16) static const uint8_t fall_model_data[{len(tflite_model)}] = {{\n")

        for i in range(0, len(tflite_model), 12):
            chunk = tflite_model[i:i+12]
            f.write("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",\n")

        f.write("};\n")

    print(f"  Header: {header_path}")


# ═══════════════════════════════════════════════════════════════════════════════
#  5. Evaluar TFLite
# ═══════════════════════════════════════════════════════════════════════════════
def evaluate_tflite(X_test, y_test):
    interp = tf.lite.Interpreter(model_path=str(MODEL_DIR / "fall_model.tflite"))
    interp.allocate_tensors()

    inp = interp.get_input_details()[0]
    out = interp.get_output_details()[0]
    in_s = inp["quantization_parameters"]["scales"][0]
    in_z = inp["quantization_parameters"]["zero_points"][0]
    out_s = out["quantization_parameters"]["scales"][0]
    out_z = out["quantization_parameters"]["zero_points"][0]

    preds = []
    for i in range(len(X_test)):
        data = (X_test[i:i+1] / in_s + in_z).astype(np.int8)
        interp.set_tensor(inp["index"], data)
        interp.invoke()
        raw = interp.get_tensor(out["index"])[0][0]
        prob = (raw - out_z) * out_s
        preds.append(1 if prob > 0.5 else 0)

    preds = np.array(preds)
    print(classification_report(y_test, preds, target_names=["No caida", "Caida"]))
    print(f"  Confusion matrix:\n{confusion_matrix(y_test, preds)}")
    return np.mean(preds == y_test)


# ═══════════════════════════════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════════════════════════════
def main():
    print("=" * 50)
    print("  FallDetector — Entrenamiento con datos propios")
    print("=" * 50)

    print("\n-- Cargando datos --")
    X, y = load_dataset()

    print("\n-- Split train/test --")
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.15, random_state=RANDOM_SEED, stratify=y
    )
    print(f"  Train: {len(X_train)} | Test: {len(X_test)}")

    print("\n-- Augmentation --")
    X_aug, y_aug = augment_data(X_train, y_train)
    print(f"  Aumentado: {len(X_aug)} ventanas")

    print("\n-- Modelo --")
    model = build_model()
    model.summary()

    print("\n-- Entrenamiento --")
    history = model.fit(
        X_aug, y_aug,
        validation_split=0.2,
        epochs=EPOCHS,
        batch_size=BATCH_SIZE,
        callbacks=[
            keras.callbacks.EarlyStopping(monitor="val_loss", patience=20, restore_best_weights=True),
            keras.callbacks.ReduceLROnPlateau(monitor="val_loss", factor=0.5, patience=7, min_lr=1e-6),
        ],
        verbose=1,
    )

    print("\n-- Evaluacion Float32 --")
    y_pred = (model.predict(X_test).flatten() > 0.5).astype(int)
    print(classification_report(y_test, y_pred, target_names=["No caida", "Caida"]))

    print("\n-- Conversion TFLite --")
    tflite_int8 = convert_to_tflite(model, X_train)
    generate_c_header(tflite_int8)

    print("\n-- Evaluacion TFLite INT8 --")
    acc = evaluate_tflite(X_test, y_test)

    print(f"\n{'=' * 50}")
    print(f"  Accuracy: {acc*100:.1f}%")
    print(f"  Modelo:   {len(tflite_int8)} bytes")
    print(f"\n  Sube firmware ttgo-t-watch al reloj para probar")
    print("=" * 50)


if __name__ == "__main__":
    main()
