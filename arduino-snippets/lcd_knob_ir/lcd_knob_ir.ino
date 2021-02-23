#include <Wire.h>
#include "Adafruit_MCP23017.h"
Adafruit_MCP23017 mcp;

#include <LiquidCrystal.h>
LiquidCrystal lcd(12, 11, 5, 4, 9, 8);

#define PIN_ENCODER_A 2
#define PIN_ENCODER_B 3
// see https://www.arduino.cc/en/Reference/PortManipulation
#define PORT_REGISTER PIND

static uint8_t enc_prev_pos = 0;
static uint8_t enc_flags    = 0;

#define BOARD_ANALOG 0
#define BOARD_DIGITAL 1
#define TOTAL_INPUTS 8
#define TOTAL_OUTPUTS 4

#define PIN_KNOB_PRESS 10

#define ANALOG_SWITCH_1 0
#define ANALOG_SWITCH_2 1
#define ANALOG_SWITCH_3 2
#define ANALOG_SWITCH_4 3
#define ANALOG_INPUTS 4

int analog_inputs[] = {ANALOG_SWITCH_1, ANALOG_SWITCH_2, ANALOG_SWITCH_3, ANALOG_SWITCH_4};

typedef struct {
  bool is_digital;
  String name;
  byte board;
  byte board_input;
} audio_input;

typedef struct {
  String name;
  byte board_output;
} audio_output;

audio_input inputs[8];
byte selected_input = 0;

audio_output outputs[4];
byte selected_output = 0;

int knob_pressed_state = 0;
int knob_pressed_state_prev = 0;
bool is_showing_inputs = true;

#define STATUS_TIMEOUT_SEC 3
bool is_showing_status = true;
byte in_selection_counter = 0;

#include "PinDefinitionsAndMore.h"
#undef LED_BUILTIN
#define LED_BUILTIN 13
#define IRMP_PROTOCOL_NAMES 1 // Enable protocol number mapping to protocol strings - requires some FLASH. Must before #include <irmp*>
#define IRMP_SUPPORT_NEC_PROTOCOL        1 // this enables only one protocol
#include <irmp.c.h>
IRMP_DATA irmp_data;

#define REMOTE_LEFT 0x1C
#define REMOTE_RIGHT 0x11
#define REMOTE_EJECT 0x1E

void setup() {
  Serial.begin(9600);

  init_expansion_board();

  initAnalogInputs();

  initDigitalInputs();

  initOutputs();

  initAnalogSwitch();

  setup_timer();

  setup_ir();
  
  lcd.begin(16, 2);

  display_selected_type();
  display_status();

  pinMode(PIN_KNOB_PRESS, INPUT);

  // see https://www.arduino.cc/en/Tutorial/Foundations/AnalogInputPins
  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);

  if (digitalRead(PIN_ENCODER_A) == LOW) {
    enc_prev_pos |= (1 << 0);
  }
  if (digitalRead(PIN_ENCODER_B) == LOW) {
    enc_prev_pos |= (1 << 1);
  }

  sei();
}

void loop() {
  knob_pressed_state = digitalRead(PIN_KNOB_PRESS);

  if (knob_pressed_state != knob_pressed_state_prev && knob_pressed_state == HIGH) {
    is_showing_inputs = !is_showing_inputs;
    display_selected_type();
    delay(100);
  }

  knob_pressed_state_prev = knob_pressed_state;
  
  int8_t enc_action = 0; // 1 or -1 if moved, sign is direction
 
  uint8_t enc_cur_pos = 0;
  
  // read in the encoder state first
  //
  // digitalRead() is not fast enough
  if (bit_is_clear(PORT_REGISTER, PIN_ENCODER_A)) {
    enc_cur_pos |= (1 << 0);
  }
  if (bit_is_clear(PORT_REGISTER, PIN_ENCODER_B)) {
    enc_cur_pos |= (1 << 1);
  }
 
  // if any rotation at all
  if (enc_cur_pos != enc_prev_pos) {
    if (enc_prev_pos == 0x00) {
      // this is the first edge
      if (enc_cur_pos == 0x01) {
        enc_flags |= (1 << 0);
      } else if (enc_cur_pos == 0x02) {
        enc_flags |= (1 << 1);
      }
    }
 
    if (enc_cur_pos == 0x03) {
      // this is when the encoder is in the middle of a "step"
      enc_flags |= (1 << 4);
    } else if (enc_cur_pos == 0x00) {
      // this is the final edge
      if (enc_prev_pos == 0x02) {
        enc_flags |= (1 << 2);
      } else if (enc_prev_pos == 0x01) {
        enc_flags |= (1 << 3);
      }
 
      // check the first and last edge
      // or maybe one edge is missing, if missing then require the middle state
      // this will reject bounces and false movements
      if (bit_is_set(enc_flags, 0) && (bit_is_set(enc_flags, 2) || bit_is_set(enc_flags, 4))) {
        enc_action = 1;
      } else if (bit_is_set(enc_flags, 2) && (bit_is_set(enc_flags, 0) || bit_is_set(enc_flags, 4))) {
        enc_action = 1;
      } else if (bit_is_set(enc_flags, 1) && (bit_is_set(enc_flags, 3) || bit_is_set(enc_flags, 4))) {
        enc_action = -1;
      } else if (bit_is_set(enc_flags, 3) && (bit_is_set(enc_flags, 1) || bit_is_set(enc_flags, 4))) {
        enc_action = -1;
      }
 
      enc_flags = 0; // reset for next time
    }
  }
 
  enc_prev_pos = enc_cur_pos;

  if (irmp_get_data(&irmp_data)) {
      irmp_result_print(&irmp_data);
      if (irmp_data.command == REMOTE_LEFT) {
        toggle_selection(true);
        display_selected_type();
      } else if (irmp_data.command == REMOTE_RIGHT) {
        toggle_selection(false);
        display_selected_type();
      } else if ((irmp_data.command == REMOTE_EJECT) && !(irmp_data.flags & IRMP_FLAG_REPETITION)) {
        is_showing_inputs = !is_showing_inputs;
        display_selected_type();
      }
  }
  
  if (enc_action > 0) {
    toggle_selection(true);
    display_selected_type();
  } else if (enc_action < 0) {
    toggle_selection(false);
    display_selected_type();
  }
}

