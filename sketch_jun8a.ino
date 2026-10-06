#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <unistd.h>
#include <ctime>
#include <cmath>
#include <deque>
#include <vector>
#include <algorithm>
#include <functional>

#include "ThreeScrambler.h"
#include "TwoScrambler.h"
#include "SkewbScrambler.h"
#include "PyraScrambler.h"
#include "FourScrambler.h"
#include "FiveScrambler.h"
#include "SixSevenScrambler.h"
#include "SqoneScrambler.h"
#include "FtoScrambler.h"
#include "MegaScrambler.h"
#include "ClockScrambler.h"
#include "Iconos.h"

// Pines de hardware
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 5
#define TFT_BL_PIN 27
#define SENSOR_PIN 35

#define DEBOUNCE_MS 300

#define BUFFER_RECORDS 64
#define MAX_BUFFER_SOLVES 20000

#define BUTTON_W 60
#define BUTTON_H 25
#define BUTTON_CUBE_W 80
#define FLECHA_X 280
#define FLECHA_Y 218
#define FLECHA_WH 30

#define SOLVES_GRID_COLS 4
#define SOLVES_GRID_ROWS 6
#define SOLVES_PER_PAGE (SOLVES_GRID_COLS * SOLVES_GRID_ROWS) // 12 solves por pantalla

#define GRID_START_X 8
#define GRID_START_Y 32
#define CELL_W 71
#define CELL_H 24
#define CELL_GAP_X 6
#define CELL_GAP_Y 6

#define ELIMINAR_TIEMPO 0
#define ELIMINAR_SESION 1
#define DESARCHIVAR_TIEMPOS 2
#define ARCHIVAR_SESION 3
#define ARCHIVAR_TIEMPO 4

TFT_eSPI tft = TFT_eSPI();
SPIClass sdSPI(HSPI);

enum EstadoTimer { DETENIDO, ESPERANDO, PREPARADO, CORRIENDO, INSPECCION };
EstadoTimer estado = DETENIDO;

typedef std::string (*ScrambleFunc)();

struct CubeConfig {
  const char* label;
  const char* file;
  const uint16_t* icon;
  int page;
  int x;
  int y;
  ScrambleFunc scrambler;
};

struct SolveRecord {
  int32_t tiempo;
  uint8_t archivado;
  uint8_t penalty;
  uint32_t offsetMezcla;
  uint32_t longMezcla;
};

struct SessionStats {
  uint32_t count = 0;
  uint32_t dnfs = 0;
  int32_t  bestSingle = -2;
  int32_t  worstSingle = -2;
  int32_t  bestAo5 = -2;
  int32_t  bestAo12 = -2;
  int32_t  bestAo50 = -2;
  int32_t  bestAo100 = -2;
  int32_t  bestAo1000 = -2;
  int32_t  currentAo5 = -2;
  int32_t  currentAo12 = -2;
  int32_t  currentAo50 = -2;
  int32_t  currentAo100 = -2;
  int32_t  currentAo1000 = -2;
  int32_t  media = -2;
  float    desviacion = 0.0f;
};

struct PopupData {
  SolveRecord record;
  String scramble;
  int indiceGlobal;
};

struct SessionItem {
  int32_t  tiempo;
  uint8_t  penalty;
  uint32_t indexSD;
};

struct SettingsData {
  uint8_t inspection;
  uint8_t hideTime;
  int32_t cuboActual;
};

// Generadores específicos con parámetros adaptados a la firma ScrambleFunc
std::string getScramble6x6() { return SixSevenScrambler::scramble(80); }
std::string getScramble7x7() { return SixSevenScrambler::scramble(100); }

const CubeConfig CUBOS[] = {
  // Página 1
  {" 2x2 ", "2x2", twobuttonD, 1, 10,  38, TwoScrambler::scramble},
  {" 3x3 ", "3x3", threebuttonD, 1, 80,  38, ThreeScrambler::scramble},
  {" 4x4 ", "4x4", fourbuttonD, 1, 150, 38, FourScrambler::scramble},
  {" 5x5 ", "5x5", fivebuttonD, 1, 220, 38, FiveScrambler::scramble},
  {" 6x6 ", "6x6", sixbuttonD, 1, 10,  108, getScramble6x6},
  {" 7x7 ", "7x7", sevenbuttonD, 1, 80,  108, getScramble7x7},
  {"blind", "blind", blindbuttonD, 1, 150, 108, ThreeScrambler::scramble},
  {" sq1 ", "sq1", sqonebuttonD, 1, 220, 108, SqoneScrambler::scramble},
  {"pyram", "pyram", pyrabuttonD, 1, 10,  178, PyraScrambler::scramble},
  {"clock", "clock", clockbuttonD, 1, 80,  178, ClockScrambler::scramble},
  {"megam", "megam", megabuttonD, 1, 150, 178, MegaScrambler::scramble},
  {"skewb", "skewb", skewbbuttonD, 1, 220, 178, SkewbScrambler::scramble},
  // Página 2
  {" 3oh ", "3oh", ohbuttonD, 2, 10,  38, ThreeScrambler::scramble},
  {"4blnd", "4blnd", fourbldbuttonD, 2, 80,  38, FourScrambler::scramble},
  {"5blnd", "5blnd", fivebldbuttonD, 2, 150, 38, FiveScrambler::scramble},
  {" fto ", "fto", ftobuttonD, 2, 220, 38, FtoScrambler::scramble}
};
const size_t TOTAL_CUBOS = sizeof(CUBOS) / sizeof(CUBOS[0]);

std::deque<SessionItem> sessionSolves;

int cuboActual = 1; // Índice por defecto (3x3)
unsigned long tiempoMano = 0;
unsigned long tiempoInicio = 0;
unsigned long tiempoTranscurrido = 0;
unsigned long tiempoInicioInspeccion = 0;
uint8_t penaltyInspeccion = 0;
int ultimoSegundoPintado = -99;
unsigned long ultimoToque = 0;
unsigned long debounceFinTimer = 0;

int pantallaActual = 1;
String mezcla = "";
String ultimaMezcla = "";
SolveRecord ultimaSolve;
int paginaScramble = 0;
int paginaCubos = 1;
bool sdDisponible = false;
bool averagesShown = false;
bool mezclaShown = false;
int paginaSolves = 0;
int solveSeleccionada = -1;
bool popupSolveVisible = false;
PopupData solvePopup;
bool popupSiNoVisible = false;
int accionPendiente = -1;
bool popupNumVisible = false;
int parametroN = 0;
int sessionGlobal = 0;
bool inspection = false;
bool hideTime = false;

inline bool puntoEnArea(int px, int py, int x, int y, int w, int h) {
  return (px >= x && px <= (x + w) && py >= y && py <= (y + h));
}

inline long getTiempoEfectivo(int32_t tiempoBase, uint8_t penalty) {
  if (penalty == 2) return -1; // DNF
  if (penalty == 1) return tiempoBase + 2000; // +2s
  return tiempoBase; // OK
}

