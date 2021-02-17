## vex

A terminal-based hex editor with vi-like keybinds.

I wrote this because I was dissatisfied with all the available terminal-based hex editors I could find, namely because they all lacked in one (or more) of these:
- Speed:  Read/write multi-gigabyte files without chugging (and with color)
- Navigation:  Move around a file quickly and painlessly
- Interface:  Work with me (not against me) to edit files

In its current state, vex is not yet fully functional.  However, it is capable of reading and writing to the beginning of files (because scrolling support has not yet been implemented).

### Screenshot

![Demo](https://github.com/Cubified/vex/blob/main/demo.png)

### Compiling

`vex` has no dependencies.

     $ make

Will compile, and

     $ ./vex [file]

Will open `[file]` for editing.

### Usage

`vex` supports the basics of any vi-like editor, namely:

- `i`:  Enter insert mode
- `hjkl` or arrow keys:  Navigate through file
- `:`:  Enter commands
     - `:w`:  Write file
     - `:q`:  Quit editor
     - `:wq`: Write file, then quit
     - `:q!`: Quit editor without saving
     - `:89ab`:  Go to offset `0x89ab` in the file
- Escape:  Exit insert mode/command entry, return to normal mode

### To-Do

- Fix scrolling/dynamic reading of file
