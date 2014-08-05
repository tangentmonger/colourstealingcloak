//Cloak v2.3
//Twinkles white by default.
//On 'sense' button, washes the sensed colour from top to bottom
//On 'mode' button, switches between white, fire, absinthe and sea modes

#include "Adafruit_WS2801.h"
#include "timer.h"
#include "SPI.h"

//0 = no glove connected
//1 = glove is connected
#define gloveconnected 1

uint8_t LED_DATA_PIN = 4; // LED White wire 
uint8_t LED_CLOCK_PIN = 5; // LED Green wire
const int nPixels = 150;
Adafruit_WS2801 strip = Adafruit_WS2801(nPixels, LED_DATA_PIN, LED_CLOCK_PIN);

#if gloveconnected
int BUTTON_SENSE_PIN = 2; //green, fingertip 
int BUTTON_MODE_PIN = 3; //blue, handwards
//also, sensor green into board RX
//sensor white into board TX
//i.e. reversed compared to looking at serial on PC via arduino
#endif

volatile bool twinkleDue = false;
const int sizeTwinkle = 20;
byte twinkle[sizeTwinkle];
byte twinkleTable[20] = {0,7,13,20,13,9,7,4,3,2,1,1,0,0,0,0,0,0};
enum modes {defaultWhite, fire, absinthe, sea, sensedColour, sensedWipe} mode = absinthe;
modes previousMode = defaultWhite;

const byte alterFactor = 7;


//LED indices in order from the topmost to bottommost
//i.e. pixelInPlace[i] = the pixel in the i'th place from the top
byte pixelInPlace[150] = {6, 42, 5, 7, 43, 41, 4, 8, 44, 39, 9, 2, 3, 38, 40, 45, 10, 37, 46, 1, 11, 12, 36, 47, 14, 13, 15, 48, 0, 35, 16, 31, 49, 18, 19, 21, 20, 22, 29, 30, 32, 17, 23, 24, 25, 33, 34, 27, 28, 26, 50, 54, 51, 53, 98, 100, 101, 52, 55, 99, 102, 103, 108, 107, 149, 97, 104, 56, 106, 109, 128, 96, 105, 76, 127, 148, 57, 95, 110, 129, 75, 126, 77, 111, 147, 58, 74, 94, 130, 125, 59, 78, 146, 93, 112, 131, 73, 124, 60, 79, 145, 92, 113, 132, 61, 72, 123, 80, 144, 91, 114, 133, 62, 71, 122, 81, 143, 90, 115, 134, 63, 70, 121, 142, 82, 116, 89, 135, 64, 69, 119, 120, 141, 83, 88, 117, 118, 136, 65, 68, 84, 140, 87, 137, 66, 67, 85, 138, 139, 86};

//LED place location of each pixel
//i.e. xxx[i] = pixel i's position in the order
byte placeOfPixel[150] = {28, 19, 11, 12, 6, 2, 0, 3, 7, 10, 16, 20, 21, 25, 24, 26, 30, 41, 33, 34, 36, 35, 37, 42, 43, 44, 49, 47, 48, 38, 39, 31, 40, 45, 46, 29, 22, 17, 13, 9,  14, 5, 1, 4, 8, 15, 18, 23, 27, 32, 50, 52, 57, 53, 51, 58, 67, 76, 85, 90, 98, 104, 112, 120, 128, 138, 144, 145, 139, 129, 121, 113, 105, 96, 86, 80, 73, 82, 91, 99,  107, 115, 124, 133, 140, 146, 149, 142, 134,126, 117, 109, 101, 93, 87, 77, 71, 65, 54, 59, 55, 56, 60, 61, 66, 72, 68, 63, 62, 69, 78, 83, 94, 102, 110, 118, 125, 135, 136, 130, 120, 122, 114,106, 97, 89, 81, 74, 70, 79, 88, 95, 103, 111, 119, 127, 137, 143, 147, 148, 141, 132, 123, 116, 108, 100, 92, 84, 75, 64};

int stripeSize = 20;
int wipeTable[20] = {0,3,5,10,15, 20,20,20,20,20, 20,20,20,20,20, 15,10,5,3,0};

int stripeTop;

const byte brightnessDivider = 2; //1=full, etc

