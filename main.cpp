/*
 * Justinas Miseikis
 */

// Drum
#define T1 500
#define T2 1200

#define AMP_CUT_OFF 300    // Drum

unsigned int globalCtr = 0;
unsigned int avgFreq;

// For spectrum analyzer shield, these three pins are used.
// You can move pinds 4 and 5, but you must cut the trace on the shield and re-route from the 2 jumpers. 
int spectrumReset=5;
int spectrumStrobe=4;
int spectrumAnalog=0;  // 0 for the left channel, 1 for the right one.

// Spectrum analyzer read values will be kept here.
int Spectrum[7];
long unsigned int FreqList[7] = { 63, 160, 400, 1000, 2500, 6250, 16000 };

// Timer reload value, globally available
unsigned int tcnt2;

// Toggle HIGH or LOW digital write
int toggle1 = 0;
int toggle2 = 0;
int toggle3 = 0;
int toggle4 = 0;
int toggle5 = 0;

// Keep track of when each note needs to be switched
int count1 = 0;
int count2 = 0;
int count3 = 0;
int count4 = 0;
int count5 = 0;

// Frequencies for outputs
int outFreq = 50;

// On off the sensor
int on1 = 0;
int on2 = 0;
int on3 = 0;
int on4 = 0;
int on5 = 0;

int actFreqList[9] = {500, 142, 83, 58, 45, 37, 31, 26, 22};

/* Frequency Output Pins */
#define PIN9 9
#define PIN10 10
#define PIN11 11
#define PIN12 12
#define PIN13 13

//Setup Function will run once at initialization
void setup()
{
	Serial.begin(9600);
	
	// First disable the timer overflow interrupt
	TIMSK2 &= ~(1<<TOIE2);
	
	// Configure timer2 in normal mode (no PWM)
	TCCR2A &= ~((1<<WGM21) | (1<<WGM20));
	TCCR2B &= ~(1<<WGM22);
	
	// Select clock source: internal I/O clock
	ASSR &= ~(1<<AS2);
	
	// Disable Compare Match A interrupt (only overflow)
	TIMSK2 &= ~(1<<OCIE2A);
	
	// Configure the prescaler to CPU clock divided by 128
	TCCR2B |= (1<<CS22)  | (1<<CS20); // Set bits
	TCCR2B &= ~(1<<CS21);             // Clear bit
	
	/* We need to calculate a proper value to load the counter.
	 * The following loads the value 248 into the Timer 2 counter
	 * The math behind this is:
	 * (Desired period) = 64us.
	 * (CPU frequency) / (prescaler value) = 125000 Hz -> 8us.
	 * (desired period) / 8us = 8.
	 * MAX(uint8) - 8 = 248;
	 */
	
	// Save value globally for later reload in ISR
	tcnt2 = 248;
	
	// Finally load end enable the timer
	TCNT2 = tcnt2;
	TIMSK2 |= (1<<TOIE2);
	
	//Configure I/O Pin Directions
	pinMode(PIN9,    OUTPUT);
	pinMode(PIN10,    OUTPUT);
	pinMode(PIN11,    OUTPUT);
	pinMode(PIN12,    OUTPUT);
	pinMode(PIN13,    OUTPUT);
	
	// Spectrum analyser setup
	byte Counter;
	
	//Setup pins to drive the spectrum analyzer. 
	pinMode(spectrumReset, OUTPUT);
	pinMode(spectrumStrobe, OUTPUT);
	
	//Init spectrum analyzer
	digitalWrite(spectrumStrobe,LOW);
    delay(1);
	digitalWrite(spectrumReset,HIGH);
    delay(1);
	digitalWrite(spectrumStrobe,HIGH);
    delay(1);
	digitalWrite(spectrumStrobe,LOW);
    delay(1);
	digitalWrite(spectrumReset,LOW);
    delay(5);
    
	// Reading the analyzer now will read the lowest frequency.
}

