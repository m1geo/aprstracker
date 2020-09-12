/*
 *	atprogrammenu.c
 *
 *      Copyright 2003-2005 Jeroen Vreeken (pe1rxq@amsat.org),
 *                2005-2006 Arno Verhoeven (pe1icq@amsat.org)
 *
 *	Published under the terms of the GNU General Public License
 *	version 2.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <time.h>
#include <termios.h>

#define memver 0x04
#define MaxCommentLength 43
#define cse_spd_length 7
#define alt_length 10

unsigned char ee_data[256];
         char tversion;		// software version as read from connected tracker
         char tmemver;		// memory map version as read from connected tracker
unsigned char eeprom_size;	// EEPROM size as read from connected tracker
         char sb_enabled;	// Flag indicating if SmartBeaconing is enabled
         char cse_spd_enabled;	// Flag indicating if Course/Speed is enabled
         char alt_enabled;	// Flag indicating if Altitude is enabled

enum {
	ATBAUD_4K8,
	ATBAUD_9K6
};

int baud = ATBAUD_9K6;

int read_eeprom(int fd_ser)
{
	char buffer[2];
	int i;

	for (i=0; i<=eeprom_size; i++) {
		buffer[0]='R';
		buffer[1]=i;
		write(fd_ser, buffer, 2);
		if (read(fd_ser, ee_data+i, 1)!=1)
			return -1;
	}
	
	return 0;
}

int write_eeprom(int fd_ser)
{
	unsigned char buffer[3];
	int i;

	for (i=0; i<=eeprom_size; i++) {
		buffer[0]='W';
		buffer[1]=i;
		buffer[2]=ee_data[i];
		write(fd_ser, buffer, 3);
		if (read(fd_ser, buffer, 1)!=1)  {
			return -1;
		}
		if (buffer[0]!=ee_data[i]) {
			return 1;
		}
	}
	
	return 0;
}

void leave_programming(int fd_ser)
{
	int i;
	/* Feed the tracker some characters to get into a known state.
	 * The $GP string is used by the tracker to autodetect baudrate.
	 */
	for (i=0; i<80; i++)
		write(fd_ser, "$GP", 3);
	/* Wait a while */
	for (i=0; i<8000; i++);
	/* and do it again */
	for (i=0; i<80; i++)
		write(fd_ser, "$GP", 3);
	
}

int enter_programming(int fd_ser)
{
	unsigned char buffer[5];
	fd_set	fdread;
	struct timeval timeout;
	char hdrlength;
	
	leave_programming(fd_ser);
	write(fd_ser, "!", 1);
	usleep(100000);
	FD_ZERO(&fdread);
	FD_SET(fd_ser, &fdread);
	timeout.tv_sec=2;
	timeout.tv_usec=0;
	select(fd_ser+1, &fdread, NULL, NULL, &timeout);
	if (!FD_ISSET(fd_ser, &fdread)) {
		return 1;
	}
	hdrlength=read(fd_ser, buffer, 5);
	if (hdrlength!=4 && hdrlength!=5) {
		return 1;
	}
	if (!strncmp(buffer, "AT", 2)) {
		tversion=buffer[2];
		tmemver=buffer[3];
		if (hdrlength==5)
			eeprom_size=buffer[4];
		else
			eeprom_size=127;

		printf("APRSTracker ver: %d memver: %d in programming mode\n", 
		    tversion, tmemver);
		if (tmemver != memver) {
			printf("Cannot handle this memver!\n");
			return 1;
		}
		return 0;
	}
	return 1;
}

int init_serial(char *serdev)
{
	int fd_ser;
	struct termios ser_tio;

	baud = !baud;

	if ((fd_ser=open(serdev, O_RDWR))<0)
		return -1;
	if (tcgetattr(fd_ser, &ser_tio)<0)
		return -1;
	cfmakeraw(&ser_tio);
	if (baud==ATBAUD_4K8) {
		printf("Trying 4800 baud.\n");
		ser_tio.c_cflag = B4800 | CS8 | CLOCAL | CREAD;
	} else {
		printf("Trying 9600 baud.\n");
		ser_tio.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	}
	if (tcsetattr(fd_ser, TCSANOW, &ser_tio)<0)
		return -1;
	return fd_ser;
}

