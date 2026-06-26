#ifndef ALARMS_H
#define ALARMS_H

// Codigos de alarma/aviso (usados como bitmask) y estructuras del estado
// ambiental que evalua el gateway.

#define ALARM_NONE          0x00
#define ALARM_CO2           0x01
#define ALARM_GAS           0x02
#define ALARM_TEMP_HIGH     0x04
#define ALARM_TEMP_LOW      0x08
#define ALARM_HUM_HIGH      0x10
#define ALARM_HUM_LOW       0x20

// Avisos preventivos (severidad menor que una alarma).
#define WARN_TEMP_HIGH      0x40
#define WARN_TEMP_LOW       0x80

enum AlarmLevel {
    LEVEL_OK = 0,
    LEVEL_WARN = 1,
    LEVEL_ALARM = 2
};

struct AlarmEvent {
    uint8_t     code;           // codigo (ALARM_CO2, ALARM_GAS, ...)
    AlarmLevel  level;
    const char* id;             // etiqueta breve: "CO2", "GAS", "TEMP_H", ...
    const char* descripcion;
    const char* riesgo;         // riesgo para la persona mayor
    const char* accion;         // accion recomendada
    float       valor;          // valor medido
    float       umbral;         // umbral superado
};

#define MAX_ALARMAS 8

struct AlarmState {
    AlarmEvent  eventos[MAX_ALARMAS];
    uint8_t     count;          // numero de eventos activos
    uint8_t     activeFlags;    // bitmask para consulta rapida
    uint8_t     numAlarmas;     // eventos de nivel ALARM
    uint8_t     numAvisos;      // eventos de nivel WARN
};

#endif