void setup() {
  pinMode(TFT_BL_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  tft.init();
  //tft.invertDisplay(true);
  //tft.setRotation(1);
  //uint16_t calData[5] = { 229, 3433, 369, 3383, 1 };
  tft.setRotation(3);
  digitalWrite(TFT_BL_PIN, HIGH);
  drawIntroMessage();
  uint16_t calData[5] = { 210, 3456, 371, 3387, 7 };
  //uint16_t calData[5];
  //tft.calibrateTouch(calData, TFT_WIHTE, TFT_BLACK, 15);
  tft.setTouch(calData);
  //for (int i = 0; i < 5; i++) {
  //  tft.println(calData[i]);
  //  if (i < 4) Serial.print(", ");
  //}
  tft.setSwapBytes(true);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdDisponible = SD.begin(SD_CS, sdSPI, 25000000);

  std::srand(esp_random());

  loadSettings();
  loadSession();
  mezcla = generarMezcla();

  delay(1200);

  drawBasic();
  drawTimer();
  imprimirAlgoritmo(mezcla);
  //for (int i = 0; i < 30; i++) {
  //  ultimaMezcla = generarMezcla();
  //  long tiempoAleatorio = random(10000, 15001);
  //  registrarTiempo(tiempoAleatorio);
  //}
  mostrarTiempo(0);
}

void loop() {
  uint16_t touchX = 0, touchY = 0;
  bool tocadoPantalla = tft.getTouch(&touchX, &touchY);
  long tiempoEnPantalla = (ultimaSolve.tiempo != 0) ? getTiempoEfectivo(ultimaSolve.tiempo, ultimaSolve.penalty) : 0;

  if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {

    // Popups
    if (popupSiNoVisible) {
      procesarToquePopupSiNo(touchX, touchY);
      return;
    }
    else if (popupNumVisible) {
      procesarToquePopupNum(touchX, touchY);
      return;
    }

    // Barra de pestañas superior
    // Selector de cubos
    if (puntoEnArea(touchX, touchY, 0, 0, BUTTON_CUBE_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 0) { 
        pantallaActual = 0;
        drawCubes();
        sessionGlobal = 0;
        paginaSolves = 0;
        averagesShown = false;
        mezclaShown = false;
      }
    } 
    // Timer
    else if (puntoEnArea(touchX, touchY, 80, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 1) {
        pantallaActual = 1;
        drawTimer();
        if (mezcla.length() == 0) mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);
        if (ultimaSolve.tiempo != 0) mostrarTiempo(getTiempoEfectivo(ultimaSolve.tiempo, ultimaSolve.penalty));
        else mostrarTiempo(0);
        sessionGlobal = 0;
        paginaCubos = 1;
        paginaSolves = 0;
      }
    } 
    // Times
    else if (puntoEnArea(touchX, touchY, 140, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 2) {
        pantallaActual = 2; drawTimes();
        averagesShown = false;
        mezclaShown = false;
        sessionGlobal = 0;
        paginaCubos = 1;
      }
    } 
    // Stats
    else if (puntoEnArea(touchX, touchY, 200, 0, BUTTON_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 3) {
        pantallaActual = 3; drawStats();
        averagesShown = false;
        mezclaShown = false;
        paginaSolves = 0;
        paginaCubos = 1;
      }
    }
    // Settings
    else if (puntoEnArea(touchX, touchY, 260, 0, BUTTON_CUBE_W, BUTTON_H)) {
      ultimoToque = millis();
      if (pantallaActual != 4) {
        pantallaActual = 4; drawSettings();
        averagesShown = false;
        mezclaShown = false;
        sessionGlobal = 0;
        paginaCubos = 1;
        paginaSolves = 0;
      }
    }
  }

  switch (pantallaActual) {
    case 0: // Selección de Cubos
      if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {
        if (puntoEnArea(touchX, touchY, FLECHA_X, FLECHA_Y, FLECHA_WH, FLECHA_WH)) {
          ultimoToque = millis();
          paginaCubos = (paginaCubos == 1) ? 2 : 1;
          drawCubes();
          break;
        }

        for (size_t i = 0; i < TOTAL_CUBOS; i++) {
          if (CUBOS[i].page == paginaCubos && puntoEnArea(touchX, touchY, CUBOS[i].x, CUBOS[i].y, bigIconW, bigIconH)) {
            ultimoToque = millis();
            compactarArchivosCubo();
            cuboActual = i;
            saveSettings();
            pantallaActual = 1;
            paginaCubos = 1;
            drawTimer();
            mezcla = generarMezcla();
            loadSession();
            imprimirAlgoritmo(mezcla);
            mostrarTiempo(0);
            break;
          }
        }
      }
      break;

    case 1: // Cronómetro
      if (tocadoPantalla && estado == INSPECCION && (millis() - ultimoToque > DEBOUNCE_MS)) {
        ultimoToque = millis();
        estado = DETENIDO;
        debounceFinTimer = millis() + 300;
        imprimirAlgoritmo(mezcla); // Restaura la mezcla en el área central
        mostrarTiempo(tiempoEnPantalla); // Restaura el tiempo en blanco
        break;
      }

      if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {
        
        bool esGrande = (cuboActual >= 4 && cuboActual <= 5) || cuboActual == 10; // 6x6, 7x7, megaminx

        // Cambio de página en mezclas largas
        if (esGrande && puntoEnArea(touchX, touchY, 2, 30, 317, 138)) {
          ultimoToque = millis();
          paginaScramble = 1 - paginaScramble;
          if (averagesShown) {
            tft.fillRect(100, 75, 219, 90, TFT_BLACK);
            tft.drawFastVLine(245, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            averagesShown = false;
          }
          if (mezclaShown) {
            tft.fillRect(1, 50, 220, 115, TFT_BLACK);
            tft.drawFastVLine(75, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            mezclaShown = false;
          }
          imprimirAlgoritmo(mezcla);
        }
        // Ver mezcla
        else if (puntoEnArea(touchX, touchY, 10, 175, bigIconW, bigIconH)) {
          ultimoToque = millis();
          mezclaShown = !mezclaShown;
          if (mezclaShown) {
            if (averagesShown) {
              tft.fillRect(100, 75, 219, 90, TFT_BLACK);
              tft.drawFastVLine(245, 165, 60, TFT_BLACK);
              tft.drawFastHLine(0, 165, 320, TFT_WHITE);
              averagesShown = false;
            }
            drawMezcla();
          } else {
            tft.fillRect(1, 50, 220, 115, TFT_BLACK);
            tft.drawFastVLine(75, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            imprimirAlgoritmo(mezcla);
          }
        }
        // Eliminar última solve
        else if (puntoEnArea(touchX, touchY, 75, 205, iconW, iconH)) {
          ultimoToque = millis();
          if (!sessionSolves.empty() && (tiempoTranscurrido != 0 || ultimaSolve.longMezcla > 0)) {
            abrirPopupSiNo(ELIMINAR_TIEMPO, 0);
          }
        }
        // Rehacer última mezcla
        else if (puntoEnArea(touchX, touchY, 112, 205, iconW, iconH)) {
          ultimoToque = millis();
          if (ultimaMezcla != "" && (tiempoTranscurrido != 0 || ultimaSolve.longMezcla > 0)) {
            if (averagesShown) {
            tft.fillRect(100, 75, 219, 90, TFT_BLACK);
            tft.drawFastVLine(245, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            averagesShown = false;
            }
            if (mezclaShown) {
            tft.fillRect(1, 50, 220, 115, TFT_BLACK);
            tft.drawFastVLine(75, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            mezclaShown = false;
            }
            imprimirAlgoritmo(ultimaMezcla);
          }
        }
        // DNF
        else if (puntoEnArea(touchX, touchY, 150, 206, iconW, iconH)) {
          ultimoToque = millis();
          if (tiempoTranscurrido > 0 || ultimaSolve.longMezcla > 0) {
            uint8_t nuevaPen = (ultimaSolve.penalty == 2) ? 0 : 2;
            actualizarRegistro(nuevaPen);
            mostrarTiempo(getTiempoEfectivo(ultimaSolve.tiempo, ultimaSolve.penalty));
          }
        }
        // +2 segundos
        else if (puntoEnArea(touchX, touchY, 186, 205, iconW, iconH)) {
          ultimoToque = millis();
          if (tiempoTranscurrido > 0 || ultimaSolve.longMezcla > 0) {
            uint8_t nuevaPen = (ultimaSolve.penalty == 1) ? 0 : 1;
            actualizarRegistro(nuevaPen);
            mostrarTiempo(getTiempoEfectivo(ultimaSolve.tiempo, ultimaSolve.penalty));
          }
        }
        // Nueva mezcla
        else if (puntoEnArea(touchX, touchY, 222, 206, iconW, iconH)) {
          ultimoToque = millis();
          mezcla = generarMezcla();
          if (averagesShown) {
            tft.fillRect(100, 75, 219, 90, TFT_BLACK);
            tft.drawFastVLine(245, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            averagesShown = false;
          }
          if (mezclaShown) {
            tft.fillRect(1, 50, 220, 115, TFT_BLACK);
            tft.drawFastVLine(75, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            mezclaShown = false;
          }
          imprimirAlgoritmo(mezcla);
        }
        // Ver medias (Averages)
        else if (puntoEnArea(touchX, touchY, 256, 175, bigIconW, bigIconH)) {
          ultimoToque = millis();
          averagesShown = !averagesShown;
          if (averagesShown) {
            if (mezclaShown) {
              tft.fillRect(1, 50, 220, 115, TFT_BLACK);
              tft.drawFastVLine(75, 165, 60, TFT_BLACK);
              tft.drawFastHLine(0, 165, 320, TFT_WHITE);
              mezclaShown = false;
            }
            drawAverages();
          } else {
            tft.fillRect(100, 75, 219, 90, TFT_BLACK);
            tft.drawFastVLine(245, 165, 60, TFT_BLACK);
            tft.drawFastHLine(0, 165, 320, TFT_WHITE);
            imprimirAlgoritmo(mezcla);
          }
        }
      }
      break;
    case 2: // pestaña de solves
      if (tocadoPantalla && (millis() - ultimoToque > DEBOUNCE_MS)) {
        ultimoToque = millis();

        if (popupSolveVisible) {
          // cerrar
          if (puntoEnArea(touchX, touchY, 275, 36, 22, 22)) {
            cerrarPopupSolve();
          }
          // ver mezcla
          else if (puntoEnArea(touchX, touchY, 20, 176, 42, 42)) {
            //drawMezclaPeque(); //dos métodos pero ambos llaman a generar mezcla en cada scrambler
          }
          // DNF
          else if (puntoEnArea(touchX, touchY, 155, 198, iconW, iconH)) {
            solvePopup.record.penalty = (solvePopup.record.penalty == 2) ? 0 : 2;

            uint32_t idxSD = sessionSolves[solvePopup.indiceGlobal].indexSD;
            String pathDat = getCubePath(".dat");
            File f = SD.open(pathDat.c_str(), "r+");
            if (f) {
              f.seek((size_t)idxSD * sizeof(SolveRecord));
              f.write((const uint8_t*)&solvePopup.record, sizeof(SolveRecord));
              f.close();
            }
            sessionSolves[solvePopup.indiceGlobal].penalty = solvePopup.record.penalty;
            if (solvePopup.record.offsetMezcla == ultimaSolve.offsetMezcla) ultimaSolve = solvePopup.record;
            drawPopupSolve();
          }
          // +2
          else if (puntoEnArea(touchX, touchY, 193, 198, iconW, iconH)) {
            solvePopup.record.penalty = (solvePopup.record.penalty == 1) ? 0 : 1;

            uint32_t idxSD = sessionSolves[solvePopup.indiceGlobal].indexSD;
            String pathDat = getCubePath(".dat");
            File f = SD.open(pathDat.c_str(), "r+");
            if (f) {
              f.seek((size_t)idxSD * sizeof(SolveRecord));
              f.write((const uint8_t*)&solvePopup.record, sizeof(SolveRecord));
              f.close();
            }
            sessionSolves[solvePopup.indiceGlobal].penalty = solvePopup.record.penalty;
            if (solvePopup.record.offsetMezcla == ultimaSolve.offsetMezcla) ultimaSolve = solvePopup.record;
            drawPopupSolve();
          }
          // archivar
          else if (puntoEnArea(touchX, touchY, 235, 198, iconW, iconH)) {
            abrirPopupSiNo(ARCHIVAR_TIEMPO, 0);
          }
          // eliminar
          else if (puntoEnArea(touchX, touchY, 275, 198, iconW, iconH)) {
            abrirPopupSiNo(ELIMINAR_TIEMPO, 0);
          }
          break;
        }
        // eliminar sesión
        if (puntoEnArea(touchX, touchY, 12, 215, iconW, iconH)) {
          abrirPopupSiNo(ELIMINAR_SESION, 0);
        }
        // archivar sesión
        else if (puntoEnArea(touchX, touchY, 62, 215, iconW, iconH)) {
          abrirPopupSiNo(ARCHIVAR_SESION, 0);
        }
        // desarchivar sesión
        else if (puntoEnArea(touchX, touchY, 112, 215, iconW, iconH)) {
          abrirPopupNum();
        }
        // flecha izquierda
        else if (puntoEnArea(touchX, touchY, 160, 216, 40, 24)) {
          if (paginaSolves > 0) {
            paginaSolves--;
            drawTimes();
          }
        }
        // flecha derecha
        else if (puntoEnArea(touchX, touchY, 270, 216, 40, 24)) {
          size_t total = sessionSolves.size();
          int totalPags = (total == 0) ? 1 : ((total - 1) / SOLVES_PER_PAGE + 1);
          if (paginaSolves + 1 < totalPags) {
            paginaSolves++;
            drawTimes();
          }
        }
        // Selección de solve
        else {
          int primerIndice = sessionSolves.size() - 1 - (paginaSolves * SOLVES_PER_PAGE);
          for (int r = 0; r < SOLVES_GRID_ROWS; r++) {
            for (int c = 0; c < SOLVES_GRID_COLS; c++) {
              int x = GRID_START_X + c * (CELL_W + CELL_GAP_X);
              int y = GRID_START_Y + r * (CELL_H + CELL_GAP_Y);
              if (puntoEnArea(touchX, touchY, x, y, CELL_W, CELL_H)) {
                int itemIdx = r * SOLVES_GRID_COLS + c;
                int solveIdx = primerIndice - itemIdx;
                if (solveIdx >= 0 && (size_t)solveIdx < sessionSolves.size()) {
                  abrirPopupSolve(solveIdx);
                }
                break;
              }
            }
          }
        }
      }
      break;
    case 3: // pestaña de stats
      if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {
        if (puntoEnArea(touchX, touchY, 1, 155, 158, 20)) {
          ultimoToque = millis();
          if (sessionGlobal != 0) {
            sessionGlobal = 0;
            drawStats();
          }
        }
        else if (puntoEnArea(touchX, touchY, 161, 155, 158, 20)) {
          ultimoToque = millis();
          if (sessionGlobal != 1) {
            sessionGlobal = 1;
            drawStats();
          }
        }
      }
      break;
    case 4: // ajustes
      if (tocadoPantalla && estado == DETENIDO && (millis() - ultimoToque > DEBOUNCE_MS)) {
        if (puntoEnArea(touchX, touchY, 285, 30, 25, 25)) {
          ultimoToque = millis();
          if (inspection) {
            tft.fillRect(285, 30, 25, 25, TFT_BLACK);
            tft.drawRect(285, 30, 25, 25, TFT_WHITE);
          } else tft.fillRect(285, 30, 25, 25, TFT_WHITE);
          inspection = !inspection;
          saveSettings();
        }
        else if (puntoEnArea(touchX, touchY, 285, 65, 25, 25)) {
          ultimoToque = millis();
          if (hideTime) {
            tft.fillRect(285, 65, 25, 25, TFT_BLACK);
            tft.drawRect(285, 65, 25, 25, TFT_WHITE);
          } else tft.fillRect(285, 65, 25, 25, TFT_WHITE);
          hideTime = !hideTime;
          saveSettings();
        }
      }
      break;
  }

  // Lectura del sensor para el Timer
  bool tocadoSensor = (digitalRead(SENSOR_PIN) == HIGH) && pantallaActual == 1;
  static uint8_t subEstadoMano = 0;
  static bool sensorPresionadoInicioInsp = false;

  switch (estado) {
    case DETENIDO:
      if (inspection) {
        if (tocadoSensor && (millis() > debounceFinTimer)) {
          sensorPresionadoInicioInsp = true;
        } else if (!tocadoSensor && sensorPresionadoInicioInsp) {
          sensorPresionadoInicioInsp = false;
          estado = INSPECCION;
          tiempoInicioInspeccion = millis();
          penaltyInspeccion = 0;
          ultimoSegundoPintado = -99;
          subEstadoMano = 0;
          drawInspection(0);
          debounceFinTimer = millis() + 200;
        }
      } else {
        if (tocadoSensor && (millis() > debounceFinTimer)) {
          tiempoMano = millis();
          estado = ESPERANDO;
          mostrarTiempo(tiempoEnPantalla);
        }
      }
      break;

    case INSPECCION: {
      unsigned long tInsp = millis() - tiempoInicioInspeccion;

      if (tInsp >= 17000) {
        estado = DETENIDO;
        debounceFinTimer = millis() + 500;

        ultimaMezcla = mezcla;
        mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);

        registrarTiempo(0);
        actualizarRegistro(2);
        mostrarTiempo(-1);
        break;
      }

      if (tInsp >= 15000) {
        penaltyInspeccion = 1;
      }

      drawInspection(tInsp);

      if (!tocadoSensor) {
        if (subEstadoMano == 2) {
          tiempoInicio = millis();
          imprimirAlgoritmo(mezcla);
          subEstadoMano = 0;
          estado = CORRIENDO;
          break;
        } else if (subEstadoMano == 1) {
          subEstadoMano = 0;
          mostrarTiempo(tiempoEnPantalla);
        }
      } 
      else if (millis() > debounceFinTimer) {
        if (subEstadoMano == 0) {
          subEstadoMano = 1;
          tiempoMano = millis();
          estado = ESPERANDO;
          mostrarTiempo(tiempoEnPantalla);
          estado = INSPECCION;
        } else if (subEstadoMano == 1) {
          if (millis() - tiempoMano >= 500) {
            subEstadoMano = 2;
            estado = PREPARADO;
            mostrarTiempo(tiempoEnPantalla);
            estado = INSPECCION;
          }
        }
      }
      break;
    }

    case ESPERANDO:
      if (!tocadoSensor) {
        estado = DETENIDO;
        mostrarTiempo(tiempoEnPantalla);
      } else if (millis() - tiempoMano >= 500) {
        estado = PREPARADO;
        mostrarTiempo(tiempoEnPantalla);
      }
      break;

    case PREPARADO:
      if (!tocadoSensor) {
        tiempoInicio = millis();
        estado = CORRIENDO;
      }
      break;

    case CORRIENDO:
      tiempoTranscurrido = millis() - tiempoInicio;
      mostrarTiempo(tiempoTranscurrido);

      if ((tocadoSensor || tocadoPantalla) && (tiempoTranscurrido > 200)) {
        ultimoToque = millis();
        estado = DETENIDO;
        debounceFinTimer = millis() + 500;

        ultimaMezcla = mezcla;
        mezcla = generarMezcla();
        imprimirAlgoritmo(mezcla);

        if (tocadoPantalla) {
          tiempoTranscurrido = 0;
          mostrarTiempo(tiempoTranscurrido);
        } else {
          registrarTiempo(tiempoTranscurrido);
          if (inspection && penaltyInspeccion == 1) {
            actualizarRegistro(1);
          }
          mostrarTiempo(getTiempoEfectivo(ultimaSolve.tiempo, ultimaSolve.penalty));
        }
      }
      break;
  }
}

void drawTimes() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(141, 25, 59, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 210, TFT_BLACK);

  tft.pushImage(12,  215, iconW, iconH, binD);
  tft.pushImage(62,  215, iconW, iconH, archivarD);
  tft.pushImage(112, 215, iconW, iconH, desarchivarD);

  tft.drawRect(0, 0, 320, 240, TFT_WHITE);
  tft.drawFastHLine(0, 215, 320, TFT_WHITE);
  tft.drawFastVLine(50, 215, 25, TFT_WHITE);
  tft.drawFastVLine(100, 215, 25, TFT_WHITE);
  tft.drawFastVLine(150, 215, 25, TFT_WHITE);

  popupSolveVisible = false;

  size_t totalSolves = sessionSolves.size();
  int totalPaginas = (totalSolves == 0) ? 1 : ((totalSolves - 1) / SOLVES_PER_PAGE + 1);
  if (paginaSolves >= totalPaginas) paginaSolves = totalPaginas - 1;
  if (paginaSolves < 0) paginaSolves = 0;

  char buf[32];
  tft.setTextFont(1);

  // Barra inferior: botones de paginación e info
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("<", 170, 220);
  tft.drawString(">", 290, 220);

  snprintf(buf, sizeof(buf), "%d/%d", paginaSolves + 1, totalPaginas);
  tft.drawString(buf, 220, 220);

  if (totalSolves == 0) {
    tft.setTextSize(3);
    tft.drawCentreString("Sin solves", 160, 110, 1);
    return;
  }

  // Cuadrícula de 24 celdas
  int primerIndice = totalSolves - 1 - (paginaSolves * SOLVES_PER_PAGE);

  for (int row = 0; row < SOLVES_GRID_ROWS; row++) {
    for (int col = 0; col < SOLVES_GRID_COLS; col++) {
      int itemIdx = row * SOLVES_GRID_COLS + col;
      int solveIdx = primerIndice - itemIdx;

      int x = GRID_START_X + col * (CELL_W + CELL_GAP_X);
      int y = GRID_START_Y + row * (CELL_H + CELL_GAP_Y);

      if (solveIdx >= 0) {
        tft.drawRect(x, y, CELL_W, CELL_H, TFT_WHITE);

        tft.setTextSize(1);
        SessionItem item = sessionSolves[solveIdx];
        if (item.penalty == 2) {
          tft.setTextColor(TFT_RED, TFT_BLACK);
          tft.drawCentreString("DNF", x + (CELL_W / 2), y + 7, 1);
        } else {
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          long tEfectivo = getTiempoEfectivo(item.tiempo, item.penalty);
          formatearTiempoAO(tEfectivo, buf, sizeof(buf));
          if (item.penalty == 1) {
            strncat(buf, "+", sizeof(buf) - strlen(buf) - 1);
          }
          tft.drawCentreString(buf, x + (CELL_W / 2), y + 7, 1);
        }
      }
    }
  }
}

void abrirPopupSolve(int indiceSession) {
  if (!sdDisponible) return;

  uint32_t idxSD = sessionSolves[indiceSession].indexSD;

  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), FILE_READ);
  if (!fileDat) return;

  size_t offsetByte = (size_t)idxSD * sizeof(SolveRecord);
  if (fileDat.size() < offsetByte + sizeof(SolveRecord)) {
    fileDat.close();
    return;
  }

  fileDat.seek(offsetByte);
  fileDat.read((uint8_t*)&solvePopup.record, sizeof(SolveRecord));
  fileDat.close();

  solvePopup.scramble = "";
  solvePopup.indiceGlobal = indiceSession;

  if (solvePopup.record.longMezcla > 0) {
    String pathTxt = getCubePath(".txt");
    File fileTxt = SD.open(pathTxt.c_str(), FILE_READ);
    if (fileTxt) {
      fileTxt.seek(solvePopup.record.offsetMezcla);
      char* buf = new char[solvePopup.record.longMezcla + 1];
      fileTxt.read((uint8_t*)buf, solvePopup.record.longMezcla);
      buf[solvePopup.record.longMezcla] = '\0';
      solvePopup.scramble = String(buf);
      delete[] buf;
      fileTxt.close();
    }
  }

  popupSolveVisible = true;
  drawPopupSolve();
}

void cerrarPopupSolve() {
  popupSolveVisible = false;
  drawTimes();
}

void drawPopupSolve() {
  // Marco del popup
  tft.fillRect(15, 30, 290, 195, TFT_BLACK);
  tft.drawRect(15, 30, 290, 195, TFT_WHITE);
  tft.drawFastHLine(15, 170, 52, TFT_WHITE);
  tft.drawFastVLine(67, 170, 54, TFT_WHITE);
  tft.drawFastVLine(150, 196, 28, TFT_WHITE);
  tft.drawFastHLine(150, 196, 155, TFT_WHITE);

  tft.pushImage(20, 176, 42, 42, vermezclapequeD);
  tft.pushImage(155, 198, iconW, iconH, dnfD);
  tft.pushImage(193, 198, iconW, iconH, plustwoD);
  tft.pushImage(235, 198, iconW, iconH, archivarD);
  tft.pushImage(275, 198, iconW, iconH, binD);

  char buf[32];
  tft.setTextSize(2);
  tft.drawRect(275, 36, 22, 22, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("X", 287, 40, 1);

  // Tiempo central
  tft.setTextSize(3);
  long tEfectivo = getTiempoEfectivo(solvePopup.record.tiempo, solvePopup.record.penalty);
  
  if (solvePopup.record.penalty == 2) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("DNF", 160, 38, 1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    formatearTiempoAO(tEfectivo, buf, sizeof(buf));
    if (solvePopup.record.penalty == 1) {
      strncat(buf, "+", sizeof(buf) - strlen(buf) - 1);
    }
    tft.drawCentreString(buf, 160, 38, 1);
  }

  tft.drawFastHLine(25, 65, 270, TFT_DARKGREY);

  // Mostrar la mezcla envuelta en líneas
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  int x = 25, y = 70;
  int anchoMax = 270;
  int cursorX = x, cursorY = y;
  int altoLinea = tft.fontHeight(1) + 2;
  int espacioAncho = tft.textWidth(" ", 1);

  String palabra = "";
  for (unsigned int i = 0; i <= solvePopup.scramble.length(); i++) {
    if (i < solvePopup.scramble.length() && solvePopup.scramble[i] != ' ') {
      palabra += solvePopup.scramble[i];
    } else if (palabra.length() > 0) {
      int anchoPalabra = tft.textWidth(palabra, 1);
      if (cursorX + anchoPalabra > x + anchoMax) {
        cursorX = x;
        cursorY += altoLinea;
      }
      if (cursorY <= 170) {
        tft.setCursor(cursorX, cursorY);
        tft.print(palabra);
        cursorX += anchoPalabra + espacioAncho;
      }
      palabra = "";
    }
  }

  tft.setTextSize(2);
}

int32_t calcularAODeCola(const std::deque<long>& ventana, int n) {
  if (ventana.size() < (size_t)n) return -1;
  std::vector<long> v(ventana.begin(), ventana.end());
  int dnfs = 0;
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] < 0) {
      dnfs++;
      v[i] = 2147483647;
    }
  }
  if (dnfs > 1) return -1;
  std::sort(v.begin(), v.end());
  int64_t suma = 0;
  for (int i = 1; i < n - 1; i++) suma += v[i];
  return (int32_t)(suma / (n - 2));
}

SessionStats calcularEstadisticasSesion() {
  SessionStats stats;
  if (sessionSolves.empty()) return stats;

  int64_t suma = 0;
  std::vector<long> validos;
  validos.reserve(sessionSolves.size());

  std::deque<long> w5, w12, w50, w100, w1000;

  for (const auto& item : sessionSolves) {
    stats.count++;
    long t = getTiempoEfectivo(item.tiempo, item.penalty);

    if (t < 0) {
      stats.dnfs++;
    } else {
      validos.push_back(t);
      suma += t;
      if (stats.bestSingle == -2 || t < stats.bestSingle) stats.bestSingle = t;
      if (stats.worstSingle == -2 || t > stats.worstSingle) stats.worstSingle = t;
    }

    // Ao5
    w5.push_back(t);
    if (w5.size() > 5) w5.pop_front();
    if (w5.size() == 5) {
      int32_t val = calcularAODeCola(w5, 5);
      if (val > 0 && (stats.bestAo5 == -2 || val < stats.bestAo5)) stats.bestAo5 = val;
    }

    // Ao12
    w12.push_back(t);
    if (w12.size() > 12) w12.pop_front();
    if (w12.size() == 12) {
      int32_t val = calcularAODeCola(w12, 12);
      if (val > 0 && (stats.bestAo12 == -2 || val < stats.bestAo12)) stats.bestAo12 = val;
    }

    // Ao50
    w50.push_back(t);
    if (w50.size() > 50) w50.pop_front();
    if (w50.size() == 50) {
      int32_t val = calcularAODeCola(w50, 50);
      if (val > 0 && (stats.bestAo50 == -2 || val < stats.bestAo50)) stats.bestAo50 = val;
    }

    // Ao100
    w100.push_back(t);
    if (w100.size() > 100) w100.pop_front();
    if (w100.size() == 100) {
      int32_t val = calcularAODeCola(w100, 100);
      if (val > 0 && (stats.bestAo100 == -2 || val < stats.bestAo100)) stats.bestAo100 = val;
    }

    // Ao1000
    w1000.push_back(t);
    if (w1000.size() > 1000) w1000.pop_front();
    if (w1000.size() == 1000) {
      int32_t val = calcularAODeCola(w1000, 1000);
      if (val > 0 && (stats.bestAo1000 == -2 || val < stats.bestAo1000)) stats.bestAo1000 = val;
    }
  }

  if (!validos.empty()) {
    stats.media = (int32_t)(suma / validos.size());

    float sumaVarianza = 0.0f;
    for (long t : validos) {
      float diff = t - stats.media;
      sumaVarianza += diff * diff;
    }
    stats.desviacion = std::sqrt(sumaVarianza / validos.size()) / 1000.0f;
  }

  stats.currentAo5    = (w5.size() >= 5)       ? calcularAODeCola(w5, 5)       : -2;
  stats.currentAo12   = (w12.size() >= 12)     ? calcularAODeCola(w12, 12)     : -2;
  stats.currentAo50   = (w50.size() >= 50)     ? calcularAODeCola(w50, 50)     : -2;
  stats.currentAo100  = (w100.size() >= 100)   ? calcularAODeCola(w100, 100)   : -2;
  stats.currentAo1000 = (w1000.size() >= 1000) ? calcularAODeCola(w1000, 1000) : -2;

  return stats;
}

SessionStats calcularEstadisticasGlobales() {
  SessionStats stats;
  if (!sdDisponible) return stats;

  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), FILE_READ);
  if (!fileDat) return stats;

  size_t totalRecords = fileDat.size() / sizeof(SolveRecord);
  if (totalRecords == 0) {
    fileDat.close();
    return stats;
  }

  SolveRecord bloque[BUFFER_RECORDS];
  int64_t suma = 0;
  uint32_t validos = 0;

  std::deque<long> w5, w12, w50, w100, w1000;

  for (size_t i = 0; i < totalRecords; i += BUFFER_RECORDS) {
    size_t aLeer = std::min((size_t)BUFFER_RECORDS, totalRecords - i);
    fileDat.read((uint8_t*)bloque, aLeer * sizeof(SolveRecord));

    for (size_t j = 0; j < aLeer; j++) {
      if (bloque[j].archivado == 2) continue; // Descartar borradas

      stats.count++;
      long t = getTiempoEfectivo(bloque[j].tiempo, bloque[j].penalty);

      if (t < 0) {
        stats.dnfs++;
      } else {
        suma += t;
        validos++;
        if (stats.bestSingle == -2 || t < stats.bestSingle) stats.bestSingle = t;
        if (stats.worstSingle == -2 || t > stats.worstSingle) stats.worstSingle = t;
      }

      // Ao5
      w5.push_back(t);
      if (w5.size() > 5) w5.pop_front();
      if (w5.size() == 5) {
        int32_t val = calcularAODeCola(w5, 5);
        if (val > 0 && (stats.bestAo5 == -2 || val < stats.bestAo5)) stats.bestAo5 = val;
      }

      // Ao12
      w12.push_back(t);
      if (w12.size() > 12) w12.pop_front();
      if (w12.size() == 12) {
        int32_t val = calcularAODeCola(w12, 12);
        if (val > 0 && (stats.bestAo12 == -2 || val < stats.bestAo12)) stats.bestAo12 = val;
      }

      // Ao50
      w50.push_back(t);
      if (w50.size() > 50) w50.pop_front();
      if (w50.size() == 50) {
        int32_t val = calcularAODeCola(w50, 50);
        if (val > 0 && (stats.bestAo50 == -2 || val < stats.bestAo50)) stats.bestAo50 = val;
      }

      // Ao100
      w100.push_back(t);
      if (w100.size() > 100) w100.pop_front();
      if (w100.size() == 100) {
        int32_t val = calcularAODeCola(w100, 100);
        if (val > 0 && (stats.bestAo100 == -2 || val < stats.bestAo100)) stats.bestAo100 = val;
      }

      // Ao1000
      w1000.push_back(t);
      if (w1000.size() > 1000) w1000.pop_front();
      if (w1000.size() == 1000) {
        int32_t val = calcularAODeCola(w1000, 1000);
        if (val > 0 && (stats.bestAo1000 == -2 || val < stats.bestAo1000)) stats.bestAo1000 = val;
      }
    }
  }

  if (validos > 0) {
    stats.media = (int32_t)(suma / validos);

    fileDat.seek(0);
    double sumaVarianza = 0.0;
    for (size_t i = 0; i < totalRecords; i += BUFFER_RECORDS) {
      size_t aLeer = std::min((size_t)BUFFER_RECORDS, totalRecords - i);
      fileDat.read((uint8_t*)bloque, aLeer * sizeof(SolveRecord));

      for (size_t j = 0; j < aLeer; j++) {
        if (bloque[j].archivado == 2) continue;
        long t = getTiempoEfectivo(bloque[j].tiempo, bloque[j].penalty);
        if (t >= 0) {
          double diff = t - stats.media;
          sumaVarianza += diff * diff;
        }
      }
    }
    stats.desviacion = std::sqrt(sumaVarianza / validos) / 1000.0f;
  }

  fileDat.close();

  stats.currentAo5    = (w5.size() >= 5)       ? calcularAODeCola(w5, 5)       : -2;
  stats.currentAo12   = (w12.size() >= 12)     ? calcularAODeCola(w12, 12)     : -2;
  stats.currentAo50   = (w50.size() >= 50)     ? calcularAODeCola(w50, 50)     : -2;
  stats.currentAo100  = (w100.size() >= 100)   ? calcularAODeCola(w100, 100)   : -2;
  stats.currentAo1000 = (w1000.size() >= 1000) ? calcularAODeCola(w1000, 1000) : -2;

  return stats;
}

