#include <Arduino.h>

#define CONSOLE_UART Serial

// char consoleBuf[128] = {0};
String consoleBuf = "";

const char * help_text = R""""(Simple Rocket CLI Help
help            - prints this help
ls              - list files in flash
dump <id>       - dumps the telemetry file with the given ID
hexdump <id>    - dumps a file as hex
delete <id>     - delte telemetry file
format          - deletes all telemetry files (use with caution)
)"""";

bool formatInitiated = false;

void consoleHandle(String input) {
    String token[5];

    // Split input string by spaces
    int numParsedTokens = 0;
    int nextTokenIdx = 0;
    for (int i = 0; i < sizeof(token) / sizeof(token[0]); i++) {
        int spaceIdx = input.indexOf(' ', nextTokenIdx);
        if (spaceIdx == -1) {
            spaceIdx = input.length();
            numParsedTokens--;
        }
        token[i] = input.substring(nextTokenIdx, spaceIdx);
        nextTokenIdx = spaceIdx + 1;
        numParsedTokens++;
    }

    // handle commands
    String cmd = token[0];
    if (cmd == "") {
        // do nothing
    }
    else if (cmd == "help" || cmd == "\"help\"") {
        Serial.println(help_text);
    }
    else if (cmd == "ls") {
        listDir(LittleFS, "/", 1);
    }
    else if (cmd == "dump") {
        // TODO
    }
    else if (cmd == "hexdump") {
        if (numParsedTokens >= 2) {
            int id = token[1].toInt();
            telemetryHexdump(id);
        }
    }
    else if (cmd == "delete") {
        if (numParsedTokens >= 2) {
            int id = token[1].toInt();
            telemetryDeleteFile(id);
        }
    }
    else if (cmd == "format") {
        Serial.print("This will format all data stored in Flash! Are you sure? \nType \"yes\" to confirm: ");
        Serial.flush();
        formatInitiated = true;
        return;
    }
    else if (cmd == "yes") {
        if (formatInitiated) {
            telemetryFormat();
            formatInitiated = false;
        }
    }
    else {
        Serial.println("Unknown command, try \"help\"");
    }

    if (formatInitiated) {
        Serial.println(" Aborted.");
        formatInitiated = false;
    }

}

void consoleLoop() {
    while (CONSOLE_UART.available()) {
        char c = CONSOLE_UART.read();
        CONSOLE_UART.print(c);          // echo back input
        if (c == '\n') {                // handle input on newline
            consoleBuf += ' ';          // replace newline with space for easier parsing
            consoleHandle(consoleBuf);
            consoleBuf = "";            // clear receive buffer
        }
        else if (c == '\b') {           // backspace
            if (consoleBuf.length() > 0) {
                consoleBuf.remove(consoleBuf.length() - 1); // remove last char
            }
            Serial.print(" \b");        // clear char on screen
        }
        else if (c != '\r') {           // ignore carriage return char
            consoleBuf += c;            // add received char to buffer
        }
    }
}