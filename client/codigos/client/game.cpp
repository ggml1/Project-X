#include <iostream>
#include "allegro5/allegro.h"
#include "allegro5/allegro_image.h"
#include "allegro5/allegro_primitives.h"
#include "allegro5/allegro_font.h"
#include "allegro5/allegro_ttf.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <omp.h>
#include <chrono>
#include <thread>
#include "client.h"

// BOARD RELATED
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

using namespace cv;
using namespace std;

// open cv related
#define VIDEO_FILE 1
#define READ_BOARD 1
#define WRITE_BOARD 2
#define UPDATE_SSEG2 3

enum COLORS {BLUE, RED, YELLOW, ORANGE, GREEN, PINK, NCOLORS};

string colorNames[] = {"BLUE", "RED", "YELLOW", "ORANGE", "GREEN", "PINK"};

Mat imgOriginal;
Mat imgHSV;
Mat imgThresholded;
Mat colors[NCOLORS];
vector<pair<pair<int, int>, int>> points;

vector< pair<Scalar, Scalar> > colorRanges[NCOLORS] = {
  {make_pair(Scalar(90,100,50), Scalar(130,255,255))},
  {make_pair(Scalar(15, 70, 50), Scalar(20, 255, 255)), make_pair(Scalar(170, 70, 50), Scalar(179, 255, 255))},
  {make_pair(Scalar(23,100,100), Scalar(40,255,255))},
  {make_pair(Scalar(5,50,50), Scalar(15,255,255))}, //ORANGE
  {make_pair(Scalar(60,60,60), Scalar(90,255,255))}, // GREEN
  {make_pair(Scalar(10, 100, 30), Scalar(10, 255, 255)), make_pair(Scalar(125, 100, 30), Scalar(164, 255, 255))}
};

// ALLEGRO RELATED
#define WIDTH 800
#define HEIGHT 640
#define TILE_SIZE 32
#define BASE_X 40
#define BASE_Y 20
#define MAP_W 9
#define MAP_H 9
#define ZOOM 2
#define FPS 60
#define MAX_CARROTS 105
#define MAX_STAGES 3

// DE2I-150 BOARD RELATED
#define ON_BOTAO 1
#define ON_LEDR 2
#define ON_LEDG 3
#define ON_SWITCH 4
#define ON_SSEG 5
#define ON_SSEG2 6
#define MENU_DELAY 15
#define MENU_BOTAO_UP 14
#define MENU_BOTAO_DOWN 13
#define MENU_BOTAO_ENTER 11
#define MENU_BOTAO_ESCAPE 7
#define BUTTONS_MAX_VALUE 15

ALLEGRO_DISPLAY *mainWindow;
ALLEGRO_EVENT_QUEUE *eventsQueue;
ALLEGRO_FONT *font_upheavtt_94;
ALLEGRO_FONT *font_upheavtt_94_md;
ALLEGRO_FONT *font_upheavtt_94_sm;
ALLEGRO_BITMAP *tileset;
ALLEGRO_BITMAP *forwardIcon;
ALLEGRO_BITMAP *loopIcon;
ALLEGRO_BITMAP *turnRightIcon;
ALLEGRO_BITMAP *carrotBitmap;
ALLEGRO_BITMAP *numbers;
ALLEGRO_BITMAP *rabbit;
ALLEGRO_BITMAP *stageBackground;
ALLEGRO_BITMAP *mapa1, *mapa2, *mapa3;
ALLEGRO_BITMAP *navbar;
ALLEGRO_BITMAP *loop;
ALLEGRO_BITMAP *winScreen;
ALLEGRO_BITMAP *levelsMenu;
ALLEGRO_BITMAP *menuBackground;
ALLEGRO_BITMAP *bg;
ALLEGRO_BITMAP *right_arrow;
ALLEGRO_BITMAP *lockedLevel[MAX_STAGES], *unlockedLevel[MAX_STAGES];

double startingTime;

bool initModules();
bool coreInit();
bool windowInit(char title[]);
bool inputInit();
bool loadGraphics();
bool fontInit();
void allegroEnd();
void startTimer();
void FPSLimit();
double getTimer();
void readStage(int *totalCarrots, int currentStage);
void drawStage(int currentStage);
void drawPlayer(int *drawingStep);
void drawCommandBar(int ingameState, string& commandSequence, int commandPos);
void drawWinScreen();
void drawMenu(int menuOption);
void drawHelpMenu();
void move(string &s, int &pos, int *drawingStep, int *coletou, int totalCarrots);
bool collectedAllCarrots(int totalCarrots);
void drawLevelsMenu(int pointer, int maxLevels);
int getPlayerUnlocks();
void checkUnlocks(int currentStage);
enum conn_ret_t tryConnect();
void printHello();
void assertConnection();
int getHex(int x);

enum menuOptions {PLAY, HELP, EXITGAME};
enum gameStates {MENU, INIT_STAGE, LEVEL_MENU, INGAME, HELP_MENU, WIN, EXIT};
enum ingameStates {WAITING_COMMAND, MOVING, RESET_MAP};
enum gameObjects {GROUND, PLAYER_SPAWN, CARROT};
enum directions {DOWN, LEFT, UP, RIGHT};

