# ds-playthrough-map

https://www.youtube.com/watch?v=3Vt8WuTZHOk

![alt text](https://raw.githubusercontent.com/Kvel2D/ds-playthrough-map/master/screenshot.png)

## Dependencies:

Need GLFW and OpenGL to compile ds-replay.
To build this I used make with MinGW but you can use any other build system.

## How it works:

To record, run the game and log into a character. Set character name and health in settings.txt to match the current ones in-game and run the recorder. The output positions are appended to "positions.txt" file. The format is "[milliseconds since last position],[x],[y],[z]". Coordinates are floating points but written as hex bytes.

Recorder still has bugs! It sometimes fails to continue recording after death and will beep to warn you of that.
And I also had two hard crashes during my playthrough when bonfire warping. Not sure what is causing that, might even be caused by some hardware problem rather then the program.

To replay, copy "positions.txt" into replayer folder and run it. The controls are WASD QE for movement and arrows/mouse(if enabled) for camera. SPACE/ENTER for play/pause playback. settings.txt has fullscreen/windowed, resolution and other settings.
