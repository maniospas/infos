#include "keyboard.h"
#include "../io.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define KB_BUF_SIZE 32

// Circular buffer for incoming scancodes
static volatile unsigned char buffer[KB_BUF_SIZE];
static volatile int head = 0;
static volatile int tail = 0;

void keyboard_init(void) {
    // Flush any pending data in the PS/2 buffer
    while (inb(PS2_STATUS) & 1)
        inb(PS2_DATA);
    head = tail = 0;
}

void keyboard_clear(void) {
    head = tail = 0;
}

// === Called from ISR ===
// Stores *every* scancode, including extended/non-printable ones
void keyboard_handler_c(unsigned char scancode) {
    int next = (head + 1) % KB_BUF_SIZE;
    if (next != tail) {  // prevent overwrite if buffer full
        buffer[head] = scancode;
        head = next;
    }
}

// === Called from kernel or console ===
// Returns next scancode in FIFO order, or 0 if empty
unsigned char keyboard_read(void) {
    if (head == tail)
        return 0;  // no data available
    char sc = buffer[tail];
    tail = (tail + 1) % KB_BUF_SIZE;
    return sc;
}

// Standard US QWERTY keymap (scancode set 1)
static const char keymap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', 0,
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
    'z','x','c','v','b','n','m',',','.','/', 0,'*', 0,' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

bool key_released(unsigned char scancode) {
    return (scancode & 0x80) != 0;
}

bool key_extended(unsigned char scancode) {
    return scancode == 0xE0 || scancode == 0xE1;
}

char key_printable(unsigned char scancode) {
    if (key_released(scancode) || scancode >= 128)
        return 0;
    char c = keymap[scancode];
    return c ? c : 0;
}