unsigned char hexdigit[] = {0x3F, 0x06, 0x5B, 0x4F,
                            0x66, 0x6D, 0x7D, 0x07, 
                            0x7F, 0x6F, 0x77, 0x7C,
			                      0x39, 0x5E, 0x79, 0x71};

int getHexWriteValue(int j) {
  int k = hexdigit[j & 0xF]
      | (hexdigit[(j >>  4) & 0xF] << 8)
      | (hexdigit[(j >>  8) & 0xF] << 16)
      | (hexdigit[(j >> 12) & 0xF] << 24);
  k = ~k;
  return k;
}

void sleepMs(int milliseconds) {
#ifdef WIN32
    sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    usleep(milliseconds * 1000);
#endif
}

typedef struct {
  int x, y;
} PT;

bool operator != (const PT& a, const PT& b) {
  return a.x != b.x || a.y != b.y;
}

typedef struct {
  int id;
  int tilemap[MAP_W][MAP_H];
  PT startingPosition;
} Stage;

typedef struct {
  PT position;
  PT oldPosition;
  int dir = RIGHT;
} Player;

typedef struct {
  PT position;
  bool active;
} Carrot;

typedef struct {
  int operacao, valor, local;
} Pacote;

Pacote dados;
Stage stage;
Player player;
Carrot carrots[MAP_W * MAP_H];
int carrotID[MAP_W][MAP_H];
bool pressed_keys[ALLEGRO_KEY_MAX];
bool pressed_keys_FPGA[BUTTONS_MAX_VALUE];

