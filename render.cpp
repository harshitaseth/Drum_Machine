/*

 * drum machine which plays sampled drum sounds in loops.
 *
 * This code runs on the Bela embedded audio platform (bela.io).
 */


#include <Bela.h>
#include <cmath>
#include "drums.h"
#include <libraries/Scope/Scope.h>
#include <libraries/Biquad/Biquad.h>
Biquad lpFilter;	// Biquad low-pass frequency;
Biquad hpFilter;


/* Variables which are given to you: */

/* Drum samples are pre-loaded in these buffers. Length of each
 * buffer is given in gDrumSampleBufferLengths.
 */
extern float *gDrumSampleBuffers[NUMBER_OF_DRUMS];
extern int gDrumSampleBufferLengths[NUMBER_OF_DRUMS];

int gIsPlaying = 0;			/* Whether we should play or not. Implement this in Step 4b. */

/* Read pointer into the current drum sample buffer.
 *
 * TODO (step 3): you will replace this with two arrays, one
 * holding each read pointer, the other saying which buffer
 * each read pointer corresponds to.
 */
#define readpointers_number 16
int gReadPointer = 0;
int gReadPointers[readpointers_number] = {0};
int  gDrumBufferForReadPointer[readpointers_number]= {-1};
/* Patterns indicate which drum(s) should play on which beat.
 * Each element of gPatterns is an array, whose length is given
 * by gPatternLengths.
 */
extern int *gPatterns[NUMBER_OF_PATTERNS];
extern int gPatternLengths[NUMBER_OF_PATTERNS];

/* These variables indicate which pattern we're playing, and
 * where within the pattern we currently are. Used in Step 4c.
 */
int gCurrentPattern = 0;
int gCurrentIndexInPattern = 0;

/* This variable holds the interval between events in **milliseconds**
 * To use it (Step 4a), you will need to work out how many samples
 * it corresponds to.
 */
int gEventIntervalMilliseconds = 250;
int gcountsamples = 0;

/* This variable indicates whether samples should be triggered or
 * not. It is used in Step 4b, and should be set in gpio.cpp.
 */
extern int gIsPlaying;

/* This indicates whether we should play the samples backwards.
 */
int gPlaysBackwards = 0;

/* For bonus step only: these variables help implement a fill
 * (temporary pattern) which is triggered by tapping the board.
 */
int gShouldPlayFill = 0;
int gPreviousPattern = 0;
int gfill_pattern = 2;

/* TODO: Declare any further global variables you need here */
/* variables for butthons and LED */
const int gButtonPin1 = 1;
const int gButtonPin2 = 1;
const int kLedPin = 0;
int gPreviousButtonValue = 0;
int gTriggerButton = 0;
bool gLed;

/* variables for poteniometer input */
const unsigned int kInputTempo = 0;

/* varaible for. accelerometer input */ 
int x_inp = 2;
int y_inp = 3;
int z_inp = 4;

/*variables to calculate orientations */
float acc_initial = 0.425;
float upperthreshhold =  0.07;
float lowthreshhold = -0.07;
float ghighpassthresh = 0.25;
int orientation_name = 0;

/*variables to store filters parameters value*/
float gHPfreq = 800.0;
float gLPfreq = 1500.0;
float gFilterQ = 0.707; 
float x_afterhighpass = 0.0;


// Bela oscilloscope
Scope gScope;

// setup() is called once before the audio rendering starts.
// Use it to perform any initialisation and allocation which is dependent
// on the period size or sample rate.
//
// userData holds an opaque pointer to a data structure that was passed
// in from the call to initAudio().
//
// Return true on success; returning false halts the program.

bool setup(BelaContext *context, void *userData)
{
	/* Step 2: initialise GPIO pins */
	if(context->audioFrames != context->digitalFrames) {
		rt_fprintf(stderr, "This example needs audio and digital running at the same rate.\n");
		return false;
	}
	
	for (int i = 0; i < readpointers_number; i++){
        gReadPointers[i] = 0;
        gDrumBufferForReadPointer[i] = -1;
    }
	// Set the mode of digital pins
	pinMode(context, 0, gButtonPin1, INPUT);
	pinMode(context, 0, gButtonPin2, INPUT); 
	pinMode(context, 0, kLedPin, OUTPUT);
	
	// setting up filters from Biquad library
	Biquad::Settings settings{
			.fs = context->audioSampleRate,
			.cutoff = gLPfreq,
			.type = Biquad::lowpass,
			.q = gFilterQ,
			.peakGainDb = 0,
			};
	
	lpFilter.setup(settings);
	settings.cutoff = gHPfreq;
	settings.type = Biquad::highpass;
	hpFilter.setup(settings);
	
	// Initialise the Bela oscilloscope with 2 channels
	gScope.setup(2, context->audioSampleRate);
	return true;
}

// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numMatrixFrames
// will be 0.

