/*
 *	aprstracker.c	v 0.13
 *
 *      Copyright 2003-2005 Jeroen Vreeken (pe1rxq@amsat.org),
 *                2005-2006 Arno Verhoeven (pe1icq@amsat.org),
 *                2020      George Smart   (george@m1geo.com)
 *
 *	Published under the terms of the GNU General Public License
 *	version 2. Read the file COPYING for more details.
 *
 *  Originally built with Hi-Tech C Compiler V8.02PL1.
 *
 *  Build notes:
 *   Builds with Hi-Tech C V9.65 (from Microchip FTP Archive)
 *    "C:\Program Files (x86)\HI-TECH Software\PICC\PRO\9.65\bin\picc.exe" aprstracker.c -Os -16F648A
 *
 *   Builds with XC8 2.30 (see __XC defines for differences)
 *    "XC8 Global Options", and change "C Standard" from "C 99" to "C 90"
 *
 */

#include <pic.h>
#include "aprstracker.h"

unsigned int seconds = 0; 
unsigned char latitude[8];
unsigned char longitude[9];
unsigned char course[3] = { '0', '0', '0' };
unsigned char speed[3] = { '0', '0', '0' };
char alt5, alt4, alt3, alt2, alt1, alt0;

unsigned char proppath;
unsigned char btextinterval = 1;
bit RMC;	// set if we receive $GPRMC from connected GPS receiver
bit GGA;	// set if we receive $GPGGA from connected GPS receiver
bit VTG;	// set if we receive $GPVTG from connected GPS receiver
bit altitude_enable;
bit CSE_SPD_enable;
bit ser4800bd;	// If set then we communicate at 4800bd, if unset then at 9600bd.
bit crc;
bit stuffing;
bit tone1200;
bit sendbit;
bit beacon_now;
bit status_now;
bit ser_pol;	// FALSE if serial input is inverted
unsigned char crcl;
unsigned char crch;
unsigned char stuff_cnt;
unsigned char modem_cnt=0;
unsigned char modem_dly;
unsigned char modem_wave;


/* SmartBeaconing(tm) stuff. */
int sb_POSIT_rate;		// Computed SmartBeaconing posit rate (secs)
int sb_last_heading = -1;	// Heading at time of last posit
int sb_current_heading;		// Most recent heading parsed from GPS sentence
int sb_current_speed;		// Most recent speed parsed from GPS sentence (knots)
unsigned char turn_threshold;	// Computed turn threshold
signed int heading_change_since_beacon;


/* Convert three ascii digits to int
 * This is used instead of atoi() because it produces smaller code.
 */
int tdtoi(char *s) {
	int result = 0;
	char c = 0;
	while (c < 3) {
		result = 10*result + ((int)(s[c] & 0x0F));
		c++;
	}
	return(result);
}

char eep_read(char addr)
{
/* This ugly hack will give a compiler warning. Just ignore it.
 * There is no return-statement here, but in fact the W-register is returned.
 * This in-line assembly funtion produces much smaller (faster) code than
 * the EEPROM_READ() macro. The EEPROM_READ() macro will result in code
 * with more bank switching.
 */
#if defined(_16F88)
	TMR0IE = 0;
#else
	T0IE = 0;
#endif
#asm
	bsf 	3,5 	;Bank 1
	movwf	27  	;W contains the address to read
	bsf 	28,0	;Read EE
	movf	26,w	;Move data to W
	bcf 	3,5		;Bank 0
#endasm
#if defined(_16F88)
	TMR0IE = 1;
#else
	T0IE = 1;
#endif
}

#define eep_write(addr, val) EEPROM_WRITE(addr, val)

#define TCNT1200	(61)
#define TCNT2200	(32)

#define TWAIT1200	32
#define TWAIT2200	58


// interrupt void modem_isr(void)
// {
// #asm
	// movf	_modem_dly, w
	// movwf	_TMR0

	// incf	_modem_cnt		/* modem_cnt++ */
	// incf	_modem_wave		/* modem_wave++ */

	// movlw	HIGH get_wave		/* get portb value from jump table */
	// movwf	_PCLATH, f
	// movf	_modem_wave, w
	// andlw	0x1f			/* modem_wave &= 0x1f */
	// call	get_wave
// #if defined(_16F636)
        // movwf   _PORTC                  /* set new DA value */
