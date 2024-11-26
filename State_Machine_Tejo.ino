#include <MD_MAX72xx.h>
#include <SPI.h>

#define DEBUG 1  // Enable or disable (default) debugging output

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN 13   // or SCK
#define DATA_PIN 11  // or MOSI
#define CS_PIN 10    // or SS

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);  // SPI hardware interface

//#define SCROLL_DELAY 30  // Delay for scroll speed (adjust as needed)
uint32_t SCROLL_DELAY = 40;
#define CHAR_SPACING 1   // pixels between characters
#define BUF_SIZE 75      // buffer size for the message


unsigned long tiempoInicio = 0;  // Marca de tiempo para el inicio de la condición
const unsigned long tiempoRequerido = 3000; //Tiempo, que dura la palabra que indica la cantidad de puntos del jugador actual
bool Flag_Puntos_Show = true;

//Mensaje en pantalla
char mensaje[50];
volatile bool interrupt = false;
const char* newMessage = nullptr;
//Actualizar mensaje
void updateMessage(const char* msg) {
  interrupt = true;
  newMessage = msg;
}


// ========== General Variables ===========//
uint32_t prevTimeAnim = 0;  // Used for remembering the millis() value in animations

// ========== Text routines ===========//
const char *msgTab[] = {
  "CANCHA DE TEJO DIGITAL"
};

// Function to handle the scrolling text animation
bool scrollText(bool bInit, const char *pmsg) {
  static char curMessage[BUF_SIZE];
  static char *p = curMessage;
  static uint8_t state = 0;
  static uint8_t curLen, showLen;
  static uint8_t cBuf[8];
  uint8_t colData;

  if (interrupt) {
    interrupt = false;
    bInit = true;
    pmsg = newMessage;
    newMessage = nullptr;
  }
  // Initialize the message
  if (bInit) {
    resetMatrix();
    strcpy(curMessage, pmsg);
    state = 0;
    p = curMessage;
    bInit = false;
  }

  // Is it time to scroll the text?
  if (millis() - prevTimeAnim < SCROLL_DELAY)
    return (bInit);

  // Scroll the display
  mx.transform(MD_MAX72XX::TSL);  // Scroll along
  prevTimeAnim = millis();        // Update for the next scroll

  // Handle the scroll animation states
  switch (state) {
    case 0:  // Load the next character from the font table
      showLen = mx.getChar(*p++, sizeof(cBuf) / sizeof(cBuf[0]), cBuf);
      curLen = 0;
      state = 1;
      // Fallthrough to next state to start displaying

    case 1:  // Display the next part of the character
      colData = cBuf[curLen++];
      mx.setColumn(0, colData);
      if (curLen == showLen) {
        showLen = ((*p != '\0') ? CHAR_SPACING : mx.getColumnCount() - 1);
        curLen = 0;
        state = 2;
      }
      break;

    case 2:  // Display inter-character spacing (blank column)
      mx.setColumn(0, 0);
      if (++curLen == showLen) {
        state = 0;
        bInit = (*p == '\0');
      }
      break;

    default:
      state = 0;
  }

  return (bInit);
}

// Reset the display
void resetMatrix(void) {
  mx.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY / 2);
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  mx.clear();
  prevTimeAnim = 0;
}


// Configuración de pines
const int sensorPines[4] = {5, 6, 7, 8}; // Sensores de puntaje 1
const int sensorBonusPin = 4;            // Sensor de puntaje 6
const int botonJugadoresPin = 2;         // Botón para seleccionar jugadores
const int botonInicioPin = 3;            // Botón para iniciar el juego

// Variables de juego
int estadoActual = 0;           // Estado de la máquina de estados
const int STANDBY = 0;
const int SELECT_PLAYERS = 1;
const int PLAYING_TURN = 2;
const int GAME_OVER = 3;

int numJugadores = 1;           // Cantidad de jugadores seleccionados
int puntajes[4] = {0, 0, 0, 0}; // Puntajes de los jugadores
int jugadorActual = 0;          // Índice del jugador actual
const int puntajeMeta = 10;     // Puntaje necesario para ganar