int main() {
  assertConnection();
  string s;
  bool run = 1, swapxyg = 0, invertpolarityg = 0;
  int gameState = MENU;
  int totalCarrots, coletou;
  int hasCarrots;
  int comecouJogo = 0;
  
  #pragma omp parallel sections shared(s, run, swapxyg, invertpolarityg, gameState, dados, totalCarrots, coletou, hasCarrots, comecouJogo)
  {
    #pragma omp section    //game section
    {
      if (!initModules()) {
        printf("Error on core initialization.");
        exit(1);
      }
      
      ALLEGRO_TIMER *timer = al_create_timer(1.0 / FPS); al_start_timer(timer);
      ALLEGRO_TRANSFORM transform;
      al_register_event_source(eventsQueue, al_get_timer_event_source(timer));
      bool redraw = false;
      bool strReceived = false;
      int ingameState = RESET_MAP;
      int currentStage = 0;
      int menuOption = 0;
      int maxLevels = getPlayerUnlocks();
      string commandSequence;
      int countedFrames = 0, commandPos = 0, menuDelay = 0;
      int drawingStep = -1;
     
      while (run) {
        ALLEGRO_EVENT event;
        al_wait_for_event(eventsQueue, &event);
        
        switch (event.type) {
          case ALLEGRO_EVENT_DISPLAY_CLOSE: {
            gameState = EXIT;
            break;
          }
          case ALLEGRO_EVENT_KEY_DOWN: {
            pressed_keys[event.keyboard.keycode] = 1;
            break;
          }
          case ALLEGRO_EVENT_KEY_UP: {
            pressed_keys[event.keyboard.keycode] = 0;
            break;
          }
          case ALLEGRO_EVENT_TIMER: {
            redraw = true;
            break;
          }
        }
        switch (gameState) {
          case MENU: {
            if (menuDelay) {
              menuDelay--;
              break;
            }
            menuDelay = MENU_DELAY;
            if (pressed_keys[ALLEGRO_KEY_UP] || pressed_keys_FPGA[MENU_BOTAO_UP]) {
              menuOption = (menuOption + 2) % 3;
              if (pressed_keys_FPGA[MENU_BOTAO_UP]) {
                pressed_keys_FPGA[MENU_BOTAO_UP] = 0;
              }
            } else if (pressed_keys[ALLEGRO_KEY_DOWN] || pressed_keys_FPGA[MENU_BOTAO_DOWN]) {
              menuOption = (menuOption + 1) % 3;
              if (pressed_keys_FPGA[MENU_BOTAO_DOWN]) {
                pressed_keys_FPGA[MENU_BOTAO_DOWN] = 0;
              }
            } else if (pressed_keys[ALLEGRO_KEY_ENTER] || pressed_keys_FPGA[MENU_BOTAO_ENTER]) {
              if (pressed_keys_FPGA[MENU_BOTAO_ENTER]) {
                pressed_keys_FPGA[MENU_BOTAO_ENTER] = 0;
              }
              switch (menuOption) {
                case PLAY:
                  gameState = LEVEL_MENU;
                  currentStage = 0;
                  break;
                case HELP:
                  gameState = HELP_MENU;
                  menuDelay = MENU_DELAY;
                  break;
                case EXITGAME:
                  run = 0;
                  break;
              }
            } else {
              menuDelay = 0;
            }
            break;
          }
          case LEVEL_MENU: {
            if (menuDelay) {
              menuDelay--;
              break;
            }
            maxLevels = getPlayerUnlocks();
            menuDelay = MENU_DELAY;
            if (pressed_keys[ALLEGRO_KEY_RIGHT] || pressed_keys_FPGA[MENU_BOTAO_UP]) {
              currentStage = (currentStage + 1) % maxLevels;
              if (pressed_keys_FPGA[MENU_BOTAO_UP]) {
                pressed_keys_FPGA[MENU_BOTAO_UP] = 0;
              }
            } else if (pressed_keys[ALLEGRO_KEY_LEFT] || pressed_keys_FPGA[MENU_BOTAO_DOWN]) {
              currentStage = (currentStage + maxLevels - 1) % maxLevels;
              if (pressed_keys_FPGA[MENU_BOTAO_DOWN]) {
                pressed_keys_FPGA[MENU_BOTAO_DOWN] = 0;
              }
            } else if (pressed_keys[ALLEGRO_KEY_ESCAPE] || pressed_keys_FPGA[MENU_BOTAO_ESCAPE]) {
              gameState = MENU;
              if (pressed_keys_FPGA[MENU_BOTAO_ESCAPE]) {
                pressed_keys_FPGA[MENU_BOTAO_ESCAPE] = 0;
              }
            } else if (pressed_keys[ALLEGRO_KEY_ENTER] || pressed_keys_FPGA[MENU_BOTAO_ENTER]) {
              gameState = INGAME;
              ingameState = RESET_MAP;
              if (pressed_keys_FPGA[MENU_BOTAO_ENTER]) {
                pressed_keys_FPGA[MENU_BOTAO_ENTER] = 0;
              }
            } else {
              menuDelay = 0;
            }
            break;
          }
          case HELP_MENU: {
            if (menuDelay) {
              menuDelay--;
              break;
            }
            menuDelay = MENU_DELAY;
            if (pressed_keys[ALLEGRO_KEY_ESCAPE] || pressed_keys_FPGA[MENU_BOTAO_ESCAPE]) {
              gameState = MENU;
              if (pressed_keys_FPGA[MENU_BOTAO_ESCAPE]) {
                pressed_keys_FPGA[MENU_BOTAO_ESCAPE] = 0;
              }
            } else {
              menuDelay = 0;
            }
            break;
          }
          case INGAME: {
            switch (ingameState) {
              case WAITING_COMMAND: {
                if(pressed_keys[ALLEGRO_KEY_ESCAPE] || pressed_keys_FPGA[MENU_BOTAO_ESCAPE]){
                  gameState = LEVEL_MENU;
                  menuDelay = MENU_DELAY;
                  if (pressed_keys_FPGA[MENU_BOTAO_ESCAPE]) {
                    pressed_keys_FPGA[MENU_BOTAO_ESCAPE] = 0;
                  }
                  break;
                }
                #pragma omp critical (axis)
                {
                  if(pressed_keys[ALLEGRO_KEY_S]) {
                    swapxyg = !swapxyg; // troca de eixos
                  }
                  if(pressed_keys[ALLEGRO_KEY_I]) {
                    invertpolarityg = !invertpolarityg; // troca ordem de leitura (esq -> dir) (dir -> esq)
                  }
                }
                #pragma omp critical
                {
                  commandSequence = s; // formar string pelo OpenCV
                  // commandSequence = "FUUUUG4"; // usar string de movimentos estatica
                }
                if(countedFrames % FPS == 0){
                  cout << commandSequence << endl;
                }
                strReceived = (pressed_keys[ALLEGRO_KEY_ENTER] || pressed_keys_FPGA[MENU_BOTAO_ENTER]);
                if (strReceived) {
                  if (pressed_keys_FPGA[MENU_BOTAO_ENTER]) {
                    pressed_keys_FPGA[MENU_BOTAO_ENTER] = 0;
                  }
                  ingameState = MOVING;
                  countedFrames = 0;
                  commandPos = 0;
                  coletou = 0;
                }
                break;
              }
              case MOVING: {
                if (countedFrames == FPS) {
                  if (commandPos < commandSequence.size()) move(commandSequence, commandPos, &drawingStep, &coletou, totalCarrots);
                  else {
                    if (collectedAllCarrots(totalCarrots)) {
                      gameState = WIN;
                    } else {
                      ingameState = RESET_MAP;
                    }
                  }
                  countedFrames = 0, commandPos++;
                }
                break;
              }
              case RESET_MAP: {
                readStage(&totalCarrots, currentStage); // stages are 0-indexed
                for (int i = 0; i < totalCarrots; ++i) {
                  carrots[i].active = 1;
                }
                int x = getHex(totalCarrots);
                int toW = getHexWriteValue(x);
                dados.operacao = UPDATE_SSEG2;
                dados.valor = toW;
                dados.local = ON_SSEG2;
                sendMsgToServer(&dados, sizeof(Pacote));
                player.position.x = stage.startingPosition.x;
                player.position.y = stage.startingPosition.y;
                switch (currentStage) {
                  case 0:
                    player.dir = RIGHT;
                    break;
                  case 1:
                    player.dir = RIGHT;
                    break;
                  case 2:
                    player.dir = DOWN;
                    break;
                }
                ingameState = WAITING_COMMAND;
                commandSequence.clear();
                strReceived = false;
                drawingStep = -1;
                pressed_keys[ALLEGRO_KEY_ENTER] = 0;
                break;
              }
            }
            break;
          }
          case WIN: {
            checkUnlocks(currentStage);
            if (pressed_keys[ALLEGRO_KEY_ENTER] || pressed_keys_FPGA[MENU_BOTAO_ENTER]) {
              gameState = LEVEL_MENU;
              if (pressed_keys_FPGA[MENU_BOTAO_ENTER]) {
                pressed_keys_FPGA[MENU_BOTAO_ENTER] = 0;
              }
            }
            break;
          }
          case EXIT: {
            run = 0;
            break;
          }
        }
        if (redraw) {
          redraw = 0;
          al_clear_to_color(al_map_rgb(0, 0, 0));
          switch (gameState) {
            case MENU: {
              drawMenu(menuOption);
              break;
            }
            case LEVEL_MENU: {
              al_identity_transform(&transform);
					    al_scale_transform(&transform, 1, 1);
					    al_use_transform(&transform);
              drawLevelsMenu(currentStage, maxLevels);
              break;
            }
            case INGAME: {
              al_identity_transform(&transform);
              al_scale_transform(&transform, ZOOM, ZOOM);
              al_use_transform(&transform);
              drawStage(currentStage), drawPlayer(&drawingStep);
              drawCommandBar(ingameState, commandSequence, commandPos);
              break;
            }
            case WIN: {
              al_identity_transform(&transform);
					    al_scale_transform(&transform, 1, 1);
					    al_use_transform(&transform);
              drawWinScreen();
              break;
            }
            case HELP_MENU: {
              drawHelpMenu();
              break;
            }
          }
          al_flip_display();
          countedFrames++;
        }
      }
      al_destroy_display(mainWindow);
      al_destroy_event_queue(eventsQueue);
    } // end game section

    #pragma omp section // secao pra ler botoes da placa continuamente
    {
      while (1) {
        dados.operacao = READ_BOARD;
        dados.valor = 0;
        dados.local = ON_BOTAO;
        sendMsgToServer(&dados, sizeof(Pacote));
        int leitura = 0;
        recvMsgFromServer(&leitura, WAIT_FOR_IT);
        if (leitura == MENU_BOTAO_UP) {
          pressed_keys_FPGA[MENU_BOTAO_UP] = 1;
        } else if (leitura == MENU_BOTAO_DOWN) {
          pressed_keys_FPGA[MENU_BOTAO_DOWN] = 1;
        } else if (leitura == MENU_BOTAO_ENTER) {
          pressed_keys_FPGA[MENU_BOTAO_ENTER] = 1;
        } else if (leitura == MENU_BOTAO_ESCAPE) {
          pressed_keys_FPGA[MENU_BOTAO_ESCAPE] = 1;
        }

        // leitura dos switches -- TO DO: obter valores das leituras e colocar aqui
        //
        // dados.operacao = READ_BOARD;
        // dados.valor = 0;
        // dados.local = ON_SWITCH;
        // sendMsgToServer(&dados, sizeof(Pacote));
        // leitura = 0;
        // recvMsgFromServer(&leitura, WAIT_FOR_IT);
        // if (leitura == SWITCH_CHANGE_AXIS) {
        //   pressed_keys_FPGA[SWITCH_CHANGE_AXIS] = 1;
        // } else if (leitura == SWITCH_CHANGE_POLARITY) {
        //   pressed_keys_FPGA[SWITCH_CHANGE_POLARITY] = 1;
        // }
        sleepMs(400);
      }
    }

    #pragma omp section     //secao de processamento do opencv
    {
      VideoCapture cap(0);
      if (!cap.isOpened()) {
        cout << "Cannot open the web cam" << endl;
        pthread_cancel(pthread_self());
      }
      
      while (run) {

        bool bSuccess = cap.read(imgOriginal);

        if (!bSuccess) {
          cout << "Cannot read a frame from video stream" << endl;
          run = 0;
          break;
        }

        cvtColor(imgOriginal, imgHSV, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV

        for (int i = 0; i < NCOLORS; i++) {
          Mat mask;
          colors[i] ^= colors[i];
          for (pair<Scalar, Scalar> u : colorRanges[i]) {
            cv::inRange(imgHSV, u.first, u.second, mask);
            colors[i] = mask | colors[i];
          }
          imgThresholded = colors[i];
          //morphological opening (remove small objects from the foreground)
          erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
          dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5))); 

          //morphological closing (fill small holes in the foreground)
          dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5))); 
          erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
          
          Mat labels;
          Mat stats;
          Mat centroids;
          connectedComponentsWithStats(imgThresholded, labels, stats, centroids);
          
          for (int j = 1; j < (int) centroids.rows; j++) {
            if (stats.at<int>(j, CC_STAT_AREA) > 600) {
              int x = centroids.at<double>(j, 0);
              int y = centroids.at<double>(j, 1);
              points.push_back({{x, y}, i});
            }
          }
        }
        
        bool swapxy, invertpolarity;
        
        #pragma omp critical (axis)
        {
          swapxy = swapxyg;
          invertpolarity = invertpolarityg;
        }
        
        for (int i = 0; i < points.size(); i++) {
          if(swapxy) swap(points[i].first.first, points[i].first.second);
          if(invertpolarity) points[i].first.first = -points[i].first.first;
        }
        sort(points.begin(), points.end());
        
        string seq;
        for (int i = 0; i < points.size(); i++) {
          if (invertpolarity) points[i].first.first = -points[i].first.first;
          if (swapxy) swap(points[i].first.first, points[i].first.second);
          string s = to_string(i);
          putText(imgOriginal, s.c_str(), Point(points[i].first.first, points[i].first.second), FONT_HERSHEY_SIMPLEX, 1, Scalar(0,200,200), 4);
          if (!seq.empty() && seq.back() == '0') {
            seq.back() = points[i].second + 2 + '0';
            continue;
          }
          switch (points[i].second) {
            case BLUE:
              seq.push_back('U');
              break;
            case RED:
              seq.push_back('G');
              break;
            case YELLOW:
              seq.push_back('F');
              break;
            case GREEN:
              seq.push_back('4');
              break;
          }
        }
        points.clear();
        #pragma omp critical
        {
          // passando a sequencia de movimentos formada p/ secao do jogo
          s = seq;
        }
        
        imshow("Original", imgOriginal); //show the original image

        if (waitKey(30) == 27) {
          cout << "esc key was pressed by user" << endl;
          run = 0;
        }
      }
    } // end opencv section

    #pragma omp section   // de2i150 - led da vitoria
    {
      while (1) {
        int L = (1 << 17), R = 1, state = L + R;
        int ledVerdeON = 1;
        int ledVerdeVal = 255;
        while (gameState == WIN) {
          dados.operacao = WRITE_BOARD;
          dados.valor = state;
          dados.local = ON_LEDR;
          sendMsgToServer(&dados, sizeof(Pacote));
          if (ledVerdeON) {
            dados.operacao = WRITE_BOARD;
            dados.valor = ledVerdeVal;
            dados.local = ON_LEDG;
          } else {
            dados.operacao = WRITE_BOARD;
            dados.valor = 0;
            dados.local = ON_LEDG;
          }
          sendMsgToServer(&dados, sizeof(Pacote));
          ledVerdeON ^= 1;
          L >>= 1;
          R <<= 1;
          if (L == 0) L = (1 << 17);
          if (R == (1 << 18)) R = 1;
          state = L + R;
          sleepMs(200);
        }
      }
    }
  }

  return 0;
}

