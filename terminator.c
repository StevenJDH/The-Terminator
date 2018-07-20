/**
 * This file is part of the Terminator distribution (https://github.com/StevenJDH).
 * Copyright (C) 2018 Steven Jenkins De Haro.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <process.h>
#include <Tlhelp32.h>
#include <winbase.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h> /* access, getopt */
#include <errno.h>

// Update the version information here.
#define VERSION "1.0.0"
#define VERSION_DATE "2018/07/19"

// Console colors
enum colors { BLACK = 0, BLUE, GREEN, AQUA, RED, PURPLE, YELLOW, WHITE, GREY, LIGHTBLUE,
        LIGHTGREEN, LIGHTAQUA, LIGHTRED, LIGHTPURPLE, LIGHTYELLOW, BRIGHTWHITE };

// Prototypes
int argsCLI(int argc, char** argv);
int printUsage(const char *program_name);
void printLogo(void);
unsigned int displayMainScreen(void);
unsigned int displayKillScreen(void);
unsigned int displayKillRenameScreen(void);
unsigned int displayKillDeleteScreen(void);
unsigned int returnToMainScreen(void);
void getStringInput(const char *message, char *input, size_t size);
bool killOption(const char *process_name);
bool killRenameOption(const char *process_name, const char *old_name, const char *new_name);
bool killDeleteOption(const char *process_name, const char *filename);
bool killProcessByName(const char *process_name);
const char *gnuBasename(const char *filename);
void setConsoleColors(enum colors textColor, enum colors bgColor);

/**
 * Program entry point that controls the operation mode and and screen navigation.
 */
int main(int argc, char** argv) {
    // Using CLI mode if arguments were passed.
    if (argc > 1) {
        return argsCLI(argc, argv);
    }

    // Changes the console's foreground and background colors.
    setConsoleColors(LIGHTYELLOW, BLUE);

    unsigned int choice = 0; // Used for determining which screen to show.

    // The following handles the menu system and navigation.
    do {
        choice = displayMainScreen();
        switch (choice) {
            case 99:
                break; // Special code used to return to the main menu.
            case 1:
                choice = displayKillScreen();
                break;
            case 2:
                choice = displayKillRenameScreen();
                break;
            case 3:
                choice = displayKillDeleteScreen();
                break;
            case 4: // Falls through to exit.
            default:
                // A controlled exit from the program.
                printf("\nI'll be back. ");
                // We flush buffer before system() screen I/O, which is required.
                fflush(stdout);
                // Pauses and then clears screen after unpausing to clean things up.
                // We also reset console colors in between calls this way to play well with CLS.
                system("pause & COLOR 07 & cls");
        }
    } while (99 == choice); // Returns control back to main menu.

    return EXIT_SUCCESS;
}

/**
 * Command line mode. Takes the arguments for options and values, parses them, and calls the needed operation.
 */