// Variables de temporizadores
unsigned long lastButtonTime = 0;      // Última vez que se registró un botón
unsigned long lastSensorTime = 0;      // Última vez que se detectó un sensor
unsigned long lastActivityTime = 0;    // Última actividad registrada
const unsigned long debounceDelay = 200; // Tiempo de debounce para botones
const unsigned long inactivityLimit = 30000; // Tiempo máximo de inactividad

// Configuración inicial
void setup() {
  Serial.begin(115200);

  // Configurar pines como entradas
  pinMode(9, OUTPUT);
  pinMode(botonJugadoresPin, INPUT_PULLUP);
  pinMode(botonInicioPin, INPUT_PULLUP);
  for (int i = 0; i < 4; i++) {
    pinMode(sensorPines[i], INPUT);
  }
  pinMode(sensorBonusPin, INPUT);

  Serial.println("Cancha de tejo digital - Standby");

  mx.begin();
  prevTimeAnim = millis();
  snprintf(mensaje, sizeof(mensaje), "CANCHA DE TEJO");
  
}

// Función principal
void loop() {
  static bool bInit = true;
  unsigned long currentMillis = millis();
  if (currentMillis - lastActivityTime > inactivityLimit) {
    lastActivityTime = currentMillis;
    Serial.println("Cancha de tejo digital - Standby");
    estadoActual = STANDBY;
  }
  
  bInit = scrollText(bInit, mensaje);
  switch (estadoActual) {
    
    case STANDBY:
      if (digitalRead(botonJugadoresPin) == LOW && (currentMillis - lastButtonTime > debounceDelay)) {
        digitalWrite(9, LOW);
        lastButtonTime = currentMillis;
        lastActivityTime = currentMillis;
        estadoActual = SELECT_PLAYERS;
        numJugadores = 1; // Reiniciar selección
        snprintf(mensaje, sizeof(mensaje), "%d Jugador seleccionado", numJugadores);
        updateMessage(mensaje);
        Serial.println("Cantidad de jugadores seleccionado: 1");
      }
      if (digitalRead(botonJugadoresPin) == LOW && digitalRead(botonInicioPin) == LOW) {
        estadoActual = STANDBY;
        digitalWrite(9, LOW);
        snprintf(mensaje, sizeof(mensaje), "CANCHA DE TEJO");
        bInit = scrollText(bInit, mensaje);
      }
      bInit = scrollText(bInit, mensaje);
      break;

    case SELECT_PLAYERS:
    
      if (digitalRead(botonJugadoresPin) == LOW && digitalRead(botonInicioPin) == LOW) {
        estadoActual = STANDBY;
        snprintf(mensaje, sizeof(mensaje), "CANCHA DE TEJO");
        bInit = scrollText(bInit, mensaje);
      }
      if (digitalRead(botonJugadoresPin) == LOW && (currentMillis - lastButtonTime > debounceDelay)) {
        lastButtonTime = currentMillis;
        lastActivityTime = currentMillis;
        numJugadores++;
        if (numJugadores > 4) numJugadores = 1;
        snprintf(mensaje, sizeof(mensaje), "%d Jugadores seleccionados", numJugadores);
        updateMessage(mensaje);
        Serial.print("Cantidad de jugadores seleccionados: ");
        Serial.println(numJugadores);
      }
      if (digitalRead(botonInicioPin) == LOW && (currentMillis - lastButtonTime > debounceDelay)) {
        lastButtonTime = currentMillis;
        lastActivityTime = currentMillis;
        Serial.println("Juego iniciado");
        Serial.print("Turno jugador ");
        Serial.println(jugadorActual + 1);
        snprintf(mensaje, sizeof(mensaje), "Turno: %d", jugadorActual + 1);
        updateMessage(mensaje);
        estadoActual = PLAYING_TURN;
        jugadorActual = 0;
        memset(puntajes, 0, sizeof(puntajes)); // Reiniciar puntajes
      }
      break;

    case PLAYING_TURN:
      lastActivityTime = currentMillis;
      if (digitalRead(botonJugadoresPin) == LOW && digitalRead(botonInicioPin) == LOW) {
        estadoActual = STANDBY;
        snprintf(mensaje, sizeof(mensaje), "CANCHA DE TEJO");
        bInit = scrollText(bInit, mensaje);
      }
      // Verificar sensores
      for (int i = 0; i < 4; i++) {
        if (digitalRead(sensorPines[i]) == LOW && (currentMillis - lastSensorTime > debounceDelay)) {
          tiempoInicio=0;
          puntajes[jugadorActual]++;
          lastSensorTime = currentMillis;
          Serial.print("Jugador ");
          Serial.print(jugadorActual + 1);
          Serial.print(" obtuvo 1 punto. Total: ");
          Serial.println(puntajes[jugadorActual]);
          snprintf(mensaje, sizeof(mensaje), "1 Punto!");
          updateMessage(mensaje);
          Flag_Puntos_Show = true;
        }
      }
      if (tiempoInicio == 0) {
        tiempoInicio = millis();  // Guardar el tiempo actual
      }
      else if (millis() - tiempoInicio >= tiempoRequerido && Flag_Puntos_Show) {
        Flag_Puntos_Show = false;
        tiempoInicio=0;
        digitalWrite(9, LOW);
        //SCROLL_DELAY = 80;
        snprintf(mensaje, sizeof(mensaje), "J%d: %d Puntos",jugadorActual+1, puntajes[jugadorActual]);
        updateMessage(mensaje);
      } 
      // Verificar si se presionó el botón "botonInicioPin" para cambiar el turno
      if (digitalRead(botonInicioPin) == LOW && (currentMillis - lastButtonTime > debounceDelay)) {
        tiempoInicio=0;
        lastButtonTime = currentMillis;
        tiempoInicio=0;
        jugadorActual++; // Cambiar al siguiente jugador
        if (jugadorActual >= numJugadores) jugadorActual = 0; // Volver al primer jugador si se excede
        Serial.print("Cambio de turno. Ahora es el turno del jugador ");
        Serial.println(jugadorActual + 1);
        snprintf(mensaje, sizeof(mensaje), "Turno: %d", jugadorActual + 1);
        updateMessage(mensaje);
      }

      if (digitalRead(sensorBonusPin) == LOW && (currentMillis - lastSensorTime > debounceDelay)) {
        tiempoInicio=0;
        digitalWrite(9, HIGH);
        Flag_Puntos_Show = true;
        puntajes[jugadorActual] += 6;
        lastSensorTime = currentMillis;
        Serial.print("Jugador ");
        Serial.print(jugadorActual + 1);
        Serial.print(" obtuvo 6 puntos. Total: ");
        Serial.println(puntajes[jugadorActual]);
        snprintf(mensaje, sizeof(mensaje), "6 Puntos!!!");
        updateMessage(mensaje);
      }
      
      // Verificar si alguien ganó
      if (puntajes[jugadorActual] >= puntajeMeta) {
        Serial.print("Jugador ");
        Serial.print(jugadorActual + 1);
        Serial.println(" ha ganado!");
        SCROLL_DELAY = 100;
        snprintf(mensaje, sizeof(mensaje), "!!GANA JUGADOR: %d !!!!!", jugadorActual + 1);
        updateMessage(mensaje);
        estadoActual = GAME_OVER;
        Serial.println("Juego terminado");
        Serial.print("Ganador: Jugador ");
        Serial.println(jugadorActual + 1);
      } else if (digitalRead(botonInicioPin) == LOW && (currentMillis - lastButtonTime > debounceDelay)) {
        lastButtonTime = currentMillis;
        jugadorActual++;
        if (jugadorActual >= numJugadores) jugadorActual = 0;
      }
      break;

    case GAME_OVER:
      
      if (digitalRead(botonJugadoresPin) == LOW && digitalRead(botonInicioPin) == LOW) {
        estadoActual = STANDBY;
        snprintf(mensaje, sizeof(mensaje), "CANCHA DE TEJO");
        bInit = scrollText(bInit, mensaje);
      }
      
      if (digitalRead(botonInicioPin) == LOW && (currentMillis - lastButtonTime > debounceDelay)) {
        lastButtonTime = currentMillis;
        estadoActual = STANDBY;
        Serial.println("Cancha de tejo digital - Standby");
      }
      break;
  }
}
