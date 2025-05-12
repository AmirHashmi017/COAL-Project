.include "m328pdef.inc"

; Constants for UART configuration
.equ BAUD_RATE = 9600
.equ F_CPU = 16000000
.equ UBRR_VALUE = F_CPU/16/BAUD_RATE - 1

; Buffer size for UART reception
.equ BUFFER_SIZE = 32

; Servo positions (PWM values for Timer1)
.equ SERVO_CLOSED = 2000  ; 1ms pulse - 0 degrees
.equ SERVO_OPEN = 4000    ; 2ms pulse - 180 degrees

; Macro to add delay (blocking) up to 2500ms
.macro delay
    push r18
    push r24
    push r25
    ldi r18,@0/10
    L1:
    ldi r24,LOW(39998)    ; initialize inner loop count
    ldi r25,HIGH(39998)   ; loop high and low registers
    L2:
    sbiw r24,1           ; decrement inner loop registers
    brne L2              ; branch to L2 if registers != 0
    dec r18              ; decrement outer loop register
    brne L1              ; branch to L1 if outer loop register != 0
    nop                  ; no operation
    pop r25
    pop r24
    pop r18
.endmacro

; Macros to Read/Write 16-bit registers
.macro STSw
    cli
    STS @0, @1
    STS @0-1, @2
    sei
.endmacro

.macro LDSw
    cli
    LDS @1, @2-1
    LDS @0, @2
    sei
.endmacro

; Data section for buffer
.dseg
UART_BUFFER: .byte BUFFER_SIZE   ; Reserve buffer space for UART reception
buffer_index: .byte 1           ; Buffer index tracker
cmd_ready: .byte 1              ; Flag to indicate command ready

; Code section
.cseg
.org 0x0000
    jmp RESET                   ; Reset vector
    
.org 0x0024                     ; USART RX Complete interrupt vector
    jmp USART_RX_complete       ; Jump to UART RX interrupt handler

; Main program starts here
.org 0x0030
RESET:
    ; Initialize the stack pointer
    ldi r16, HIGH(RAMEND)
    out SPH, r16
    ldi r16, LOW(RAMEND)
    out SPL, r16
    
    ; Initialize UART buffer index
    ldi r16, 0
    sts buffer_index, r16
    
    ; Clear command ready flag
    ldi r16, 0
    sts cmd_ready, r16
    
    ; Set pin PB1 (OC1A) as output for servo control
    ; PB1 is digital pin 9 on Arduino UNO
    sbi DDRB, 1
    
    ; Initialize status LED on PB5 (Arduino pin 13)
    sbi DDRB, 5
    
    ; Flash status LED to indicate startup
    sbi PORTB, 5      ; LED on
    delay 100         ; Delay 100ms
    cbi PORTB, 5      ; LED off
    delay 100         ; Delay 100ms
    sbi PORTB, 5      ; LED on
    delay 100         ; Delay 100ms
    cbi PORTB, 5      ; LED off
    
    ; Initialize UART communication
    rcall UART_init
    
    ; Initialize Timer1 for PWM generation
    rcall Timer1_init
    
    ; Set servo to closed position initially
    ldi r16, LOW(SERVO_CLOSED)
    ldi r17, HIGH(SERVO_CLOSED)
    STSw OCR1AH, r17, r16
    
    ; Enable global interrupts
    sei
    
    ; Send startup message via UART
    ldi ZL, LOW(2*startup_msg)
    ldi ZH, HIGH(2*startup_msg)
    rcall UART_send_string

; Main program loop
main_loop:
    ; Check if a command is ready
    lds r16, cmd_ready
    cpi r16, 1
    brne main_loop
    
    ; Process the command
    rcall process_command
    
    ; Clear command ready flag
    ldi r16, 0
    sts cmd_ready, r16
    
    rjmp main_loop

; Initialize Timer1 for servo control
Timer1_init:
    ; Set Fast PWM mode with ICR1 as TOP
    ; COM1A1: Clear OC1A on compare match, set at BOTTOM
    ; WGM11: Fast PWM mode bit 1
    ldi r16, (1<<COM1A1) | (1<<WGM11)
    sts TCCR1A, r16
    
    ; Set WGM12, WGM13 and CS11 (Prescaler = 8)
    ; This gives Mode 14 (Fast PWM with ICR1 as TOP)
    ldi r16, (1<<WGM13) | (1<<WGM12) | (1<<CS11)
    sts TCCR1B, r16
    
    ; Clear counter
    ldi r16, 0
    ldi r17, 0
    STSw TCNT1H, r17, r16
    
    ; Set TOP value for 50Hz (20ms) period
    ; 16MHz / 8 prescaler = 2MHz timer frequency
    ; 2MHz / 50Hz = 40,000 counts for 20ms
    ldi r16, LOW(40000)
    ldi r17, HIGH(40000)
    STSw ICR1H, r17, r16
    
    ret

