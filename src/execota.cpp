/* Copyright (C) 2019-2020 Lee C. Bussy (@LBussy)

This file is part of Lee Bussy's Brew Bubbbles (brew-bubbles).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include "execota.h"

void execfw() {
    Log.notice(F("Starting the Firmware OTA pull, will reboot without notice." CR));

    // Stop web server before OTA update - will restart on reset
    stopWebServer();

    // Have to set this here because we have no chance after update
    config.dospiffs1 = true;
    config.dospiffs2 = false;
    config.didupdate = false;
    saveConfig();

    ESPhttpUpdate.setLedPin(LED, LOW);
    // "http://www.brewbubbles.com/firmware/firmware.bin"
    WiFiClient _client;
    t_httpUpdate_return ret = ESPhttpUpdate.update(_client, F(FIRMWAREURL), "0");

    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Log.error(F("HTTP Firmware OTA Update failed error (%d): %s" CR), ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            // Don't allow anything to proceed
            config.dospiffs1 = false;
            config.dospiffs2 = false;
            config.didupdate = false;
            saveConfig();
            ESP.restart();
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Log.notice(F("HTTP Firmware OTA Update: No updates." CR));
            // Don't allow anything to proceed
            config.dospiffs1 = false;
            config.dospiffs2 = false;
            config.didupdate = false;
            saveConfig();
            ESP.restart();
            break;
        
        case HTTP_UPDATE_OK:
            // We should never actually reach this as the controller
            // resets after OTA
            Log.notice(F("HTTP Firmware OTA Update complete, restarting." CR));
            ESP.restart();
            _delay(1000);
            break;
    }
}

void execspiffs() {
    if (config.dospiffs1) {
        Log.notice(F("Rebooting a second time before File System OTA pull." CR));
        config.dospiffs1 = false;
        config.dospiffs2 = true;
        config.didupdate = false;
        saveConfig();
        _delay(3000);
        ESP.restart();
        _delay(1000);
    } else if (config.dospiffs2) {
        Log.notice(F("Starting the File System OTA pull." CR));

        // Stop web server before OTA update - will restart on reset
        stopWebServer();

        ESPhttpUpdate.setLedPin(LED, LOW);
        // "http://www.brewbubbles.com/firmware/spiffs.bin"
        WiFiClient client;
        t_httpUpdate_return ret = ESPhttpUpdate.updateFS(client, F(LITTLEFSURL), "");

        switch(ret) {
            case HTTP_UPDATE_FAILED:
                Log.error(F("HTTP File System OTA Update failed error (%d): %s" CR), ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                break;

            case HTTP_UPDATE_NO_UPDATES:
                Log.notice(F("HTTP File System OTA Update: No updates." CR));
                break;

            case HTTP_UPDATE_OK:
                // Reset File System update flag
                config.dospiffs1 = false;
                config.dospiffs2 = false;
                config.didupdate = true;
                saveConfig(); // This not only saves the flags, it (re)saves the whole config after File System wipes it
                _delay(1000);
                Log.notice(F("HTTP File System OTA Update complete, restarting." CR));
                ESP.restart();
                _delay(1000);
                break;
        }
    } else {
        Log.verbose(F("No OTA pending." CR));
    }
}
