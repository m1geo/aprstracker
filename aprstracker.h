/*
 *	aprstracker.h
 *
 *      Copyright 2003-2005 Jeroen Vreeken (pe1rxq@amsat.org),
 *                2005-2006 Arno Verhoeven (pe1icq@amsat.org)
 *
 *	Published under the terms of the GNU General Public License
 *	version 2. Read the file COPYING for more details.
 */

#ifndef _INCLUDE_APRSTRACKER_H_
#define _INCLUDE_APRSTRACKER_H_


/* Don't know how to set the second config word in a 16f88
	__CONFIG(2, FCMDIS & IESODIS);
   and
	__CONFIG2(FCMDIS & IESODIS);
   don't work with my compiler version.
   For the moment I'm just ignoring these settings....
*/

#if defined(_16F628) || defined(_16F648) || defined(_16F88)
	__CONFIG(HS & WDTEN & PWRTEN & BOREN & LVPDIS & UNPROTECT);
#endif
#if defined(_16F636)
	__CONFIG(HS & WDTEN & PWRTEN & BODEN & IESODIS & MCLRDIS & UNPROTECT);
#endif


#define APRSTRACKER_VERSION	0x0C

#if defined(_16F628)
	const char version_text[] = { '{'<<1, 'A'<<1, 'T'<<1, '0'<<1, 'B'<<1, '2'<<1, '}'<<1,
	                              'a'<<1, 'p'<<1, 'r'<<1, 's'<<1, 't'<<1, 'r'<<1, 'a'<<1, 
				      'c'<<1, 'k'<<1, 'e'<<1, 'r'<<1, '-'<<1, '0'<<1, '.'<<1, 
				      '1'<<1, '2'<<1, '-'<<1, '1'<<1, '6'<<1, 'f'<<1, '6'<<1, 
				      '2'<<1, '8'<<1, 0 };
#elif defined(_16F636)
	const char version_text[] = { '{'<<1, 'A'<<1, 'T'<<1, '0'<<1, 'C'<<1, '3'<<1, '}'<<1, 0 };
#elif defined(_16F648)
	const char version_text[] = { '{'<<1, 'A'<<1, 'T'<<1, '0'<<1, 'C'<<1, '4'<<1, '}'<<1, 0 };
#elif defined(_16F88)
	const char version_text[] = { '{'<<1, 'A'<<1, 'T'<<1, '0'<<1, 'C'<<1, '8'<<1, '}'<<1, 0 };
#endif


/*
 *	EEPROM defaults
 */

/* memmory layout version */
#define APRSTRACKER_EEVERSION	0x04

#asm
	psect	eeprom_data,class=EEDATA,delta=2

	/* position 0x00-0x06 destination address APERXQ-0 */
	db	'A'<<1, 'P'<<1, 'E'<<1, 'R'<<1, 'X'<<1, 'Q'<<1, '0'<<1
	/* position 0x07-0x0d source address NOCALL-0 */
	db	'N'<<1, 'O'<<1, 'C'<<1, 'A'<<1, 'L'<<1, 'L'<<1, '0'<<1
	/* position 0x0e-0x14 first digipeater RELAY-0 */
	db	'W'<<1, 'I'<<1, 'D'<<1, 'E'<<1, '1'<<1, ' '<<1, '1'<<1
	/* position 0x15 proportional pathing rate */
	#define	EE_PROPPATH 0x15
	db	3
	/* position 0x16 beacon text every */
	#define	EE_BTEXTINTERVAL 0x16
	db	2
	/*
	 * SmartBeaconing(tm) variables. 
	 * Change these to tweak SmartBeaconing(tm) behaviour.
	 * But be carefull because small changes could easily lead to
	 * flooding the network!
	 */
	// Fast beacon rate (secs)
	#define EE_SB_POSIT_FAST	0x17
	db	90
	// Slow beacon rate (mins)
	#define EE_SB_POSIT_SLOW	0x18
	db	20
	// Speed below which SmartBeaconing(tm) is disabled we'll beacon at
	// the POSIT_slow rate (knots)
	#define EE_SB_LOW_SPEED_LIMIT	0x19
	db	3
	// Speed above which we'll beacon at the POSIT_fast rate (knots)
	#define EE_SB_HIGH_SPEED_LIMIT	0x1a
	db	50
	
	/* Tracker configration bits
	 *
	 * 0: (  1) CSE_SPD_enabled
	 * 1: (  2) PHGD_enabled (not implemented yet)
	 * 2: (  4) altitude_enabled
	 * 3: (  8) 
	 * 4: ( 16) 
	 * 5: ( 32) 
	 * 6: ( 64) 
	 * 7: (128) 
	 */
	#define EE_TRACKER_CONFIG	0x1b
	db	5
	/* position 0x1c beaconinterval (minutes)
	 * when set to 0 then smartbeaconting(tm) is enabled
	 * any other value sets fixed beacon interval */
	#define EE_BEACONTIME	0x1c
	db	0
	/* position 0x1d txdelay (flags) */
	#define EE_TXDELAY	0x1d
	db	60
	/* position 0x1e symbol table */
	#define EE_SYMBOLTABLE	0x1e
	db	'/'
	/* position 0x1f symbol id */
	#define EE_SYMBOLID	0x1f
	db	'>'
	/* position 0x20-0x3f beacontext (null terminated) */
	#define EE_BEACONTEXT	0x20
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0

	/* position 0x48-0x7f
	 * User definable GPS configuration string (null terminated)
	 * can also be extended status text if previous block is not
	 * null terminated */
	#define EE_GPS_CONFIG	0x48
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0
	db	0, 0, 0, 0, 0, 0, 0, 0

