## vex

A terminal-based hex editor with vi-like keybinds.

I wrote this because I was dissatisfied with all the available terminal-based hex editors I could find, namely because they all lacked in one (or more) of these:
- Speed:  Read/write multi-gigabyte files without chugging (and with color)
- Navigation:  Move around a file quickly and painlessly (with both mouse and keyboard)
- Interface:  Work with me (not against me) to edit files

### Screenshot and Demo

![Screenshot](https://github.com/Cubified/vex/blob/main/demo.png)

![Demo](https://github.com/Cubified/vex/blob/main/demo.gif)

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

As well as some other useful ones, such as:

- Mouse:  Click and scroll to navigate around file
- Page Up/Page Down:  Navigate vertically through file
- Home and End keys:  Navigate horizontally along the current line

### To-Do

- More robust end-of-file checks on user input
- Wrapping when in insert mode
