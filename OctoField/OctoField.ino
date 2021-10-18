/*
Description:
This code is to setup an arduino to record a button press sequence, and transmit it to a arduino that has the octobanger firmware already loaded onto it for field programming.
The hope is to make it as easy to program an arduino in the field as it is for a PicoBoo.

Operation:
-User will connect the FieldProgrammer to the octobanger
-Led on FieldProgrammer will blink till it it initialized
-User will press record button, which will start recording button sequence
-User will press buttons in the sequence the want tthe relays to trigger
-While pressing the buttons during the program sequence the relays on the Octobanger will trigger in response.
-Once complete the user will press the record button again to end the sequence, and the Field programmer will start sending the new sequence to the octobanger.


Contributors:
James Manley - james.manley@scareforacure.org



Pin Mapping
Arduino Nano
[0/1] Tx/Rx - Used to transmit seriall commands to OctoBanger
[2] Btn_1
[3] Btn_2
[4] Btn_3
[5] Btn_4
[6] Btn_5
[7] Btn_6
[8] Btn_7
[9] Btn_8
[11]Record Button



Hex Commands:
Streaming Commands:
40 4d xx = instantly tell octobanger to activate corresponding relays
  xx = hex conversion of binary of active channels. 
  ch1 = least bit, ch8 = greatest bit


Programming commands:
40 56 = clear memory
0a 00 00 00 = reset memory position
40 53 = begin to receive new program
XX 00 = 2x # of frames to be received (second bit is for if more than 128 frames is to be transmitted)
___________
Xx = relay combo for each frame
xx = # of 1/20 seconds frame is active
------------------- 
00 = off frame for end of program
00 = zero time duration for end of program


RJ45 connector - T586B 10/100 DC mode B
| OctoBanger
| [RX]  [TX]  [12VDC] [TX]  [GND]
  1 2   3     4 5     6     7 8
| [TX]  [RX]  [VIN]   [RX]  [GND]
| OctoField

*/



//Libraries


////////////////////////////////////////////////////////////////
/// V Define System Below V ///


///^ Define system above ^///
//////////////////////////////////////////////////////////
/// V leave code Below alone V ///

//inputs

//outputs

//global variables

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////
///V- Sub Functions -V ///
//////////////////////////





/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////
///V-Main Functions-V///
//////////////////////// 

void setup(){

  
}

void loop(){

  
///^ INPUT BASED EVENTS GO ABOVE ^///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///V TIMING BASED EVENTS GO BELOW V///
  
  
  
}
