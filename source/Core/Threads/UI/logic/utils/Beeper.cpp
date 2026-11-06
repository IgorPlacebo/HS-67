#include "Beeper.h"
#include "BSP.h"
#include "FreeRTOS.h"
#include "task.h"

static TickType_t beepEnd   = 0;
static bool       beepOn    = false;

void beep(uint16_t durationMs) {
  beepEnd = xTaskGetTickCount() + (TickType_t)durationMs * configTICK_RATE_HZ / 1000;
  beepOn  = true;
  setBuzzer(true);
}

void beeperService() {
  if (beepOn && (int32_t)(xTaskGetTickCount() - beepEnd) >= 0) {
    beepOn = false;
    setBuzzer(false);
  }
}