int getHex(int x) {
  int b = 1;
  int s = 0;
  while (x) {
    s += (x % 10) * b;
    b *= 16;
    x /= 10;
  }
  return s;
}

enum conn_ret_t tryConnect() {
  char server_ip[30];
  printf("Please enter the server IP: ");
  scanf(" %s", server_ip);
  printf("Entrando no server %s\n", server_ip);
  getchar();
  return connectToServer(server_ip);
}

void printHello() {
  puts("Hello! Welcome to Project X.");
  puts("We need some infos to start up!");
}

void assertConnection() {
  printHello();
  enum conn_ret_t ans = tryConnect();
  while (ans != SERVER_UP) {
    if (ans == SERVER_DOWN) {
      puts("Server is down!");
    } else if (ans == SERVER_FULL) {
      puts("Server is full!");
    } else if (ans == SERVER_CLOSED) {
      puts("Server is closed for new connections!");
    } else {
      puts("Server didn't respond to connection!");
    }
    printf("Want to try again? [Y/n] ");
    int res;
    while (res = tolower(getchar()), res != 'n' && res != 'y' && res != '\n'){
      puts("anh???");
    }
    if (res == 'n') {
      exit(EXIT_SUCCESS);
    }
    ans = tryConnect();
  }
}

bool collectedAllCarrots(int totalCarrots) {
  for (int i = 0; i < totalCarrots; ++i) {
    if (carrots[i].active) return false;
  }
  return true;
}

