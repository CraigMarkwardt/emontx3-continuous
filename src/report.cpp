//   EMONTX3-CONTINUOUS - continuous sampling Arduino firmware
// 
//   Copyright (C) 2018 C. B. Markwardt
//   License: GNU GPL V3
//
//   Reporting of data
//

#include <Arduino.h>
#include <Math.h>
#include "cont.h"
#include "cal.h"

// Macros to find if the report ring buffer is full or empty
#define WRAP(n) ((n)%N_REPORT)
#define FULL (WRAP(report_write_index+1) == report_read_index)
#define EMPTY (report_write_index == report_read_index)

// Ring buffer for reports
struct report_struct report_buffer[N_REPORT];
uint8_t report_read_index = 0;
uint8_t report_write_index = 0;

// ============================= PUSH REPORTS INTO RING BUFFER
// push_report_break() - push a "line break" which indicates we are 
//   reporting a new kind of data
void push_report_break()
{
  struct report_struct *r = &(report_buffer[report_write_index]);
  if (FULL) return;
  // If a break is already in place then don't do another one
  if (report_write_index != report_read_index &&
      report_buffer[WRAP(report_write_index-1)].type == BREAK_TYPE) return;
  r->name[0] = 0;
  r->type = BREAK_TYPE;
  report_write_index = WRAP(report_write_index+1);
}

// push_report_float() - push a floating point variable
//   name - 4-character name of variable
//   value - floating point value of variable
//   digits - number of floating point digits to report after decimal
//   retained - is this an MQTT retained variable?  (1=yes; 0=no)
void push_report_float(const char name[6], float value, uint8_t digits, uint8_t retained)
{
  struct report_struct *r = &(report_buffer[report_write_index]);
  if (FULL) return;
  strncpy(r->name,name,5);
  r->name[6] = 0;
  r->type = FLOAT_TYPE;
  r->value.floatval = value;
  r->digits = digits | (retained ? 0x80 : 0x00);
  report_write_index = WRAP(report_write_index+1);
}


// push_report_int32() - push an integer variable
//   name - 4-character name of variable
//   value - integer value of variable
//   retained - is this an MQTT retained variable?  (1=yes; 0=no)
void push_report_int32(const char name[6], int32_t value, uint8_t retained)
{
  struct report_struct *r = &(report_buffer[report_write_index]);
  if (FULL) return;
  strncpy(r->name,name,5);
  r->name[6] = 0;
  r->type = INT32_TYPE;
  r->value.int32val = value;
  r->digits = retained ? 0x80 : 0x00;
  report_write_index = WRAP(report_write_index+1);
}

// push_report_uint32() - push an unsigned integer variable
//   name - 4-character name of variable
//   value - unsigned integer value of variable
//   retained - is this an MQTT retained variable?  (1=yes; 0=no)
void push_report_uint32(const char name[6], uint32_t value, uint8_t retained)
{
  struct report_struct *r = &(report_buffer[report_write_index]);
  if (FULL) return;
  strncpy(r->name,name,5);
  r->name[6] = 0;
  r->type = UINT32_TYPE;
  r->value.uint32val = value;
  r->digits = retained ? 0x80 : 0x00;
  report_write_index = WRAP(report_write_index+1);
}

// ============================= SEND REPORTS FROM RING BUFFER

// send_report() - send a single report from the ring buffer
//   data is sent over the Serial() line.
void send_report()
{
  uint8_t retained, digits;
  struct report_struct *r = &(report_buffer[report_read_index]);
  if (EMPTY) return;
  
  digits   = r->digits;
  retained = r->digits & 0x80;
  digits   &= 0x7f;

  // Output depends on the data type
  switch(r->type) {
    case VOID_TYPE: break;  // VOID_TYPE: do nothing
    case BREAK_TYPE: Serial.println(""); break; // BREAK_TYPE: line break

    // Numerical types
    default:
      if (retained) Serial.print("_");
      Serial.print(r->name);
      Serial.print(":");
      switch(r->type) {
        case FLOAT_TYPE: 
          if (digits > 0) Serial.print(r->value.floatval,digits);
          else            Serial.print(r->value.floatval); 
          break;
        case INT32_TYPE:  Serial.print(r->value.int32val); break;
        case UINT32_TYPE: Serial.print(r->value.uint32val); break;
      }
      Serial.print(",");
      break;
  }

  // Reset this ring buffer entry
  r->name[0] = 0; r->type = VOID_TYPE;
  report_read_index = WRAP(report_read_index+1);
}

