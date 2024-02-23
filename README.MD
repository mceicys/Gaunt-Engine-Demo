# Gaunt Engine Demo

A bare-bones work-in-progress 3D game engine.
Video: https://www.youtube.com/watch?v=uo0teRjAab8

This repository is just a code dump to serve as a proof of work, but if you want to run it or build it, the assets, a pre-built binary, and a VS2010 solution are included in the Gaunt folder.

## Configuration

You can change settings in `Gaunt/DEFAULT/config.txt`. You can also press the grave key (\`) at runtime to open the terminal and issue commands.

CTRL-F `video` in `config.txt` to find the video mode command. Syntax is `video width height fullscreen_bool vsync_bool`, e.g. `video 1920 1080 true false`. The renderer has only been tested with a GeForce RTX 3060 Ti so far. There are issues with different video cards, especially older and integrated ones.

Default controls are:

	Aim            = Mouse
	Thrust         = W
	Brake          = Space
	Roll Left      = A
	Roll Right     = D
	Select Minigun = 1
	Select Rocket  = 2
	Fire           = Left Mouse Button

The plane is pretty hard to control, the physics still need adjusting. Type `god` into the terminal to get infinite health.

To quit, press ALT-F4. You can also type `quit` into the terminal.