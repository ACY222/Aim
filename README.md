# Aim Editor

Aim is a lightweight, Vim-like terminal text editor built from scratch using
C++. It implements the core philosophy of modal editing, supporting a rich set
of Vim motions, operators and search capabilities.

## Features

- **Modal Editing**: Supports Normal, Insert, CommandLine and Search modes.
- **Vim-like Keybindings**:
    - Basic cursor movements: `h`, `j`, `k`, `l`
    - Word-based motions: `w`, `W`, `b`, `B`, `e`, `E`
    - Line navigation: `0`, `$`, `gg`, `G`
    - Inline character search: `f`, `F`, `t`, `T`
- Operators & Multipliers:
    - Supports operations with numerical prefixes (counts), such as `2dd`,
    `d3w`, `c2l`
    - Standard editing operators: `d`(delete), `c`(change), `y`(yank), `p`/`P`(paste),
    `x`(delete char), `J`(join lines)
- File Operations & Search:
    - Command line mode for saving and quitting: `:w`, `:q`, `:wq`
    - Forward and backward pattern search using `/` and `?`, with `n`/`N` for
      next/previous matches
- Terminal UI:
    - Relative line numbers
    - Context-aware cursor styles (e.g., block in Normal mode, vertical bar in
      Insert mode)

## Project Structure

The codebase is organized to seperate concerns:
- `include/`: Header files defining the core components
    - `config.hpp`: Global constants, key definitions, and mode enums
    - `editor.hpp`: The central state machine, handling mode transitions and
      keypress dispatching
    - `buffer.hpp`: Text buffer management, file I/O, and string manipulation
    - `terminal.hpp`: Low-level terminal control, raw mode toggling, and VT100
      escape sequence parsing
    - `view.hpp`: The rendering engine, responsible for drawing the text UI,
      status bar, and message bar
- `src/`: Implementation files for the respective headers

## Future Work

The following features are planned in the future:
+ Fully supports for Vim's mode: I may implement Visual mode, support more
  commands in CommandLine mode
+ UI improvements: The current UI is too rudimentary, I may improve it later.
    - Search Highlighting: Real-time highlighting of matches during `/` or `?`
      searches
    - Syntax Highlighting: Integration of regex-based highlighting for common
      programming languages
+ Configuration File: Support for a `.aimrc` file to allow user-defined
  keybindings and settings
