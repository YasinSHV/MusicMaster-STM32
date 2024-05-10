# Music Player Mini Project

## Important Notes:
- **Do not write any code in a while loop in the main function.**
- UART reception must be initialized using interrupts.
- Drive the 7-segment display using an IC.

## Introduction:
The aim of this mini project is to design and implement a music player. The necessary hardware includes external buttons, Volume, a 7-segment display, a buzzer.

Upon starting the program, the music player is in pause mode. In this mode, the 7-segment display doesn't show any number, and no sound or music is played from the buzzer. 
- Pressing the first external button turns on the music player, the 7-segment display shows the number of the currently playing music, and music starts playing from the buzzer. 
- If the first external button is pressed again, the music player and the 7-segment display turn off, and no music will play. 
- Holding the second external button allows you to change the currently playing music by slowly rotating the volume control. 
  - Note that there should be at least 4 music tracks in your playlist. 
  - Additionally, after finishing each track, the next track in the playlist should be played, and once the playlist ends, playback should start from the beginning. 
  - Each track played should be accompanied by its corresponding number displayed on the 7-segment display.
- Furthermore, it's necessary to normalize the ADC values received with Volume and eliminate any noise. 
- Releasing the second external button confirms the selected music number, and the music starts playing. 
- Again, the 7-segment display shows the music number. 
- Holding the third external button shows the volume level of the currently playing music. 
  - The volume ranges from 0 to 100, where 0 indicates no sound (essentially mute) and 100 represents the maximum volume. 
- Releasing the third external button confirms the volume, and the 7-segment display again shows the music number.

## UART Command List:
- **Change Music Number:**
  - Command: `MUSIC_SET(Number)`
    - Replace "Number" with the desired music number. 
    - If the entered number is not in the playlist or is invalid, display a warning message.
    - If the input value is correct, display a confirmation message.
    
- **Change Music Volume:**
  - Command: `Change_Volume(Volume)`
    - Replace "Volume" with the desired volume level between 0 and 100.
    - If the input value is out of range or invalid, display an error message.
    - If the volume value is valid, adjust the buzzer volume accordingly and display a confirmation message.

- **Set Pause Time:**
  - Command: `Pause_After(seconds)`
    - Replace "seconds" with the desired pause time in seconds.
    - If "seconds" is not a valid number, display an error message.
    - If the pause time is valid, display a confirmation message.

- **Change Play Mode:**
  - Command: `Play_Mode(SHUFFLE)` or `Play_Mode(ORDERED)`
    - Replace "SHUFFLE" or "ORDERED" with the desired play mode.
    - If the mode is correct, display a confirmation message.
    - If the mode is invalid, display an error message.