void set_call(char *callsign, int offset, int end)
{
	int i;
	
	for (i=0; i<6; i++) {
		if (*callsign && *callsign!='-') {
			if (*callsign < '0' || *callsign > '9')
				*callsign&=~32;
			ee_data[offset+i]=(*callsign)<<1;
			callsign++;
		} else {
			ee_data[offset+i]=' '<<1;
		}
	}
	if (*callsign=='-')
		callsign++;
	if (*callsign)
		ee_data[offset+6]=*callsign<<1;
	else
		ee_data[offset+6]='0'<<1;
	if (end) {
		ee_data[offset+6]|=1;
	}
}

void set_status(char *status)
{
	if (!strlen(status)) {
		ee_data[128]=0;
	} else {
		status[127]=0;
		strcpy(ee_data+128, status);
	}
}

void set_comment(char *comment)
{
	if (!strlen(comment)) {
		ee_data[32]=0;
	} else {
		comment[39]=0;
		if (strlen(comment) > (MaxCommentLength-cse_spd_enabled))
			comment[0] |= 128;
		if (strlen(comment) > (MaxCommentLength-cse_spd_enabled-alt_enabled))
			comment[1] |= 128;
		strcpy(ee_data+32, comment);
	}
}


void set_gpsconfig(char *cfgstr)
{
	int i=0;
	int j;
	
	if (!strlen(cfgstr)) {
		ee_data[72]=0;
	} else {
		while (cfgstr[i]) {
		        if ((cfgstr[i] == '\\') && (cfgstr[i+1] == '\\')) {
		                cfgstr[i++]='\\';
		                j=i;
		                while (cfgstr[j]) {
		                        cfgstr[j]=cfgstr[j+1];
		                        j++;
				}
		        }
		        if ((cfgstr[i] == '\\') && (cfgstr[i+1] == 't')) {
		                cfgstr[i++]='\t';
		                j=i;
		                while (cfgstr[j]) {
		                        cfgstr[j]=cfgstr[j+1];
		                        j++;
				}
		        }
		        if ((cfgstr[i] == '\\') && (cfgstr[i+1] == 'r')) {
		                cfgstr[i++]='\r';
		                j=i;
		                while (cfgstr[j]) {
		                        cfgstr[j]=cfgstr[j+1];
		                        j++;
				}
		        }
		        if ((cfgstr[i] == '\\') && (cfgstr[i+1] == 'n')) {
		                cfgstr[i++]='\n';
		                j=i;
		                while (cfgstr[j]) {
		                        cfgstr[j]=cfgstr[j+1];
		                        j++;
				}
		        }
		        i++;
		}
		cfgstr[55]=0;
		strcpy(ee_data+72, cfgstr);
	}
}

