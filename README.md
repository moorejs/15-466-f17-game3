# Pop 'em

Pop 'em is Jared Moore's implementation of [*Robot Fun Police*](http://graphics.cs.cmu.edu/courses/15-466-f17/game2-designs/jmccann/) for game2 in 15-466-f17.

![alt text](https://github.com/moorejs/15-466-f17-base2/blob/master/screenshots/main.png?raw=true)

## Asset Pipeline

The whole asset pipeline is about gathering data from the blender scene (.blend file). For each mesh in the scene, we need the mesh data. The script provided with us collected some of this, but I had to get color data as well. This is all output into one binary file. The scene data is captured as well, which was provided for us. But I added more data to keep track of the hierarchy (track what are the parents). The meshes, when loaded, map their name in blender to the mesh data. The scene, when loaded, adds a bunch of objects with their respective transforms/meshes to a list.

## Architecture

My code has a mapping to all the scene objects by their name. This is good enough because there is only one of each name and there are only a few. Additionally, we need to manipulate the Links and Base parts of the robot arm individually. The balloons are also scene objects, but are kept in a vector because this is simple amd I will never need to specifically refer to one.

## Reflection

This was my first real exposure to 3D game programming. I learned a lot and it gives me a lot of confidence for my future. It was difficult at first to understand all of the 3D math involved, but the base code was very helpful. Furthermore, glm is a great and convenient library. Adding the BRDF code was something I should have added too, because implementing cool shaders one of the essentials in my mind.

The design document was pretty clear and simple. I had to come up with how/where balloons should spawn, but that was simple. I would add continually spawning balloons if I were to take this further and make them bob and such.


# About Base2

This game is based on Base2, starter code for game2 in the 15-466-f17 course. It was developed by Jim McCann, and is released into the public domain.

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