void dibujarGrafica(size_t total, std::function<SessionItem(size_t)> obtenerItem, long tMin, long tMax) {
  if (total < 2 || tMin >= tMax) return;

  int gx = 65, gy = 30, gw = 235, gh = 120;

  int numPuntos = (total < (size_t)gw) ? total : gw;

  int prevX = -1, prevY = -1;
  long pr = -1;
  int prPrevX = -1, prPrevY = -1;

  int ao5PrevX = -1, ao5PrevY = -1;
  int ao12PrevX = -1, ao12PrevY = -1;

  std::deque<long> wAo5;
  std::deque<long> wAo12;
  size_t lastIdx = 0;

  for (int i = 0; i < numPuntos; i++) {
    size_t targetIdx = (total < (size_t)gw) ? i : (i * (total - 1)) / (gw - 1);

    while (lastIdx <= targetIdx) {
      SessionItem item = obtenerItem(lastIdx);
      long tRaw = getTiempoEfectivo(item.tiempo, item.penalty);
      wAo5.push_back(tRaw);
      if (wAo5.size() > 5) wAo5.pop_front();
      wAo12.push_back(tRaw);
      if (wAo12.size() > 12) wAo12.pop_front();
      lastIdx++;
    }

    int px = gx + (int)((i * (gw - 1)) / (numPuntos - 1));

    SessionItem targetItem = obtenerItem(targetIdx);
    long t = getTiempoEfectivo(targetItem.tiempo, targetItem.penalty);
    if (t >= 0) {
      int py = gy + gh - 1 - (int)(((t - tMin) * (gh - 1)) / (tMax - tMin));
      if (prevX != -1) tft.drawLine(prevX, prevY, px, py, TFT_CYAN);
      prevX = px;
      prevY = py;

      if (t < pr || pr == -1) {
        tft.fillCircle(px, py, 2, TFT_YELLOW);
        if (pr != -1) tft.drawLine(prPrevX, prPrevY, px, py, TFT_YELLOW);
        prPrevX = px;
        prPrevY = py;
        pr = t;
      }
    }

    int32_t valAo5 = calcularAODeCola(wAo5, 5);
    if (valAo5 >= 0 && valAo5 >= tMin && valAo5 <= tMax) {
      int pyAo5 = gy + gh - 1 - (int)(((valAo5 - tMin) * (gh - 1)) / (tMax - tMin));
      if (ao5PrevX != -1) tft.drawLine(ao5PrevX, ao5PrevY, px, pyAo5, TFT_RED);
      ao5PrevX = px;
      ao5PrevY = pyAo5;
    } else {
      ao5PrevX = -1;
    }

    int32_t valAo12 = calcularAODeCola(wAo12, 12);
    if (valAo12 >= 0 && valAo12 >= tMin && valAo12 <= tMax) {
      int pyAo12 = gy + gh - 1 - (int)(((valAo12 - tMin) * (gh - 1)) / (tMax - tMin));
      if (ao12PrevX != -1) tft.drawLine(ao12PrevX, ao12PrevY, px, pyAo12, TFT_GREEN);
      ao12PrevX = px;
      ao12PrevY = pyAo12;
    } else {
      ao12PrevX = -1;
    }
  }
}

