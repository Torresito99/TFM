#ifndef ALARMS_H
#define ALARMS_H

// --- Codigos de alarma (usar como bitmask) ---
#define ALARM_NONE          0x00
#define ALARM_CO2           0x01
#define ALARM_GAS           0x02
#define ALARM_TEMP_HIGH     0x04
#define ALARM_TEMP_LOW      0x08
#define ALARM_HUM_HIGH      0x10
#define ALARM_HUM_LOW       0x20

// --- Codigos de aviso preventivo ---
#define WARN_TEMP_HIGH      0x40
#define WARN_TEMP_LOW       0x80

// --- Niveles de severidad ---
enum AlarmLevel {
    LEVEL_OK = 0,
    LEVEL_WARN = 1,
    LEVEL_ALARM = 2
};

// --- Estructura de una alarma individual ---
struct AlarmEvent {
    uint8_t     code;           // Codigo identificador (ALARM_CO2, ALARM_GAS, etc.)
    AlarmLevel  level;          // LEVEL_OK, LEVEL_WARN, LEVEL_ALARM
    const char* id;             // Identificador texto: "CO2", "GAS", "TEMP_H", etc.
    const char* descripcion;    // Descripcion corta del problema
    const char* riesgo;         // Riesgo para persona mayor
    const char* accion;         // Accion recomendada
    float       valor;          // Valor medido
    float       umbral;         // Umbral superado
};

// --- Estado global de alarmas ---
#define MAX_ALARMAS 8

struct AlarmState {
    AlarmEvent  eventos[MAX_ALARMAS];
    uint8_t     count;          // Numero de alarmas/avisos activos
    uint8_t     activeFlags;    // Bitmask de alarmas activas (para consultar rapido)
    uint8_t     numAlarmas;     // Solo las de nivel ALARM
    uint8_t     numAvisos;      // Solo las de nivel WARN
};

#endif