const Colour black(0,0,0);
const Colour white(255, 255, 255);
Colour targetColour; //can change in ISRs
Colour pixelColour[nPixels]; //changes in ISRs
Colour previousTargetColour;
void setup() {
    #if gloveconnected
    Serial.begin(38400);
    Serial.write('E'); //end continuous sampling mode
    Serial.write(13);
    Serial.flush();
    #endif

    targetColour = white;
    previousTargetColour = white;
    for(int i=0; i<nPixels; i++){
        pixelColour[i] = targetColour;
    }

    pinMode(13, OUTPUT); //LED 13
    
    #if gloveconnected
    pinMode(BUTTON_MODE_PIN, INPUT);
    pinMode(BUTTON_SENSE_PIN, INPUT);
    #endif

    strip.begin();

    //clear out any remaining serial data caused by default of continuous read
    #if gloveconnected
    while(Serial.available()) {
        Serial.read(); //throw away data
    }
    #endif

    //All interrupts are individually masked with the Timer Interrupt Mask Register (TIMSK1).
    //TOIE1: Timer/Counter1, Overflow Interrupt Enable
    //assumption: global interrupts are enabled
    // _BV = bit value macro for deriving a byte with the Nth bit enabled
    
    TIMSK1 |= _BV(TOIE1);

    #if gloveconnected
    //EICRA: external interrupt control register A
    //rising edge of INT1 generates interrupt
    EICRA |= _BV(ISC11);
    EICRA |= _BV(ISC10);
    //rising edge of INT0 generates interrupt
    EICRA |= _BV(ISC01);
    EICRA |= _BV(ISC00);

    //EIMSK – External Interrupt Mask Register
    EIMSK |= _BV(INT0); //enable interrupt 0 (digital pin 2)
    EIMSK |= _BV(INT1); //enable interrupt 1 (digital pin 3)
    #endif
}


void loop() {
    #if gloveconnected
    static char sensorstring[30] = "";
    
    //if the hardware serial port receives a char
    if (Serial.available()) { 
        char inchar = (char)Serial.read();
        
        switch (inchar){
            case '\n': // end of serial communication. Discard it
                break;
            case '\r': //end of data from sensor
                interpretSensorData(sensorstring);
                strcpy(sensorstring, ""); //clear buffer
                //EIFR – External interrupt flag register
                //clear sensor trigger button caused by bouncing
                EIFR = 0x3; //set flags to 1 to clear
                EIMSK |= _BV(INT0); //re-enable interrupt 0 (sensor trigger button)
                break;
            default:
                strncat(sensorstring, (char *) &inchar, 1); //buffer character
                break;
        }
    }
    #endif

    if (twinkleDue) {
        doTwinkle();
        twinkleDue = false;
    }
}

//ISRs ----------------------------------------

// fingertip = sense
ISR(INT0_vect) {
    EIMSK ^= _BV(INT0); //disable interrupt 0 until the sensing has completed (assumption: it will actually complete)
    Serial.write('R');
    Serial.write(13);
}

//handwards
ISR(INT1_vect) {
    static unsigned long last_interrupt_time = 0;
    unsigned long interrupt_time = millis();
    // If interrupts come faster than 200ms, assume it's a bounce and ignore
    if (interrupt_time - last_interrupt_time > 200) {
        advanceMode();
    }
    last_interrupt_time = interrupt_time;
}

ISR(TIMER1_OVF_vect) {
    //nano is 16MHz, ie 16000000 times per second
    //timer1 is 16 bits, ie 0 to 65535
    //so with default prescaler, it will overflow ~244 times per second
    //not changing prescaler because it's shared by timer0 which is used by many delay funtions
    //BUT I am actually seeing overflow 488/s
    //TODO: figure out why this is different

    static int count = 0;
    count++;
    //if(count==488){ // ~1Hz
    //if(count==48){ // ~10Hz
   
    
    //different modes twinkle at different rates
    switch(mode) {
        case defaultWhite:
        case fire:
        case sensedColour:
        case sensedWipe:
        case absinthe:
            if(count>=15){ //~40Hz
                count = 0;
                twinkleDue = true;
            }
            break;
        case sea:
            if(count>=30){ //~20Hz
                count = 0;
                twinkleDue = true;
            }
            break;
       }
}

//--------------------------------------------------------

void advanceMode() {
    switch(mode) {
        case defaultWhite:
            mode = fire;
            break;
        case fire:
            mode = absinthe;
            break;
        case absinthe:
            mode = sea;
            break;
        case sensedColour:
        case sensedWipe: //won't look great because some stripe will be left behind
            EIFR = 0x3; //set flags to 1 to clear
            EIMSK |= _BV(INT0); //reenable the other interrupt, ie a reset in case of no response from sensor
            //fallthrough
        case sea:
            mode = defaultWhite;
            targetColour = white;
            break;
    }
}

void doTwinkle() {
    shiftTwinkleList();
    addNewPixelToTwinkleList();
    calculateNewPixelColour();
    updateTwinkle();   
    if(mode == sensedWipe) {
        updateWipe();
    }
    strip.show();
}