int argsCLI(int argc, char** argv) {
    // Used to set what mode is being requested.
    enum mode {
        OPT_KILL = 'f',
        OPT_KILL_RENAME = 'r',
        OPT_KILL_DELETE = 'd',
    };

    int option = 0;
    const char *processName = NULL;
    const char *filename = NULL;
    const char *newFilename = NULL;
    enum mode optMode = OPT_KILL; // Defaults the mode to Kill Process and can only be changed once to parse valid arguments.
    extern char *optarg;

    // Options and values are parsed in any order, and they are case insensitive.
    // The statement opterr = 0; placed here can optionally hide getopt error message.
    while ((option = getopt(argc, argv, "-k:K:r:R:n:N:d:D:hH?")) != -1) { // Colons require value with option, dash requires options with value.
        switch (option) {
            case 'K':
            case 'k':
                if (processName != NULL) {
                    return printUsage(argv[0]);
                } // Ensures option flag is only used once.
                processName = optarg;
                break;
            case 'R':
            case 'r':
                if (filename != NULL || optMode != OPT_KILL) {
                    return printUsage(argv[0]);
                } // Ensures only one mode is set.
                filename = optarg;
                optMode = OPT_KILL_RENAME;
                break;
            case 'N':
            case 'n':
                if (newFilename != NULL) {
                    return printUsage(argv[0]);
                }
                newFilename = optarg;
                break;
            case 'D':
            case 'd':
                if (filename != NULL || optMode != OPT_KILL) {
                    return printUsage(argv[0]);
                }
                filename = optarg;
                optMode = OPT_KILL_DELETE;
                break;
            case 'h':
            case 'H':
            case '?': // For errors thrown by getopt: Unknown option.
            case ':': // For errors thrown by getopt: Missing value.
            default: // For errors thrown by getopt: Value with no option.
                return printUsage(argv[0]);
        }
    }

    // Calls the correct function operation to perform based on the arguments captured above.
    switch (optMode) {
        case OPT_KILL:
            if (NULL == processName) { // Ensures that there are no missing arguments.
                return printUsage(argv[0]);
            } else if (killOption(processName)) {
                return EXIT_SUCCESS;
            }
            break;
        case OPT_KILL_RENAME:
            if (NULL == processName || NULL == newFilename) { // Ensures that there are no missing arguments.
                return printUsage(argv[0]);
            } else if (killRenameOption(processName, filename, newFilename)) {
                return EXIT_SUCCESS;
            }
            break;
        case OPT_KILL_DELETE:
            if (NULL == processName) { // Ensures that there are no missing arguments.
                return printUsage(argv[0]);
            } else if (killDeleteOption(processName, filename)) {
                return EXIT_SUCCESS;
            }
    }

    return EXIT_FAILURE;
}

/**
 * Provides the usage information for command line use, then exits.
 */
int printUsage(const char *program_name) {
    fprintf(stderr, "\nUsage: %s -? | -k <process_name> [-r <old_filename> -n <new_filename> | -d <filename>]\n", program_name);
    fputs("\nOptions:\n"
            "  -k, -K \t Kills running process by name.\n"
            "  -r, -R \t Use with -k plus it renames a file.\n"
            "  -n, -N \t Use with -r to specify new filename.\n"
            "  -d, -D \t Use with -k plus it deletes a file.\n"
            "  -?, -h, -H \t Displays this usage information.\n", stderr);
    fputs("\nI'll be back . . .\n", stderr);

    return EXIT_FAILURE;
}

/**
 * Displays the main menu screen that provides access to different operations.
 */
unsigned int displayMainScreen(void) {
    unsigned int selection = 0;

    do {
        printLogo();
        printf("MAIN MENU:\n\n"
                "(1) - Kill Process\n"
                "(2) - Kill Process and Rename a File\n"
                "(3) - Kill Process and Delete a File\n"
                "(4) - Quit\n"
                "\n"
                "Please enter a menu selection [1-4]: ");
        const char input = fgetc(stdin);
        selection = strtoul(&input, NULL, 10); // Converts and sets to 0 if could not convert.
        fflush(stdin); // Clears stdin of any extra characters.
    } while (selection < 1 || selection > 4);

    return selection;
}

/**
 * Kill Process screen to manually enter required information.
 */
unsigned int displayKillScreen(void) {
    printLogo();

    char processName[50+1+1]; // n+1 for null terminator, n+1+1 for newline from fgets.

    puts("KILL PROCESS:\n");
    getStringInput("Enter process name to kill (i.e. explorer.exe): ", processName, sizeof (processName));
    printf("\n"); // Some separation for the results.

    killOption(processName);

    return returnToMainScreen();
}

/** 
 * Kill and Rename screen to manually enter required information.
 */