void drawStats() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(201, 25, 59, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 213, TFT_BLACK);
  tft.drawFastHLine(1, 155, 320, TFT_WHITE);
  tft.drawFastHLine(1, 175, 320, TFT_WHITE);
  tft.drawFastVLine(160, 155, 20, TFT_WHITE);
  tft.drawRect(62, 27, 241, 126, TFT_WHITE);

  SessionStats stats;
  if (sessionGlobal == 0) {
    stats = calcularEstadisticasSesion();
  } else {
    stats = calcularEstadisticasGlobales();
  }

  if (sessionGlobal == 0) tft.drawFastHLine(1, 175, 159, TFT_BLACK);
  else tft.drawFastHLine(161, 175, 158, TFT_BLACK);

  int y = 50;
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("Solve", 20, y + 15);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("Best", 20, y + 30);
  tft.setTextColor(TFT_RED);
  tft.drawString("Ao5", 20, y + 45);
  tft.setTextColor(TFT_GREEN);
  tft.drawString("Ao12", 20, y + 60);
  tft.setTextColor(TFT_WHITE);

  tft.drawCentreString("mejor sesion", 80, 161, 1);
  tft.drawCentreString("mejor global", 240, 161, 1);

  if (stats.count == 0 || stats.count == 1) {
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Sin datos", 182, 80, 1);
    tft.setTextSize(2);
    tft.drawCentreString("Sin datos", 160, 200, 1);
    tft.setTextSize(1);
    tft.drawString("--", 20, 30);
    tft.drawString("--", 20, 142);
    return;
  }

  char buf[32];
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);

  // Fila 1: ao5 y ao1000
  formatearTiempoAO(stats.bestAo5, buf, sizeof(buf));
  tft.drawString("Ao5:    " + String(buf), 5, 180);

  formatearTiempoAO(stats.bestAo1000, buf, sizeof(buf));
  tft.drawString("Ao1000:      " + String(buf), 165, 180);

  // Fila 2: ao12 y cuenta
  formatearTiempoAO(stats.bestAo12, buf, sizeof(buf));
  tft.drawString("Ao12:   " + String(buf), 5, 195);

  snprintf(buf, sizeof(buf), "Cuenta:      %u", stats.count);
  tft.drawString(buf, 165, 195);

  // Fila 3: ao50 y media
  formatearTiempoAO(stats.bestAo50, buf, sizeof(buf));
  tft.drawString("Ao50:   " + String(buf), 5, 210);

  formatearTiempoAO(stats.media, buf, sizeof(buf));
  tft.drawString("Media:       " + String(buf), 165, 210);

  // Fila 4: ao100 y desviación
  formatearTiempoAO(stats.bestAo100, buf, sizeof(buf));
  tft.drawString("Ao100:  " + String(buf), 5, 225);

  snprintf(buf, sizeof(buf), "Desviacion:  %.2fs", stats.desviacion);
  tft.drawString(buf, 165, 225);

  if (stats.bestSingle != -1 && stats.worstSingle != -1 && stats.bestSingle != stats.worstSingle) {
    formatearTiempoAO(stats.worstSingle, buf, sizeof(buf));
    tft.drawString(buf, 20, 30);
    formatearTiempoAO(stats.bestSingle, buf, sizeof(buf));
    tft.drawString(buf, 20, 142);

    if (sessionGlobal == 0) {
      dibujarGrafica(
        sessionSolves.size(),
        [](size_t idx) -> SessionItem { return sessionSolves[idx]; },
        stats.bestSingle,
        stats.worstSingle
      );
    } else {
      String pathDat = getCubePath(".dat");
      File f = SD.open(pathDat.c_str(), FILE_READ);
      if (f && stats.count >= 2) {
        size_t totalRecords = f.size() / sizeof(SolveRecord);

        // Búfer de salida con solves válidas
        SessionItem bufferValidos[BUFFER_RECORDS];
        size_t bufferStartIdx = 0;
        size_t bufferCount = 0;

        // Búfer intermedio para lecturas eficientes de SD en bloques de 64
        SolveRecord bloqueSD[BUFFER_RECORDS];
        size_t fileRecordIdx = 0; // Índice global en el archivo
        size_t sdBufPos = 0;      // Cursor dentro del bloqueSD actual
        size_t sdBufLen = 0;      // Cuántos registros válidos se leyeron en bloqueSD

        dibujarGrafica(
          stats.count,
          [&](size_t targetIdx) -> SessionItem {
            // Si el índice solicitado está fuera del bloque actual de válidos, cargar el siguiente
            while (targetIdx >= bufferStartIdx + bufferCount && (fileRecordIdx < totalRecords || sdBufPos < sdBufLen)) {
              bufferStartIdx += bufferCount;
              bufferCount = 0;

              // Llenar bufferValidos hasta que se complete (64) o se termine el archivo
              while (bufferCount < BUFFER_RECORDS && (fileRecordIdx < totalRecords || sdBufPos < sdBufLen)) {
                // Si consumimos el bloqueSD de la SD, leemos el siguiente tramo físico
                if (sdBufPos >= sdBufLen) {
                  size_t aLeer = std::min((size_t)BUFFER_RECORDS, totalRecords - fileRecordIdx);
                  if (aLeer == 0) break;
                  f.seek(fileRecordIdx * sizeof(SolveRecord));
                  f.read((uint8_t*)bloqueSD, aLeer * sizeof(SolveRecord));
                  sdBufLen = aLeer;
                  sdBufPos = 0;
                }

                // Procesar elemento a elemento sin saltar nada
                if (bloqueSD[sdBufPos].archivado != 2) {
                  bufferValidos[bufferCount++] = {
                    bloqueSD[sdBufPos].tiempo,
                    bloqueSD[sdBufPos].penalty,
                    (uint32_t)(fileRecordIdx + sdBufPos)
                  };
                }

                sdBufPos++;
                if (sdBufPos >= sdBufLen) {
                  fileRecordIdx += sdBufLen;
                }
              }
            }

            if (targetIdx >= bufferStartIdx && targetIdx < bufferStartIdx + bufferCount) {
              return bufferValidos[targetIdx - bufferStartIdx];
            }

            return { -1, 2, 0 };
          },
          stats.bestSingle,
          stats.worstSingle
        );
        f.close();
      }
    }
  }

  tft.setTextSize(2);
}

