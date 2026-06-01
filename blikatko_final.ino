#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

#define BUTTON_PIN 4
#define LED1 0
#define LED2 1
#define LED3 2
#define LED4 3

bool enabled = false;
byte mode = 0;

unsigned long lastBlink = 0;
unsigned long pressStart = 0;
bool lastButton = HIGH;
bool longPressDone = false;
bool ledState = false;
unsigned long offStart = 0;

volatile bool wakeUpFlag = false;

// battery + warning
unsigned long lastBatteryCheck = 0;
bool warningActive = false;
byte warningBlinkCount = 0;
unsigned long warningTimer = 0;
bool warningState = false;

enum WarningPhase {
  WARNING_IDLE,
  WARNING_PRE_GAP,
  WARNING_BLINK,
  WARNING_POST_GAP
};

WarningPhase warningPhase = WARNING_IDLE;

ISR(PCINT0_vect){ wakeUpFlag = true; }

// ---------- LED CONTROL ----------
void writeAllLEDs(bool state) {
  digitalWrite(LED1, state);
  digitalWrite(LED2, state);
  digitalWrite(LED3, state);
  digitalWrite(LED4, state);
}

void setAll(bool state) { writeAllLEDs(state); }
void allOff() { writeAllLEDs(LOW); }
void allOn() { writeAllLEDs(HIGH); }

// ---------- SLEEP ----------
void goToSleep(){
  allOff();
  lastButton = HIGH;
  GIMSK = (1 << PCIE);
  PCMSK = (1 << PCINT4);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_enable();

#if defined(BODS) && defined(BODSE)
  MCUCR = (1 << BODS) | (1 << BODSE);
  MCUCR = (MCUCR & ~(1 << BODSE)) | (1 << BODS);
#endif

  sei();
  sleep_cpu();

  sleep_disable();
  PCMSK &= ~(1 << PCINT4);
  wakeUpFlag=false;
}

// ---------- ADC BATTERY ----------
long readVcc() {
  power_adc_enable();
  ADCSRA |= (1 << ADEN);

  ADMUX = _BV(MUX3) | _BV(MUX2);
  delay(2);

  // dummy reads
  for (byte i = 0; i < 2; i++) {
    ADCSRA |= _BV(ADSC);
    while (bit_is_set(ADCSRA, ADSC));
  }

  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));

  long result = ADC;
  result = 1126400L / result;

  ADCSRA &= ~(1 << ADEN);
  power_adc_disable();

  return result;
}

void checkBattery() {
  if (!enabled) return;

  if (millis() - lastBatteryCheck >= 30000) {
    lastBatteryCheck = millis();

    long vcc = readVcc();

    if (vcc < 3300) {
      warningActive = true;
      warningPhase = WARNING_PRE_GAP;
      warningTimer = millis();
    }
  }
}

void handleLowBatteryWarning() {
  if (!warningActive) return;

  switch (warningPhase) {

    case WARNING_PRE_GAP:
      setAll(LOW);
      if (millis() - warningTimer >= 300) {
        warningPhase = WARNING_BLINK;
        warningTimer = millis();
        warningBlinkCount = 0;
        warningState = false;
      }
      break;

    case WARNING_BLINK:
      if (millis() - warningTimer >= 120) {
        warningTimer = millis();

        warningState = !warningState;
        setAll(warningState);

        if (warningState) {
          warningBlinkCount++;
        }

        if (warningBlinkCount >= 5) {
          warningPhase = WARNING_POST_GAP;
          warningTimer = millis();
        }
      }
      break;

    case WARNING_POST_GAP:
      setAll(LOW);
      if (millis() - warningTimer >= 300) {
        warningActive = false;
        warningPhase = WARNING_IDLE;
      }
      break;

    default:
      warningActive = false;
      warningPhase = WARNING_IDLE;
      break;
  }
}

// ---------- SETUP ----------
void setup() {
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  ADCSRA &= ~(1 << ADEN);
  power_adc_disable();
  power_timer1_disable();
  power_usi_disable();

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  allOff();
}

