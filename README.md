# 4096-bytes-intro "Sprites are us"

This is a little (only 4096 bytes) intro I wrote back in 2004. It was an attempt to showcase that it is quite possible to generate a tiny executable that actually does something (3d graphics and audio) using a high level language such as C/CPP.

Here is a link to a YouTube video of the intro. The video capture shows heavy compression artifacts and only runs at 30 FPS, the intro itself easily hits 60 FPS on decent 2004 hardware.

https://user-images.githubusercontent.com/29707648/221999745-6d4a007e-bffc-45be-bc10-1077ad7c9ae1.mp4

"Sprites Are Us" features 100.000 3D sprites each colored using a pseudo random number drawn using DirectX 9 and includes a public domain MIDI soundtrack (details in the file mucke.h). The sprites are animated in differend animation patterns and morph into an starfield at the end. 

The original intro contained a different MIDI track but due to copyright restrictions I had to replace it with this public domain track. 
Unfortunatly it is hard to find an royalty free midi track... (Yes there are lots of sites that offer 'free' midi files but there are still subject to copyright and therefore only free to use for personal usage). In order to get an royalty free midi track to need to fall back to public domain tracks and these are composed prior to the 20th century.

# Trivia
To reach the goal of 4096 bytes the PE executable is packed into an old faschioned **.com** executable because the header is much smaller (the smallest PE header has 512 bytes w/o payload).

The pseudo random numbers are generated using the realtime stamp counter (TSC) of the CPU.

Bear in mind that a small size of the executable was the prime goal of this project. To achieve this all error checks and memory freeing operations have been diliberatly removed. So do NOT take this as a example of CleanCoding, it is **deliberatly** NOT!

You also need a 32 Bit Windows system such as Windows XP (the orginal target plattform at that time) because **.com** files are no longer supported in 64 Bit operating systems. 

It appears to run fine in an XP VM with 3D acceleration enabled. **Update** looks like VirtualBox 6.x broke 3D acceleration in XP guests to fix a security hole.

# Building
The project was build using Visual Studio 6 and the DirectX9-SDK on an WindowsXP system. 
In order to build the executable you need to set up two things. 
* Add the DX9 include and lib paths to VS6 
* Add the postbuild.bat as an postbuild step.

Should build fine then. A side note: The vertex processing is set up for hardware vertex processing so this wont run on GPUs that dont support that (Intel GMA-3150 for example).

The post build step will convert and compress the **.exe** into the final **.com** executable.

# Release
An precompiled version of the intro can be found in the release section. 

Have fun with it,
Lutz
