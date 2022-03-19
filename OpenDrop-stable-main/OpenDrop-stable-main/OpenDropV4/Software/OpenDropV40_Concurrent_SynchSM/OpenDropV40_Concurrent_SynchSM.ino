/*
    Basic Code to run the OpenDrop V4, Research platform for digital microfluidics
    Object codes are defined in the OpenDrop.h library
    Written by Urs Gaudenz from GaudiLabs, 2020
*/

#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <OpenDrop.h>
#include <OpenDropAudio.h>

#include "hardware_def.h"

#include <vector>
#include <string>

using namespace std;

unsigned long int findGCD(unsigned long int a, unsigned long int b)
{
    unsigned long int c;
    while(1) {
        c = a%b;
        if(c==0) { return b; }
        a = b;
        b = c;
    }
    return 0;
}


typedef struct task {
    int state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;


static task task1, task2, task3, task4, task5, /*task6,*/ Move_Magnetic_Droplet, Move_Clear_Droplet;
task *tasks[] = { &task1, &task2, &task3, &task4, &task5, /*&task6,*/ &Move_Magnetic_Droplet, &Move_Clear_Droplet};
const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

const char start = -1;
unsigned short i;
unsigned long GCD = tasks[1]->period;


OpenDrop OpenDropDevice = OpenDrop();
Drop *myDrop = OpenDropDevice.getDrop();

bool FluxCom[16][8];
int ControlBytesIn[16];
int ControlBytesOut[24];
int readbyte;

int JOY_value;
int joy_x, joy_y;
int x, y;
int del_counter = 0;
int del_counter2 = 0;

bool SWITCH_state = true;
bool SWITCH_state2 = true;
bool idle = true;

bool Magnet1_state = false;
bool Magnet2_state = false;

//local variables - Complexity 1
bool moved = false;
//shared variables - Complexity 1
int expected_x_pos;
int expected_y_pos;
int actual_x_pos;
int actual_y_pos;
bool continue_movement = true;

int j = 0;

//local variables - Complexity 2
//task: Move_Magnetic_Droplet
bool m_moved = false;
vector<string> directions = {"right", "right", "right", "right", "down", "down", "left", "left", "left"};
int m_index = 0;
bool magnet_raised = false;
int mag_x_pos;
int mag_y_pos;
int cnt = 0;
bool m_mag = false;

//task: Move_Clear_Droplet
vector<string> cl_directions = {"left", "left", "left", "left", "left", "left", "left", "left", "left", "down", "down", "right", "right"};
int cl_index = 0;
bool magnet_lowered = false;
int cl_x_pos;
int cl_y_pos;
int k = 0;
bool cl_mag = false;
bool cl_done = false;

//shared variables - Complexity 2
bool done;


// 2. serial communication
enum Comm_States { SC_Init };

int TickFct_Comm(int state) {

    switch(state) {
        case SC_Init:
            if (Serial.available() > 0) {  // receive data from App
                readbyte = Serial.read();

                if (x < FluxlPad_width)
                    for (y = 0; y < 8; y++)
                        FluxCom[x][y] = (((readbyte) >> (y)) & 0x01);
                else
                    ControlBytesIn[x - FluxlPad_width] = readbyte;
                x++;

                digitalWrite(LED_Rx_pin,HIGH);
                if (x == (FluxlPad_width + 16)) {
                    OpenDropDevice.set_Fluxels(FluxCom);
                    OpenDropDevice.drive_Fluxels();
                    OpenDropDevice.update_Display();

//                    if ((ControlBytesIn[0] & 0x2) && (Magnet1_state == false)) {
//                        Magnet1_state = true;
//                        OpenDropDevice.set_Magnet(0, HIGH);
//                    };
//
//                    if (!(ControlBytesIn[0] & 0x2) && (Magnet1_state == true)) {
//                        Magnet1_state = false;
//                        OpenDropDevice.set_Magnet(0, LOW);
//                    };
//
//                    if ((ControlBytesIn[0] & 0x1) && (Magnet2_state == false)) {
//                        Magnet2_state = true;
//                        OpenDropDevice.set_Magnet(1, HIGH);
//                    };
//
//                    if (!(ControlBytesIn[0] & 0x1) && (Magnet2_state == true)) {
//                        Magnet2_state = false;
//                        OpenDropDevice.set_Magnet(1, LOW);
//                    };

                    for (x = 0; x < 24; x++)
                        Serial.write(ControlBytesOut[x]);

                    x = 0;
                };
            }
            else
                digitalWrite(LED_Rx_pin, LOW);

            state = SC_Init;
            break;

        default:
            state = SC_Init;
            break;
    }

    switch(state) {
        case SC_Init:
            break;

        default:
            break;
    }

    return state;
}


// 3. update display
enum Display_States { UD_Init };

int TickFct_Display(int state) {

    switch(state) {
        case UD_Init:
            del_counter--;

            if (del_counter < 0) {  // update Display
                OpenDropDevice.update_Display();
                del_counter = 1000;
            }

            state = UD_Init;
            break;

        default:
            state = UD_Init;
            break;
    }

    switch(state) {
        case UD_Init:
            break;

        default:
            break;
    }

    return state;
}


// 4. activate menu
enum Menu_States { AM_Init };

int TickFct_Menu(int state) {

    switch(state) {
        case AM_Init:
            SWITCH_state = digitalRead(SW1_pin);

            if (!SWITCH_state) {  // activate Menu
                OpenDropAudio.playMe(1);
                Menu(OpenDropDevice);
                OpenDropDevice.update_Display();
                del_counter2 = 200;
            }

            state = AM_Init;
            break;

        default:
            state = AM_Init;
            break;
    }

    switch(state) {
        case AM_Init:
            break;

        default:
            break;
    }

    return state;
}


// 5. activate reservoirs
enum Reservoir_States { AR_Init };

int TickFct_Reservoir(int state) {

    switch(state) {
        case AR_Init:
            SWITCH_state2 = digitalRead(SW2_pin);

            if (!SWITCH_state2) {  // activate Reservoirs
                if ((myDrop->position_x() == 15) && (myDrop->position_y() == 3)) {
                    myDrop->begin(14, 1);
                    OpenDropDevice.dispense(1, 1200);
                }

                if ((myDrop->position_x() == 15) && (myDrop->position_y() == 4)) {
                    myDrop->begin(14, 6);
                    OpenDropDevice.dispense(2, 1200);
                }

                if ((myDrop->position_x() == 0) && (myDrop->position_y() == 3)) {
                    myDrop->begin(1, 1);
                    OpenDropDevice.dispense(3, 1200);
                }

                if ((myDrop->position_x() == 0) && (myDrop->position_y() == 4)) {
                    myDrop->begin(1, 6);
                    OpenDropDevice.dispense(4, 1200);
                }

                if ((myDrop->position_x() == 10) && (myDrop->position_y() == 2)) {
//                    if (Magnet1_state) {
//                        OpenDropDevice.set_Magnet(0, HIGH);
//                        Magnet1_state = false;
//                    }
//                    else {
//                        OpenDropDevice.set_Magnet(0, LOW);
//                        Magnet1_state = true;
//                    }
                    while (!digitalRead(SW2_pin));
                }

                if ((myDrop->position_x() == 5) && (myDrop->position_y() == 2)) {
//                    if (Magnet2_state) {
//                        OpenDropDevice.set_Magnet(1, HIGH);
//                        Magnet2_state = false;
//                    }
//                    else {
//                        OpenDropDevice.set_Magnet(1, LOW);
//                        Magnet2_state = true;
//                    }
                    while (!digitalRead(SW2_pin));
                }
            }

            state = AR_Init;
            break;

        default:
            state = AR_Init;
            break;
    }

    switch(state) {
        case AR_Init:
            break;

        default:
            break;
    }

    return state;
}


// 6. navigate using joystick
bool joystick_moved = true;
enum Joystick_States { NJ_Init };

int TickFct_Joystick(int state) {

    switch (state) {
        case NJ_Init:
            JOY_value = analogRead(JOY_pin);  // navigate using Joystick

            if ((JOY_value < 950) & (del_counter2 == 0)) {
                if (JOY_value < 256) {
                    myDrop->move_right();
                    joystick_moved = true;
                    Serial.println("right");
                }
                if ((JOY_value > 725) && (JOY_value < 895)) {
                    myDrop->move_up();
                    joystick_moved = true;
                    Serial.println("up");
                }
                if ((JOY_value > 597) && (JOY_value < 725)) {
                    myDrop->move_left();
                    joystick_moved = true;
                    Serial.println("left");
                }
                if ((JOY_value > 256) && (JOY_value < 597)) {
                    myDrop->move_down();
                    joystick_moved = true;
                    Serial.println("down");
                }

                OpenDropDevice.update_Drops();
                OpenDropDevice.update();

                if (idle) {
                    del_counter2 = 1800;
                    idle = false;
                } else
                    del_counter2 = 400;

                del_counter = 1000;
            }

            if (JOY_value > 950) {
                del_counter2 = 0;
                idle = true;
            }

            if (del_counter2 > 0)
                del_counter2--;

            state = NJ_Init;
            
            if(joystick_moved){
              //OpenDropDevice.read_Fluxels();
              //byte* fluxel_array = OpenDropDevice.get_pad_feedback_array();
              Serial.print("fluxel_array: [");
              for(int i = 0; i < 128; i++){
                //Serial.print(fluxel_array[i]); //or *(fluxel_array + i)
                byte feedback_val = OpenDropDevice.pad_feedback[i];
                Serial.print(feedback_val);
                Serial.print(", ");
              }
              Serial.print("]");
              Serial.println();
              joystick_moved = false;
            }
            
            break;

        default:
            state = NJ_Init;
            break;
    }

    switch(state) {
        case NJ_Init:
            break;

        default:
            break;
    }

    return state;
}

//Complexity 2
enum MagneticDroplet_States {MD_Start, Init, Move_m_Droplet, Raise_Magnet, MD_Stop};
int TickFct_MagneticDropletMovement(int state){
  switch(state){ //transitions
    case MD_Start:
      state = Init; Serial.println("state = Init");
      break;
    case Init:
      state = Move_m_Droplet; Serial.println("state = Move_m_Droplet");
      break;
    case Move_m_Droplet:
      if(done){
        state = MD_Stop; Serial.println("state = MD_Stop");
        break;  
      }
      if(m_mag){
        cnt = 0;
        state = Raise_Magnet; Serial.println("state = Raise_Magnet");
      }
      break;
    case Raise_Magnet:
      if(cnt <= 3){
        state = Raise_Magnet; Serial.println("state = Raise_Magnet");
      }
      if(cnt > 3){
        m_mag = false;
        state = Move_m_Droplet; Serial.println("state = Move_m_Droplet");
      }
      break;
    case MD_Stop:
      break;
  }
  switch(state){ //actions
    case MD_Start:
      break;
    case Init:
      myDrop->begin(1, 1);
      OpenDropDevice.update();
      mag_x_pos = 1;
      mag_y_pos = 1;
      m_mag = false;
      m_index = 0;
      magnet_raised = false;
      cnt = 0;
      done = false;
      break;
    case Move_m_Droplet:
      if(!magnet_raised && (mag_x_pos == 5) && (mag_y_pos == 2)){
        m_mag = true;
        break;
      }
      if(m_index < directions.size()){
        if((directions.at(m_index) == "up") && (mag_y_pos > 0)){
          mag_y_pos -= 1;
          myDrop->move_up();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
        }
        if((directions.at(m_index) == "down") && (mag_y_pos < 7)){
          mag_y_pos += 1;
          myDrop->move_down();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
        }
        if((directions.at(m_index) == "left") && (mag_x_pos > 1)){
          mag_x_pos -= 1;
          myDrop->move_left();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
        }
        if((directions.at(m_index) == "right") && (mag_x_pos < 14)){
          mag_x_pos += 1;
          myDrop->move_right();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
        }
        m_index++;
      }
      if(m_index >= directions.size()){
        done = true;
      }
      break;
    case Raise_Magnet:
      magnet_raised = true;
      cnt++;
      //OpenDropDevice.set_Magnet(1, true);
      break;
    case MD_Stop:
      break;
  }

  return state;
}

//Complexity 2
enum ClearDroplet_States {CD_Start, CD_Init, Move_cl_Droplet, Lower_Magnet, CD_Stop};
int TickFct_ClearDropletMovement(int state){
  switch(state){ //transitions
    case CD_Start:
      if(done){
        state = CD_Init; Serial.println("state = CD_Init");
      }
      break;
    case CD_Init:
      state = Move_cl_Droplet; Serial.println("state = Move_cl_Droplet");
      break;
    case Move_cl_Droplet:
      if(cl_done){
        state = CD_Stop; Serial.println("state = CD_Stop");
        break;  
      }
      if(cl_mag){
        k = 0;
        state = Lower_Magnet; Serial.println("state = Lower_Magnet");
      }
      break;
    case Lower_Magnet:
      if(k <= 3){
        state = Lower_Magnet; Serial.println("state = Lower_Magnet");
      }
      if(k > 3){
        cl_mag = false;
        state = Move_cl_Droplet; Serial.println("state = Move_cl_Droplet");
      }
      break;
    case CD_Stop:
      break;
  }
  switch(state){ //actions
    case CD_Start:
      break;
    case CD_Init:
      myDrop->begin(14, 1);
      OpenDropDevice.update();
      cl_x_pos = 14;
      cl_y_pos = 1;
      cl_mag = false;
      cl_index = 0;
      magnet_lowered = false;
      k = 0;
      cl_done = false;
      break;
    case Move_cl_Droplet:
      if(!magnet_lowered && (cl_x_pos == 5) && (cl_y_pos == 2)){
        cl_mag = true;
        break;
      }
      if(cl_index < cl_directions.size()){
        if((cl_directions.at(cl_index) == "up") && (cl_y_pos > 0)){
          cl_y_pos -= 1;
          myDrop->move_up();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
        }
        if((cl_directions.at(cl_index) == "down") && (cl_y_pos < 7)){
          cl_y_pos += 1;
          myDrop->move_down();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
        }
        if((cl_directions.at(cl_index) == "left") && (cl_x_pos > 1)){
          cl_x_pos -= 1;
          myDrop->move_left();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
        }
        if((cl_directions.at(cl_index) == "right") && (cl_x_pos < 14)){
          cl_x_pos += 1;
          myDrop->move_right();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
        }
        cl_index++;
      }
      if(cl_index >= cl_directions.size()){
        cl_done = true;
      }
      break;
    case Lower_Magnet:
      magnet_lowered = true;
      k++;
      //OpenDropDevice.set_Magnet(1, false);
      break;
    case CD_Stop:
      break;
  }

  return state;
}

//Complexity 1
enum Pre_Defined_Drop_Movement_States {M_Start, Initial, Pos1_1, Pos2_1, Pos3_1, Pos3_2, Pos3_3, Pos2_3, Pos1_3, Pos1_2};
int TickFct_DropMovement(int state){
  switch(state){ //transitions
     case M_Start:
        state = Initial; //Serial.println("state = Initial");
        break;
     case Initial:
        state = Pos1_1; //Serial.println("state = Pos1_1");
        break;
     case Pos1_1:
        if(continue_movement){
          moved = false;
          state = Pos2_1; //Serial.println("state = Pos2_1");
        }else{
          state = Pos1_1; //Serial.println("state = Pos1_1");
        }
        break;
     case Pos2_1:
        if(continue_movement){
          moved = false;
          state = Pos3_1; //Serial.println("state = Pos3_1");
        }else{
          state = Pos2_1; //Serial.println("state = Pos2_1");
        }
        break;
     case Pos3_1:
        if(continue_movement){
          moved = false;
          state = Pos3_2; //Serial.println("state = Pos1_1");
        }else{
          state = Pos3_1; //Serial.println("state = Pos3_1");
        }
        break;
     case Pos3_2:
        if(continue_movement){
          moved = false;
          state = Pos3_3; //Serial.println("state = ");
        }else{
          state = Pos3_2; //Serial.println("state = ");
        }
        break;
     case Pos3_3:
        if(continue_movement){
          moved = false;
          state = Pos2_3; //Serial.println("state = ");
        }else{
          state = Pos3_3; //Serial.println("state = ");
        }
        break;
     case Pos2_3:
        if(continue_movement){
          moved = false;
          state = Pos1_3; //Serial.println("state = ");
        }else{
          state = Pos2_3; //Serial.println("state = ");
        }
        break;
     case Pos1_3:
        if(continue_movement){
          moved = false;
          state = Pos1_2; //Serial.println("state = ");
        }else{
          state = Pos1_3; //Serial.println("state = ");
        }
        break;
     case Pos1_2:
        if(continue_movement){
          moved = false;
          state = Pos1_1; //Serial.println("state = ");
        }else{
          state = Pos1_2; //Serial.println("state = ");
        }
        break;
        
  }
  switch(state){ //actions
     case M_Start:
        break;
     case Initial:
        myDrop->begin(1, 1);
        moved = true;
        expected_x_pos = 1;
        expected_y_pos = 1;
        //OpenDropDevice.update_Drops();
        OpenDropDevice.update();
        break;
     case Pos1_1:
        if(!moved){
          myDrop->move_up();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
          OpenDropDevice.update_Display();
          moved = true;
          expected_x_pos = 1;
          expected_y_pos = 1;
        }
        break;
     case Pos2_1:
        if(!moved){
          myDrop->move_right();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
          OpenDropDevice.update_Display();
          moved = true;
          expected_x_pos = 2;
          expected_y_pos = 1;
        }
        break;
     case Pos3_1:
        if(!moved){
          myDrop->move_right();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
          OpenDropDevice.update_Display();
          moved = true;
          expected_x_pos = 3;
          expected_y_pos = 1;
        }
        break;
     case Pos3_2:
        if(!moved){
          myDrop->move_down();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
          OpenDropDevice.update_Display();
          moved = true;
          expected_x_pos = 3;
          expected_y_pos = 2;
        }
        break;
     case Pos3_3:
        if(!moved){
          myDrop->move_down();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
          OpenDropDevice.update_Display();
          moved = true;
          expected_x_pos = 3;
          expected_y_pos = 3;
        }
        break;
    case Pos2_3:
       if(!moved){
          myDrop->move_left();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
          OpenDropDevice.update_Display();
          moved = true;
          expected_x_pos = 2;
          expected_y_pos = 3;
        }
        break;
    case Pos1_3:
       if(!moved){
          myDrop->move_left();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
          OpenDropDevice.update_Display();
          moved = true;
          expected_x_pos = 1;
          expected_y_pos = 3;
        }
        break;
    case Pos1_2:
       if(!moved){
          myDrop->move_up();
          OpenDropDevice.update_Drops();
          OpenDropDevice.update();
          OpenDropDevice.update_Display();
          moved = true;
          expected_x_pos = 1;
          expected_y_pos = 2;
        }
        break;
  }
  return state;
}



// 1. setup loop
// the setup function runs once when you press reset or power the board
void setup() {
    task1.state = start;
    task1.period = 1;                   // period of 1 ms
    task1.elapsedTime = task1.period;
    task1.TickFct = &TickFct_Comm;

    task2.state = start;
    task2.period = 1;                   // period of 1 ms
    task2.elapsedTime = task2.period;
    task2.TickFct = &TickFct_Display;

    task3.state = start;
    task3.period = 1;                   // period of 1 ms
    task3.elapsedTime = task3.period;
    task3.TickFct = &TickFct_Menu;

    task4.state = start;
    task4.period = 1;                   // period of 1 ms
    task4.elapsedTime = task4.period;
    task4.TickFct = &TickFct_Reservoir;

    task5.state = start;
    task5.period = 1;                   // period of 1 ms
    task5.elapsedTime = task5.period;
    task5.TickFct = &TickFct_Joystick;
    /*
    task6.state = M_Start;
    task6.period = 10000;
    task6.elapsedTime = 0;
    task6.TickFct = &TickFct_DropMovement;
    */

    Move_Magnetic_Droplet.state = MD_Start;
    Move_Magnetic_Droplet.period = 10000;
    Move_Magnetic_Droplet.elapsedTime = Move_Magnetic_Droplet.period;
    Move_Magnetic_Droplet.TickFct = &TickFct_MagneticDropletMovement;

    Move_Clear_Droplet.state = CD_Start;
    Move_Clear_Droplet.period = 10000;
    Move_Clear_Droplet.elapsedTime = Move_Clear_Droplet.period;
    Move_Clear_Droplet.TickFct = &TickFct_ClearDropletMovement;
    
    
    for(i = 1; i < numTasks; i++) {
        GCD = findGCD(GCD, tasks[i]->period);
    }


    Serial.begin(115200);
    //OpenDropDevice.begin("c02");
    OpenDropDevice.begin();

    // ControlBytesOut[23] = OpenDropDevice.get_ID();

    OpenDropDevice.set_voltage(240, false, 1000);

    OpenDropDevice.set_Fluxels(FluxCom);

    pinMode(JOY_pin, INPUT);

    OpenDropAudio.begin(16000);
    OpenDropAudio.playMe(2);
    delay(2000);

    OpenDropDevice.drive_Fluxels();
    OpenDropDevice.update_Display();
    Serial.println("Welcome to OpenDrop");

    myDrop->begin(7, 4);
    OpenDropDevice.update();
    //OpenDropDevice.set_Magnet(1, true);
    if(feedback_flag){
      Serial.println("feedback_flag = true");  
    }else{
      Serial.println("feedback_flag = false"); 
    }
}


void loop() {
    for(i = 0; i < numTasks; i++) {
        if(tasks[i]->elapsedTime == tasks[i]->period) {
            tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
            tasks[i]->elapsedTime = 0;
        }
        tasks[i]->elapsedTime += 1;
    }
}
