# FAT32 File System Utility

Our project aimed to design and implement a user-space, shell-like utility capable of interpreting FAT32 file system images. This utility allows users to manipulate the given file system image using basic commands without compromising its integrity. The file system image remains uncorrupted throughout the execution of the program.

Our project also possess a comprehensive understanding of the fundamental commands required to manipulate the file system image. It also exhibits robustness by handling various errors that may occur during its execution. Whenever an error arises, the program should provide descriptive error messages to assist users in troubleshooting. Furthermore, the program should continue running smoothly, leaving the file system's state unchanged, even in the presence of erroneous commands or data.

## Group Members
- **Daniel Halterman**: drh22a@fsu.edu
- **Ryan Kurfirst**: rsk22a@fsu.edu
- **Caleb Dindinger**: ab19@fsu.edu
- **Alejandro Valdes**: av22q@fsu.edu

## Division of Labor

### Part 1: Mounting the Image
- **Responsibilities**: Integrated the info and exit commands. The info command parses the boot sector. Prints the field name and corresponding value for each entry, one per line. The exit command safely closes the program and frees up any allocated resources.
- **Assigned to**: Daniel Halterman

### Part 2: Navigation
- **Responsibilities**: Implemented the cd command which changes the current working directory to DIRNAME. Also implemented the ls command which prints the name filed for the directories and files within the current working directory including the “.” and “..” directories.
- **Assigned to**: Daniel Halterman

### Part 3: Create
- **Responsibilities**: Implemented the mkdir cmd and the creat cmd
- **Assigned to**: Caleb Dindinger

### Part 4: Read
- **Responsibilities**: Implemented commands for opening, closing, and reading files. Also implemented listing open files, changing the read/write position, and displaying file contents.
- **Assigned to**: Alejandro Valdes

### Part 5: Update
- **Responsibilities**: Implemented writing text to files. This includes automatically expanding the file when needed. Also created the mv command for files and directories.
- **Assigned to**: Ryan Kurfirst

### Part 6: Delete
- **Responsibilities**: Implemented commands for removing directories. Also made sure files are freed correctly and that directories are only removed when they are empty.
- **Assigned to**: Ryan Kurfirst

## File Listing
```
filesys/
│
├── src/
│ ├── fat32.c
│ ├── lexer.c
│ ├── main.c
│ └── shell.c
│
├── include/
│ ├── fat32.h
│ ├── lexer.h
│ └── shell.h
├── bin/         # Used to store filesys (Not included on Github)
│ ├── filesys
├── obj/         # Holds all of the object files (Not included on Github)
│ ├── fat32.o
│ ├── lexer.o
│ ├── main.o
│ └── shell.o
├── fat32.img    # The FAT32 image file (Not included on Github)
├── README.md
└── Makefile
```
## How to Compile & Execute

*** Part 1 Execution ***

First, compile the project
```bash
make clean
make
```

Next, mount the image
```bash
./bin/filesys fat32.img
```

At the prompt, type:
```bash
info
```

The fields printed out should look like this:
```
    position of root cluster (in cluster #)
    bytes per sector
    sectors per cluster
    total # of clusters in data region
    # of entries in one FAT
    size of image (in bytes)
```

Using an invalid command will result in an error message!

Type exit in order to exit
```bash
exit
```

Finally, remove all of the created files
```bash
make clean
```

*** Part 2 Execution ***

First, compile the project
```bash
make clean
make
```

Start up the program
```bash
./bin/filesys fat32.img
```

Use cd to move between directories. Replace the X's with your directory name
```bash
cd XXXXX
```

Use ls to print the name filed for the directories and files within the current working directory including the “. and “..” directories.
```bash
ls XXXXX
```


### Requirements
- **Compiler**: e.g., `gcc` for C

### Compilation
For a C/C++ example:
```bash
make
```
This will build the executable in ...
### Execution
```bash
make run
```
This will run the program ...

## Considerations
[Description]
