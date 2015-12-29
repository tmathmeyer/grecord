#grecord
A utility for recording subsections of a screen in X11.

##usage
after running the grecord binary, click on one corner of the rectangle
to be recorded. Move the mouse to the opposite corner (note the purple
box drawn around the rectangle) and click again. The second click will
start the recording with ffmpeg using x11 capture. At any time, press
'q' to stop the recording.

##building
run 'make' in this repository. X11 and pthread libraries are required.