void drawSettings() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(261, 25, 58, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 213, TFT_BLACK);
  
  tft.setTextSize(2);

  //inspección
  tft.drawString("Habilitar inspeccion", 5, 40);
  if (!inspection) {
    tft.fillRect(285, 30, 25, 25, TFT_BLACK);
    tft.drawRect(285, 30, 25, 25, TFT_WHITE);
  } else tft.fillRect(285, 30, 25, 25, TFT_WHITE);
  tft.drawFastHLine(1, 60, 320, TFT_WHITE);

  //esconder tiempo
  tft.drawString("Esconder tiempo", 5, 75);
  if (!hideTime) {
    tft.fillRect(285, 65, 25, 25, TFT_BLACK);
    tft.drawRect(285, 65, 25, 25, TFT_WHITE);
  } else tft.fillRect(285, 65, 25, 25, TFT_WHITE);
  tft.drawFastHLine(1, 95, 320, TFT_WHITE);
}

void loadSettings() {
  if (!sdDisponible) return;

  const char* path = "/settings.dat";

  if (!SD.exists(path)) {
    saveSettings();
    return;
  }

  File file = SD.open(path, FILE_READ);
  if (!file) return;

  if (file.size() < sizeof(SettingsData)) {
    file.close();
    saveSettings();
    return;
  }

  SettingsData data;
  if (file.read((uint8_t*)&data, sizeof(SettingsData)) == sizeof(SettingsData)) {
    inspection = (data.inspection == 1);
    hideTime   = (data.hideTime == 1);
    if (data.cuboActual >= 0 && (size_t)data.cuboActual < TOTAL_CUBOS) {
      cuboActual = data.cuboActual;
    }
  }
  file.close();
}

