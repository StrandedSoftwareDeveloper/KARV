Note: To my understanding, all memory accesses in this emulator are 32-bit.

0x10000000: Primary (and currently only) UART. Reading returns the ASCII value of the next character in the keyboard buffer (or 0 if there is none). Writing sends the provided ASCII value to the VT-100 emulator.
0x10000005: UART buffer status. Reading returns 0x60 if the buffer is empty, or 0x61 if the buffer has at least one character in it. Writing does nothing.
0x11004000: Programmable timer low word. Reading returns 0. Writing sets the timer target's low word to the provided value. When the timer reaches the target, an interrupt is fired, and the timer is *not* reset, meaning you'll have to update the timer target if you want this to fire again.
0x11004004: Programmable timer high word. Same as the previous, just the upper half of it.
0x1100bff8: Timer low word. Reading returns the lower word of the time since boot in microseconds. Writing does nothing.
0x1100bffc: Timer high word. Upper half of the above. Writing does nothing.
0x11100000: SYSCON. Reading returns nothing. Writing causes the emulator core's step function to stop and return the provided value, though this does not seem to be handled well in the wrapper at the moment.