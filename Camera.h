#include <Arduino.h>
void cameraBegin();
bool cameraRead();
static int const ccameraWidth= 160;
static int const ccameraHeight= 120;
extern uint8_t cameraImage[ccameraHeight*ccameraWidth]; // QQVGA image buffer in B&W format
static int cameraWidth() { return ccameraWidth; }
static int cameraHeight() { return ccameraHeight; }