void render(BelaContext *context, void *userData)
{
	static int printCounter = 0;
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float out = 0;
		
		// Read the digital input
		
		//Reading button input
		int status_b1 = digitalRead(context, n, gButtonPin1);
		
		// Reading button and previous button value to trigger the gIsPlaying value 
			if(status_b1 == 0 && gPreviousButtonValue != 0) {
				// Button clicked: is the metronome off?
				gTriggerButton = 1;
            	gIsPlaying = !gIsPlaying;
            	gcountsamples = 0;
				}
		
			// storing last button value to gPreviousButtonValue		
			gPreviousButtonValue = status_b1;
		
			// reading poteniometer input
			float input = analogRead(context,  n/2, kInputTempo);
		
			// mapping poteniometer input to event intervals between 50-100
			gEventIntervalMilliseconds = map(input, 0, 3.3/4.096, 50, 1000);
	
			// if gIsPlaying is 1, trigger the LED with the change in tempo
			if(gIsPlaying)
				{
				if(gcountsamples >= gEventIntervalMilliseconds * 0.001 * context->audioSampleRate) 
					{
                		startNextEvent();
                		gcountsamples = 0;
                		if(gLed == LOW)
                    		gLed = HIGH;
                		else
                    		gLed = LOW;
                
            		}
            	gcountsamples++;
            	// writing LED output
            	digitalWriteOnce(context, n, kLedPin, gLed);
				}
			
		
			// Reading x, y, and z axis value
			// Subtracting the acc_initial value to get the negative value. 
			// acc_initial is calculated taking resting position as reference position (x=0,y=0)
			float x = analogRead(context,n/2,x_inp) - acc_initial;
			float y = analogRead(context,n/2,y_inp) - acc_initial;
			float z = analogRead(context,n/2,z_inp) - acc_initial;
			
		
			/* low pass filter_output
			// Applying low pass filter to x, y, z
			// commented this part as later applying high pass filter
			float x_low = lpFilter.process(x);
			float y_low = lpFilter.process(y);
			float z_low = lpFilter.process(z);
			rt_printf()
			*/
		
			// Intializing x_state, y_state, z_state
			// Value will get updated after applying Hysteresis
			int x_state = 0;
			int y_state = 0;
			int z_state = 0;
	
			// Adding if condition to calculate the orientation in certain intervals	
			if(printCounter++ > 0.05*context->audioSampleRate)
			{
				printCounter = 0;
			
				// if conditions to decide final value of x_state
				if(lowthreshhold < x &&  x < upperthreshhold )
					x_state = 0;
				else if(x >= upperthreshhold)
					x_state = 1;
				else if(x<=lowthreshhold)
					x_state =-1;
		
				// if conditions to decide final value of y_state
				if(lowthreshhold < y &&  y < upperthreshhold)
					y_state = 0;
				else if(y >= upperthreshhold)
					y_state = 1;
				else if(y <= lowthreshhold)
					y_state =-1;
			
				// if conditions to decide final value of y_state
				if(lowthreshhold < z && z < upperthreshhold )
					z_state = 0;
				else if(z >= upperthreshhold)
					z_state = 1;
				else if(z <= lowthreshhold)
					z_state =-1;
				
				
			
				// • Resting flat (X = 0, Y = 0, Z = positive) 
				if (x_state == 0 && y_state ==0 && z_state == 1)
					orientation_name = 1;  //"Resting"
					gPlaysBackwards = 0;
			
				// • Turned vertically on left side (X = negative, Y = 0, Z = 0)
				if (x_state == -1 && y_state ==0 && z_state == 0){
					orientation_name = 2; //"Left"
					gPlaysBackwards = 0;
					}
				// • Turned vertically on right side (X = positive, Y = 0, Z = 0) 
				if (x_state == 1 && y_state ==0 && z_state == 0){
					orientation_name = 3; //"Right"
					gPlaysBackwards = 0;
					}
				//• Turned vertically on front side (X = 0, Y = negative, Z = 0) 
				if (x_state == 0 && y_state ==-1 && z_state == 0){
					orientation_name = 4; //"Front"
					gPlaysBackwards = 0;
					}
				// • Turned vertically on back side (X = 0, Y = positive, Z = 0) 
				if (x_state == 0 && y_state ==1 && z_state == 0){
					orientation_name = 5; //"Back"
					gPlaysBackwards = 0;
					}
				//• Turned upside down
				if (x_state == 0 && y_state ==0 && z_state == -1){
					orientation_name = 0; //"upside down"
					gPlaysBackwards = 1;
			}
		
		
		
			//Applying high pass filter to x, y and z value
			float x_afterhighpass = hpFilter.process(sqrt(pow(x,2)));
			float y_afterhighpass = hpFilter.process(sqrt(pow(y,2)));
			float z_afterhighpass = hpFilter.process(sqrt(pow(z,2)));
		
		
			// if any x_afterhighpass, y_afterhighpass, z_afterhighpass greater than the threshold, gShouldPlayFill =1 
			// if fill_pattern is not playing, save the current pattern to gPreviousPattern to play later
			if (x_afterhighpass > ghighpassthresh || y_afterhighpass > ghighpassthresh || z_afterhighpass >ghighpassthresh){
				gShouldPlayFill = 1;
				if (gCurrentPattern != gfill_pattern)
                	gPreviousPattern = gCurrentPattern;
					gCurrentIndexInPattern = 0;
				}
			

			rt_printf("orientation_name  gShouldPlayFill,%d %d \n",gCurrentPattern, gShouldPlayFill);
				
			}	
			// gScope.log(z, 0);
			
			// check for gShouldPlayFill, if true play fill_pattern
			// if false play the patter give by orientation 
			if (gShouldPlayFill == 1){
				gCurrentPattern = gfill_pattern;
				}
			else if(gShouldPlayFill == 0){
				gCurrentPattern = orientation_name;
				}

		
			// Adding the samples of two drum sound and increasing the readpointers
			// Empty the Drum buffer if readpointer >= buffer length
			// if gPlaysBackwards is true, to play the drum sound backward,  intialize the gReadPointers with gDrumSampleBufferLengths-1
			// if gPlaysBackwards is False, play the sound forward, intialize the gReadPointers with 0  
			for(int i = 0; i < readpointers_number; i++)
            	{
				if(gDrumBufferForReadPointer[i] >= 0)
					{	
						if(gPlaysBackwards==0)
						{
							out = gDrumSampleBuffers[gDrumBufferForReadPointer[i]][gReadPointers[i]];
							gReadPointers[i]++;
							if(gReadPointers[i] >= gDrumSampleBufferLengths[gDrumBufferForReadPointer[i]]) 
							{
								gReadPointers[i] = 0;
								gDrumBufferForReadPointer[i] = -1;
                    
							}	
	
						}
					
    					if(gPlaysBackwards==1)
    					{
							out = gDrumSampleBuffers[gDrumBufferForReadPointer[i]][gReadPointers[i]];
							gReadPointers[i]--;
							if(gReadPointers[i] < 0) 
							{
								gReadPointers[i] = gDrumSampleBufferLengths[gDrumBufferForReadPointer[i]];
								gDrumBufferForReadPointer[i] = -1;
                    		}	
						}
					
					}
            	}
	

					
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			// Write the sample to every audio output channel
    		audioWrite(context, n, channel, out);
    	}
    	
    	// Log the digital input and the audio output to the scope
    	// gScope.log(x_afterhighpass, y_afterhighpass);
		
	}

}