unsigned int displayKillRenameScreen(void) {
    printLogo();

    char processName[50+1+1]; // n+1 for null terminator, n+1+1 for newline from fgets.
    char oldName[255+1+1];
    char newName[255+1+1];

    puts("KILL PROCESS AND RENAME A FILE:\n");
    getStringInput("Enter process name to kill (i.e. explorer.exe): ", processName, sizeof (processName));
    getStringInput("Enter filename to rename (i.e. C:\\example.txt): ", oldName, sizeof (oldName));
    getStringInput("Enter new name for file (i.e. C:\\example2.txt): ", newName, sizeof (newName));
    printf("\n"); // Some separation for the results.

    killRenameOption(processName, oldName, newName);

    return returnToMainScreen();
}

/**
 * Kill and Delete screen to manually enter required information.
 */
unsigned int displayKillDeleteScreen(void) {
    printLogo();

    char processName[50+1+1]; // n+1 for null terminator, n+1+1 for newline from fgets.
    char filename[255+1+1];

    puts("KILL PROCESS AND DELETE A FILE:\n");
    getStringInput("Enter process name to kill (i.e. explorer.exe): ", processName, sizeof (processName));
    getStringInput("Enter filename to delete (i.e. C:\\example.txt): ", filename, sizeof (filename));
    printf("\n"); // Some separation for the results.

    killDeleteOption(processName, filename);

    return returnToMainScreen();
}

/**
 * Prompts user with custom message and retrieves their input.
 */
void getStringInput(const char *message, char *input, size_t size) {
    size_t len = 0;

    printf(message);
    // Bounded input to prevent buffer overflow. Ensures there is actual input provided.
    while (fgets(input, size, stdin) == NULL || (len = strlen(input)) <= 1 || input[0] == ' ') {
        fflush(stdin); // Clears stdin of any extra characters.
        printf("Invalid input, try again: ");
    }
    fflush(stdin);

    // Replaces newline included by fgets with null terminator.
    if ('\n' == input[len - 1]) {
        input[len - 1] = '\0';
    }
}

/**
 * Terminates a process by name. This is just a wrapper function for killProcessByName().
 */
bool killOption(const char *process_name) {
    if (killProcessByName(process_name) == false) {
        fprintf(stderr, "Error: Process '%s' was not found.\n", process_name);
        return false;
    }
    return true;
}

/**
 * Terminates a process and renames a file that was locked by that process.
 */
bool killRenameOption(const char *process_name, const char *old_name, const char *new_name) {
    if (strcasecmp(old_name, new_name) == 0) {
        fprintf(stderr, "Error: New filename '%s' is the same as the original.\n", gnuBasename(new_name));
        return false;
    }

    if (killProcessByName(process_name) == false) {
        fprintf(stderr, "Error: Process '%s' was not found. The file '%s' will not be renamed.\n", process_name, gnuBasename(old_name));
    } else {
        // First checks if file exists and assumes correct folder permissions, then it tries to rename it.
        if (access(old_name, F_OK) == 0 && rename(old_name, new_name) == 0) {
            printf("File '%s' was renamed to '%s' successfully.\n", gnuBasename(old_name), gnuBasename(new_name));
            return true;
        } else {
            fprintf(stderr, "Error: Unable to rename the file '%s'. %s.\n", gnuBasename(old_name), strerror(errno));
        }
    }
    return false;
}

/**
 * Terminates a process and deletes a file that was locked by that process.
 */
bool killDeleteOption(const char *process_name, const char *filename) {
    if (killProcessByName(process_name) == false) {
        fprintf(stderr, "Error: Process '%s' was not found. The file '%s' will not be deleted.\n", process_name, gnuBasename(filename));
    } else {
        // First checks if file exists and assumes correct folder permissions, then it tries to delete it.
        if (access(filename, F_OK) == 0 && remove(filename) == 0) {
            printf("File '%s' was deleted successfully.\n", gnuBasename(filename));
            return true;
        } else {
            fprintf(stderr, "Error: Unable to delete the file '%s'. %s.\n", gnuBasename(filename), strerror(errno));
        }
    }
    return false;
}

/**
 * Generates and displays the application's title logo.
 */
