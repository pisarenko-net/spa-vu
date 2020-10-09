// original code is from https://learn.adafruit.com/trinket-usb-volume-knob/code

#define PIN_ENCODER_A 3
#define PIN_ENCODER_B 2
// see https://www.arduino.cc/en/Reference/PortManipulation
#define PORT_REGISTER PIND

static uint8_t enc_prev_pos = 0;
static uint8_t enc_flags    = 0;

void setup() {
  // see https://www.arduino.cc/en/Tutorial/Foundations/AnalogInputPins
  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);

  if (digitalRead(PIN_ENCODER_A) == LOW) {
    enc_prev_pos |= (1 << 0);
  }
  if (digitalRead(PIN_ENCODER_B) == LOW) {
    enc_prev_pos |= (1 << 1);
  }

  Serial.begin(9600);
}

void loop() {
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
  
  if (enc_action > 0) {
    Serial.println("LEFT");
  } else if (enc_action < 0) {
    Serial.println("RIGHT");
  }
}