void print_content(void)
{
	int i;
	printf("****************************************************************************\n");
	printf("Found APRS Tracker software version %d ", tversion);
	printf("in %d baud mode.\n", baud==ATBAUD_4K8 ? 4800 : 9600);
	printf("%d bytes EEPROM contents:\n", eeprom_size+1);
	printf("----------------------------------------------------------------------------\n");
	printf("[1] Source address: \t");
	for (i=0; i<6; i++) {
		printf("%c", ee_data[i+7]>>1);
	}
	printf("-%c\t", ee_data[13]>>1);
	if ((ee_data[14]>>1) == 'R')
		printf("[P] Using New-EU Paradigm Digi Path\n");
	if ((ee_data[14]>>1) == 'W')
		printf("[P] Using New-N Paradigm Digi Path\n");
	printf("[2] Proportional Pathing: ");
	if (ee_data[0x15] == 255)
		printf("Disabled\n");
	else
		printf("%d\n", ee_data[0x15]);
	printf("[3] Beacon interval:\t%d", ee_data[0x1c]);
	if (ee_data[0x1c] == 0) {
		sb_enabled = 1;
		printf(" (SmartBeaconing enabled)\n");
	} else {
		sb_enabled = 0;
		printf(" (SmartBeaconing disabled)\n");
	}
	printf("[4] TX Delay count:\t%d\n", ee_data[0x1d]);
	printf("[5] Symbol table:\t%c\n", ee_data[0x1e]);
	printf("[6] Symbol id:\t\t%c\n", ee_data[0x1f]);
	ee_data[0x20]&=127;	/* Mask MSB (which is used as a flag) */
	ee_data[0x21]&=127;	/* Mask MSB (which is used as a flag) */
	ee_data[0x47]=0;	/* Just in case... */
	printf("[7] Beacon comment:\t%s\n", ee_data+0x20);
	printf("[8] Comment interval:\t%d\n", ee_data[0x16]);
	if (eeprom_size == 255) {
		ee_data[0xff]=0;	/* Just in case... */
		printf("[9] Status text:\t%s\n", ee_data+0x80); 
	}
	ee_data[0x7f]=0;	/* Just in case... */
	printf("[C] GPS Config String:\t%s\n", ee_data+0x48);
	
	if (sb_enabled == 1) {
		printf("[D] Slow beacon rate:\t%d (minutes)\t", ee_data[0x18]);
		printf("[E] Slow speed threshold:  %d (knots)\n", ee_data[0x19]);
		printf("[F] Fast beacon rate:\t%d (seconds)\t", ee_data[0x17]);
		printf("[G] Fast speed threshold:  %d (knots)\n", ee_data[0x1a]);
	}
	
	/* Tracker configration bits
	 *
	 * 0: (  1) CSE_SPD_enable
	 * 1: (  2) PHGD_enable (not implemented yet)
	 * 2: (  4) altitude_enable
	 * 3: (  8) 
	 * 4: ( 16) 
	 * 5: ( 32) 
	 * 6: ( 64) 
	 * 7: (128)                                    
	 * #define EE_TRACKER_CONFIG       0x47
	 */
	if ( !(ee_data[0x1b] & 1) ) {
		cse_spd_enabled=0;
		printf("[H] Course/Speed are disabled\n");
	} else {
		cse_spd_enabled=cse_spd_length;
		printf("[H] Course/Speed are enabled\n");
	}
	if ( !(ee_data[0x1b] & 4) ) {
		alt_enabled=0;
		printf("[I] Altitude is disabled\n");
	} else {
		alt_enabled=alt_length;
		printf("[I] Altitude is enabled\n");
	}
}

void print_menu(void)
{
	printf("****************************************************************************\n");
	printf("Options:\n");
	printf("----------------------------------------------------------------------------\n");
	printf("0   Reread EEPROM\n");
	printf("1   Set Source address\n");
	printf("2   Set Proportional Pathing rate. A number greater than 5 disables option.\n");
	printf("3   Set Beacon interval\n");
	printf("4   Set TX delay (flags)\n");
	printf("5   Set Symbol table\n");
	printf("6   Set Symbol id\n");
	printf("7   Set Beacon comment text\n");
	printf("8   Set Beacon comment interval\n");
	if (eeprom_size == 255) {
		printf("9   Set status text\n");
	}
	printf("C   Set GPS Config String\n");
	printf("S   Enable SmartBeaconing\n");
	printf("D   Set Slow beacon rate (minutes)\n");
	printf("E   Set Slow speed threshold (knots)\n");
	printf("F   Set Fast beacon rate (seconds)\n");
	printf("G   Set Fast speed threshold (knots)\n");
	printf("H   Enable/Disable sending of Course and Speed\n");
	printf("I   Enable/Disable sending of Altitude (only if beacontext is short enough)\n");
	printf("P   Switch Paradigm\n");
	printf("N   Set GPS config string for enabling GGA & VTG on a Garmin GPS\n");
	printf("R   Set GPS config string for enabling VTG on a SiRF GPS\n");
	printf("Y   Set GPS config string for enabling VTG on a Sony GPS\n");
	printf("\nPress Enter to continue:");
	getchar();
}