void printLogo(void) {
    system("cls"); // Clears the console window.

    // Requires -std=gnu11 set in compiler to support raw strings.
printf(R"(
                     <((((((\\\
                     /      . }\
                     ;--..--._|}
  (\                 '--/\--'  )
   \\                | '-'  :'|
    \\               . -==- .-|
     \\               \.__.'   \--._
     [\\          __.--|       //  _/'--.
     \ \\       .'-._ ('-----'/ __/      \
      \ \\     /   __>|      | '--.       |
       \ \\   |   \   |     /    /       /
        \ '\ /     \  |     |  _/       /
         \  \       \ |     | /        /
          \  \      \        /        
 _______ _            _______                  _             _             
|__   __| |          |__   __|                (_)           | |            
   | |  | |__   ___     | | ___ _ __ _ __ ___  _ _ __   __ _| |_ ___  _ __ 
   | |  | '_ \ / _ \    | |/ _ \ '__| '_ ` _ \| | '_ \ / _` | __/ _ \| '__|
   | |  | | | |  __/    | |  __/ |  | | | | | | | | | | (_| | || (_) | |   
   |_|  |_| |_|\___|    |_|\___|_|  |_| |_| |_|_|_| |_|\__,_|\__\___/|_|  v%s
   By Steven Jenkins De Haro - %s                                

)", VERSION, VERSION_DATE);
}

/**
 * Handles returning back to the main menu.
 */
unsigned int returnToMainScreen(void) {
    printf("\nReturning to the main menu. ");
    fflush(stdout); // We flush buffer before system() screen I/O, which is required.
    system("pause");
    return 99; // Special code to return to main menu.
}

/**
 * Terminates any running processes that match the process name passed to it.
 */
bool killProcessByName(const char *process_name) {
    bool hasKilled = false;

    // Take a snapshot of all processes in the system.
    HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
    if (hSnapShot == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: Unable to acquire process list.");
        return false;
    }

    // Set the size of the structure before using it.
    PROCESSENTRY32 pEntry;
    pEntry.dwSize = sizeof (pEntry);

    // Retrieves information about the first process and exits if unsuccessful.
    if (!Process32First(hSnapShot, &pEntry)) {
        fprintf(stderr, "Error: Unable to retrieve process information.");
        CloseHandle(hSnapShot); // clean the snapshot object
        return false;
    }

    // Now walk the snapshot of processes, and terminate those that match.
    do {
        if (strcasecmp(pEntry.szExeFile, process_name) == 0) // If current process matches passed process...
        {
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, false, pEntry.th32ProcessID);
            if (hProcess != NULL) {
                TerminateProcess(hProcess, 9);
                CloseHandle(hProcess);
                printf("Terminated process - %s (PID: %lu)\n", pEntry.szExeFile, pEntry.th32ProcessID);
                hasKilled = true;
            }
        }
    } while (Process32Next(hSnapShot, &pEntry));

    CloseHandle(hSnapShot);
    return hasKilled;
}

/**
 * Simple way to get filename from path using the non-modifying approach of the GNU basename() function that
 * is manually provided here rather than dealing with compiler options like -D_GNU_SOURCE and directives like
 * #define _GNU_SOURCE with #include <string.h> to attempt to override the explicit or implicit inclusion of
 * POSIX's #include <libgen.h> which has its own basename() function that modifies a cstring, and therefore, 
 * needs a cast of (char *) when a const char * is used. The result is more portable and safer code.
 */
const char *gnuBasename(const char *filename) {
    const char *base = strrchr(filename, '\\'); // Returns pointer to last occurrence or null if none.
    return base ? ++base : filename; // One more than the last occurrence, which is the filename, otherwise it's already the filename.
}

/**
 * Changes the foreground and the background of the console. Default Colors: (WHITE, BLACK).
 */
void setConsoleColors(enum colors textColor, enum colors bgColor) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (textColor + (bgColor * 16)));
}