// #else
        // movwf   _PORTB                  /* set new DA value */
// #endif
	// bcf	11,2			/* T0IF=0*/
// #endasm
	// return;
// #asm

// get_wave:
	// addwf	_PCL, f
	// retlw	8
	// retlw	9
	// retlw	10
	// retlw	12
	// retlw	13
	// retlw	14
	// retlw	14
	// retlw	15
	
	// retlw	15
	// retlw	15
	// retlw	14
	// retlw	14
	// retlw	13
	// retlw	12
	// retlw	10
	// retlw	9

	// retlw	8
	// retlw	6
	// retlw	5
	// retlw	3
	// retlw	2
	// retlw	1
	// retlw	1
	// retlw	0

	// retlw	0
	// retlw	0
	// retlw	1
	// retlw	1
	// retlw	2
	// retlw	3
	// retlw	5
	// retlw	6
// #endasm
// }


interrupt void modem_isr(void)
{
#asm
       Goto afterWave

get_wave:
#if defined(__XC)
       addwf  PCL, f
#else
       addwf  _PCL, f
#endif
       retlw  8
       retlw  9
       retlw  10
       retlw  12
       retlw  13
       retlw  14
       retlw  14
       retlw  15
       
       retlw  15
       retlw  15
       retlw  14
       retlw  14
       retlw  13
       retlw  12
       retlw  10
       retlw  9

       retlw  8
       retlw  6
       retlw  5
       retlw  3
       retlw  2
       retlw  1
       retlw  1
       retlw  0

       retlw  0
       retlw  0
       retlw  1
       retlw  1
       retlw  2
       retlw  3
       retlw  5
       retlw  6

afterWave:
       movf   _modem_dly, w
       movwf  _TMR0

       incf   _modem_cnt          /* modem_cnt++ */
       incf   _modem_wave         /* modem_wave++ */

       movlw  HIGH get_wave       /* get portb value from jump table */
       
#if defined(__XC)
       movwf  PCLATH, f
#else
       movwf  _PCLATH, f
#endif
       movf   _modem_wave, w
       andlw  0x1f                /* modem_wave &= 0x1f */
       call   get_wave
	   
#if defined(_16F636)
#if defined(__XC)
       movwf   PORTC                  /* set new DA value */
#else
       movwf   _PORTC                  /* set new DA value */
#endif
#else
#if defined(__XC)
       movwf   PORTB                  /* set new DA value */
#else
       movwf   _PORTB                  /* set new DA value */
#endif
#endif
       bcf    11,2                /* T0IF=0*/
#endasm
       return;
}



#define wait_bit() do \
{ \
	if (tone1200) \
		while (modem_cnt < TWAIT1200); \
	else \
		while (modem_cnt < TWAIT2200); \
	modem_cnt=0; \
} while (0)

#define send_zero() do \
{ \
	if (!tone1200) \
		modem_dly = 6 - TCNT1200; \
	else \
		modem_dly = 6 - TCNT2200; \
	tone1200=!tone1200; \
	stuff_cnt = 0; \
} while (0)


void ax25_putc(char c)
{
	char i;
	CLRWDT();
	for (i=0; i<8; i++) {
		sendbit = c & 1;
		if (crc) {
			#asm
				bcf	03, 0
				rrf	_crch, f
				rrf	_crcl, f
			#endasm
			if (CARRY != sendbit) {
				crch ^= 0x84;
				crcl ^= 0x08;
			}
		}
		c = c >> 1;
		wait_bit();
		if (sendbit) {
			stuff_cnt++;
			if (stuffing && stuff_cnt == 5) {
				wait_bit();
				send_zero();
			}
		} else
			send_zero();
	}
	
}

unsigned char w;
char getch(void)
{
	char c;
	char i;

	while ((!RXD && ser_pol) || (RXD && !ser_pol)) {
		CLRWDT();
		if (RXDET)
			LSTAT = 0;
		else
			LSTAT = 1;
	}
	c = 0;
	
	w = 40;
	#asm
	getch_wait1:
	decfsz	_w
	goto getch_wait1
	#endasm
	
	i = 8;
	do {
		CLRWDT();
		if (ser4800bd)
			w = 170;
		else
			w = 80;
		#asm
		getch_wait2:
		decfsz	_w
		goto 	getch_wait2
		#endasm
	
		c /= 2;
		if (ser_pol) {
			if (!RXD)
				c |= 128;
		} else {
			if (RXD)
				c |= 128;
		}
	} while (--i);

	if (ser4800bd)
		w = 170;
	else
		w = 80;
	#asm
	getch_wait3:
	decfsz	_w
	goto	getch_wait3
	#endasm

	return c;
}


