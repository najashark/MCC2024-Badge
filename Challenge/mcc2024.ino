#include "TinyDriver.h"
#include "GameData.h"
#include <ssd1306xled.h>

enum GameState {
  TITLE_SCREEN,
  GAME_PLAY,
  USERNAME_SCREEN,
  FLAG_SCREEN
};

GameState currentState = TITLE_SCREEN;
uint16_t playerX = 64;
uint16_t playerY = 2;
bool prevButtonState = true;
uint8_t frameCount = 0;
bool isSongPlaying = false;
unsigned long lastNoteTime = 0;
uint8_t currentNote = 0;

// Simple melody for flag screen (Mario tune)
const int PROGMEM melody[] = {
  660, 660, 0, 660, 0, 520, 660, 0, 784, 0, 0, 0, 392, 0, 0, 0,
};
const int PROGMEM noteDurations[] = {
  100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 300, 100, 100, 100, 300, 100
};

void setup() {
  TinyOLED_init();
  pinMode(1, INPUT);
  pinMode(4, OUTPUT);
  pinMode(A0, INPUT);
  pinMode(A3, INPUT);
  SSD1306.ssd1306_fillscreen(0x00);
}

#define TINYJOYPAD_LEFT (analogRead(A0) >= 750) && (analogRead(A0) < 950)
#define TINYJOYPAD_RIGHT (analogRead(A0) > 500) && (analogRead(A0) < 750)
#define TINYJOYPAD_DOWN (analogRead(A3) >= 750) && (analogRead(A3) < 950)
#define TINYJOYPAD_UP (analogRead(A3) > 500) && (analogRead(A3) < 750)
#define BUTTON_DOWN (digitalRead(1) == 0)
#define BUTTON_UP (digitalRead(1) == 1)

char username[7] = "000000";  // 6 chars + null terminator
uint8_t currentPos = 0;
const char validChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const uint8_t numValidChars = sizeof(validChars) - 1;
uint8_t currentCharIndex = 0;

void playBeep() {
  tone(4, 1000, 10);  // Short beep at 1kHz
}

void playFlagSong() {
  if (!isSongPlaying) {
    isSongPlaying = true;
    currentNote = 0;
    lastNoteTime = millis();
    return;
  }

  unsigned long currentTime = millis();
  if (currentTime - lastNoteTime > pgm_read_word(&noteDurations[currentNote])) {
    lastNoteTime = currentTime;
    currentNote++;

    if (currentNote >= sizeof(melody) / sizeof(melody[0])) {
      currentNote = 0;
    }

    int note = pgm_read_word(&melody[currentNote]);
    if (note > 0) {
      tone(4, note, pgm_read_word(&noteDurations[currentNote]));
    } else {
      noTone(4);
    }
  }
}

void drawTitleScreen() {
  static bool needRedraw = true;

  if (needRedraw) {
    uint8_t y, x;
    for (y = 0; y < 8; y++) {
      // Start sending data for this page/row
      TinyOLED_Data_Start(y);

      for (x = 0; x < 128; x++) {
        // Send splash image data
        TinyOLED_Send(pgm_read_byte(&splash[x + (y * 128)]));
      }

      // End transmission for this page/row
      TinyOLED_End();
    }

    needRedraw = false;
  }

  // Draw blinking "PRESS START" text at the bottom
  SSD1306.ssd1306_setpos(30, 7);
  if ((frameCount >> 4) & 1) {
    SSD1306.ssd1306_string_font6x8("PRESS START");
  } else {
    SSD1306.ssd1306_string_font6x8("           ");
  }

  frameCount++;
}

// Add these new functions
void drawUsernameScreen() {
    static bool needRedraw = true;
    
    if(needRedraw) {
        SSD1306.ssd1306_fillscreen(0x00);
        needRedraw = false;
    }
    
    // Draw title
    SSD1306.ssd1306_setpos(0, 0);
    SSD1306.ssd1306_string_font6x8("ENTER USERNAME:");
    
    // Draw username with cursor
    SSD1306.ssd1306_setpos(16, 3);
    for(uint8_t i = 0; i < 6; i++) {
        if(i == currentPos && (frameCount & 0x10)) {
            SSD1306.ssd1306_string_font6x8("_"); // Blinking cursor
        } else {
            char c[2] = {username[i], 0};
            SSD1306.ssd1306_string_font6x8(c);
        }
    }
    
    // Draw instructions
    SSD1306.ssd1306_setpos(0, 6);
    SSD1306.ssd1306_string_font6x8("JOY:CHAR BTN:NEXT");
}

// C++ version for the game
char* generateFlag() {
    static char flag[17];  // CTF{xxxxKEYWxxx}
    const char* keywords[] = {"MALA", "YSIA", "CYBE", "RSEC", "URIT", "YCAM", "P202", "4ALE"};
    const uint8_t NUM_KEYWORDS = 8;
    
    strcpy(flag, "MCC[");
    
    // Calculate keyword position and select keyword based on username
    uint8_t keyPos = (username[0] >= 'A' ? username[0] - 'A' : username[0] - '0') % 3;
    keyPos *= 2;  // Convert to actual position (0, 2, or 4)
    
    uint8_t keywordIdx = (username[5] >= 'A' ? username[5] - 'A' : username[5] - '0') % NUM_KEYWORDS;
    const char* selectedKeyword = keywords[keywordIdx];
    
    // First part of username
    for(uint8_t i = 0; i < keyPos; i++) {
        char c = username[i];
        uint8_t rot = (i + 1) * 7;
        
        if(c >= 'A' && c <= 'Z') {
            c = ((c - 'A' + rot) % 26) + 'A';
        } else if(c >= '0' && c <= '9') {
            c = ((c - '0' + rot) % 10) + '0';
        }
        
        flag[4+i] = c;
    }
    
    // Add keyword
    strcpy(&flag[4 + keyPos], selectedKeyword);
    
    // Rest of username
    for(uint8_t i = keyPos; i < 6; i++) {
        char c = username[i];
        uint8_t rot = (i + 1) * 7;
        
        if(c >= 'A' && c <= 'Z') {
            c = ((c - 'A' + rot) % 26) + 'A';
        } else if(c >= '0' && c <= '9') {
            c = ((c - '0' + rot) % 10) + '0';
        }
        
        flag[8 + keyPos + (i - keyPos)] = c;
    }
    
    flag[14] = ']';
    flag[15] = '\0';
    
    return flag;
}


