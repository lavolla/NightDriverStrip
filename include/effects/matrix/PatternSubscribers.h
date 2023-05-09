//+--------------------------------------------------------------------------
//
// File:        PatternSubscribers.h
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//    If not, see <https://www.gnu.org/licenses/>.
//
//
// Description:
//
//   Effect code ported from Aurora to Mesmerizer's draw routines
//
// History:     Jun-25-202         Davepl      Adapted from own code
//
//---------------------------------------------------------------------------

#ifndef PatternSub_H
#define PatternSub_H

#include "deviceconfig.h"

#define SUB_CHECK_INTERVAL 60000
#define SUB_CHECK_ERROR_INTERVAL 20000

#define DEFAULT_CHANNEL_GUID "9558daa1-eae8-482f-8066-17fa787bc0e4"
#define DEFAULT_CHANNEL_NAME1 "Daves Garage"

class PatternSubscribers : public LEDStripEffect
{
  private:
    long subscribers        = 0;
    long views              = 0;
    String strChannelGuid;

    unsigned long millisLastCheck  = 0;
    bool succeededBefore    = false;

    TaskHandle_t sightTask   = nullptr;
    time_t latestUpdate      = 0;

    WiFiClient http;
    std::unique_ptr<YouTubeSight> sight = nullptr;

    // Thread entry point so we can update the subscriber data asynchronously
    static void SightTaskEntryPoint(void * pv)
    {
        PatternSubscribers * pObj = (PatternSubscribers *) pv;

        for(;;)
        {
            pObj->UpdateSubscribers();

            // Suspend ourself until Draw() wakes us up
            vTaskSuspend(NULL);
        }
    }

    bool UpdateGuid()
    {
        const String &configChannelGuid = g_ptrDeviceConfig->GetYouTubeChannelGuid();

        if (strChannelGuid == configChannelGuid)
            return false;

        strChannelGuid = configChannelGuid;
        return true;
    }

    void UpdateSubscribers()
    {
        if (!sight || UpdateGuid())
            sight = std::make_unique<YouTubeSight>(strChannelGuid, http);

        // Use the YouTubeSight API call to get the current channel stats
        if (sight->getData())
        {
            subscribers = atol(sight->channelStats.subscribers_count.c_str());
            views       = atol(sight->channelStats.views.c_str());
            succeededBefore = true;
        }
        else
        {
            debugW("YouTubeSight Subscriber API failed\n");
        }
    }

    void SetConfigIfUnset()
    {
        if (g_ptrDeviceConfig->GetYouTubeChannelGuid().isEmpty())
            g_ptrDeviceConfig->SetYouTubeChannelGuid(DEFAULT_CHANNEL_GUID);

        if (g_ptrDeviceConfig->GetYouTubeChannelName1().isEmpty())
            g_ptrDeviceConfig->SetYouTubeChannelName1(DEFAULT_CHANNEL_NAME1);
    }

  public:

    PatternSubscribers() : LEDStripEffect(EFFECT_MATRIX_SUBSCRIBERS, "Subs")
    {
    }

    PatternSubscribers(const JsonObjectConst& jsonObject) : LEDStripEffect(jsonObject)
    {
    }

    virtual void Draw() override
    {
        if (WiFi.isConnected())
        {
            unsigned long millisSinceLastCheck = millis() - millisLastCheck;
            // If we've never succeeded, we try every 20 seconds, but then we settle down to the SUBCHECK_INTERVAL
            if (millisLastCheck == 0 || (!succeededBefore && millisSinceLastCheck > SUB_CHECK_ERROR_INTERVAL) || (millisSinceLastCheck > SUBCHECK_INTERVAL))
            {
                millisLastCheck = millis();

                // Lazily create the subscriber update task the first time we need it
                if (sightTask == nullptr)
                {
                    debugW("Spawning thread to get subscriber data...");
                    xTaskCreatePinnedToCore(SightTaskEntryPoint, "Weather", 4096, (void *) this, NET_PRIORITY, &sightTask, NET_CORE);
                }
                else
                {
                    debugW("Triggering thread to get subscriber data now...");
                    // Resume the subscriber task. If it's not suspended then there's an update running and this will do nothing,
                    //   so we'll silently skip the update this would otherwise trigger.
                    vTaskResume(sightTask);
                }
            }
        }

        LEDMatrixGFX::backgroundLayer.fillScreen(rgb24(0, 16, 64));
        LEDMatrixGFX::backgroundLayer.setFont(font5x7);

        // Draw a border around the edge of the panel
        LEDMatrixGFX::backgroundLayer.drawRectangle(0, 1, MATRIX_WIDTH-1, MATRIX_HEIGHT-2, rgb24(160,160,255));

        // Draw the channel name
        LEDMatrixGFX::backgroundLayer.drawString(2, 3, rgb24(255,255,255), g_ptrDeviceConfig->GetYouTubeChannelName1().c_str());

        // Start in the middle of the panel and then back up a half a row to center vertically,
        // then back up left one half a char for every 10s digit in the subscriber count.  This
        // shoud center the number on the screen

        const int CHAR_WIDTH = 6;
        const int CHAR_HEIGHT = 7;
        int x = MATRIX_WIDTH / 2 - CHAR_WIDTH / 2;
        int y = MATRIX_HEIGHT / 2 - CHAR_HEIGHT / 2 - 3;        // -3 to put it above the caption
        long z = subscribers;                                  // Use a long in case of Mr Beast

        while (z/=10)
          x-= CHAR_WIDTH / 2;

        String result = str_sprintf("%ld", subscribers);
        const char * pszText = result.c_str();

        LEDMatrixGFX::backgroundLayer.setFont(gohufont11b);
        LEDMatrixGFX::backgroundLayer.drawString(x-1, y,   rgb24(0,0,0),          pszText);
        LEDMatrixGFX::backgroundLayer.drawString(x+1, y,   rgb24(0,0,0),          pszText);
        LEDMatrixGFX::backgroundLayer.drawString(x,   y-1, rgb24(0,0,0),          pszText);
        LEDMatrixGFX::backgroundLayer.drawString(x,   y+1, rgb24(0,0,0),          pszText);
        LEDMatrixGFX::backgroundLayer.drawString(x,   y,   rgb24(255,255,255),    pszText);
    }
};

#endif
