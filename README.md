# 8 Ball Bulldozer

8 Ball Bulldozer is Jared Moore's implementation of [Pool Dozer](http://graphics.cs.cmu.edu/courses/15-466-f17/game3-designs/jmccann/) for game3 in 15-466-f17.

![screenshot](https://github.com/moorejs/15-466-f17-game3/blob/master/screenshots/main.png?raw=true)

## Asset Pipeline

The asset pipeleine is the same as in [game2](https://github.com/moorejs/15-466-f17-base2/blob/master/README.md).

## Architecture

The architecture was similar to the last project's. However, I did not use a map this time to store the scene objects. Instead, I was able to use a few vectors for each object type (balls, goals, and players).

The two players were handled each individually (which led to a fair amount of code duplication..). Every ball checks for collisions with the players, all other balls, and the goals each frame. This was not the most efficient way to handle this, but it works for this amount of objects. The objects were all assumed to be spheres so overlaps were determined by radius.

The camera position was the world coordinate between the two players and the camera's radius was the length of the line between them (with a minimum). It provided a bird's eye view. This all allowed the players to always be in view.

## Reflection

Loading the scene was not very hard as it was a continuation of the last game's scene loading. The most difficult part was building the physics system. I had to learn about two dimensional ellastic collisions between two moving objects. It's hard to tell if the resulting velocities I got out of my attempts at mimicing the equations actually worked correctly every time. Also, I only wanted to handle collision for one frame, even though the balls may be together for multiple frames (cannot always resolve in one frame), so I tried to check whether the objects were moving toward each other first. Additionally, faking the rotation of the balls didn't seem to come out right, so that was a challenge.

I would have liked to polished pretty much all the major components of the games. The controls did not seem quite right or fun enough, and sometimes the physics was not fun enough either. The player should be able to really send the balls but for not they kind of just bounce off. I am not sure how I would polish the controls more (it is hard) but they definitely could be better.

Another interesting thing I could have done was dividing the map into 6 areas, one for each goal, and only checking collisions for balls in each area. This would take a bit of time so I could not implement it for this game.

# About Base2

This game is based on Base2, starter code for game2 and game3 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

## Requirements

 - modern C++ compiler
 - glm
 - libSDL2
 - libpng
 - blender (for mesh export script)

On Linux or OSX these requirements should be available from your package manager without too much hassle.

## Building

This code has been set up to be built with [FT jam](https://www.freetype.org/jam/).

### Getting Jam

For more information on Jam, see the [Jam Documentation](https://www.perforce.com/documentation/jam-documentation) page at Perforce, which includes both reference documentation and a getting started guide.

On unixish OSs, Jam is available from your package manager:
```
	brew install ftjam #on OSX
	apt get ftjam #on Debian-ish Linux
```

On Windows, you can get a binary [from sourceforge](https://sourceforge.net/projects/freetype/files/ftjam/2.5.2/ftjam-2.5.2-win32.zip/download),
and put it somewhere in your `%PATH%`.
(Possibly: also set the `JAM_TOOLSET` variable to `VISUALC`.)

### Bulding
Open a terminal (on windows, a Visual Studio Command Prompt), change to this directory, and type:
```
	jam
```

### Building (local libs)

Depending on your OSX, clone 
[kit-libs-linux](https://github.com/ixchow/kit-libs-linux),
[kit-libs-osx](https://github.com/ixchow/kit-libs-osx),
or [kit-libs-win](https://github.com/ixchow/kit-libs-win)
as a subdirectory of the current directory.

The Jamfile sets up library and header search paths such that local libraries will be preferred over system libraries.
