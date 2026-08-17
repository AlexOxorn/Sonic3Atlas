# Sonic 3 & Knuckles Atlas Encoder

--------------
## How to use:

This Project consists of two parts:
- `logGameData.lua` which runs with BizHawk2.11+ which will capture
    all the needed data from an actively running Sonic3K game,
    and writes the data to a binary file.
- The Encoder then will read that binary file, and use it to recreate the zoomed out visuals.

### Logging:
After starting Sonic 3 & Knuckles in Bizhawk, run the `logGameData.lua` script the lua
window found under `Tools -> Lua Console`

You will then be prompted with a dialogue box with various options.
First it will ask for the output type:
1. `file` Will write the logged data to a file
2. `socket` Will open a socket at localhost:5000, then send the logged data through the socket

If you select file, you need to select where the file should be written to,
and the name the file should have.

Once all that is set up, press start.
If you have a movie loaded, it will automatically start the movie from the beginning,
while also automatically stop once the movie has "finished".

The program will continuously write the data to the destination of choice.
Some larger data will only be written to when there is a change, including
- Chunk Data
- Level Data
- Block Data

Tile Data will log writes to the VDP status/data registers, and will only send
the tiles which have been updated.
If a level is loading, it will send the entirety of VRAM every frame to avoid
missing data.

Once you want to stop writing, simply press stop.

### Encoding

On starting up the encoder, you will have a menu with a bunch of options.

For the Input Type:
Is the encoder reading from a binary file,
or receiving data from a socket?

For Output, the program will render the data to a window regardless,
but you can optionally also directly render the encoding to a video using
`ffmpeg`.

Simply select the output directory, the output filename, the video encoder,
any additional ffmpeg options you want to pass. Also you can optionally pass
in a audio file to be mixed with the video. Useful if you already have a `.wav`
render from BizHawk.

For Rendering Options
- Internal Resolution: Effectively a Zoom option. If the resolution is higher, 
    more of the game will be rendered
  - The `Full Height` option will dynamically change the internal resolution,
    such that the vertical resolution will fit the entire height of the screen,
    while also fixing the vertical scroll to 0, showing the entire vertical span of a level
- Output Resolution: This is both the resolution of the window showing the render, but also
    the resolution of the outputed video (assuming you are rendering to a video file)
- Horizontal/Vertical Out of Bounds: What to do when scrolling the screen past the
    bounds of a level? 
  - `None` will render nothing (just showing the BG colour).
  - `Prevent Scrolling` will force the scrolling to not exceed the respective bounds
  - `Simulate Loopback/Sewer` (WIP) will try to mimic how the game would actually render out of bounds (in greyscale)

Once you have everything setup, press start, and it will start rendering.
During rendering, there are certain keyboard controls available to you.

- `Esc`: Stop rendering and return to menu.

The rest are only available if NOT outputting to a video file.
- `P`: Speedup rendering by 1.5x
- `O`: Slowdown rendering by 0.67x
- `I`: Reset speed to default
- `Space`: Pause rendering and advance one frame at a time
- `Pause`: Pause or Unpause rendering
- `Z`: Skip to next zone
- `X`: Skip to next BG event
- `C`: Skip to next FG event
- `R`: Restart from beginning (File only)

--------------
## Installation

While not inherently a Linux exclusive program, as this so far is a personal project,
The code as is, expects my Linux environment. If you have a Windows OS, feel free to
add support for building on Windows, and make a PR.

The installation uses CMAKE along with llvm-23 (clang and libc++).
First make sure to initialize the submodules under vendor. This folder consists of
library sources this project uses (SLD3 + imgui).

Then do what you would usually do to use CMAKE, and that would be it.

-------------------
## Contributing

I am open to contributions given the following rules for each type of contribution.

### Universal Rule
1. Make sure formatting follows the `.clang-format` rules

### Windows Support
This I am the least picky about as this would not affect me, nor would I even be able to test it.
Just make sure whatever you do, it makes it as painless as possible for other Windows users,
and it does not interfer with my using the program as a Linux User.

### Adding Options
A lot of choice on how the game should be rendered are hard coded to my personal preferences.
If you wish to change the rendering, please do so by:
1. making it an option inside the Option struct `common.h`
2. Making sure the option is serialized/deserializes correctly
3. Make sure the default value in the Option struct is what it hardcoded to as previously
4. Add the Option to the main menu `main_menu.cpp`
5. The behaviour of the render given the default value of the new option should be the
    same as before

### Bug Fixes
1. Explain the nature of the bug and its fix as specifically as possible.
2. If relevant, site the SK disassembly for backing up your argument.
3. If feasible, attach a log file/bizhawk movie file that demonstrates the bug being fixed

### New Feature
1. Ask permission before starting, as the feature you might want to add is one I
    already have a vision for.