Colour interpretSensorData(char* sensorstring) {
    //M1 format: something like RR,GGG,B\n or RRR,GGG,BBB,*\n
    //M2 format: something like RRLL,GL,BLL,T,B\n or RRLL,GLLL,BLLL,TOT,BEY*\n
    //M3 format: something like RR,GGG,B,RRLL,GL,BLL,T,B\n or RRR,GGG,BBB,RRLL,GLLL,BLLL,TOT,BEY*\n

    //using M2
    char *rluxstart = strtok(sensorstring, ",");
    char *gluxstart = strtok(NULL, ",");
    char *bluxstart = strtok(NULL, ","); //comma because there are more values
    Colour luxIn(atoi(rluxstart), atoi(gluxstart), atoi(bluxstart));

    static const Colour luxMax(100,85,95); //based on observations
    static const Colour luxMin(65, 50, 70); //was 65 50 80 but blue was too weak

    //stretch raw values from this range out to 0-255
    Colour luxOut;
    for(int i=0; i<3; i++){
        int shifted = luxIn.array[i] - luxMin.array[i];
        if (shifted < 0) shifted = 0;
        luxOut.array[i] = (shifted * 255) / (luxMax.array[i] - luxMin.array[i]); // not limiting brightness here
    }

    //todo: could maximise brightness, by converting to HSL first
    //todo: refactor this to split out stripe preparation

    //prepare for colour stripe wash
    stripeTop = -(stripeSize);
    previousTargetColour = targetColour;
    targetColour = luxOut; //set atomically because of interrupts
    previousMode = mode;
    mode = sensedWipe;
}


void shiftTwinkleList() {
    //shift the LED indices down by one to make room
    //last pixel falls off and is discarded (its twinkle is complete)
    for (int i=sizeTwinkle-1; i>0; i--) {
        twinkle[i] = twinkle[i-1];
    }

}

void addNewPixelToTwinkleList() {
    //Select a pixel to begin twinkling
    bool ok = true;
    do {
        modes useMode = mode;
        if(mode == sensedWipe) {
            //there's a sensed colour wipe going through
            //are we going to choose above or below?
            int selectedArea = random(0, nPixels);
            if (selectedArea > stripeTop + stripeSize) {
                //below the stripe, so use previous mode to select a pixel
                useMode = previousMode;
            }
        }
        
        if(useMode == absinthe && random(0,10) > 0) {
            //90% chance of selecting the next pixel along to cause coruscation effect
            twinkle[0] = (twinkle[1]+1) % nPixels;
        }
        else
        {
            twinkle[0] = random(0, nPixels);
        }

        //check that chosen pixel is not already twinkling
        ok = true;
        for (int i=1; i<sizeTwinkle; i++) {
            if (twinkle[0] == twinkle[i]) {
                ok = false;
                break;
            }
        }
    } while (ok == false);

}

void calculateNewPixelColour() {

    //work out newly twinking pixel's colour
    switch(mode) {
        case defaultWhite:
        case sensedColour:
            pixelColour[twinkle[0]] = alteredColour(targetColour);
            break;
        case fire:
            pixelColour[twinkle[0]] = getFirePixelColour(twinkle[0]);
            break;
        case absinthe:
            pixelColour[twinkle[0]] = getAbsinthePixelColour(twinkle[0]);
            break;
        case sea:
            pixelColour[twinkle[0]] = getSeaPixelColour(twinkle[0]);
            break;
        case sensedWipe:
            if(placeOfPixel[twinkle[0]] >= (stripeTop + stripeSize)) {
                switch(previousMode) {
                    case defaultWhite:
                    case sensedColour:
                        pixelColour[twinkle[0]] = alteredColour(previousTargetColour);
                        break;
                    case fire:
                        pixelColour[twinkle[0]] = getFirePixelColour(twinkle[0]);
                        break;
                    case absinthe:
                        pixelColour[twinkle[0]] = getAbsinthePixelColour(twinkle[0]);
                        break;
                    case sea:
                        pixelColour[twinkle[0]] = getSeaPixelColour(twinkle[0]);
                        break;
                }
            } else {
                pixelColour[twinkle[0]] = alteredColour(targetColour);
            }
    }
}

Colour getFirePixelColour(int pixelID) {
    //from the top, in layers:
    //red, yellow, white

    //roughly whereabouts is this pixel?
    int distanceFromTop = placeOfPixel[pixelID];

    //these represent the layer at which the colour is pure. Fade around them
    int whiteBand = nPixels;
    int yellowBand = int(nPixels * 0.8);
    int redBand = int(nPixels * 0.4);
    int blackBand = 0;

    Colour newColour;
    if (distanceFromTop < redBand) {
        newColour = Colour(255,0,0);
    }
    else if (distanceFromTop < yellowBand) {
        newColour = selectFade(Colour(255,0,0), Colour(255,255,0), yellowBand - redBand, distanceFromTop - redBand);
    }
    else if (distanceFromTop < whiteBand) {
        newColour = selectFade(Colour(255,255,0), Colour(255,255,128), whiteBand - yellowBand, distanceFromTop - yellowBand);
    } 
    else {
        newColour = Colour(255,255,128); 
    }
    return newColour;
}

