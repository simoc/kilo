# kilo

Implementation of a simple text editor in C, following the steps in the
[Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/) tutorial.

Completed all steps from 1 to 184.

Also ported to Microsoft Windows, adding `#ifdef _WIN32`
to replace UNIX-specific system
calls with Windows API `GetConsoleMode`,
`SetConsoleMode`,
`GetConsoleScreenBufferInfo`,
`ReadConsole` and `WriteConsole` calls.

To compile on Linux use:

    make -f Makefile

To compile on Microsoft Windows use:

    nmake /f Makefile.win32
