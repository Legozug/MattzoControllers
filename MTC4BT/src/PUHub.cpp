#include <Arduino.h>

#include "PUHub.h"
#include "log4MC.h"

#define MAX_PUHUB_CHANNEL_COUNT 2

static BLEUUID remoteControlServiceUUID(PU_REMOTECONTROL_SERVICE_UUID);
static BLEUUID remoteControlCharacteristicUUID(PU_REMOTECONTROL_CHARACTERISTIC_UUID);

PUHub::PUHub(BLEHubConfiguration *config, int16_t speedStep, int16_t brakeStep)
    : BLEHub(config, speedStep, brakeStep)
{
}

bool PUHub::SetWatchdogTimeout(const uint8_t watchdogTimeOutInTensOfSeconds)
{
  _watchdogTimeOutInTensOfSeconds = watchdogTimeOutInTensOfSeconds;

  if (!attachCharacteristic(remoteControlServiceUUID, remoteControlCharacteristicUUID))
  {
    log4MC::error("BLE : Unable to attach to remote control service.");
    return false;
  }

  if (!_remoteControlCharacteristic->canWrite())
  {
    log4MC::error("BLE : Remote control characteristic doesn't allow writing.");
    return false;
  }

  log4MC::vlogf(LOG_INFO, "BLE : Watchdog timeout successfully set to s/10: ", _watchdogTimeOutInTensOfSeconds);

  return true;
}

void PUHub::DriveTaskLoop()
{
  for (;;)
  {
    for (int channel = 0; channel < _channelControllers.size(); channel++)
    {
      // Update current channel speeds, if we're not emergency breaking.
      if (_ebreak || _channelControllers.at(channel)->UpdateCurrentSpeedPerc())
      {
        // Serial.print(channel);
        // Serial.print(": rawspd=");
        // Serial.println(MapSpeedPercToRaw(_channelControllers.at(channel)->GetCurrentSpeedPerc()));

        // Construct drive command.
        byte targetSpeed = getRawChannelSpeedForController(_channelControllers.at(channel));
        byte setMotorCommand[8] = {0x81, (byte)channel, 0x11, 0x51, 0x00, targetSpeed};
        int size = 6;

        byte byteCmd[size + 2] = {(byte)(size + 2), 0x00};
        memcpy(byteCmd + 2, setMotorCommand, size);

        // Send drive command.
        if (!_remoteControlCharacteristic->writeValue(byteCmd, sizeof(byteCmd), false))
        {
          log4MC::error("BLE : Drive failed. Unabled to write to characteristic.");
        }
      }
    }

    // Wait half the watchdog timeout (converted from s/10 to s/1000).
    vTaskDelay(_watchdogTimeOutInTensOfSeconds * 50 / portTICK_PERIOD_MS);
  }
}

int16_t PUHub::MapSpeedPercToRaw(int speedPerc)
{
  if (speedPerc == 0)
  {
    return 0; // 0 = float, 127 = stop motor
  }

  if (speedPerc > 0)
  {
    return map(speedPerc, 0, 100, PU_MIN_SPEED_FORWARD, PU_MAX_SPEED_FORWARD);
  }

  return map(abs(speedPerc), 0, 100, PU_MIN_SPEED_REVERSE, PU_MAX_SPEED_REVERSE);
}