void readStage(int *totalCarrots, int currentStage) {
  char fname[20];
  sprintf(fname, "stages/%d.txt", currentStage);
  FILE *f = fopen(fname, "r");
  (*totalCarrots) = 0;
  for (int i = 0; i < MAP_H; i++) {
    for (int j = 0; j < MAP_W; j++) {
      char c;
      while (!isdigit((c = fgetc(f))));
      stage.tilemap[i][j] = (c - '0');
      printf("%d", stage.tilemap[i][j]);
      switch (stage.tilemap[i][j]) {
        case CARROT: {
          carrotID[i][j] = (*totalCarrots) + 1;
          carrots[(*totalCarrots)].position.x = i;
          carrots[(*totalCarrots)++].position.y = j;
          break;
        }
        case PLAYER_SPAWN: {
          stage.startingPosition.x = i;
          stage.startingPosition.y = j;
        }
      }
    }
    printf("\n");
    fgetc(f);
  }
  printf("---------------\n\n");
  fclose(f);
}

void checkUnlocks(int currentStage) {
  FILE *playerUnlocks = fopen("stages/player_level.txt", "r");
  int limit;
  fscanf(playerUnlocks, "%d", &limit);
  fclose(playerUnlocks);
  if (currentStage == limit - 1) {
    playerUnlocks = fopen("stages/player_level.txt", "w");
    fprintf(playerUnlocks, "%d", limit + 1);
    fclose(playerUnlocks);
  }
}

int getPlayerUnlocks() {
  FILE *playerUnlocks = fopen("stages/player_level.txt", "r");
  int limit;
  fscanf(playerUnlocks, "%d", &limit);
  fclose(playerUnlocks);
  return limit;
}

