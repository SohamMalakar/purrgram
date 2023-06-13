# Purrgram

This project is a C project that utilizes a Makefile for building, running, and cleaning the project. It provides an organized file structure for the project, along with instructions on how to use the Makefile.

## File Structure

The project follows the following file structure:

```
project-name
│ README.md
│ Makefile
│ .gitignore
│ LICENSE
│
└───build
│ └───debug
│ └───release
│
└───src
│ ...
│
└───test
│ test.prr
```

- `README.md`: Provides an overview of the project and instructions on how to use the Makefile.
- `Makefile`: Contains instructions for building, running, and cleaning the project.
- `.gitignore`: Specifies files and directories to be ignored by Git.
- `LICENSE`: The Unlicense.
- `build`: Directory to store build artifacts.
- `debug`: Directory to store debug build artifacts.
- `release`: Directory to store release build artifacts.
- `src`: Directory to store source code files.
- `test`: Directory to store test cases.

## Usage

To use the Makefile, follow the steps below:

1. Make sure you have `gcc` installed on your system.
2. Navigate to the project directory in the terminal.

### Build

To build the project, use the following command:

make [debug | release]

- `make debug`: Builds the debug version of the project.
- `make release`: Builds the release version of the project.

The built executable will be placed in the respective `build/debug` or `build/release` directory.

### Run

To run the project, use the following command:

make run [ARGS="args"]

- `make run`: Executes the debug version of the project.
- `ARGS="args"`: Optional argument to pass command-line arguments to the program.

Example:

```
make run ARGS="arg1 arg2 arg3"
```

### Clean

To clean the project and remove all build artifacts, use the following command:

```
make clean
```

This will remove the `build` directory and its contents.

## Contributing

If you encounter any issues or have suggestions for improvements, please feel free to contribute to the project by creating an issue or submitting a pull request.

## License

This project is licensed under the [Unlicense](LICENSE).