void putch(char val)
{
	char i;

	TXD = ser_pol;	/* start bit */

	i = 10;		/* 11 = 2 stop bits */
	do {
		if (ser4800bd)
		w = 170;
	else
		w = 80;
		#asm
			putch_wait:
			decfsz	_w
			goto	putch_wait
		#endasm
		CLRWDT();
		if (val & 1)
			TXD = !ser_pol;
		else
			TXD = ser_pol;
		val = (val >> 1) | 0x80;
	} while (--i);
}


void ax25_send(void)
{
	unsigned char i, j;

	#if defined(_16F636)
		putch(XOFF);	/* send XOFF to control KF163 PLL before TX */
	#else
		if (!SW1)
			LSTAT = 1; /* LSTAT used as control for PLL before TX */
	#endif

	for (i=0; i<255; i++) {	/* 130ms delay to allow PLL to tune and lock */
		for (j=0; j<255; j++) {
			CLRWDT();
		}
	}	
	for (i=1; i; i++) {
		CLRWDT();
		if (!RXDET && SW1) {
			i=1;
			LSTAT = 1;
		}
		else if (SW1)
			LSTAT = 0;
		for (j=0; j<32; j++);
	}

	/* Do preprocessing as much as possible before we start ax25 stream */

	if (eep_read(EE_TRACKER_CONFIG) & 1)
		CSE_SPD_enable = 1;
 	else
 		CSE_SPD_enable = 0;
 	
	if ((eep_read(EE_TRACKER_CONFIG)&4) && (GGA))
		altitude_enable = 1;
 	else
 		altitude_enable = 0;
 
	/* Retrieve user configured interval if we have reached 0
	 * in previous cycle. Otherwise decrement.
	 */
	if (!(btextinterval--)) {
		btextinterval = eep_read(EE_BTEXTINTERVAL);
	}	/* We'll send beacontext this cycle if we have reached 0 */

	/* The APRS spec defines a maximum packet length.
	 * We need to check if the current APRS frame does not execeed that
	 * maximum. If too long then disable Course/Speed and Altitude info
	 * in the APRS frame. The actual length checks are done by atprogrammenu
	 * and the result of those checkes are stuffed in the MSB of the first
	 * and second characters in the beacon comment text strings.
	 */
	if (!btextinterval) {
		if (eep_read(EE_BEACONTEXT) & 128)
			CSE_SPD_enable = 0;
		if (eep_read(EE_BEACONTEXT+1) & 128)
			altitude_enable = 0;
	}
	
	/* Don't do proportional pathing if 255 */
	if (eep_read(EE_PROPPATH) == 255)
		proppath=3;
	else if ( proppath == eep_read(EE_PROPPATH) )
		proppath=0;
	else
		proppath++;
		
	PTT = 1;

	tone1200 = 1;
	PSA = 1;
	T0CS = 0;
	TMR0 = 6 - TCNT1200;

	GIE = 1;
	
	crcl = 0xff;
	crch = 0xff;
	crc = 0;

	/* we count txdelay in flags instead of ms */	
	for (i=eep_read(EE_TXDELAY); i; i--)
		ax25_putc(AX25_FLAG);
	ax25_putc(AX25_FLAG);

	crc = 1;
	stuffing = 1;
	stuff_cnt = 0;

 	/* destination, source */
	i=0;
	do {
		j=eep_read(i);
		i++;
		if ((i == 14) && (proppath == 0))
			j++;
		ax25_putc(j);
	} while (i != 14);
	
	/* relay */
	while ((i != 21) && proppath >= 1) {
		j=eep_read(i++);
		if ((i == 21) && (proppath == 1))
			j++;
		ax25_putc(j);
	}

	/* digis */
	if (proppath >= 2) {
		ax25_putc('W'<<1);
		ax25_putc('I'<<1);
		ax25_putc('D'<<1);
		ax25_putc('E'<<1);
	}
	if (proppath == 2) {
		ax25_putc('2'<<1);
		ax25_putc(' '<<1);
		ax25_putc(('1'<<1)+1);
	}
	if (proppath == 3) {
		ax25_putc('2'<<1);
		ax25_putc(' '<<1);
		ax25_putc(('2'<<1)+1);
	}
	if (proppath == 4) {
		ax25_putc('3'<<1);
		ax25_putc(' '<<1);
		ax25_putc(('3'<<1)+1);
	}
	if (proppath == 5) {
		ax25_putc('4'<<1);
		ax25_putc(' '<<1);
		ax25_putc(('4'<<1)+1);
	}

	/* ax25 control & PID */
	ax25_putc(AX25_UI);
	ax25_putc(AX25_PID_NL3);

	if (status_now) {
		ax25_putc('>');		/* APRS PID for status text */
		i=0;
		while (version_text[i])
			ax25_putc(version_text[i++]>>1);
		#if defined(_16F636) || defined(_16F648A) || defined(_16F88)
			i=EE_STATUSTEXT;
			while (j=eep_read(i++))
				ax25_putc(j);
		#endif
	} else {
		/* aprs packet identifier */
		ax25_putc(APRS_PID);
		
		/* latitude */
		for (i=0; i<8; i++)
			ax25_putc(latitude[i]);
		
		/* symbol table */
		ax25_putc(eep_read(EE_SYMBOLTABLE));
		
		/* longitude */
		for (i=0; i<9; i++)
			ax25_putc(longitude[i]);
		
		/* symbol id */	ax25_putc(eep_read(EE_SYMBOLID));
		
		/* course/speed */
		if (CSE_SPD_enable) {
			for (i=0; i<3; i++)
				ax25_putc(course[i]);
			ax25_putc('/');
			for (i=0; i<3; i++)
				ax25_putc(speed[i]);
		}

		/* Altitude */
		if (altitude_enable) {
			ax25_putc('/');
			ax25_putc('A');
			ax25_putc('=');
			ax25_putc(alt5+'0');
			ax25_putc(alt4+'0');
			ax25_putc(alt3+'0');
			ax25_putc(alt2+'0');
			ax25_putc(alt1+'0');
			ax25_putc(alt0+'0');
		}
		
		i=EE_BEACONTEXT;
		while ((j=eep_read(i++)) && !btextinterval) {
			/* Mask MSB as in the first 2 bytes it is used as a flag
			 * to indicate if the comment text is short enough to allow
			 * transmission of course/speed and altitude.
			 */
			ax25_putc(j&127);
		}
	}

	crc = 0;
	crcl ^= 0xff;
	crch ^= 0xff;
	ax25_putc(crcl);
	ax25_putc(crch);

	stuffing = 0;
	ax25_putc(AX25_FLAG);
	ax25_putc(AX25_FLAG);

	PTT = 0;
	GIE = 0;
	
	#if defined(_16F636)
		putch(XON);	/* send XON to control KF163 PLL after TX */
		putch(XON); /* and again in case it is not detected */
	#else
		if (!SW1)
			LSTAT = 0; /* LSTAT used as control for PLL after TX */
	#endif
}