#if defined(_16F636) || defined(_16F648) || defined(_16F88)
	/* position 0x80-onwards statustext (null terminated) */
	#define EE_STATUSTEXT	0x80
	db	'a', 'p', 'r', 's', 't', 'r', 'a', 'c'
	db	'k', 'e', 'r', '-', '0', '.', '1', '2'
#if defined(_16F636)
	db	'-', '1', '6', 'f', '6', '3', '6', 0
#elif defined(_16F648)
	db	'-', '1', '6', 'f', '6', '4', '8', 0
#elif defined(_16F88)
	db	'-', '1', '6', 'f', '8', '8', 0, 0
#endif
#endif
#endasm



/*
 *	IO ports
 */
#if defined(_16F628) || defined(_16F648) || defined(_16F88)
	static bit LSTAT	@(unsigned)&PORTA*8+0;
	static bit LGPS 	@(unsigned)&PORTA*8+1;
	static bit PTT 		@(unsigned)&PORTA*8+2;
	static bit TXD		@(unsigned)&PORTA*8+3;
	static bit RXD		@(unsigned)&PORTA*8+4;
	
	static bit R8K2		@(unsigned)&PORTB*8+0;
	static bit R3K9		@(unsigned)&PORTB*8+1;
	static bit R2K0		@(unsigned)&PORTB*8+2;
	static bit R1K0		@(unsigned)&PORTB*8+3;

	// keep in mind that RXDET is TRUE if channel is free
	static bit RXDET	@(unsigned)&PORTB*8+4;
	static bit SEND		@(unsigned)&PORTB*8+5;
	static bit SW1		@(unsigned)&PORTB*8+6;
	static bit SW2		@(unsigned)&PORTB*8+7;

	/* Only the receiving pins */
	/* A4 = 16 */
	#define INIT_PORTA	TRISA = 16
	/* B4 = 16, B5 = 32, B6 = 64, B7 = 128 */
	#define INIT_PORTB	TRISB = 16 + 32 + 64 + 128
#endif
#if defined(_16F636)
	bit LSTAT;
	static bit LGPS 	@(unsigned)&PORTA*8+0;

	// keep in mind that RXDET is FALSE if channel is free
	static bit RXDET	@(unsigned)&PORTA*8+1;
	static bit PTT 		@(unsigned)&PORTA*8+2;
	static bit SEND		@(unsigned)&PORTA*8+3;

	static bit R8K2		@(unsigned)&PORTC*8+0;
	static bit R3K9		@(unsigned)&PORTC*8+1;
	static bit R2K0		@(unsigned)&PORTC*8+2;
	static bit R1K0		@(unsigned)&PORTC*8+3;
	
	static bit TXD		@(unsigned)&PORTC*8+4;
	static bit RXD		@(unsigned)&PORTC*8+5;

	bit SW1;	// not available
	bit SW2;	// not available

	/* Only the receiving pins */
	/* A1= 2, A3 = 8 */
	#define INIT_PORTA	TRISA = 2 + 8
	/* C5 = 32 */
	#define INIT_PORTC	TRISC = 32
#endif

#define SOH 	0x01	/* start of heading */
#define EOT 	0x04	/* end of transmission */
#define XON 	0x11	/* Device Control 1 */
#define XOFF	0x13	/* Device Control 3 */

/*
 *	AX.25 stuff
 */
#define AX25_FLAG	0x7e
#define AX25_UI		0x03
#define AX25_PID_NL3	0xf0
/*
 *	APRS stuff
 */
#define APRS_PID	'!'	/* lat/long without time */


#endif