void saveSettings() {
  if (!sdDisponible) return;

  const char* path = "/settings.dat";

  File file = SD.open(path, FILE_WRITE);
  if (!file) return;

  SettingsData data = {
    (uint8_t)(inspection ? 1 : 0),
    (uint8_t)(hideTime ? 1 : 0),
    (int32_t)cuboActual
  };
  file.write((const uint8_t*)&data, sizeof(SettingsData));
  file.close();
}

void drawTimer() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(81, 25, 59, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 213, TFT_BLACK);
  tft.fillRect(1, 1, 79, 23, TFT_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(CUBOS[cuboActual].label, 11, 6, 1);
  
  tft.drawFastHLine(0, 165, 320, TFT_WHITE);

  tft.pushImage(10,  175, bigIconW, bigIconH, vermezclaD);
  tft.pushImage(75,  205, iconW,    iconH,    binD);
  tft.pushImage(112, 205, iconW,    iconH,    againD);
  tft.pushImage(150, 206, iconW,    iconH,    dnfD);
  tft.pushImage(186, 205, iconW,    iconH,    plustwoD);
  tft.pushImage(222, 205, iconW,    iconH,    nuevamezclaD);
  tft.pushImage(256, 175, bigIconW, bigIconH, bigsolvesD);
}

void drawAverages() {
  tft.fillRect(120, 55, 199, 110, TFT_BLACK);
  tft.fillRect(1, 75, 129, 90, TFT_BLACK);
  tft.fillRect(246, 163, 73, 5, TFT_BLACK);
  tft.drawFastHLine(120, 55, 318, TFT_WHITE);
  tft.drawFastHLine(44, 75, 77, TFT_WHITE);
  tft.drawFastVLine(120, 55, 20, TFT_WHITE);
  tft.drawFastVLine(44, 75, 90, TFT_WHITE);
  tft.drawFastVLine(245, 165, 30, TFT_WHITE);

  int32_t ao5    = (sessionSolves.size() >= 5)    ? calcularAO(5)    : -2;
  int32_t ao12   = (sessionSolves.size() >= 12)   ? calcularAO(12)   : -2;
  int32_t ao50   = (sessionSolves.size() >= 50)   ? calcularAO(50)   : -2;
  int32_t ao100  = (sessionSolves.size() >= 100)  ? calcularAO(100)  : -2;
  int32_t ao1000 = (sessionSolves.size() >= 1000) ? calcularAO(1000) : -2;

  int32_t bestSingle = -1;
  int64_t sumaTiempos = 0;
  uint32_t validos = 0;

  for (size_t i = 0; i < sessionSolves.size(); i++) {
    long t = getTiempoEfectivo(sessionSolves[i].tiempo, sessionSolves[i].penalty);
    if (t >= 0) {
      if (bestSingle == -1 || t < bestSingle) {
        bestSingle = t;
      }
      sumaTiempos += t;
      validos++;
    }
  }

  long lastTimes[5] = {-2, -2, -2, -2, -2};
  int total = sessionSolves.size();
  for (int i = 0; i < 5 && i < total; i++) {
    const auto& solve = sessionSolves[total - 1 - i];
    lastTimes[i] = getTiempoEfectivo(solve.tiempo, solve.penalty);
  }

  int32_t mediaAritmetica = (validos > 0) ? (int32_t)(sumaTiempos / validos) : -2;

  char buf[16];
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Columna Izquierda (Ao5, Ao12, Ao50, Ao100)
  tft.drawString("Ao5: ", 54, 85);
  formatearTiempoAO(ao5, buf, sizeof(buf));
  tft.drawString(buf, 92, 85);

  tft.drawString("Ao12: ", 54, 105);
  formatearTiempoAO(ao12, buf, sizeof(buf));
  tft.drawString(buf, 92, 105);

  tft.drawString("Ao50: ", 54, 125);
  formatearTiempoAO(ao50, buf, sizeof(buf));
  tft.drawString(buf, 92, 125);

  tft.drawString("Ao100: ", 54, 145);
  formatearTiempoAO(ao100, buf, sizeof(buf));
  tft.drawString(buf, 92, 145);

  // Columna Derecha (Ao1k, Media, Best, Cuenta)
  tft.drawString("Ao1000: ", 155, 85);
  formatearTiempoAO(ao1000, buf, sizeof(buf));
  tft.drawString(buf, 200, 85);

  tft.drawString("Media: ", 155, 105);
  formatearTiempoAO(mediaAritmetica, buf, sizeof(buf));
  tft.drawString(buf, 200, 105);

  tft.drawString("Best: ", 155, 125);
  formatearTiempoAO((bestSingle != -1) ? bestSingle : -2, buf, sizeof(buf));
  tft.drawString(buf, 200, 125);

  tft.drawString("Cuenta: ", 155, 145);
  snprintf(buf, sizeof(buf), "%u", (unsigned int)sessionSolves.size());
  tft.drawString(buf, 200, 145);

  tft.drawString("Resultados recientes: ", 130, 65);
  for (int i = 0; i < 5; i++) {
    formatearTiempoAO(lastTimes[i], buf, sizeof(buf));
    tft.drawString(buf, 265, 65 + (i * 20));
  }

  tft.setTextSize(2);
}

void formatearTiempoAO(int32_t ms, char* buffer, size_t len) {
  if (ms == -1) {
    snprintf(buffer, len, "DNF");
  } else if (ms == -2) {
    snprintf(buffer, len, "--");
  } else {
    unsigned long minutos = ms / 60000;
    unsigned long segundos = (ms % 60000) / 1000;
    unsigned long centesimas = (ms % 1000) / 10;
    if (minutos > 0) {
      snprintf(buffer, len, "%lu:%02lu.%02lu", minutos, segundos, centesimas);
    } else {
      snprintf(buffer, len, "%lu.%02lu", segundos, centesimas);
    }
  }
}

void drawIntroMessage() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, 320, 240, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(4);
  tft.drawCentreString("PocketTimer", 160, 90, 1);
  tft.setTextSize(2);
  tft.drawCentreString("by Vic", 160, 140, 1);
}

void drawMezcla() {
  tft.fillRect(1, 50, 220, 115, TFT_BLACK);
  tft.fillRect(1, 163, 74, 5, TFT_BLACK);
  tft.drawFastHLine(1, 50, 220, TFT_WHITE);
  tft.drawFastVLine(220, 50, 115, TFT_WHITE);
  tft.drawFastVLine(75, 165, 30, TFT_WHITE);

  //TODO hay que escribir el código para que se muestre la mezcla
}