int mainloop (int argc, char **argv)
{
	int fd_ser;
	char buffer[80];
	char value[800];
	char *serdevice="/dev/ttyS0";
	int i=0;

	if (argc>1)
		serdevice=argv[1];

	printf("\n");
	printf("****************************************************************************\n");
	printf("APRSTracker programmer\n");
	printf("Copyright 2003-2005 Jeroen Vreeken (pe1rxq@amsat.org),\n");
	printf("          2005-2006 Arno Verhoeven (pe1icq@amsat.org)\n");
	printf("memver: %d\n", memver);
	printf("****************************************************************************\n");
	printf("\n");

	printf("Initializing serial port...\n");
	if ((fd_ser=init_serial(serdevice))<0) {
		perror("Unable to initialize serial port");
		return 1;
	}

	printf("Connect APRSTracker to the serial port (%s or COM1) and press Enter\n", serdevice);
	read(0, buffer, 80);
	do { 
		printf("Putting APRSTracker in programming mode...\n");
		if (enter_programming(fd_ser)) {
			printf("No programmable APRSTracker found\n");
			printf("Reopening serial port...\n");
			close(fd_ser);
			if ((fd_ser=init_serial(serdevice))<0) {
				perror("Unable to initialize serial port");
			return 1;
			}
			if (i==10)
				goto bailout;
			printf("Retrying...\n");
		} else
			break;
		i++;
	} while (1);

	printf("Reading current eeprom contents...\n");
	if (read_eeprom(fd_ser)) {
		perror("Error reading eeprom contents");
		goto bailout;
	}

menu:
	print_content();
	printf("\n****************************************************************************\n");
	printf("Type your choice and press Enter (? for help): ");
	fflush(0);
	buffer[read(0, buffer, 79)]=0;;
	while (strlen(buffer) && 
	    (buffer[strlen(buffer)-1]=='\n' || buffer[strlen(buffer)-1]=='\r'))
	    	buffer[strlen(buffer)-1]=0;
	if ((buffer[0]!='?') && (buffer[0]!='0') &&
	    (buffer[0]!='h') && (buffer[0]!='H') &&
	    (buffer[0]!='i') && (buffer[0]!='I') &&
	    (buffer[0]!='p') && (buffer[0]!='P') &&
	    (buffer[0]!='n') && (buffer[0]!='N') &&
	    (buffer[0]!='r') && (buffer[0]!='R') &&
	    (buffer[0]!='s') && (buffer[0]!='S') &&
	    (buffer[0]!='y') && (buffer[0]!='Y')) {
		printf("New value: ");
		fflush(0);
		value[read(0, value, 129)]=0;
		while (strlen(value) && 
		    (value[strlen(value)-1]=='\n' || value[strlen(value)-1]=='\r'))
		    	value[strlen(value)-1]=0;
	}
	switch(buffer[0]) {
		case '?':
			print_menu();
			goto menu;
		case '0':
			printf("Rereading current eeprom contents...\n");
			if (read_eeprom(fd_ser)) {
				perror("Error reading eeprom contents\n");
				goto bailout;
			}
			break;
		case '1':
			printf("Setting source address...\n");
			set_call(value, 7, 0);
			break;
		case '2':
			if (atoi(value) > 5 ) {
				printf("Disabling Proportional Pathing...\n");
				ee_data[0x15]=255;
			} else {
				printf("Setting Proportional Pathing...\n");
				ee_data[0x15]=atoi(value);
			}
			break;
		case '3':
			printf("Setting beacon interval...\n");
			ee_data[0x1c]=atoi(value);
			break;
		case '4':
			printf("Setting txdelay...\n");
			ee_data[0x1d]=atoi(value);
			break;
		case '5':
			printf("Setting table...\n");
			ee_data[0x1e]=value[0];
			break;
		case '6':
			printf("Setting id...\n");
			ee_data[0x1f]=value[0];
			break;
		case '7':
			printf("Setting comment string...\n");
			set_comment(value);
			break;
		case '8':
			printf("Beacon comment interval...\n");
			ee_data[0x16]=atoi(value);
			break;
		case 'd':
		case 'D':
			printf("Setting slow beacon rate...\n");
			ee_data[0x18]=atoi(value);
			break;
		case 'e':
		case 'E':
			printf("Setting slow speed threshold...\n");
			ee_data[0x19]=atoi(value);
			break;
		case 'f':
		case 'F':
			printf("Setting fast beacon rate...\n");
			ee_data[0x17]=atoi(value);
			break;
		case 'g':
		case 'G':
			printf("Setting fast speed threshold...\n");
			ee_data[0x1a]=atoi(value);
			break;
		case 's':
		case 'S':
			printf("Enabling SmartBeaconing...\n");
			ee_data[0x1c]=0;
			break;
		case 'c':
		case 'C':
			printf("Setting GPS configuration string...\n");
			set_gpsconfig(value);
			break;
		case 'h':
		case 'H':
			if (cse_spd_enabled == 0) {
				printf("Enabling Course/Speed...\n");
				cse_spd_enabled=cse_spd_length;
			} else {
				printf("Disabling Course/Speed...\n");
				cse_spd_enabled=0;
			}
			ee_data[0x1b] ^= 1;
			break;
		case 'i':
		case 'I':
			if (alt_enabled == 0) {
				printf("Enabling Altitude...\n");
				alt_enabled=alt_length;
			} else {
				printf("Disabling Altitude...\n");
				alt_enabled=0;
			}
			ee_data[0x1b] ^= 4;
			break;
		case 'p':
		case 'P':
			if ((ee_data[14]>>1) == 'W') {
				printf("Switching to New-EU Paradigm...\n");
				strcpy(value, "RELAY");
				set_call(value, 14, 0);
			} else {
				printf("Switching to New-N Paradigm...\n");
				strcpy(value, "WIDE1 1");
				set_call(value, 14, 0);
			}
			break;
		case 'n':
		case 'N':
			printf("Setting GPS config string for Garmin...\n");
			strcpy(ee_data+72, "$PGRMO,,2*75\r\n$PGRMO,GPGGA,1*20\r\n$PGRMO,GPVTG,1*24\r\n");
			break;
		case 'r':
		case 'R':
			printf("Setting GPS config string for SiRF...\n");
			strcpy(ee_data+72, "$PSRF103,04,00,00,01*20\r\n$PSRF103,05,00,01,01*20\r\n");
			break;
		case 'y':
		case 'Y':
			printf("Setting GPS config string for Sony...\n");
			strcpy(ee_data+72, "@NC10000100\r\n");
			break;
		case '9':
			if (eeprom_size == 255) {
				printf("Setting status string...\n");
				set_status(value);
				break;
			}
		default:
			printf("Unknown option\n");
			goto menu;
	}
	printf("Writing eeprom contents...\n");
	/* Re-run the set_comment functions to make sure the MSB of
	 * the first 2 bytes are set correctly (comment too long flags)
	 */
	set_comment(ee_data+32);

	if (write_eeprom(fd_ser)) {
		perror("Error writing eeprom");
		goto bailout;
	}
	goto menu;

bailout:
	leave_programming(fd_ser);
	return 1;
}

int main (int argc, char **argv) {
	while (1) {
		mainloop(argc, argv);
		sleep(1);
	}
	return 1;
}