// Install the Interrupt Service Routine (ISR) for Timer2.
ISR(TIMER2_OVF_vect) {
	
	// Reload the timer
	TCNT2 = tcnt2;
	
	// Update all counters
	count1++; count2++; count3++; count4++; count5++;
	
	if (count1 >= outFreq)
	{
		if (on1 == 1) {
			digitalWrite(PIN9, toggle1 == 0 ? HIGH : LOW);
		}
		toggle1 = ~toggle1;
		count1 = 0;
	}
	
	if (count2 >= outFreq)
	{
		if (on2 == 1) {
			digitalWrite(PIN10, toggle2 == 0 ? HIGH : LOW);
		}
		toggle2 = ~toggle2;
		count2 = 0;
	}
	
	if (count3 >= outFreq)
	{
		if (on3 == 1) {
			digitalWrite(PIN11, toggle3 == 0 ? HIGH : LOW);
		}
		toggle3 = ~toggle3;
		count3 = 0;
	}
	
	if (count4 >= outFreq)
	{
		if (on4 == 1) {
			digitalWrite(PIN12, toggle4 == 0 ? HIGH : LOW);
		}
		toggle4 = ~toggle4;
		count4 = 0;
	}
}

// Convert frequency to counts for the actuator (output)
void setCounts(int freq)
{
	float temp = (1 / (float)freq) * 10000;
	outFreq = (int)temp;
}

// Read 7 band equalizer.
void readSpectrum()
{
	// Band 0 = Lowest Frequencies.
	byte Band;
	for(Band=0;Band <7; Band++)
	{
		Spectrum[Band] = (analogRead(spectrumAnalog) + analogRead(spectrumAnalog) ) >>1; //Read twice and take the average by dividing by 2
		digitalWrite(spectrumStrobe,HIGH);
		digitalWrite(spectrumStrobe,LOW);     
	}
}

// Get input spectrums from the analyser board
void showSpectrum()
{
	readSpectrum();
	byte Band;
	
	// Only read middle 5 bands, other ones are not relevant in our case
	for(Band=1;Band<6;Band++)
	{
		
		// Filter out the noise
		if (Spectrum[Band] < 80)
			Spectrum[Band] = 0;
		
	}
}

void summariseFreq()
{
	// Frequencies are:
	// 63Hz, 160Hz, 400Hz, 1kHz, 2.5kHz, 6.25kHz, 16kHz
	
	// We analyse just 5 middle ones
	long int outputFreq = 0;
	int amplSum = 0;
	int maxAmpl = 0;
	int maxFreqBand;
	
	// Get the dominant frequency
	for(int Band=1; Band < 6; Band++) {
		if (Spectrum[Band] > maxAmpl) {
			maxFreqBand = Band;
			maxAmpl = Spectrum[Band];
		}
		amplSum += Spectrum[Band];
		outputFreq += Spectrum[Band] * FreqList[Band]; 
	}
	
	// Check if it's below the cut-off frequency
	if (amplSum < AMP_CUT_OFF) {
		offAll();
		globalCtr = 0;
		avgFreq = 0;
		return;
	}
	outputFreq /= amplSum;
	
	int mappedFreq = map(outputFreq, 1000, 5000, 50, 450);  // Drum
	
	// Get the average frequency
	avgFreq = mappedFreq;
    
	// Convert to counts
	setCounts(avgFreq);
	
	Serial.println(amplSum);
	
	// Check the amplitude and turn on according actuators
	if (amplSum < T1) {
		onLow();
		//Serial.println("; On Low!");
	}
	else if (amplSum >= T1) {
		onHigh();
		//Serial.println("; On Med!");
	}
}

// Turn off all actuators
void offAll()
{
	on1 = 0;
	on2 = 0;
	on3 = 0;
	on4 = 0;
	on5 = 0;
}

// Turn only two actuators on low signal
void onLow()
{
	on1 = 1;
	on2 = 0;
	on3 = 1;
	on4 = 0;
	on5 = 0;
}

// Turn all actuators on high signal
void onHigh()
{
	on1 = 1;
	on2 = 1;
	on3 = 1;
	on4 = 1;
	on5 = 0;
}

// The main loop
void loop()
{ 
	showSpectrum();
	summariseFreq();
}