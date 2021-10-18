/*
Description:
This code is to setup an arduino to record a button press sequence, and transmit it to a arduino that has the octobanger firmware already loaded onto it for field programming.
The hope is to make it as easy to program an arduino in the field as it is for a PicoBoo.

Operation:
-User will connect the FieldProgrammer to the octobanger
-Led on FieldProgrammer will blink till it is initialized
-User will press record button, which will start recording button sequence
-User will press buttons in the sequence they want the relays to trigger
-While pressing the buttons during the program sequence the relays on the Octobanger will trigger in response.
-Once complete the user will press the record button again to end the sequence, and the Field programmer will start sending the new sequence to the octobanger.


Contributors:
James Manley - james.manley@scareforacure.org


Pin Mapping
Arduino Nano
[0/1] Tx/Rx - Used to transmit seriall commands to OctoBanger Baud:115200
[2] Btn_1
[3] Btn_2
[4] Btn_3
[5] Btn_4
[6] Btn_5
[7] Btn_6
[8] Btn_7
[9] Btn_8
[11]Record Button

RJ45 connector - T586B 10/100 DC mode B
| OctoBanger
| [RX]  [TX]  [12VDC] [TX]  [GND]
  1 2   3     4 5     6     7 8
| [TX]  [RX]  [VIN]   [RX]  [GND]
| OctoField
// under normal cirucmstance the pairs on TX and RX are supposed to be opposite polarities for noise canceling, for our purposes we just wire the siganl to both for simplicity and signal reliance.


*/

//Libraries
#include <RBD_Timer.h>
#include <RBD_Button.h>


////////////////////////////////////////////////////////////////
/// V Define System Below V ///


///^ Define system above ^///
//////////////////////////////////////////////////////////
/// V leave code Below alone V ///

//inputs
RBD::Button Record(11);
RBD::Button Ch_1(2);
RBD::Button Ch_2(3);
RBD::Button Ch_3(4);
RBD::Button Ch_4(5);
RBD::Button Ch_5(6);
RBD::Button Ch_6(7);
RBD::Button Ch_7(8);
RBD::Button Ch_8(9);



//outputs

//global variables
int recording = 0; // variable for tracking if recording is currently in progress 1 = yes
int Sequence[1023][2]; // create empty 2d array with 2 columns, and more rows than could be needed to sequence [1]relay status int,[2]# of time steps int.
int RelayStat = 0 ; // number representing on/off status of each relay, ch_1 = 1, ch_2 =2, ch_3 =4, ch_4=8 etc.
int RelayStatLast = 0 ; // previous value for tracking changes
int Frame = 0 ; // frame number of sequence represents row in array
int FrameCount = 0;

//timers
RBD::Button FrameTime(50); //sets the time for each from to be at 50 ms

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////
///V- Sub Functions -V ///
//////////////////////////

void TransmitSeq(int a[][]){
//Programming hex commands:
//baud 115200
//40 56 = clear memory
//0a 00 00 00 = reset memory position
//40 53 = begin to receive new program
//XX 00 = 2x # of frames to be received (second bit is for if more than 128 frames is to be transmitted)
//___________ //V- this section repeats for each frame to be transmitted
//Xx = relay combo for each frame
//xx = # of 1/20 seconds frame is active
//------------------- //V- this section is required even if the state didn't change at the end
//00 = Relay combo for final frame // if programming on octobanger this could have a value, but for here its easier to just force to of
//00 = zero time duration for end of program
  int frametrack = 0;
  int linecount = 0;
  Serial.write(0x40, 0x56);
  Serial.write(0x0a, 0x00, 0x00, 0x00);
  Serial.write(0x40, 0x53);
  for(int y = 1023; y >=0 ; y--){// go through the array and find the last updated frame without 0 frame count, thats where our program stopped
    if(a[y][2] != 0){
      //this is the last part of our sequence
      frametrack = y;
      break; // weve already found the end we can stop looking now.
    }
  }
  linecount = ( frametrack + 2 )*2; // we need the total count of frames not index, + the closing frame, multiplied by 2 to signify that we will be sending 2 lines for each frame
  Serial.write(linecount); //transmit the bottom 8 bits then the top 8 bits
  
  for(int z = 0 ; z <= frametrack; z++){ // go through each frame and transmit both lines
    Serial.write(a[z][1]);
    Serial.write(a[z][2];
  }

  Serial.write(0x00); // go ahead and send the ending frame.
  Serial.write(0x00);


  
}

void TransmitStream(int b){
//Streaming hex Commands:
//baud 115200
//40 4d xx = instantly tell octobanger to activate corresponding relays
//  xx = hex conversion of relay states  
//  ch1 = least bit, ch8 = greatest bit
  Serial.write(0x40, 0x4d, b);



}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////
///V-Main Functions-V///
//////////////////////// 

void setup(){
  Serial.begin(115200); // override baud rate for uno, should also work for nano
  
  
}

void loop(){

  if(!recording && recording.onPressed()){
    recording = HIGH;
    FrameTime.restart();
    Frame = 0; // each time you start recoridng the frame number goes back to 0.
    for(int x=0; X<=1023; x++){ // make sure to clear the full array before starting programming.
      Sequence[x][1]=0;
      Sequence[x][2]=0; 
    }
  }

  if(recording && recording.onPressed(){
    recording = LOW;
    FrameTime.stop();
    TransmitSeq(Sequence); // starts transmission of the collected array.
  }
  
///^ INPUT BASED EVENTS GO ABOVE ^///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///V TIMING BASED EVENTS GO BELOW V///

  if(FrameTime.onRestart()){ // should repeat every 50 ms, re-reading all the buttons and recording them.
    RelayStat = 0;
    if(Ch_1.isPressed()){
      RelayStat += 1;
    }
    if(Ch_2.isPressed()){
      RelayStat += 2;
    }
    if(Ch_3.isPressed()){
      RelayStat += 4;
    }
    if(Ch_4.isPressed()){
      RelayStat += 8;
    }
    if(Ch_5.isPressed()){
      RelayStat += 16;
    }
    if(Ch_6.isPressed()){
      RelayStat += 32;
    }
    if(Ch_7.isPressed()){
      RelayStat += 64;
    }
    if(Ch_8.isPressed()){
      RelayStat += 128;
    }
    if(RelayStatlast == RelayStat){ // if the combination of relays is the same as last round, increase the counted frames in the array
      ++FrameCount;
      Sequence[Frame][2] = FrameCount; //
    }
    else{ // if the relay stat is not the same as the last round, move to a new frame in the array
      ++Frame;
      FrameCount=0;
      Sequence[Frame][1] = RelayStat; // store the relay combo in column 1 of the array
      Sequence[Frame][2] = FrameCount; // when creating new frame the frames count is 0.
    }
    RelayStatLast = RelayStat;
    SequenceStream(RelayStat); // send the current relay combo to the arduino to have the relay's mimic the buttons in real time.
  }
  
  
}
