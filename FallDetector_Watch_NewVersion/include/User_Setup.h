// User setup para ESP32-C6 2.06" AMOLED Touch Display
// Configuración de TFT_eSPI para el display QSPI CO5300

#ifndef USER_SETUP_H
#define USER_SETUP_H

// ESP32-C6 específicamente
#define ESP32_DRIVER
#define USER_SETUP_ID 206

// Display QSPI CO5300
#define TFT_SPI_HOST FSPI
#define TFT_BACKLIGHT_ON HIGH

// Pines QSPI
#define TFT_MOSI 5     // SPI MOSI (QSPI_DATA0)
#define TFT_MISO 3     // SPI MISO (QSPI_DATA1)
#define TFT_SCLK 6     // SPI Clock (QSPI_CLK)
#define TFT_CS   7     // Chip Select (QSPI_CS)

// Pines adicionales
#define TFT_DC   2     // Data/Command
#define TFT_RST  1     // Reset
#define TFT_BL   0     // Backlight

// Resolución
#define TFT_WIDTH  410
#define TFT_HEIGHT 502

// Color depth
#define TFT_RGB565

// Velocidad del SPI
#define SPI_FREQUENCY  80000000

// Rotación
#define TFT_ROTATION 0

// Usar DMA
#define USE_DMA

#endif // USER_SETUP_H