bool valid(int tile) {
  return tile == GROUND || tile == CARROT || tile == PLAYER_SPAWN;
}

void move(string &s, int &pos, int *drawingStep, int *coletou, int totalCarrots){
  if (isdigit(s[pos]) && s[pos] > '1') {
    s[pos]--;
    while (s[pos] != 'F') pos--;
  }
  if (s[pos] == 'F' || s[pos] == '1') pos++;
  auto &px = player.position.x;
  auto &py = player.position.y;
  player.oldPosition.x = px;
  player.oldPosition.y = py;
  switch (s[pos]) {
    case 'U': {
      switch (player.dir) {
        case UP: {
          if (px - 1 >= 0 && valid(stage.tilemap[px - 1][py])) px--;
          break;
        }
        case DOWN: {
          if (px + 1 < MAP_H && valid(stage.tilemap[px + 1][py])) px++;
          break;
        }
        case LEFT: {
          if (py - 1 >= 0 && valid(stage.tilemap[px][py - 1])) py--;
          break;
        }
        case RIGHT: {
          if (py + 1 < MAP_W && valid(stage.tilemap[px][py + 1])) {
            py++;
          }
          break;
        }
      }
      if (px != player.oldPosition.x || py != player.oldPosition.y) (*drawingStep) = 0;
      break;
    }
    case 'G':{
      player.dir =  (player.dir + 1) % 4;
      break;
    }
  }
  if (carrotID[px][py] && carrots[carrotID[px][py] - 1].active) {
    carrots[carrotID[px][py] - 1].active = 0;
    (*coletou)++;
    int toW = getHexWriteValue(getHex((*coletou) * 100 + totalCarrots));
    dados.operacao = UPDATE_SSEG2;
    dados.valor = toW;
    dados.local = ON_SSEG2;
    sendMsgToServer(&dados, sizeof(Pacote));
  }
}

void drawCommandBar(int ingameState, string& commandSequence, int commandPos) {
  int barLengthX = 160;
  int barLengthY = 16;
  int barCenterX = WIDTH / (ZOOM * 2);
  int barBeginY = (HEIGHT / ZOOM) - 23;
  int navbarBeginY = (HEIGHT / ZOOM) - 32;
  

  al_draw_bitmap(navbar, barCenterX - barLengthX - 20, navbarBeginY, 0);

  if (ingameState == MOVING && commandPos < commandSequence.size()){
    al_draw_rectangle(barCenterX - barLengthX + barLengthY * (commandPos - (commandPos > 0)),
                             barBeginY, 
                             barCenterX - barLengthX + barLengthY * (commandPos - (commandPos > 0)) + barLengthY,
                             barBeginY + barLengthY, al_map_rgb(255, 0, 0),
                             1.0
                             );
  }

  for (int i = 0; i < commandSequence.size(); i++) {          
    switch (commandSequence[i]) {
      case 'U': {
        al_draw_scaled_bitmap(forwardIcon, 0, 0, 64, 64, barCenterX - barLengthX + barLengthY * i, barBeginY, barLengthY, barLengthY, 0);
        break;
      }
      case 'G': {
        al_draw_scaled_bitmap(turnRightIcon, 0, 0, 64, 64, barCenterX - barLengthX + barLengthY * i, barBeginY, barLengthY, barLengthY, 0);
        break;
      }
      case 'F': {
        al_draw_bitmap(loop, barCenterX - barLengthX + barLengthY * i, barBeginY + 4, 0);
        break;
      }
    }
    if (isdigit(commandSequence[i])) {
      int number = commandSequence[i] - '0';
      al_draw_bitmap_region(numbers, (number - 1) * 16 + 2, 0, 15, 16, barCenterX - barLengthX + barLengthY * i, barBeginY, 0);
    }
  }
}

void drawLevelsMenu(int pointer, int maxLevels) {
  al_draw_scaled_bitmap(bg, 0, 0, 650, 650, 0, 0, 800, 640, 0);
  al_draw_text(font_upheavtt_94, al_map_rgb(0, 0, 0), WIDTH / 2, 75, ALLEGRO_ALIGN_CENTRE, "LEVELS");
  int posX = 100;
  int posY = 250;
  int distance = 200;
  al_draw_bitmap(lockedLevel[0], posX - 90 + pointer * distance, posY, 0);
  for (int i = 0; i < 4; ++i) {
    al_draw_filled_rectangle(posX, posY, posX + 50, posY + 50, i < maxLevels ? al_map_rgb(76, 175, 80) : al_map_rgb(221, 44, 0));
    posX += distance;
  }
}

void drawStage(int currentStage) {
  switch (currentStage) {
    case 0: {
      al_draw_bitmap(mapa1, BASE_X, BASE_Y, 0);
      break;
    }
    case 1: {
      al_draw_bitmap(mapa2, BASE_X, BASE_Y, 0);
      break;
    }
    case 2: {
      al_draw_bitmap(mapa3, BASE_X, BASE_Y, 0);
      break;
    }
  }
  for (int i = 0; i < MAP_H; i++) {
    for (int j = 0; j < MAP_W; j++) {
      int dx = BASE_X + j * TILE_SIZE;
      int dy = BASE_Y + i * TILE_SIZE;
      switch (stage.tilemap[i][j]) {
        case CARROT:
          if (carrots[carrotID[i][j] - 1].active) al_draw_bitmap(carrotBitmap, dx, dy, 0);
          break;
      }
    }
  }
}