Colour getAbsinthePixelColour(int pixelID) {

    return Colour(0,255,0);
}

Colour getSeaPixelColour(int pixelID) {
    //from the top, in layers:
    //white, blue/green, black

    //roughly whereabouts is this pixel?
    int distanceFromTop = placeOfPixel[pixelID];

    //these represent the layer at which the colour is pure. Fade around them
    int whiteBand = 0;
    int bluegreenBand = int(nPixels * 0.5);
    int blackBand = nPixels;
    
    Colour newColour;
    int greenPortion = random(0, 255);
    int bluePortion = 255 - greenPortion; 
    
    if (distanceFromTop >= bluegreenBand) {
        //fade from blue/green to black
        newColour = selectFade(Colour(0, greenPortion, bluePortion), black, blackBand - bluegreenBand, distanceFromTop - bluegreenBand);
    }
    else {
        //fade from white to blue/green
        newColour = selectFade(white, Colour(0, greenPortion, bluePortion), bluegreenBand, distanceFromTop);
    }

    return newColour;

}

Colour selectFade(Colour startColour, Colour endColour, int fadeLength, int position) {
    Colour newColour;
    for (int rgb = 0; rgb<3; rgb++) {
        //position is zero-based, so fadeLength is shortened to match
        long update = long(startColour.array[rgb]) + ((long(endColour.array[rgb] - startColour.array[rgb]) * position) / (fadeLength - 1));
        newColour.array[rgb] = int(update);
    }
    return newColour;
}


void updateTwinkle() {
    //Select the next colour for each pixel that is currently twinkling
    for (int i=0; i<sizeTwinkle; i++) {
        Colour newColour;
        if (mode == absinthe) {
            //black - white - pixelColour - black
            if (i < 4) {
                newColour = selectFade(Colour(0,0,0), Colour(255,255,255), 4, i);
            } else if (i < 12) {
                newColour = selectFade(Colour(255,255,255), pixelColour[twinkle[i]], 8, i-4);
            } else {
                newColour = selectFade(pixelColour[twinkle[i]], Colour(0,0,0), 8, i-12);
            }
        }
        else {
            //Alter colours of pixels from black to pixelColour and back, according to twinkleTable (a rough, skewed sine wave)
            for (int rgb = 0; rgb<3; rgb++) {
                double update = (pixelColour[twinkle[i]].array[rgb] * twinkleTable[i]) / sizeTwinkle;
                newColour.array[rgb] = (int)update;
            }
        }
        strip.setPixelColor(twinkle[i], dimColour(newColour).raw24); //limit brightness at the last moment
    }
}

void updateWipe(){
    pixelColour[pixelInPlace[stripeTop+stripeSize]] = alteredColour(targetColour); // set a colour for wipe (and any twinking involvement)
    for(int i=0; i<stripeSize && (stripeTop+i)<nPixels; i++) {
        Colour newColour;
        for (int rgb = 0; rgb<3; rgb++) {
            double update = (pixelColour[pixelInPlace[stripeTop+i]].array[rgb] * wipeTable[i]) / stripeSize;
            newColour.array[rgb] = (int)update;
         }
        
        if(stripeTop+i >= 0){
            strip.setPixelColor(pixelInPlace[stripeTop+i], newColour.raw24); //full brightness just for the swipe
        }
    }
    stripeTop++;
    if(stripeTop > nPixels) mode = sensedColour; //finished
}


Colour alteredColour(Colour input) {
    Colour altered = input;
    for(int i=0; i<3; i++){
        byte value = altered.array[i];
        if(value < (255-alterFactor) && (value > alterFactor)) {
            value += random(2*alterFactor) - alterFactor;
        } else if (value < (255-alterFactor)) {
            value += random(0, alterFactor);
        } else if (value > alterFactor) {
            value -=random(0, alterFactor);
        }
        altered.array[i] = value;
    }
    return altered;
}

Colour dimColour(Colour input) {
    return Colour(input.red/brightnessDivider, input.green/brightnessDivider, input.blue/brightnessDivider);
}

//Toggle state of Arduino onboard LED. Useful for debugging
void toggleLED13 () {
    static bool LED13 = false;
    if(LED13) {
        LED13 = false;
        digitalWrite(13, LOW);
    }
    else {
        LED13 = true;
        digitalWrite(13, HIGH);
    }
}
