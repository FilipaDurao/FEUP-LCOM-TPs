/* Used header files */
#include "kbd.h"
#include "StateMachine.h"
#include "Game.h"
#include "SnakeyNix.h"

static int kbdHookId = 1;
unsigned char codeByte;

int kbd_subscribe_int(){
    int kbdMaskBit = kbdHookId;   // Bit we are using for kbd interrupts

    // Subscribe the keyboard interrupts
    if(sys_irqsetpolicy(KBD_IRQ , (IRQ_REENABLE | IRQ_EXCLUSIVE) , &kbdHookId) != KBD_SUCCESS)
        return -1;    // Error occured, return negative value

    // Enable keyboard interrupts
    if(sys_irqenable(&kbdHookId) != KBD_SUCCESS)
        return -1;
    else
        return kbdMaskBit;  // No error occured, return the mask bit for the subscribed interrupt
}


int kbd_unsubscribe_int(){
    // Disable keyboard interrupts
    if(sys_irqdisable(&kbdHookId) != KBD_SUCCESS)
        return KBD_ERROR_OCCURED;

    // Unsubscribe keyboard interrupts and return failure/success
    return (sys_irqrmpolicy(&kbdHookId));
}


int kbd_read_statbyte(unsigned char* statByte){
    unsigned long stat;   // Variable for reading the kbc status Register

    // Read the status register
    if (sys_inb(KBC_STATUS_REG, &stat) != KBD_SUCCESS)              // Verify sys_inb success/failure
        return KBD_ERROR_OCCURED;
    else{
        *statByte = (unsigned char) stat;    // Reading was successful, save the status register content in statByte
        return KBD_SUCCESS;
    }
}


int kbd_read_outputbuffer(unsigned char* dataByte){
    unsigned long data;

    // Read the output buffer content
    if (sys_inb(KBC_OUT_BUF, &data) != KBD_SUCCESS)                 // Verify sys_inb success/failure
        return KBD_ERROR_OCCURED;
    else{
        *dataByte = (unsigned char) data;   // Reading was successful, save the output buffer content in dataByte
        return KBD_SUCCESS;
    }
}


int kbd_write_cmd(unsigned char cmd){
    unsigned char stat;         // Variable for reading the kbc status Register
    unsigned char tryCounter = 0;   // Counts the number of times it is not possible to write in the input buffer due to it being full

    // Loop while the input buffer is full ; When it's not, it's safe to write the command
    while( 1 ) {
        // Read the status register
        if (kbd_read_statbyte(&stat) != KBD_SUCCESS)              // Verify sys_inb success/failure
            return KBD_ERROR_OCCURED;

        /* loop while 8042 input buffer is not empty */
        if( (stat & KBC_STATUS_IBF) == 0 ) {
            return sys_outb(KBC_CMD_REG, cmd); /* no args command */
        } else {    // Input Buffer not empty, increment the tryCounter
            tryCounter++;
        }

        // Check if the maximum number of tries to write in the input buffer was reached
        if (tryCounter == KBD_MAX_INBUFF_TRIES)
            return KBD_ERROR_OCCURED;

        tickdelay(micros_to_ticks(KBD_DELAY));  // wait 20 ms
    }
}