void drawPlayer(int *drawingStep) {
  auto nx = player.position.y;
  auto ny = player.position.x;
  int ox = player.oldPosition.y;
  int oy = player.oldPosition.x;
  if ((*drawingStep) != -1) {
    switch (player.dir) {
      case UP: {
        al_draw_bitmap_region(rabbit, 32 * (*drawingStep), 32, 32, 32, BASE_X + ox * TILE_SIZE, BASE_Y + oy * TILE_SIZE - 4 * (*drawingStep), 0);
        break;
      }
      case DOWN: {
        al_draw_bitmap_region(rabbit, 32 * (*drawingStep), 0, 32, 32, BASE_X + ox * TILE_SIZE, BASE_Y + oy * TILE_SIZE + 4 * (*drawingStep), 0);
        break;
      }
      case LEFT: {
        al_draw_bitmap_region(rabbit, (7 - (*drawingStep)) * 32, 96, 32, 32, BASE_X + ox * TILE_SIZE - 4 * (*drawingStep), BASE_Y + oy * TILE_SIZE, 0);
        break;
      }
      case RIGHT: {
        al_draw_bitmap_region(rabbit, 32 * (*drawingStep) + 3, 64 + 3, 32, 32, BASE_X + ox * TILE_SIZE + 4 * (*drawingStep), BASE_Y + oy * TILE_SIZE, 0);
        break;
      }
    }
    (*drawingStep)++;
    if ((*drawingStep) == 8) (*drawingStep) = -1;
  } else {
    switch (player.dir) {
      case UP: {
        al_draw_bitmap_region(rabbit, 0, 32, 32, 32, BASE_X + nx * TILE_SIZE, BASE_Y + ny * TILE_SIZE, 0);
        break;
      }
      case DOWN: {
        al_draw_bitmap_region(rabbit, 0, 0, 32, 32, BASE_X + nx * TILE_SIZE, BASE_Y + ny * TILE_SIZE, 0);
        break;
      }
      case LEFT: {
        al_draw_bitmap_region(rabbit, 7 * 32, 96, 32, 32, BASE_X + nx * TILE_SIZE, BASE_Y + ny * TILE_SIZE, 0);
        break;
      }
      case RIGHT: {
        al_draw_bitmap_region(rabbit, 0, 64, 32, 32, BASE_X + nx * TILE_SIZE, BASE_Y + ny * TILE_SIZE, 0);
        break;
      } 
    }
  }
}

void drawMenu(int menuOption) {
  al_draw_scaled_bitmap(bg, 0, 0, 650, 650, 0, 0, 800, 640, 0);
  al_draw_text(font_upheavtt_94, al_map_rgb(0, 0, 0), WIDTH / 2, 75, ALLEGRO_ALIGN_CENTRE, "PROJECT X");
  al_draw_text(font_upheavtt_94, al_map_rgb(0, 0, 0), WIDTH / 2, 200, ALLEGRO_ALIGN_CENTRE, "PLAY");
  al_draw_text(font_upheavtt_94, al_map_rgb(0, 0, 0), WIDTH / 2, 200+100, ALLEGRO_ALIGN_CENTRE, "HELP");
  al_draw_text(font_upheavtt_94, al_map_rgb(0, 0, 0), WIDTH / 2, 200+200, ALLEGRO_ALIGN_CENTRE, "EXIT");

  al_draw_bitmap(lockedLevel[0], 100, 220 + 100 * menuOption, 0);
}

void drawWinScreen() {
  al_draw_scaled_bitmap(bg, 0, 0, 650, 650, 0, 0, 800, 640, 0);
  al_draw_text(font_upheavtt_94_md, al_map_rgb(0, 0, 0), WIDTH / 2, 75, ALLEGRO_ALIGN_CENTRE, "YOU WON!");
  al_draw_text(font_upheavtt_94_sm, al_map_rgb(0, 0, 0), WIDTH / 2, 250, ALLEGRO_ALIGN_CENTRE, "PRESS ENTER TO GO TO MENU");
}

void drawHelpMenu() {
  al_draw_scaled_bitmap(bg, 0, 0, 650, 650, 0, 0, 800, 640, 0);
  al_draw_text(font_upheavtt_94, al_map_rgb(0, 0, 0), WIDTH / 2, 75, ALLEGRO_ALIGN_CENTRE, "HELP");
}

bool coreInit() {
  if (!al_init()) {
      fprintf(stderr, "Error occurred when loading Allegro.\n");
      return false;
  }
  if (!al_init_image_addon()) {
      fprintf(stderr, "Error occurred when loading add-on allegro_image.\n");
      return false;
  }
  if (!al_init_font_addon()) {
      fprintf(stderr, "Error occurred when loading add-on allegro_font.\n");
      return false;
  }
  if (!al_init_ttf_addon()) {
      fprintf(stderr, "Error occurred when loading add-on allegro_ttf.\n");
      return false;
  }
  if (!al_init_primitives_addon()) {
      fprintf(stderr, "Error occurred when loading add-on allegro_primitives.\n");
      return false;
  }
  eventsQueue = al_create_event_queue();
  if (!eventsQueue) {
      fprintf(stderr, "Error on event queue creation.\n");
      return false;
  }
  return true;
}