void drawCubes() {
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.fillRect(1, 25, 79, 5, TFT_BLACK);
  tft.fillRect(1, 26, 318, 213, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  for (size_t i = 0; i < TOTAL_CUBOS; i++) {
    if (CUBOS[i].page == paginaCubos) {
      tft.pushImage(CUBOS[i].x, CUBOS[i].y, bigIconW, bigIconH, CUBOS[i].icon);
    }
  }
  
  tft.drawString((paginaCubos == 1) ? "[>]" : "[<]", FLECHA_X, FLECHA_Y, 1);
}

void drawBasic() {
  tft.drawRect(0, 0, 320, 240, TFT_WHITE);
  tft.drawFastHLine(0, 25, 320, TFT_WHITE);
  tft.drawFastVLine(80,  0, 25, TFT_WHITE);
  tft.drawFastVLine(140, 0, 25, TFT_WHITE);
  tft.drawFastVLine(200, 0, 25, TFT_WHITE);
  tft.drawFastVLine(260, 0, 25, TFT_WHITE);

  tft.pushImage(97,  1, iconW, iconH, cronoD);
  tft.pushImage(157, 1, iconW, iconH, solvesD);
  tft.pushImage(217, 1, iconW, iconH, estatsD);
  tft.pushImage(277, 1, iconW, iconH, settingsD);
}

void abrirPopupSiNo(int type, int n) {
  popupSiNoVisible = true;
  accionPendiente = type;
  parametroN = n;

  String option = "";
  switch (type) {
    case ELIMINAR_TIEMPO:
      option = "eliminar este tiempo";
      break;
    case ELIMINAR_SESION:
      option = "eliminar sesion";
      break;
    case DESARCHIVAR_TIEMPOS:
      option = String("desarch. ") + n + " tiempos";
      break;
    case ARCHIVAR_SESION:
      option = "archivar sesion";
      break;
    case ARCHIVAR_TIEMPO:
      option = "archivar tiempo";
      break;
  }

  // Cuadro del popup
  tft.fillRect(15, 60, 290, 120, TFT_BLACK);
  tft.drawRect(15, 60, 290, 120, TFT_WHITE);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.drawCentreString("Confirmar:", 160, 75, 1);
  tft.drawCentreString(option + "?", 160, 95, 1);

  // Botón SÍ
  tft.drawRect(60, 130, 80, 30, TFT_WHITE);
  tft.setTextSize(2);
  tft.drawCentreString("SI", 100, 137, 1);

  // Botón NO
  tft.drawRect(180, 130, 80, 30, TFT_WHITE);
  tft.drawCentreString("NO", 220, 137, 1);
}

bool procesarToquePopupSiNo(uint16_t touchX, uint16_t touchY) {
  if (!popupSiNoVisible) return false;

  // Botón SÍ
  if (puntoEnArea(touchX, touchY, 60, 130, 80, 30)) {
    ultimoToque = millis();
    popupSiNoVisible = false;

    switch (accionPendiente) {
      case ELIMINAR_TIEMPO:
        if (pantallaActual == 1) {
          eliminarUltimaSolve();
        } else if (pantallaActual == 2) {
          eliminarSolve();
        }
        break;
      case ELIMINAR_SESION:
        modificarSesion(ELIMINAR_SESION);
        break;
      case DESARCHIVAR_TIEMPOS:
        desarchivarTiempos();
        break;
      case ARCHIVAR_SESION:
        modificarSesion(ARCHIVAR_SESION);
        break;
      case ARCHIVAR_TIEMPO:
        solvePopup.record.archivado = solvePopup.record.archivado ? 0 : 1;
        uint32_t idxSD = sessionSolves[solvePopup.indiceGlobal].indexSD;
        String pathDat = getCubePath(".dat");
        File f = SD.open(pathDat.c_str(), "r+");
        if (f) {
          f.seek((size_t)idxSD * sizeof(SolveRecord));
          f.write((const uint8_t*)&solvePopup.record, sizeof(SolveRecord));
          f.close();
        }
        loadSession();
        cerrarPopupSolve();
        break;
    }

    // Redibujar la pantalla que corresponda
    if (pantallaActual == 1) {
      drawTimer();
      imprimirAlgoritmo(mezcla);
      mostrarTiempo(tiempoTranscurrido);
      mezclaShown = false;
      averagesShown = false;
    } else if (pantallaActual == 2) {
      popupSolveVisible = false;
      drawTimes();
    }
    return true;
  }

  // Botón NO
  if (puntoEnArea(touchX, touchY, 180, 130, 80, 30)) {
    ultimoToque = millis();
    popupSiNoVisible = false;

    if (pantallaActual == 1) {
      drawTimer();
      imprimirAlgoritmo(mezcla);
      mostrarTiempo(tiempoTranscurrido);
      mezclaShown = false;
      averagesShown = false;
    } else if (pantallaActual == 2) {
      if (popupSolveVisible) {
        drawPopupSolve();
      } else {
        drawTimes();
      }
    }
    return true;
  }

  return true;
}

void abrirPopupNum() {
  popupNumVisible = true;

  tft.fillRect(40, 40, 240, 160, TFT_BLACK);
  tft.drawRect(40, 40, 240, 160, TFT_WHITE);

  tft.drawFastHLine(40, 80, 240, TFT_WHITE);
  tft.drawFastHLine(40, 120, 240, TFT_WHITE);
  tft.drawFastHLine(40, 160, 240, TFT_WHITE);
  tft.drawFastVLine(100, 80, 120, TFT_WHITE);
  tft.drawFastVLine(160, 80, 120, TFT_WHITE);
  tft.drawFastVLine(220, 80, 120, TFT_WHITE);

  tft.setTextSize(2);
  tft.drawRect(253, 45, 22, 22, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("X", 265, 49, 1);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.drawString("Desarchivar:", 45, 53);
  tft.drawString("(max. 20k)", 45, 62);
  tft.setTextSize(2);
  tft.drawString("1", 65, 92);
  tft.drawString("2", 125, 92);
  tft.drawString("3", 185, 92);
  tft.drawString("4", 65, 132);
  tft.drawString("5", 125, 132);
  tft.drawString("6", 185, 132); // jaja
  tft.drawString("7", 65, 172); // sixseven
  tft.drawString("8", 125, 172);
  tft.drawString("9", 185, 172);
  tft.drawString("0", 245, 132);
  tft.drawString("<-", 237, 92);
  tft.drawString("OK", 238, 172);
}

void procesarToquePopupNum(uint16_t touchX, uint16_t touchY) {
  if (!popupNumVisible) return;

  static String bufferTeclado = "0";
  char digito = '\0';

  if (puntoEnArea(touchX, touchY, 40, 80, 60, 40)) digito = '1';
  else if (puntoEnArea(touchX, touchY, 100, 80, 60, 40)) digito = '2';
  else if (puntoEnArea(touchX, touchY, 160, 80, 60, 40)) digito = '3';
  else if (puntoEnArea(touchX, touchY, 40, 120, 60, 40)) digito = '4';
  else if (puntoEnArea(touchX, touchY, 100, 120, 60, 40)) digito = '5';
  else if (puntoEnArea(touchX, touchY, 160, 120, 60, 40)) digito = '6';
  else if (puntoEnArea(touchX, touchY, 40, 160, 60, 40)) digito = '7';
  else if (puntoEnArea(touchX, touchY, 100, 160, 60, 40)) digito = '8';
  else if (puntoEnArea(touchX, touchY, 160, 160, 60, 40)) digito = '9';
  else if (puntoEnArea(touchX, touchY, 220, 120, 60, 40)) digito = '0';
  // Retroceso
  else if (puntoEnArea(touchX, touchY, 220, 80, 60, 40)) {
    ultimoToque = millis();
    if (bufferTeclado.length() > 0) {
      bufferTeclado.remove(bufferTeclado.length() - 1);
    }
  }
  // OK
  else if (puntoEnArea(touchX, touchY, 220, 160, 60, 40)) {
    parametroN = bufferTeclado.toInt() < 20000 ? bufferTeclado.toInt() : 20000;
    bufferTeclado = "";
    popupNumVisible = false;
    drawTimes();
    abrirPopupSiNo(DESARCHIVAR_TIEMPOS, parametroN);
    return;
  }
  // cerrar
  else if (puntoEnArea(touchX, touchY, 253, 45, 22, 22)) {
    ultimoToque = millis();
    bufferTeclado = "";
    popupNumVisible = false;
    drawTimes();
    return;
  }
  else {
    return;
  }

  if (digito != '\0') {
    ultimoToque = millis();
    if (bufferTeclado == "0") bufferTeclado = "";
    if (bufferTeclado.length() < 5) {
      bufferTeclado += digito;
    }
  }

  tft.fillRect(160, 41, 89, 38, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int bufferText = 0;
  if (bufferTeclado.length() == 0) bufferText = tft.textWidth(" ", 1);
  else bufferText = tft.textWidth(" ", 1) * bufferTeclado.length();
  tft.drawString(bufferTeclado.length() > 0 ? bufferTeclado : "0", 245 - bufferText, 52);
}

String generarMezcla() {
  paginaScramble = 0;
  return CUBOS[cuboActual].scrambler().c_str();
}

void mostrarTiempo(long ms) {
  static bool puntosPintados = false;

  tft.setTextSize(3);

  if (hideTime && estado == CORRIENDO) {
    if (!puntosPintados) {
      tft.fillRect(75, 175, 150, 25, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawCentreString("...", 158, 175, 1);
      puntosPintados = true;
    }
    tft.setTextSize(2);
    return;
  }
  puntosPintados = false;

  tft.fillRect(75, 175, 150, 25, TFT_BLACK);

  if (ms < 0) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawCentreString("DNF", 158, 175, 1);
  } else {
    if (estado == ESPERANDO) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
    } else if (estado == PREPARADO) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    unsigned long minutos = ms / 60000;
    unsigned long segundos = (ms % 60000) / 1000;
    unsigned long centesimas = (ms % 1000) / 10;

    char buffer[16];
    if (minutos > 0) {
      snprintf(buffer, sizeof(buffer), "%lu:%02lu.%02lu", minutos, segundos, centesimas);
    } else {
      snprintf(buffer, sizeof(buffer), "%lu.%02lu", segundos, centesimas);
    }
    tft.drawCentreString(buffer, 158, 175, 1);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
}

void drawInspection(unsigned long transcurrido) {
  if (averagesShown) {
    tft.fillRect(100, 75, 219, 90, TFT_BLACK);
    tft.drawFastVLine(245, 165, 60, TFT_BLACK);
    tft.drawFastHLine(0, 165, 320, TFT_WHITE);
    averagesShown = false;
  }
  if (mezclaShown) {
    tft.fillRect(1, 50, 220, 115, TFT_BLACK);
    tft.drawFastVLine(75, 165, 60, TFT_BLACK);
    tft.drawFastHLine(0, 165, 320, TFT_WHITE);
    mezclaShown = false;
  }
  int segundosRestantes = 15 - (int)(transcurrido / 1000);
  int estadoVisual = (transcurrido >= 15000) ? 99 : segundosRestantes;

  if (estadoVisual == ultimoSegundoPintado) return;
  ultimoSegundoPintado = estadoVisual;

  tft.fillRect(1, 26, 318, 139, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(6);

  if (transcurrido < 8000) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  } else if (transcurrido < 12000) {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK); // Ámbar a los 8s
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);    // Rojo a los 12s
  }

  char buf[8];
  if (transcurrido < 15000) {
    snprintf(buf, sizeof(buf), "%d", segundosRestantes);
    tft.drawCentreString(buf, 160, 70, 1);
  } else {
    tft.drawCentreString("+2", 160, 70, 1);
  }

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void imprimirAlgoritmo(const String& algoritmo) {
  tft.fillRect(1, 26, 318, 139, TFT_BLACK);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int x = 10, y = 30;
  int cursorX = x, cursorY = y;
  int anchoMax = 310;
  int espacioAncho = tft.textWidth(" ", 1);
  int altoLinea = tft.fontHeight(1);

  bool requierePaginacion = (cuboActual >= 4 && cuboActual <= 5) || cuboActual == 10;
  int movActual = 0;
  String movimiento = "";

  for (unsigned int i = 0; i <= algoritmo.length(); i++) {
    if (i < algoritmo.length() && algoritmo[i] != ' ') {
      movimiento += algoritmo[i];
    } else if (movimiento.length() > 0) {
      movActual++;
      int anchoMov = tft.textWidth(movimiento, 1);

      if (cursorX + anchoMov > x + anchoMax) {
        cursorX = x;
        cursorY += altoLinea;
      }

      bool colision = requierePaginacion && (cursorY >= 135) && (cursorX + anchoMov >= 240);
      bool dentro = (cursorY <= 150);
      bool dibujar = (!requierePaginacion || 
                     (paginaScramble == 0 && movActual <= 50) ||
                     (paginaScramble == 1 && movActual > 50)) && !colision && dentro;

      if (dibujar) {
        tft.setCursor(cursorX, cursorY);
        tft.print(movimiento);
        cursorX += anchoMov + espacioAncho;
      }
      movimiento = "";
    }
  }

  if (requierePaginacion) {
    tft.drawString((paginaScramble == 0) ? "[1/2]" : "[2/2]", 250, 145, 1);
  }
}

void registrarTiempo(long ms) {
  if (!sdDisponible) return;

  String pathDat = getCubePath(".dat");
  File fileDatCheck = SD.open(pathDat.c_str(), FILE_READ);
  uint32_t nuevoIndexSD = 0;
  if (fileDatCheck) {
    nuevoIndexSD = fileDatCheck.size() / sizeof(SolveRecord);
    fileDatCheck.close();
  }

  addToSession((int32_t)ms, 0, nuevoIndexSD);

  String pathTxt = getCubePath(".txt");
  File fileTxt = SD.open(pathTxt.c_str(), FILE_APPEND);
  if (!fileTxt) return;
  
  uint32_t offsetMezcla = fileTxt.size();
  uint32_t longMezcla = ultimaMezcla.length();
  fileTxt.print(ultimaMezcla);
  fileTxt.close();

  SolveRecord solve = { 
    (int32_t)ms, 
    0, // archivado
    0, // penalty
    offsetMezcla, 
    longMezcla 
  };
  ultimaSolve = solve;

  File fileDat = SD.open(pathDat.c_str(), FILE_APPEND);
  if (!fileDat) return;
  fileDat.write((const uint8_t*)&solve, sizeof(SolveRecord));
  fileDat.close();
}

int32_t calcularAO(int n) {
  if (sessionSolves.size() < (size_t)n) return -1;

  std::vector<long> ventana;
  ventana.reserve(n);

  int dnfs = 0;
  for (auto it = sessionSolves.end() - n; it != sessionSolves.end(); ++it) {
    long t = getTiempoEfectivo(it->tiempo, it->penalty);
    if (t < 0) {
      dnfs++;
      ventana.push_back(2147483647);
    } else {
      ventana.push_back(t);
    }
  }

  if (dnfs > 1) return -1;

  std::sort(ventana.begin(), ventana.end());

  int64_t suma = 0;
  for (int i = 1; i < n - 1; i++) {
    suma += ventana[i];
  }

  return (int32_t)(suma / (n - 2));
}

void addToSession(int32_t ms, uint8_t penalty, uint32_t indexSD) {
  if (sessionSolves.size() >= MAX_BUFFER_SOLVES) {
    sessionSolves.pop_front();
  }
  sessionSolves.push_back({ms, penalty, indexSD});
}

String getCubePath(String appendage) {
  return "/" + String(CUBOS[cuboActual].file) + appendage;
}

void actualizarRegistro(uint8_t nuevaPenalty) {
  if (!sdDisponible || sessionSolves.empty()) return;

  ultimaSolve.penalty = nuevaPenalty;
  sessionSolves.back().penalty = nuevaPenalty;

  String path = getCubePath(".dat");
  File file = SD.open(path.c_str(), "r+");
  if (!file) return;

  size_t size = file.size();
  if (size < sizeof(SolveRecord)) {
    file.close();
    return;
  }

  size_t posUltimo = size - sizeof(SolveRecord);
  file.seek(posUltimo);
  file.write((const uint8_t*)&ultimaSolve, sizeof(SolveRecord));
  file.close();
}

void eliminarUltimaSolve() {
  if (!sdDisponible || sessionSolves.empty()) return;

  sessionSolves.pop_back();

  String path = getCubePath(".dat");
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) return;

  size_t size = file.size();
  if (size < sizeof(SolveRecord)) {
    file.close();
    return;
  }

  SolveRecord registroAEliminar;
  file.seek(size - sizeof(SolveRecord));
  file.read((uint8_t*)&registroAEliminar, sizeof(SolveRecord));
  file.close();

  String posixTxt = "/sd" + getCubePath(".txt");
  truncate(posixTxt.c_str(), registroAEliminar.offsetMezcla);

  String posix = "/sd" + path;
  truncate(posix.c_str(), size - sizeof(SolveRecord));

  tiempoTranscurrido = 0;
  ultimaSolve = {};
  mostrarTiempo(0);
}

void eliminarSolve() {
  if (!sdDisponible || sessionSolves.empty()) return;

  uint8_t nuevoEstado = 2;
  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), "r+");
  if (!fileDat) return;
  fileDat.seek(sessionSolves[solvePopup.indiceGlobal].indexSD * sizeof(SolveRecord) + sizeof(int32_t));
  fileDat.write(&nuevoEstado, sizeof(uint8_t));
  fileDat.close();
  loadSession();
  cerrarPopupSolve();
}

