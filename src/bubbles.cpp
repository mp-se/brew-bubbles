/* Copyright (C) 2019 Lee C. Bussy (@LBussy)

This file is part of Lee Bussy's Brew Bubbles (Brew-Bubbles).

Brew Bubbles is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

Brew Bubbles is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
General Public License for more details.

You should have received a copy of the GNU General Public License along
with Brew Bubbles. If not, see <https://www.gnu.org/licenses/>. */

#include "bubbles.h"

Bubbles *pBubbles; // Pointer to Counter class

static ICACHE_RAM_ATTR void HandleInterruptsStatic(void) { // External interrupt handler
    pBubbles->handleInterrupts(); // Calls class member handler
}

Bubbles* Bubbles::single = NULL; // Holds pointer to class

Bubbles* Bubbles::getInstance() {
    if (!single) {
        single = new Bubbles();
        single->start();
    }
    return single;
}

void Bubbles::start() {
    pinMode(COUNTPIN, INPUT);       // Change pinmode to input
    attachInterrupt(digitalPinToInterrupt(COUNTPIN), HandleInterruptsStatic, RISING); // FALLING, RISING or CHANGE
    pBubbles = single;              // Assign current instance to pointer 
    single->ulLastReport = millis();// Store the last report timer
    single->pulse = 0;              // Reset pulse counter

    // Set starting values
    unsigned long ulNow = millis();
    single->ulStart = ulNow;
    single->lastBpm = 0.0;
    
    // Set starting time
    NtpHandler *ntpTime = NtpHandler::getInstance();
    ntpTime->update();
    single->lastTime = ntpTime->Time;

    // Set starting Bubble
    strlcpy(single->bubStatus, "{}", 3);
}

void Bubbles::update() { // Regular update loop, once per minute
    // Handle NTP Time
    NtpHandler *ntpTime = NtpHandler::getInstance();
    ntpTime->update();
    single->lastTime = ntpTime->Time;

    // Store last values
    single->lastBpm = single->getRawBpm();
    single->lastAmb = single->getAmbientTemp();
    single->lastVes = single->getVesselTemp();
    // Update the web page source
    single->createBubbleJson();

    // Push values to circular buffers
    tempAmbAvg.push(single->lastAmb);
    tempVesAvg.push(single->lastVes);
    bubAvg.push(single->lastBpm);

    Log.verbose(F("Time is %s." CR), single->lastTime);
    Log.verbose(F("Bubble: %s" CR), single->bubStatus);
    Log.verbose(F("Averages: BPM = %D (%l sample), Ambient = %D (%l sample), Vessel = %D (%l sample)." CR),
        single->getAvgBpm(), bubAvg.size(),
        single->getAvgAmbient(), tempAmbAvg.size(),
        single->getAvgVessel(), tempVesAvg.size()
    );
}

void Bubbles::handleInterrupts(void) { // Bubble Interrupt handler
    digitalWrite(LED, LOW);
    unsigned long now = micros();
    if ((now - ulMicroLast) > RESOLUTION) { // Filter noise/bounce
        single->pulse++;    // Increment pulse count
    }
    Log.verbose(F("॰ₒ°৹๐" CR)); // Looks like a bubble, right?
}

float Bubbles::getRawBpm() { // Return raw pulses per minute (resets counter)
    unsigned long thisTime = millis(); // Get timer value now
    unsigned long ulLapsed = thisTime - single->ulLastReport; // Millis since last run
    float fLapsed = (float) ulLapsed; // Cast to float
    float secs = fLapsed / 60000.0; // Minutes since last request
    float ppm = (pulse / secs); // Calculate PPM
    single->pulse = 0; // Zero the pulse counter
    single->ulLastReport = millis(); // Store the last report timer
    return ppm; // Return pulses per minute
}

float Bubbles::getAmbientTemp() {
    OneWire ambient(AMBSENSOR);
    byte addrAmb[8];
    float fAmbTemp = -100.0;
    while (ambient.search(addrAmb)) { // Make sure we have a sensor
        DallasTemperature sensorAmbient(&ambient);
        sensorAmbient.begin();
        sensorAmbient.requestTemperatures();

        JsonConfig *config = JsonConfig::getInstance();
        if (config->tempinf == true)
            fAmbTemp = sensorAmbient.getTempFByIndex(0) + config->calAmbient;
        else
            fAmbTemp = sensorAmbient.getTempCByIndex(0) + config->calAmbient;
    }
    return fAmbTemp;
}

float Bubbles::getVesselTemp() {
    OneWire vessel(VESSENSOR);
    byte addrVes[8];
    float fVesTemp = -100.0;
    while (vessel.search(addrVes)) { // Make sure we have a sensor
        DallasTemperature sensorVessel(&vessel);
        sensorVessel.begin();
        sensorVessel.requestTemperatures();
        
        JsonConfig *config = JsonConfig::getInstance();
        if (config->tempinf == true)
            fVesTemp = sensorVessel.getTempFByIndex(0) + config->calVessel;
        else
            fVesTemp = sensorVessel.getTempCByIndex(0) + config->calVessel;
    }
    return fVesTemp;
}

float Bubbles::getAvgAmbient() {
    // Retrieve TEMPAVG readings to calculate average
    float avg = 0.0;
    uint8_t size = tempAmbAvg.size();
    for (int i = 0; i < tempAmbAvg.size(); i++) {
        float thisTemp = tempAmbAvg[i];
        avg += tempAmbAvg[i] / size;
    }
    return(avg);
}

float Bubbles::getAvgVessel() {
    // Return TEMPAVG readings to calculate average
    float avg = 0.0;
    uint8_t size = tempVesAvg.size();
    for (int i = 0; i < tempVesAvg.size(); i++) {
        float thisTemp = tempVesAvg[i];
        avg += tempVesAvg[i] / size;
    }
    return(avg);
}

float Bubbles::getAvgBpm() {
    // Return BUBAVG readings to calculate average
    float avg = 0.0;
    uint8_t size = bubAvg.size();
    for (int i = 0; i < bubAvg.size(); i++) {
        float thisTemp = bubAvg[i];
        avg += bubAvg[i] / size;
    }
    return(avg);
}

void Bubbles::createBubbleJson() {
    // const size_t capacity = 3*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5);
    const size_t capacity = BUBBLEJSON;
    StaticJsonDocument<capacity> doc;
    JsonConfig *config = JsonConfig::getInstance();

    doc[F("api_key")] = F(API_KEY);
    doc[F("vessel")] = config->bubname;
    doc[F("datetime")] = single->lastTime;
    
    if (config->tempinf == true) {
        doc[F("format")] = F("F");
    } else {
        doc[F("format")] = F("C");
    }

    // Get bubbles data (updated by minute, no averages)
    JsonObject data = doc.createNestedObject(F("data"));
    data[F("bpm")] = single->lastBpm;
    data[F("ambtemp")] = single->lastAmb;
    data[F("vestemp")] = single->lastVes;

    char output[capacity];
    serializeJson(doc, output, sizeof(output));
    strlcpy(single->bubStatus, output, sizeof(output));
}