void init_expansion_board() {
  mcp.begin(7);
  for (int i=0; i<16; i++) {
    mcp.pinMode(i, OUTPUT);
  }
}

void initAnalogInputs() {
  inputs[0].is_digital = false;
  inputs[0].name = "Vinyl";
  inputs[0].board = BOARD_ANALOG;
  inputs[0].board_input = 0;

  inputs[1].is_digital = false;
  inputs[1].name = "Tape";
  inputs[1].board = BOARD_ANALOG;
  inputs[1].board_input = 1;  

  inputs[2].is_digital = false;
  inputs[2].name = "Reel";
  inputs[2].board = BOARD_ANALOG;
  inputs[2].board_input = 2;

  inputs[3].is_digital = false;
  inputs[3].name = "CD Analog";
  inputs[3].board = BOARD_ANALOG;
  inputs[3].board_input = 3;  
}

void initDigitalInputs() {
  inputs[4].is_digital = true;
  inputs[4].name = "Sonos";
  inputs[4].board = BOARD_DIGITAL;
  inputs[4].board_input = 0;

  inputs[5].is_digital = true;
  inputs[5].name = "DAT";
  inputs[5].board = BOARD_DIGITAL;
  inputs[5].board_input = 1;  

  inputs[6].is_digital = true;
  inputs[6].name = "CD Digital";
  inputs[6].board = BOARD_DIGITAL;
  inputs[6].board_input = 2;

  inputs[7].is_digital = true;
  inputs[7].name = "Minidisc";
  inputs[7].board = BOARD_DIGITAL;
  inputs[7].board_input = 3;  
}

void initOutputs() {
  outputs[0].name = "Preamp";
  outputs[0].board_output = 0;

  outputs[1].name = "Tape REC";
  outputs[1].board_output = 1;

  outputs[2].name = "Reel REC";
  outputs[2].board_output = 2;

  outputs[3].name = "Minidisc REC";
  outputs[3].board_output = 3;
}

void initAnalogSwitch() {
  close_analog_input();
}

void open_analog_inputs() {
  for (int i = 0; i < ANALOG_INPUTS; i++) {
    mcp.digitalWrite(analog_inputs[i], HIGH);
  }
}

void display_selected_type() {
  lcd.clear();
  if (is_showing_inputs) {
    lcd.print(inputs[selected_input].name);
    lcd.setCursor(0, 1);
    lcd.print(inputs[selected_input].is_digital ? "  DIGITAL INPUT" : "  ANALOG INPUT");
  } else {
    lcd.print(outputs[selected_output].name);
    lcd.setCursor(0, 1);
    lcd.print("  ANALOG OUTPUT");
  }

  is_showing_status = false;
  in_selection_counter = 0;
}

void toggle_selection(bool is_left) {
  if (is_showing_inputs) {
    if (is_left) {
      selected_input = ((selected_input - 1) + TOTAL_INPUTS) % TOTAL_INPUTS;
    } else {
      selected_input = (selected_input + 1) % TOTAL_INPUTS;
    }
  } else {
    if (is_left) {
      selected_output = ((selected_output - 1) + TOTAL_OUTPUTS) % TOTAL_OUTPUTS;
    } else {
      selected_output = (selected_output + 1) % TOTAL_OUTPUTS;
    }    
  }

  toggle_analog_relays();
}

void toggle_analog_relays() {
  if (is_showing_inputs) {
    if (!inputs[selected_input].is_digital) {
      close_analog_input();
    } else {
      open_analog_inputs();
    }
  }
}

void close_analog_input() {
  open_analog_inputs();
  mcp.digitalWrite(analog_inputs[selected_input], LOW);
}

void display_status() {
  lcd.clear();
  lcd.print("IN  " + inputs[selected_input].name);
  lcd.setCursor(0, 1);
  lcd.print("OUT " + outputs[selected_output].name);
}

void setup_timer() {
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
}

// timer1 executed every second
ISR(TIMER1_COMPA_vect) {
  if (is_showing_status) {
    return;
  }

  in_selection_counter += 1;

  if (in_selection_counter > STATUS_TIMEOUT_SEC) {
    is_showing_status = true;
    display_status();
  }
}

void setup_ir() {
   irmp_init();
   irmp_irsnd_LEDFeedback(true); // Enable receive signal feedback at LED_BUILTIN
}