; Initialize UART at 9600 baud
UART_init:
    ; Set baud rate
    ldi r16, HIGH(UBRR_VALUE)
    sts UBRR0H, r16
    ldi r16, LOW(UBRR_VALUE)
    sts UBRR0L, r16
    
    ; Enable receiver and transmitter, enable RX interrupt
    ldi r16, (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0)
    sts UCSR0B, r16
    
    ; Set frame format: 8 data bits, 1 stop bit, no parity
    ldi r16, (3<<UCSZ00)
    sts UCSR0C, r16
    
    ret

; UART RX Complete Interrupt Handler
USART_RX_complete:
    push r16
    push r17
    push r30
    push r31
    
    ; Read received data
    lds r16, UDR0
    
    ; Check for end of command (newline character)
    cpi r16, 0x0A      ; Check if character is newline
    breq end_of_command
    
    ; Store the character in buffer
    lds r17, buffer_index
    
    ; Check buffer overflow
    cpi r17, BUFFER_SIZE
    brlo continue_rx   ; Branch if less than
    rjmp rx_exit       ; Long-range jump
continue_rx:
    
    ; Calculate buffer address
    ldi r30, LOW(UART_BUFFER)
    ldi r31, HIGH(UART_BUFFER)
    add r30, r17       ; Add index to buffer base address
    brcc no_carry
    inc r31
no_carry:
    st Z, r16          ; Store character in buffer
    
    ; Increment buffer index
    inc r17
    sts buffer_index, r17
    
    rjmp rx_exit
    
end_of_command:
    ; Set command ready flag
    ldi r16, 1
    sts cmd_ready, r16
    
rx_exit:
    pop r31
    pop r30
    pop r17
    pop r16
    reti

; Process received command
process_command:
    ; Get first character from buffer
    ldi r30, LOW(UART_BUFFER)
    ldi r31, HIGH(UART_BUFFER)
    ld r16, Z          ; Load first character

    ; Check command type by first character
    cpi r16, 'O'       ; Check if first letter is 'O'
    breq check_open_cmd ; Branch directly if 'O'
    cpi r16, 'D'       ; Check if first letter is 'D'
    breq check_distance_cmd ; Branch directly if 'D'
    rjmp finish_command ; Unknown command, reset buffer

check_open_cmd:
    ; Check if command is O:x
    ld r16, Z+         ; Skip 'O' (already checked)
    ld r16, Z+         ; Load second character
    cpi r16, ':'
    breq continue_open_cmd ; Branch if equal
    rjmp finish_command    ; Long-range jump
continue_open_cmd:
    ld r16, Z+         ; Load third character
    cpi r16, '1'       ; Check if it's open command (O:1)
    breq do_open_servo
    cpi r16, '0'       ; Check if it's close command (O:0)
    breq do_close_servo
    rjmp finish_command

check_distance_cmd:
    ; Check if command starts with 'D:'
    ld r16, Z+         ; Skip 'D' (already checked)
    ld r16, Z+         ; Load second character
    cpi r16, ':'
    breq continue_distance_cmd ; Branch if equal
    rjmp finish_command       ; Long-range jump
continue_distance_cmd:
    ; Process distance value if needed
    rjmp finish_command

do_open_servo:
    ; Send acknowledgment
    ldi ZL, LOW(2*open_msg)
    ldi ZH, HIGH(2*open_msg)
    rcall UART_send_string
    ; Turn on status LED
    sbi PORTB, 5
    ; Set servo to open position
    ldi r16, LOW(SERVO_OPEN)
    ldi r17, HIGH(SERVO_OPEN)
    STSw OCR1AH, r17, r16
    delay 1000         ; Wait for servo to move
    rjmp finish_command

do_close_servo:
    ; Send acknowledgment
    ldi ZL, LOW(2*close_msg)
    ldi ZH, HIGH(2*close_msg)
    rcall UART_send_string
    ; Set servo to closed position
    ldi r16, LOW(SERVO_CLOSED)
    ldi r17, HIGH(SERVO_CLOSED)
    STSw OCR1AH, r17, r16
    delay 1000         ; Wait for servo to move
    ; Turn off status LED
    cbi PORTB, 5
    rjmp finish_command

finish_command:
    ; Reset buffer index
    ldi r16, 0
    sts buffer_index, r16
    ret

; Send a null-terminated string via UART
UART_send_string:
    ; Z register should be preloaded with the address of the string
    push r16
    
send_loop:
    lpm r16, Z+        ; Load byte from program memory
    cpi r16, 0         ; Check for null terminator
    breq send_done     ; Exit if null
    
    rcall UART_send_char
    rjmp send_loop
    
send_done:
    pop r16
    ret

; Send single character via UART
UART_send_char:
    ; Wait for transmit buffer to be empty
    lds r17, UCSR0A
    sbrs r17, UDRE0
    rjmp UART_send_char
    
    ; Send character
    sts UDR0, r16
    ret

; String constants in program memory
startup_msg: .db "AVR Servo Controller Ready", 13, 10, 0
open_msg:    .db "Opening lid", 13, 10, 0
close_msg:   .db "Closing lid", 13, 10, 0