bool windowInit(char title[]) {
  mainWindow = al_create_display(WIDTH, HEIGHT);
  if (!mainWindow) {
      fprintf(stderr, "Error found while creating window.\n");
      return false;
  }
  al_set_window_title(mainWindow, title);
  al_register_event_source(eventsQueue, al_get_display_event_source(mainWindow));
  return true;
}

bool inputInit() {
  if (!al_install_mouse()) {
      fprintf(stderr, "Error when initializing the mouse.\n");
      al_destroy_display(mainWindow);
      return false;
  }
  if (!al_set_system_mouse_cursor(mainWindow, ALLEGRO_SYSTEM_MOUSE_CURSOR_DEFAULT)) {
      fprintf(stderr, "Error when allocating the mouse pointer.\n");
      al_destroy_display(mainWindow);
      return false;
  }
  if (!al_install_keyboard()) {
      fprintf(stderr, "Error when initializing the keyboard.\n");
      return false;
  }
  al_register_event_source(eventsQueue, al_get_mouse_event_source());
  al_register_event_source(eventsQueue, al_get_keyboard_event_source());
  return true;
}

bool fontInit() {
  font_upheavtt_94 = al_load_font("upheavtt.ttf", 94, 0);
  font_upheavtt_94_md = al_load_font("upheavtt.ttf", 70, 0);
  font_upheavtt_94_sm = al_load_font("upheavtt.ttf", 50, 0);
  return true;
}

bool loadGraphics() {
  tileset = al_load_bitmap("resources/town_tiles.png");
  forwardIcon = al_load_bitmap("resources/up.png");
  turnRightIcon = al_load_bitmap("resources/turn-right.png");
  carrotBitmap = al_load_bitmap("resources/carrot.png");
  numbers = al_load_bitmap("resources/numbers.png");
  stageBackground = al_load_bitmap("resources/Clouds.jpg");
  levelsMenu = al_load_bitmap("resources/levelsMenu.jpg");
  winScreen = al_load_bitmap("resources/winScreen.jpg");
  menuBackground = al_load_bitmap("resources/mainMenu.jpg");
  rabbit = al_load_bitmap("resources/rabbit.png");
  mapa1 = al_load_bitmap("resources/mapa1.png");
  mapa2 = al_load_bitmap("resources/mapa2.png");
  mapa3 = al_load_bitmap("resources/mapa3.png");
  navbar = al_load_bitmap("resources/navbar.png");
  loop = al_load_bitmap("resources/loop.png");
  bg = al_load_bitmap("resources/bg.jpg");
  right_arrow = al_load_bitmap("resources/right_arrow.jpg");

  for (int i = 0; i < MAX_STAGES; ++i) {
    string unlocked = "resources/unlocked" + to_string(i) + ".png";
    string locked = "resources/locked" + to_string(i) + ".png";
    unlockedLevel[i] = al_load_bitmap(unlocked.c_str());
    lockedLevel[i] = al_load_bitmap(locked.c_str());
    if (!unlockedLevel[i]) {
      fprintf(stderr, "Error loading bitmap %s\n", unlocked.c_str());
      return false;
    }
    if (!lockedLevel[i]) {
      fprintf(stderr, "Error loading bitmap %s\n", locked.c_str());
      return false;
    }
  }
  if (!bg) {
    fprintf(stderr, "Error loading bitmap bg.png\n");
    return false;
  }
  if (!loop) {
    fprintf(stderr, "Error loading bitmap loop.png\n");
    return false;
  }
  if (!navbar) {
    fprintf(stderr, "Error loading bitmap navbar.png\n");
    return false;
  }
  if (!rabbit) {
    fprintf(stderr, "Error loading bitmap rabbit.png\n");
    return false;
  }
  if (!menuBackground) {
    fprintf(stderr, "Error loading bitmap mainMenu.jpg\n");
    return false;
  }
  if (!tileset) {
    fprintf(stderr, "Error loading bitmap town_tiles.png\n");
    return false;
  }
  if (!forwardIcon) {
    fprintf(stderr, "Error loading bitmap up.png\n");
    return false;
  }
  if (!turnRightIcon) {
    fprintf(stderr, "Error loading bitmap turn-right.png\n");
    return false;
  }
  if (!carrotBitmap) {
    fprintf(stderr, "Error loading bitmap carrot.png\n");
    return false;
  }
  if (!numbers) {
    fprintf(stderr, "Error loading bitmap numbers.png\n");
    return false;
  }
  if (!stageBackground) {
    fprintf(stderr, "Error loading bitmap clouds.jpg\n");
    return false;
  }
  if (!levelsMenu) {
    fprintf(stderr, "Error loading bitmap levelsMenu.jpg\n");
    return false;
  }
  if (!winScreen) {
    fprintf(stderr, "Error loading bitmap winScreen.jpg\n");
    return false;
  }
  return true;
}

bool initModules() {
  if (!coreInit()) return 0;
  if (!windowInit("Project X")) return 0;
  if (!inputInit()) return 0;
  if (!fontInit()) return 0;
  if (!loadGraphics()) return 0;
  return 1;
}