int kbd_write_cmd_witharg(unsigned char cmd, unsigned char arg){
    unsigned char stat;   // Variable for reading the kbc status Register
    unsigned char tryCounter = 0;   // Counts the number of times it is not possible to write in the input buffer due to it being full

    // Writing the Command
    // Loop while the input buffer is full ; When it's not, it's safe to write the command
    while( 1 ) {
        // Read the status register
        if (kbd_read_statbyte(&stat) != KBD_SUCCESS)              // Verify sys_inb success/failure
            return KBD_ERROR_OCCURED;

        /* loop while 8042 input buffer is not empty */
        if( (stat & KBC_STATUS_IBF) == 0 ) {
            if (sys_outb(KBC_CMD_REG, cmd) == KBD_SUCCESS) /* write the command and break from the loop in order to write the argument */
                break;
            else
                return KBD_ERROR_OCCURED;
        } else {    // Input Buffer not empty, increment the tryCounter
            tryCounter++;
        }

        // Check if the maximum number of tries to write in the input buffer was reached
        if (tryCounter == KBD_MAX_INBUFF_TRIES)
            return KBD_ERROR_OCCURED;

        tickdelay(micros_to_ticks(KBD_DELAY));  // wait 20 ms
    }

    // Reset the in-buff writing try counter
    tryCounter = 0;

    // Writing the Argument
    // Loop while the input buffer is full ; When it's not, it's safe to write the argument
    while( 1 ) {
        // Read the status register
        if (kbd_read_statbyte(&stat) != KBD_SUCCESS)              // Verify sys_inb success/failure
            return KBD_ERROR_OCCURED;

        /* loop while 8042 input buffer is not empty */
        if( (stat & KBC_STATUS_IBF) == 0 ) {
            return sys_outb(KBC_IN_BUF, arg); /* write the argument */
        } else {    // Input Buffer not empty, increment the tryCounter
            tryCounter++;
        }

        // Check if the maximum number of tries to write in the input buffer was reached
        if (tryCounter == KBD_MAX_INBUFF_TRIES)
            return KBD_ERROR_OCCURED;

        tickdelay(micros_to_ticks(KBD_DELAY));  // wait 20 ms
    }
}


int kbd_read_kbc_commandByte(unsigned char * commandByte){
    unsigned char stat;

    // Write the the "Read Command Byte" command
    if (kbd_write_cmd(KBC_READ_COMMAND) != KBD_SUCCESS){
        return 1;
    }

    // Read the status register
    if (kbd_read_statbyte(&stat) != KBD_SUCCESS)              // Verify sys_inb success/failure
        return KBD_ERROR_OCCURED;

    // Verify if the output buffer is indeed full
    if( stat & KBC_STATUS_OBF ) {
        // Read the output buffer content
        if ( kbd_read_outputbuffer(commandByte) != KBD_SUCCESS)                // Verify sys_inb success/failure
            return KBD_ERROR_OCCURED;
        // Check for parity/timeout error
        if ( (stat & (KBC_STATUS_PARITY | KBC_STATUS_TIMEOUT)) == 0 )     // Check if there wasn't parity error / timeout error
            return KBD_SUCCESS;
        else  // Parity / timeout error occured
            return KBD_ERROR_OCCURED;
    }
    else  // Output buffer was not full
        return KBD_ERROR_OCCURED;
}


int kbc_flush_outputbuffer(){
    unsigned char stat;
    unsigned char flushedBufferContent;

    // Read the status register
    if(kbd_read_statbyte(&stat) != KBD_SUCCESS){
        return KBD_ERROR_OCCURED;
    }

    // If the output buffer is full, flush the content
    if (stat & KBC_STATUS_OBF){
        if (kbd_read_outputbuffer(&flushedBufferContent) != KBD_SUCCESS)
            return KBD_ERROR_OCCURED;
        else{
            /* Flush was successfull */
            return KBD_SUCCESS;
        }
    }
    else {
        /* The buffer is not full, no need to flush it! */
        return KBD_SUCCESS;
    }
}


unsigned char kbc_get_codebyte(){
    return codeByte;
}


void handle_keyboard(){
    // Call the handler and check for success
    if (kbd_int_handler() != OK){
        return;
    }

    // W Key was Pressed
    if (codeByte == MAKECODE(W_KEY))
        raise_event(get_game_state_machine(getSnakeyNix()) , W_KEY_PRESSED);

    // S Key was Pressed
    else if (codeByte == MAKECODE(S_KEY))
        raise_event(get_game_state_machine(getSnakeyNix()) , S_KEY_PRESSED);

    // A Key was Pressed
    else if (codeByte == MAKECODE(A_KEY))
        raise_event(get_game_state_machine(getSnakeyNix()) , A_KEY_PRESSED);

    // D  Key was Pressed
    else if (codeByte == MAKECODE(D_KEY))
        raise_event(get_game_state_machine(getSnakeyNix()) , D_KEY_PRESSED);

    // ESC Key was Pressed
    else if (codeByte == BREAKCODE(ESC_KEY))
        raise_event(get_game_state_machine(getSnakeyNix()) , ESC_KEY_PRESSED);
}
