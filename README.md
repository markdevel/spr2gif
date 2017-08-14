spr2gif
=======

## Description

Convert a \*.spr and an \*.act into an animated gif image. Typically these files are used in Ragnarok Online has developed by GRAVITY CO.,Ltd.

## Requirement

- Windows 10 or later
- [Microsoft Visual C++ Redistributable for Visual Studio 2017](https://go.microsoft.com/fwlink/?LinkId=746572)

## Dependency

- [libgd](https://github.com/libgd/libgd) >= 2.2 (pre-compiled binary for Win32 platform is contained)

## Build

Open "spr2gif.sln" file on Visual Studio 2017 and build all.

## Usage
~~~
Usage: spr2gif [options] [actfile] sprfile
Options:
  -w --width=WIDTH    Specify the width of the output. (1-1024, default:32)
  -h --height=HEIGHT  Specify the height of the output. (1-1024, default:32)
  -s --scaling        Enable a scaling.
  -l --loop           Enable a loop.
  -n --index=N        Only output at N in the ACT file. (-1, 0-1023, default:-1)
  -p --prefix=PREFIX  Specify the prefix of GIF files.
~~~

## Licence
[GPLv3](https://www.gnu.org/licenses/gpl-3.0.txt)

## Author
markdevel (markdevel [AT] outlook [.] com)