void modificarSesion(int type) {
  if (!sdDisponible || sessionSolves.empty()) return;

  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), "r+");
  if (!fileDat) return;

  int estado = -1;
  switch (type) {
    case ARCHIVAR_SESION:
      estado = 1;
      break;
    case ELIMINAR_SESION: //TODO reimplementar este método para limpiar los archivos cuando se eliminen bloques grandes de solves.
      estado = 2;
      break;
  }
  for (int i = 0; i < sessionSolves.size(); i++) {
    size_t offsetArchivado = (size_t)sessionSolves[i].indexSD * sizeof(SolveRecord) + sizeof(int32_t);
    fileDat.seek(offsetArchivado);
    fileDat.write((const uint8_t*)&estado, sizeof(uint8_t));
  }
  fileDat.close();

  if (type == ELIMINAR_SESION) {
    compactarArchivosCubo();
  }
  
  sessionSolves.clear();
  ultimaSolve = {};
  paginaSolves = 0;
  tiempoTranscurrido = 0;
  drawTimes();
}

void desarchivarTiempos() {
  if (parametroN <= 0 || !sdDisponible) return;

  String pathDat = getCubePath(".dat");
  File fileDat = SD.open(pathDat.c_str(), "r+");
  if (!fileDat) return;

  size_t totalRecords = fileDat.size() / sizeof(SolveRecord);
  if (totalRecords == 0) {
    fileDat.close();
    return;
  }

  int desarchivados = 0;
  int pos = totalRecords - 1;
  uint8_t estado = 0;
  uint8_t nuevoEstado = 0;

  while (desarchivados < parametroN && pos >= 0) {
    size_t offsetArchivado = (size_t)pos * sizeof(SolveRecord) + sizeof(int32_t);
    
    fileDat.seek(offsetArchivado);
    fileDat.read(&estado, sizeof(uint8_t));

    if (estado == 1) {
      fileDat.seek(offsetArchivado);
      fileDat.write(&nuevoEstado, sizeof(uint8_t));
      desarchivados++;
    }
    pos--;
  }
  fileDat.close();

  loadSession();
  drawTimes();
}

void loadSession() {
  sessionSolves.clear();
  ultimaSolve = {};
  ultimaMezcla = "";

  if (!sdDisponible) return;

  String path = getCubePath(".dat");
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) return;

  size_t totalRecords = file.size() / sizeof(SolveRecord);
  if (totalRecords == 0) {
    file.close();
    return;
  }

  SolveRecord bloque[BUFFER_RECORDS];

  uint32_t registroIdx = 0;
  for (size_t i = 0; i < totalRecords; i += BUFFER_RECORDS) {
    size_t aLeer = std::min((size_t)BUFFER_RECORDS, totalRecords - i);
    file.read((uint8_t*)bloque, aLeer * sizeof(SolveRecord));

    for (size_t j = 0; j < aLeer; j++) {
      if (bloque[j].archivado == 0) {
        addToSession(bloque[j].tiempo, bloque[j].penalty, registroIdx);
      }
      registroIdx++;
    }
  }
  file.close();
}

void compactarArchivosCubo() {
  if (!sdDisponible) return;

  String pathDat = getCubePath(".dat");
  String pathTxt = getCubePath(".txt");
  String pathDatTmp = getCubePath(".dtt");
  String pathTxtTmp = getCubePath(".ttt");

  File fDat = SD.open(pathDat.c_str(), FILE_READ);
  if (!fDat) return;

  File fTxt = SD.open(pathTxt.c_str(), FILE_READ);
  bool tieneTxt = (fTxt && fTxt.size() > 0);

  // Asegurar que no existan temporales previos huérfanos
  if (SD.exists(pathDatTmp.c_str())) SD.remove(pathDatTmp.c_str());
  if (SD.exists(pathTxtTmp.c_str())) SD.remove(pathTxtTmp.c_str());

  File fDatTmp = SD.open(pathDatTmp.c_str(), FILE_WRITE);
  File fTxtTmp = tieneTxt ? SD.open(pathTxtTmp.c_str(), FILE_WRITE) : File();

  if (!fDatTmp || (tieneTxt && !fTxtTmp)) {
    if (fDat) fDat.close();
    if (fTxt) fTxt.close();
    if (fDatTmp) fDatTmp.close();
    if (fTxtTmp) fTxtTmp.close();
    return;
  }

  size_t totalRecords = fDat.size() / sizeof(SolveRecord);
  SolveRecord bloqueEntrada[BUFFER_RECORDS];
  SolveRecord bloqueSalida[BUFFER_RECORDS];
  size_t salidaCount = 0;

  uint8_t bufferCopiaTxt[256];
  uint32_t nuevoOffsetTxt = 0;

  for (size_t i = 0; i < totalRecords; i += BUFFER_RECORDS) {
    size_t aLeer = std::min((size_t)BUFFER_RECORDS, totalRecords - i);
    fDat.read((uint8_t*)bloqueEntrada, aLeer * sizeof(SolveRecord));

    for (size_t j = 0; j < aLeer; j++) {
      if (bloqueEntrada[j].archivado == 2) continue; // Descartar eliminadas

      SolveRecord rec = bloqueEntrada[j];

      // Copiar la mezcla si existe
      if (tieneTxt && rec.longMezcla > 0) {
        fTxt.seek(rec.offsetMezcla);

        uint32_t restante = rec.longMezcla;
        while (restante > 0) {
          size_t tramo = std::min((size_t)sizeof(bufferCopiaTxt), (size_t)restante);
          fTxt.read(bufferCopiaTxt, tramo);
          fTxtTmp.write(bufferCopiaTxt, tramo);
          restante -= tramo;
        }

        rec.offsetMezcla = nuevoOffsetTxt;
        nuevoOffsetTxt += rec.longMezcla;
      } else {
        rec.offsetMezcla = 0;
        rec.longMezcla = 0;
      }

      bloqueSalida[salidaCount++] = rec;

      if (salidaCount == BUFFER_RECORDS) {
        fDatTmp.write((const uint8_t*)bloqueSalida, salidaCount * sizeof(SolveRecord));
        salidaCount = 0;
      }
    }
  }

  if (salidaCount > 0) {
    fDatTmp.write((const uint8_t*)bloqueSalida, salidaCount * sizeof(SolveRecord));
  }

  // Cerrar todos los descriptores antes de manipular archivos en el sistema
  fDat.close();
  if (fTxt) fTxt.close();
  fDatTmp.close();
  if (fTxtTmp) fTxtTmp.close();

  // Reemplazo atómico seguro para el .dat
  SD.remove(pathDat.c_str());
  SD.rename(pathDatTmp.c_str(), pathDat.c_str());

  // Reemplazo para el .txt
  if (tieneTxt) {
    SD.remove(pathTxt.c_str());
    SD.rename(pathTxtTmp.c_str(), pathTxt.c_str());
  }

  // Reconstruir la sesión en RAM con los nuevos indexSD
  loadSession();
}