// ---------- LOOP ----------
void loop() {
  bool button = digitalRead(BUTTON_PIN);

  if (lastButton == HIGH && button == LOW) {
    pressStart = millis();
    longPressDone = false;
  }

  if (button == LOW && !longPressDone && millis() - pressStart > 1000) {
    enabled = !enabled;
    longPressDone = true;
    allOff();
  }

  if (lastButton == LOW && button == HIGH) {
    unsigned long pressTime = millis() - pressStart;

    if (pressTime < 500 && enabled) {
      mode++;
      if (mode > 6) mode = 0;
      allOff();
    }
  }

  lastButton = button;

  if (!enabled) {
    allOff();
    if (offStart == 0) offStart = millis();
    if (millis() - offStart >= 5000) {
      goToSleep();
      offStart = millis();
    }
    return;
  } else {
    offStart = 0;
  }

  checkBattery();
  handleLowBatteryWarning();

  if (warningActive) return;

  switch (mode) {
    case 0: allOn(); break;
    case 1: blinkMode(500); break;
    case 2: blinkMode(100); break;
    case 3: doubleBlink(); break;
    case 4: dosebe(); break;
    case 5: dolu_a_na_horu(); break;
    case 6: prvni_druha(); break;
  }
}

// ---------- EFFECTS ----------
void setLED1(bool state){ digitalWrite(LED1, state); }
void setLED2(bool state){ digitalWrite(LED2, state); }
void setLED3(bool state){ digitalWrite(LED3, state); }
void setLED4(bool state){ digitalWrite(LED4, state); }

void blinkMode(unsigned long interval) {
  if (millis() - lastBlink >= interval) {
    lastBlink = millis();
    ledState = !ledState;
    setAll(ledState);
  }
}

void doubleBlink() {
  static int step = 0;
  unsigned long intervals[] = {100,100,100,700};

  if (millis() - lastBlink >= intervals[step]) {
    lastBlink = millis();

    switch (step) {
      case 0: setAll(HIGH); break;
      case 1: setAll(LOW); break;
      case 2: setAll(HIGH); break;
      case 3: setAll(LOW); break;
    }

    step++;
    if (step > 3) step = 0;
  }
}

void dosebe() {
  static int step = 0;
  unsigned long intervals[] = {0,250,0,250,0,250,0,250,0};

  if (millis() - lastBlink >= intervals[step]) {
    lastBlink = millis();

    switch (step) {
      case 0: setAll(LOW); break;
      case 1: setLED1(HIGH); break;
      case 2: setLED4(HIGH); break;
      case 3: setLED3(HIGH); break;
      case 4: setLED2(HIGH); break;
      case 5: setLED1(LOW); break;
      case 6: setLED4(LOW); break;
      case 7: setLED3(LOW); break;
      case 8: setLED2(LOW); break;
    }

    step++;
    if (step > 8) step = 0;
  }
}

void dolu_a_na_horu() {
  static int step = 0;
  unsigned long intervals[] = {300,300,0,300,0,300,0,300};

  if (millis() - lastBlink >= intervals[step]) {
    lastBlink = millis();

    switch (step) {
      case 0: setLED1(LOW); break;
      case 1: setLED2(LOW); break;
      case 2: setLED1(HIGH); break;
      case 3: setLED3(LOW); break;
      case 4: setLED2(HIGH); break;
      case 5: setLED3(HIGH); break;
      case 6: setLED4(LOW); break;
      case 7: setLED4(HIGH); break;
    }

    step++;
    if (step > 7) step = 0;
  }
}

void prvni_druha() {
  static int step = 0;
  unsigned long intervals[] = {0,0,300,0,0,300};

  if (millis() - lastBlink >= intervals[step]) {
    lastBlink = millis();

    switch (step) {
      case 0: setLED1(HIGH); break;
      case 1: setLED4(HIGH); break;
      case 2: setAll(LOW); break;
      case 3: setLED2(HIGH); break;
      case 4: setLED3(HIGH); break;
      case 5: setAll(LOW); break;
    }

    step++;
    if (step > 5) step = 0;
  }
}
