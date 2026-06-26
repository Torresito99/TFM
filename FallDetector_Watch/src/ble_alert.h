// =====================================================================
//  ble_alert.h — Baliza BLE de aviso de caída
//
//  Cuando suena la alarma, el reloj empieza a EMITIR (advertising) un
//  paquete BLE reconocible. No hace falta conexión: otros dispositivos
//  (que programaremos más adelante) solo tienen que escanear y detectar
//  el nombre "TFM-FALL" y/o los datos de fabricante {0xFF,0xFF,'F','A','L','L'}.
//
//  Al cancelar la alarma se detiene la emisión y se apaga la radio BLE
//  (ahorro de energía).
// =====================================================================
#pragma once

void bleAlertStart();   // inicia la emisión de la baliza de caída
void bleAlertStop();    // detiene la emisión y apaga el BLE
bool bleAlertActive();