void programmode(void)
{
	char c;

	putch('A');
	putch('T');
	putch(APRSTRACKER_VERSION);
	putch(APRSTRACKER_EEVERSION);
	putch(EEPROM_SIZE-1);

wait_programmode:
	c=getch();
	if (c=='W') {
		c=getch();
		eep_write(c, getch());
		while (WR) continue;	/* wait till EEPROM idle */
		putch(eep_read(c));
		goto wait_programmode;
	}
	if (c=='R') {
		putch(eep_read(getch()));
		goto wait_programmode;
	}
	/* a char other than W or R quits programmode */
}


void getcomma(void)
{
	while (getch() != ',');
}


void getlatlong(void)
{
	latitude[0] = getch();
	latitude[1] = getch();
	latitude[2] = getch();
	latitude[3] = getch();
	latitude[4] = getch();
	latitude[5] = getch();
	latitude[6] = getch();
	getcomma();			/* comma */
	latitude[7] = getch();		/* N/S */
	getch();			/* comma */
	longitude[0] = getch();
	longitude[1] = getch();
	longitude[2] = getch();
	longitude[3] = getch();
	longitude[4] = getch();
	longitude[5] = getch();
	longitude[6] = getch();
	longitude[7] = getch();
	getcomma();			/* comma */
	longitude[8] = getch();		/* E/W */
}


