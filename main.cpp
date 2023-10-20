#include "mbed.h"

#define max7219_reg_noop         0x00
#define max7219_reg_digit0       0x01
#define max7219_reg_digit1       0x02
#define max7219_reg_digit2       0x03
#define max7219_reg_digit3       0x04
#define max7219_reg_digit4       0x05
#define max7219_reg_digit5       0x06
#define max7219_reg_digit6       0x07
#define max7219_reg_digit7       0x08
#define max7219_reg_decodeMode   0x09
#define max7219_reg_intensity    0x0a
#define max7219_reg_scanLimit    0x0b
#define max7219_reg_shutdown     0x0c
#define max7219_reg_displayTest  0x0f

#define LOW 0
#define HIGH 1

#define DATA_SIZE 600

AnalogIn adc(PTB3);
AnalogOut dac(PTE30);
Ticker sampler;
SPI max72_spi(PTD2, NC, PTD1);
DigitalOut load(PTD0); 

unsigned int volatile previousSample = NULL;
int volatile data[DATA_SIZE];
int volatile averageData[DATA_SIZE];
int volatile sampleCounter=0; 
float alpha = 0.1;
unsigned volatile int total = 0;
bool volatile oneCycle =false;
unsigned int volatile average = 0;
unsigned int volatile lastCycleSample;
unsigned int upperThreshold = 65535*(1.5/3.3);
unsigned int lowerThreshold = 65535*(1/3.3);
bool volatile lowerThresholdPassed = true;
unsigned int volatile previousBeat =NULL;
unsigned int volatile currentBeat;
bool volatile beatHappened;
float differenceBeat;
char one[2] = {0x7c,0x00};
char digit[10][3] = {{0x7c,0x44,0x7c},{0x00,0x7c,0x00},{0x74,0x54,0x5c},{0x44,0x54,0x7c},{0x1c,0x10,0x7c},{0x5c,0x54,0x74},{0x7c,0x54,0x74},{0x04,0x04,0x7c},{0x7c,0x54,0x7c},{0x1c,0x14,0x7c}};


void sample(){

    unsigned int analogSignal = adc.read_u16();
    if(oneCycle){
        lastCycleSample = averageData[sampleCounter];
    }

    if(previousSample != NULL){
        averageData[sampleCounter] = (alpha*analogSignal+(1-alpha)*previousSample);
        data[sampleCounter] = averageData[sampleCounter]+32768-average;
    }else{
        data[sampleCounter] = analogSignal;
        averageData[sampleCounter] = analogSignal;
    }


    if(!oneCycle){
        total = total + averageData[sampleCounter];
        average = total/(sampleCounter+1);
    }else{
        total = total - lastCycleSample + averageData[sampleCounter];
        average = total/DATA_SIZE; 
    }

    previousSample = averageData[sampleCounter];
    dac.write_u16(data[sampleCounter]);
    
    if(data[sampleCounter]>upperThreshold && lowerThresholdPassed){
        
        lowerThresholdPassed = false;
        if(previousBeat == NULL){
            currentBeat = sampleCounter;
            previousBeat = sampleCounter;
        }else{
            beatHappened = true;
            previousBeat = currentBeat;
            currentBeat = sampleCounter;
            if (previousBeat>DATA_SIZE){
                previousBeat=previousBeat-DATA_SIZE;
            }
        }
    }else if(data[sampleCounter]<lowerThreshold){
        lowerThresholdPassed = true;
    }

    if (sampleCounter>= DATA_SIZE-1){
        sampleCounter = 0;
        oneCycle = true;
    }else{
        sampleCounter++;
    }

    
}
void write_to_max( int reg, int col)
{
    load = LOW;            // begin
    max72_spi.write(reg);  // specify register
    max72_spi.write(col);  // put data
    load = HIGH;           // make sure data is loaded (on rising edge of LOAD/CS)
}

void setup_dot_matrix ()
{
    // initiation of the max 7219
    // SPI setup: 8 bits, mode 0
    max72_spi.format(8, 0);
     
  
  
       max72_spi.frequency(100000); //down to 100khx easier to scope ;-)
      

    write_to_max(max7219_reg_scanLimit, 0x07);
    write_to_max(max7219_reg_decodeMode, 0x00);  // using an led matrix (not digits)
    write_to_max(max7219_reg_shutdown, 0x01);    // not in shutdown mode
    write_to_max(max7219_reg_displayTest, 0x00); // no display test
    for (int e=1; e<=8; e++) {    // empty registers, turn all LEDs off
        write_to_max(e,0);
    }
   // maxAll(max7219_reg_intensity, 0x0f & 0x0f);    // the first 0x0f is the value you can set
     write_to_max(max7219_reg_intensity,  0x08);     
 
}

void clear(){
     for (int e=1; e<=8; e++) {    // empty registers, turn all LEDs off
        write_to_max(e,0);
    }
}
void number_to_display(int number){
    int output; 
    if (number/100 == 1){
        for (int i=0;i<2;i++){
            output = one[i];  
            write_to_max(i+1,output);
        }
    }
    int tens = number%100;
    for (int i=2;i<5;i++){
        output = digit[tens/10][i-2];
        write_to_max(i+1,output);
        
    }
    int digits = tens%10;
    for (int i=5;i<8;i++){
        output = digit[digits][i-5];
        write_to_max(i+1,output);
    }
        
    
}



int main() {
    setup_dot_matrix();
    sampler.attach(&sample,12500us);
    while(1) {
        if(beatHappened){
            beatHappened = false;
            if(currentBeat<previousBeat){
                currentBeat= currentBeat + DATA_SIZE;
            }
            differenceBeat = currentBeat - previousBeat;
            
            
            unsigned int bpm = 60/(differenceBeat/80);
            if(bpm<200){
                clear();
                number_to_display(bpm);
            }
            
        }

        
        
    }

}