/* Start playing a particular drum sound given by drumIndex.
 */
void startPlayingDrum(int drumIndex) {
	/* TODO in Steps 3a and 3b */
	for(int i=0; i < readpointers_number; i++){
		if (gDrumBufferForReadPointer[i] < 0){
			gDrumBufferForReadPointer[i] = drumIndex;
			
			// adding condition for gPlaysBackwards
			// if gPlaysBackwards is true, to play the drum sound backward,  intialize the gReadPointers with gDrumSampleBufferLengths-1
			// if gPlaysBackwards is False, play the sound forward, intialize the gReadPointers with 0  
			if(gPlaysBackwards==1){
				gReadPointers[i]  = gDrumSampleBufferLengths[i] -1;
			}
			if(gPlaysBackwards ==0){
			gReadPointers[i] = 0;
			}
			break;
		}
		
	}
	
}

/* Start playing the next event in the pattern */
void startNextEvent() {
	/* TODO in Step 4 */
	
	// added this for part 4a to play single drum sound 
	// startPlayingDrum(2);
	
	// for loop check all the drum sound in event and play it through startPlayingDrum()
	int event = gPatterns[gCurrentPattern][gCurrentIndexInPattern];
    for(int i=0; i< NUMBER_OF_DRUMS; i++){
    	int out = eventContainsDrum(event,i);
    	if (out ==1){
    		startPlayingDrum(i);
    	}
    	
    	
    }
    
    //increase the patter indices
    gCurrentIndexInPattern += 1;
    //if indices goes beyond the length, re-initialize with 0
    if(gCurrentIndexInPattern >= gPatternLengths[gCurrentPattern]){
    	gCurrentIndexInPattern = 0;
    	
    	//check if fill is already playing, if it is change the currentpattern with gPreviousPattern
    	if (gCurrentPattern == gfill_pattern){
            gCurrentPattern = gPreviousPattern;
            gShouldPlayFill = 0;
        }
    }
	
}

/* Returns whether the given event contains the given drum sound */
int eventContainsDrum(int event, int drum) {
	if(event & (1 << drum))
		return 1;
	return 0;
}

// cleanup_render() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in initialise_render().

void cleanup(BelaContext *context, void *userData)
{

}
