# milli_editor
A simple Unix text editor implemented in C &amp; in just a few lines  🙂

# History
This is a project is derived from the works of the [kilo editor](https://github.com/snaptoken/kilo-src) written by @antirez. I readapted some of the code to fit my own needs and fixed all the memory leaks. I took this project up as a challenge to learn C to a very extensive level. There maybe bugs feel free to make a PR and I'll fix them :).

# Features
* Syntax highlighting for C/C++ source only( that is all I got working for now :( ).
* Opens only one file per run.
* Can save, edit and read files.
* In-program help at during startup.
* Search for specific strings.
* All features of a basic editor like vertical and horizontal scrolling files, special key mappings like Home, End, Page Up and Down keys.

# Usage
- Compile using the command below:
    ```sh
    make
    ```

- Run the text editor by providing it a file or not, like so:
    * with file:
        ```sh
        ./milli <file to open>
        ```

    * without file:
        ```sh
        ./milli
        ```
# Contributions
    Very WELCOME! 