void getcourse(void)
{
	char c;
	course[1]='0';
	course[2]='0';
	c='0';
	do {
		course[0]=course[1];
		course[1]=course[2];
		course[2]=c;
		c=getch();
	} while (c!='.' && c!=',');
}


void getspeed(void)
{
	char c;
	speed[1] = '0';
	speed[2] = '0';
	c = '0';
	do {
		speed[0]=speed[1];
		speed[1]=speed[2];
		speed[2]=c;
		c=getch();
	} while (c!='.' && c!=',');
}


void gps_config(void)
{
	unsigned char i, j;
	if (!eep_read(EE_TRACKER_CONFIG) & 16) {
		// only if gps_config eeprom block is not used for status text
		i=EE_GPS_CONFIG;
		while (j=eep_read(i++))
			putch(j);
	}
}


void main(void)
{
	unsigned char c, d;
	unsigned char garbage_cnt;	// Counter used for baudrate switching
	unsigned char firsttime;
	unsigned int t, send_status_if_zero, status_decay=8;

	CLRWDT();
	INTCON = 0;	/* Disable all interrupts (also GIE) */
	
#if defined(_16F88)
	TMR0IE = 1;
#else
	T0IE = 1;
#endif
#if defined(_16F636)
	WDTCON=0x0A;	/* Set watchdog prescaler to 32 ms */
	CMCON0=0x07;	/* Turn off comparators */
	RAPU=0;		/* enable weak pull-ups/downs on port A */
	WDA=0xff;	/* weak pull-ups on port A */
	WPUDA=2;	/* enable weak pull-up/downs on RA1 only */
	INIT_PORTC;	/* port directions */
	SW1 = 1;	// not available
#else
	CMCON=0x07;	/* Turn off comparators */
#if defined(__XC)
       OPTION_REG &= ~(1UL << 7); // clear bit 7 (enable weak pullups on port B)
#else
       RBPU=0;
#endif
	INIT_PORTB;	/* port directions */
#endif
	INIT_PORTA;	/* port directions */
	
	LSTAT = 0; 	/* status led off */	
	LGPS = 1; 	/* gps led on (show we are alive) */
	PTT = 0;	/* ptt off */
	
	send_status_if_zero = 5;	// Send status after 5 seconds
	ser4800bd = 1;			// Start with assuming 4800bd
	ser_pol = 1;			// Start with assuming no inversion
	firsttime = 1;
	
	GPSEN = 1;	/* GPS on (active low) */
	GPSEN = 0;	/* GPS on (active low) */

	/* wait for serial data */
wait_ser:
	CLRWDT();
	TXD = !ser_pol;
	
	/* status packets are transmitted by the tracker using the STANDARD APRS
	 * DECAY DESIGN... that is, the packet is transmitted imediately, then
	 * 8 seconds later, then 16, then 32, then 64, then 2 mins then 4 mins,
	 * then 8 mins and then 16 mins and then 30 mins. Thus there is excellent
	 * probabiliy that the packet is delivered quickly, but then there is
	 * redundancy in case of collisions. */
	if (send_status_if_zero == 0) {
		status_now = 1;
		ax25_send();
		status_now = 0;
		send_status_if_zero = status_decay;
		status_decay *= 2;
		if (status_decay > 1800)
			status_decay = 1800;
	}

	garbage_cnt++;
	if (garbage_cnt == 0) {
		ser_pol = !ser_pol;
		if (ser_pol)
			ser4800bd = !ser4800bd;
	}
	c = getch();
	if (c != '$') {
		if (c == '!') {
			programmode();
		}
		goto wait_ser;
	}
	if (getch() != 'G')
		goto wait_ser;
	if (getch() != 'P')
		goto wait_ser;
	if (!SEND) 
		beacon_now = 1;
	garbage_cnt=0;	// If we get here then we are using the right baudrate. So reset garbage counter.
	if (firsttime) 
		gps_config();
	firsttime = 0;
	c = getch();
	if ((c == 'V') && ( !RMC )) {
		if (getch() == 'T') {
			if (getch() == 'G')
				goto gpvtg; 
		}
	}
	else if (c == 'G'){
		if (getch() == 'G') {
			if (getch() == 'A')
				goto gpgga;
		}
	}
	else if (c == 'R') {
		if (getch() == 'M') {
			if (getch() == 'C')
				goto gprmc;
			}
	}
	goto wait_ser;

gprmc:
	RMC = 1;
	LGPS = !LGPS;	/* toggle gps led */
	getch();			/* first comma */
	/* Because of timing the next 2 statements are placed between the first
	 * getch() and getcomma(). If these 2 statements are placed before the
	 * first getch(), then on the 16F636 the tracker may miss some incoming data.
	 */
	send_status_if_zero--;
	seconds++;
	getcomma();			/* timestamp + second comma */
	if (getch() == 'V' ) {
		/* Status is V=void.
		 * We try again in next cycle. */
		LGPS = 0;		/* gps led off */
		goto wait_ser;
	}
	/* If we get to this point then we have valid GPS data
	 * i.e. GPS is locked.
	 */
	LGPS = 1;		/* gps led on */
	getch();		/* comma */
	getlatlong();	/* latitude and longitude */
	getch();		/* comma */
	getspeed();
	getcomma();		/* comma */
	getcourse();

smartbeacon:
	/* Code to compute SmartBeaconing(tm) rates. (borrowed from Xastir)
	 *
	 * SmartBeaconing(tm) was invented by Steve Bragg (KA9MVA) and
	 * Tony Arnerich (KD7TA).
	 * Its main goal is to change the beacon rate based on speed
	 * and cornering.  It does speed-variant corner pegging and
	 * speed-variant posit rate.
	 *
	 * Several special SmartBeaconing(tm) parameters come into play here:
	 *
	 * sb_turn_min          Minimum degrees at which corner pegging will
	 *                      occur.  The next parameter affects this for
	 *                      lower speeds.
	 *
	 * sb_turn_slope        Fudget factor for making turns less sensitive at
	 *                      lower speeds.  No real units on this one.
	 *                      It ends up being non-linear over the speed
	 *                      range the way the original SmartBeaconing(tm)
	 *                      algorithm works.
	 *
	 * sb_turn_time         Dead-time before/after a corner peg beacon.
	 *                      Units are in seconds.
	 *
	 * sb_posit_fast        Fast posit rate, used if >= sb_high_speed_limit.
	 *                      Units are in seconds.
	 *
	 * sb_posit_slow        Slow posit rate, used if <= sb_low_speed_limit.
	 *                      Units are in minutes.
	 *
	 * sb_low_speed_limit   Low speed limit, units are in knots.
	 *
	 * sb_high_speed_limit  High speed limit, units are in knots.
	 */

	#define sb_turn_min			20
	#define sb_turn_slope		25
	#define sb_turn_time		10
	#define sb_posit_fast		eep_read(EE_SB_POSIT_FAST)
	#define sb_posit_slow 		eep_read(EE_SB_POSIT_SLOW)
	#define sb_low_speed_limit	eep_read(EE_SB_LOW_SPEED_LIMIT)
	#define sb_high_speed_limit	eep_read(EE_SB_HIGH_SPEED_LIMIT)

	// only do SmartBeaconing if EE_BEACONTIME == 0
	if (eep_read(EE_BEACONTIME) != 0) {
		if (seconds >= (eep_read(EE_BEACONTIME) * 60)) {
			beacon_now = 1;
		}
	} else {
		sb_current_speed = tdtoi(speed);	//convert the 3 ascii chars to a single integer that we can use for calculations
		sb_current_heading = tdtoi(course);	//convert the 3 ascii chars to a single integer that we can use for calculations

		if (sb_current_speed <= sb_low_speed_limit) {
			// Set to slow posit rate
			sb_POSIT_rate = (sb_posit_slow) * 60; // Convert to seconds
			/* Reset heading_change_since_beacon to avoid cyclic 
			   triggering of beacon_now when we suddenly drop below
			   the low speed limit. */
			heading_change_since_beacon = 0;
		}
		else { // We're moving faster than the low speed limit
		       // Start with turn_min degrees as the threshold
			turn_threshold = sb_turn_min;
			
			// Adjust rate according to speed
			if (sb_current_speed > sb_high_speed_limit) {
				// We're above the high limit
				sb_POSIT_rate = sb_posit_fast;
			}
			else {  // We're between the high/low limits.  Set a between rate
				sb_POSIT_rate = (sb_posit_fast * sb_high_speed_limit) / sb_current_speed;
				// Adjust turn threshold according to speed
				turn_threshold += (unsigned char)( (sb_turn_slope * 10) / sb_current_speed);
			}
			
			// Force a maximum turn threshold of 80 degrees 
			if (turn_threshold > 80)
			    turn_threshold = 80;
			
			// Check to see if we've written anything into
			// sb_last_heading variable yet.  If not, write the current
			// course into it.
			if (sb_last_heading == -1)
			    sb_last_heading = sb_current_heading;
			
			// Corner-pegging.  Note that we don't corner-peg if we're
			// below the low-speed threshold.
			heading_change_since_beacon = sb_current_heading - sb_last_heading;
			if (heading_change_since_beacon < 0)
				heading_change_since_beacon = sb_last_heading - sb_current_heading;
			if (heading_change_since_beacon > 180)
			    heading_change_since_beacon = 360 - heading_change_since_beacon;
		}

		if ( ( (heading_change_since_beacon > turn_threshold)
		          && (seconds > sb_turn_time) )
		       || (seconds > sb_POSIT_rate) ) {
		       	#if defined(DEBUG) 
		       		putch('B');  
		       		putch('3');
		       		putch('-');
		       	#endif 
			beacon_now = 1;                       // Force a posit right away
			sb_last_heading = sb_current_heading; // and save save current course.
		}	                                      // We'll use in the the calculation above
			                                      // to determine next posit.
	}

	if ( beacon_now ) {
		/* Send the packet */
		ax25_send();
		beacon_now = 0;
		seconds = 0;
	}
	goto wait_ser;


gpgga:
	GGA = 1;
	LGPS = !LGPS;		/* toggle gps led */
	getch();			/* first comma */
	/* Because of timing the next condition is placed between the first
	 * getch() and getcomma(). If it would be placed before the first getch()
	 * then on the 16F636 the tracker may miss some incoming data.
	 */
	if ( !RMC ) {	// Only update counters if we only depend on GGA strings 
		         	// I.e. the GPS does not provide RMC strings.
		send_status_if_zero--;
		seconds++;
	}
	getcomma();			/* timestamp + second comma */
	getlatlong();		/* latitude and longitude */
	getch();			/* comma */
	if (getch() == '0' || latitude[0]==',') {
		/* Oops, no valid data....
		 * We try again in next cycle
		 */
		LGPS = 0;		/* gps led off */
		goto wait_ser;
	}
	/*
	 * If we get to this point then we have valid GPS data
	 * i.e. GPS is locked.
	 */
	LGPS = 1;			/* gps led on */
	getch();			/* comma */
	getcomma();			/* skip nr. of sats */
	getcomma();			/* skip accuracy */
	alt0=0;
	alt1=0;
	alt2=0;
	alt3=0;
	alt4=0;
	alt5=0;
	d=getch();
	c=0;
	if (d!='-') do {
		alt5=alt4;
		alt4=alt3;
		alt3=alt2;
		alt2=alt1;
		alt1=alt0;
		t=(unsigned int)((d-'0')*33);
		while (t > 99) {
			t-=100;
			alt1++;
			if (alt1 > 9) {
				alt1-=10;
				alt2++;
				if (alt2 > 9) {
					alt2-=10;
					alt3++;
					if (alt3 > 9) {
						alt3-=10;
						alt4++;
						if (alt4 > 9)
							alt4-=10;
							alt5++;
					}
				}
			}
		}
		alt0=(unsigned char)(t/10)+c;
		if (alt0 > 9) {
			alt0-=10;
			alt1++;
			if (alt1 > 9) {
				alt1-=10;
				alt2++;
				if (alt2 > 9) {
					alt2-=10;
					alt3++;
					if (alt3 > 9) {
						alt3-=10;
						alt4++;
						if (alt4 > 9) {
							alt4-=10;
							alt5++;
						}
					}
				}
			}
		}
		c=(unsigned char)t;
		while (c > 9)
			c-=10;
		d=getch();
	} while (d!=',' && d!='.');
	goto smartbeacon;


gpvtg:
	VTG = 1;
	getch();			/* first comma */
	getcourse();
	while (getch() != 'M');		/* end of magnetic course */
	getch();			/* comma */
	getspeed();
	goto smartbeacon;

}