void drawFlag() {
    static bool needRedraw = true;

    if (needRedraw) {
        SSD1306.ssd1306_fillscreen(0x00);

        SSD1306.ssd1306_setpos(0, 0);
        SSD1306.ssd1306_string_font6x8("YOUR FLAG IS:");
        
        SSD1306.ssd1306_setpos(0, 2);
        SSD1306.ssd1306_string_font6x8(generateFlag());

        SSD1306.ssd1306_setpos(0, 4);
        SSD1306.ssd1306_string_font6x8("SUBMIT:");

        SSD1306.ssd1306_setpos(0, 5);
        SSD1306.ssd1306_string_font6x8("https://mcc24.r0x.my");

        needRedraw = false;
        isSongPlaying = false;
    }

    playFlagSong();

    SSD1306.ssd1306_setpos(10, 7);
    if ((frameCount >> 4) & 1) {
        SSD1306.ssd1306_string_font6x8("BUTTON to Return");
    }
}

#pragma GCC push_options
#pragma GCC optimize ("-O0")
void drawGame() {
  static uint16_t lastX = 0xFFFF;
  static uint16_t lastY = 0xFFFF;
  static uint16_t flagX = 313;
  static uint16_t flagY = 37;

  if (playerX == flagX && playerY == flagY) {
    currentState = FLAG_SCREEN;
    noTone(4);  // Stop any active beep
    return;
  }

  if (lastX != playerX || lastY != playerY) {
    SSD1306.ssd1306_fillscreen(0x00);

    SSD1306.ssd1306_setpos(0, 0);
    char coords[16];
    sprintf(coords, "X:%d Y:%d", playerX, playerY);
    SSD1306.ssd1306_string_font6x8(coords);

    if (playerX < 128 && playerY < 8) {
      SSD1306.ssd1306_setpos(playerX, playerY);
      SSD1306.ssd1306_string_font6x8("@");
    } else {
      SSD1306.ssd1306_setpos(0, 7);
      SSD1306.ssd1306_string_font6x8("Player out of bounds!");
    }

    lastX = playerX;
    lastY = playerY;
  }
}
#pragma GCC pop_options

void checkInput() {
  bool currentButtonState = digitalRead(1);
  if (currentButtonState == 0 && prevButtonState == 1) {
    playBeep();
        if (currentState == TITLE_SCREEN) {
            currentState = USERNAME_SCREEN;  // Go to username screen first
            SSD1306.ssd1306_fillscreen(0x00);
        } else if (currentState == USERNAME_SCREEN) {
            currentPos = (currentPos + 1) % 6;  // Move to next position
            if(currentPos == 0) {  // Completed username
                currentState = GAME_PLAY;
                playerX = 64;
                playerY = 2;
            }
        } else if (currentState == FLAG_SCREEN) {
            currentState = TITLE_SCREEN;
            isSongPlaying = false;
            noTone(4);
            SSD1306.ssd1306_fillscreen(0x00);
        }
  }
  prevButtonState = currentButtonState;

    if (currentState == USERNAME_SCREEN) {
        if (TINYJOYPAD_UP || TINYJOYPAD_DOWN) {
            // Change character
            if(TINYJOYPAD_UP) {
                currentCharIndex = (currentCharIndex + 1) % numValidChars;
            } else {
                currentCharIndex = (currentCharIndex + numValidChars - 1) % numValidChars;
            }
            username[currentPos] = validChars[currentCharIndex];
            playBeep();
        }
    } else if (currentState == GAME_PLAY) {
    uint16_t oldX = playerX;
    uint16_t oldY = playerY;
    bool moved = false;

    if (TINYJOYPAD_LEFT) {
      playerX--;
      moved = true;
    }
    if (TINYJOYPAD_RIGHT) {
      playerX++;
      moved = true;
    }
    if (TINYJOYPAD_UP) {
      playerY--;
      moved = true;
    }
    if (TINYJOYPAD_DOWN) {
      playerY++;
      moved = true;
    }

    if (moved) playBeep();

    if (TINYJOYPAD_LEFT && playerX > oldX) playerX = 0;
    if (TINYJOYPAD_UP && playerY > oldY) playerY = 0;
  }
}

void loop() {
    checkInput();
    frameCount++;

    switch (currentState) {
        case TITLE_SCREEN:
            drawTitleScreen();
            break;
        case USERNAME_SCREEN:
            drawUsernameScreen();
            break;
        case GAME_PLAY:
            drawGame();
            break;
        case FLAG_SCREEN:
            drawFlag();
            break;
    }

    delay(50);
}
