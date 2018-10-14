
/* **********************************************************************
   * Front Door to Remote Access, Remote Access to Front Door.          *
   *                                                                    *
   * This program will scan the *.MSG format Echo Mail directories for  *
   * new mail and will toss them into the Remote Access message system. *
   * It then marks the messages in the *.MSG directories as having been *
   * transfered.                                                        *
   *                                                                    *
   * It will then scan the RA/QBBS message base for messages that have  *
   * been entered into it and will move them to the proper *.MSG echo   *
   * mail directory. Then the message is marked as having been moved.   *
   *                                                                    *
   * It knows about your directories and what message board they        *
   * should be tossed into by looking at the 'fdtora.cfg' file. In      *
   * this file, the directory name and then the board number that       *
   * the mail should be tagged to is found.                             *
   *                                                                    *
   *                          - - - -                                   *
   *                                                                    *
   * Command-line parameters:                                           *
   *                                                                    *
   * /delete    - Delete moved *.MSG messages                           *
   * /kill      - Delete moved RA/QBBS messages                         *
   * /sin       - Skip scanning for mail from *.MSG to RA/QBBS toss     *
   * /sout      - Skip scanning for mail from RA/QBBS to *.MSG toss     *
   * /diag      - Diagnostics                                           *
   *                                                                    *
   * If you want this program to delete the messages that are moved     *
   * from Front Doors Echo Mail areas to the Remote Access Message      *
   * Subsystem, offer /delete as an option to the command line.         *
   *                                                                    *
   * If you want messages that have been entered into the RA/QBBS data  *
   * base and then have been moved into the *.MSG echo mail directory   *
   * to be erased, offer /kill as an option to the command line.        *
   *                                                                    *
   * To skip inbound mail search, enter /sin. To skip outbound mail     *
   * search, enter /sout. These parameters are entered at the command   *
   * line.                                                              *
   *                                                                    *
   *                          - - - -                                   *
   *                                                                    *
   * To find the configuration file FDTORA.CFG, you can either be in    *
   * the default directory when the program is run or you may set an    *
   * environment variable which describes the directory of where the    *
   * file can be found.                                                 *
   *                                                                    *
   * In your AUTOEXEC.BAT file, perform the following set, changing     *
   * the path, of course, to the one where the FDTORA.CFG file may be   *
   * located. You may append a \ to the end of the path information yet *
   * the program will append one if one is not found.                   *
   *                                                                    *
   *            set FDTORA=C:\TOOLS                                     *
   *                                                                    *
   *                          - - - -                                   *
   *                                                                    *
   * Copywrite Fredric L. Rice, 1991, all rights reserved. Offered to   *
   * the Public Domain for free distribution.                           *
   *                                                                    *
   * FidoNet 1:102/901.0 - The Skeptic Tank                             *
   * November, 1991.                                                    *
   *                                                                    *
   ********************************************************************** */

/* **********************************************************************
   * Include the Turbo C prototype header files. Keep these includes in *
   * alphabetical order!                                                *
   *                                                                    *
   ********************************************************************** */

#include <alloc.h>
#include <conio.h>
#include <ctype.h>
#include <dir.h>
#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* **********************************************************************
   * Exit codes.                                                        *
   *                                                                    *
   * These are the errorlevel values we exit with.                      *
   *                                                                    *
   ********************************************************************** */

#define Exit_No_Mail_Tossed             0
#define Exit_Mail_Tossed                1
#define Error_Fail_Write                10
#define Error_Fail_Seek                 11
#define Error_Too_Many_Areas            12
#define Error_Directory_Bad             13
#define Error_Cant_Open_Message         14
#define Error_Cant_Read_Message         15
#define Error_Seek_Failed               16
#define Error_Cant_Find_Config          17
#define Error_Out_Of_Memory             18
#define Error_Cant_Open_Message_Base    19

/* **********************************************************************
   * How about any macros.                                              *
   *                                                                    *
   ********************************************************************** */

#define skipspace(s)    while(isspace(*s))  ++(s)
#define TRUE            1
#define FALSE           0
#define NIL             -1
#define BOOL            unsigned char
#define Max_Areas       250

/* **********************************************************************
   * What's our version?                                                *
   *                                                                    *
   ********************************************************************** */

#define The_Version     "2.02"

/* **********************************************************************
   * The message file format offered here is Fido format which has      *
   * been tested with OPUS and Dutchie. It represents the latest        *
   * format that I know about.                                          *
   *                                                                    *
   ********************************************************************** */

   struct fido_msg {
      char from[36];		/* Who the message is from		  */
      char to[36];		/* Who the message to to		  */
      char subject[72];		/* The subject of the message.		  */
      char date[20];            /* Message creation date/time             */
      int times;		/* Number of time the message was read	  */
      int destination_node;	/* Intended destination node		  */
      int originate_node;	/* The originator node of the message	  */
      int cost;			/* Cost to send this message		  */
      int originate_net;	/* The originator net of the message	  */
      int destination_net;	/* Intended destination net number	  */
      int destination_zone;	/* Intended zone for the message	  */
      int originate_zone;	/* The zone of the originating system	  */
      int destination_point;	/* Is there a point to destination?	  */
      int originate_point;	/* The point that originated the message  */
      unsigned reply;		/* Thread to previous reply		  */
      unsigned attribute;	/* Message type				  */
      unsigned upwards_reply;	/* Thread to next message reply		  */
   } message;			/* Something to point to this structure	  */

/* **********************************************************************
   * 'Attribute' bit definitions we will use                            *
   *                                                                    *
   ********************************************************************** */

#define Fido_Private            0x0001
#define Fido_Crash              0x0002
#define Fido_Read               0x0004
#define Fido_Sent               0x0008
#define Fido_File_Attach        0x0010
#define Fido_Forward            0x0020
#define Fido_Orphan             0x0040
#define Fido_Kill               0x0080
#define Fido_Local              0x0100
#define Fido_Hold               0x0200
#define Fido_Reserved1          0x0400
#define Fido_File_Request       0x0800
#define Fido_Ret_Rec_Req        0x1000
#define Fido_Ret_Rec            0x2000
#define Fido_Req_Audit_Trail    0x4000
#define Fido_Update_Req         0x8000

/* **********************************************************************
   * MSGINFO.BBS File structure                                         *
   *                                                                    *
   * Element 'total_on_board' is an array of words which indicates the  *
   * number of messages in each message area (board). If you wanted to  *
   * find out how many messages were on board 3, for instance, you      *
   * would access total_on_board[2].                                    *
   *                                                                    *
   ********************************************************************** */

    struct Message_Information {
	unsigned int lowest_message;
        unsigned int highest_message;
        unsigned int total_messages;
        unsigned int total_on_board[200];
    } msg_info;

/* **********************************************************************
   * MSGIDX.BBS File structure                                          *
   *                                                                    *
   ********************************************************************** */

    struct Message_Index {
	unsigned int message_number;
	unsigned char board_number;
    } msg_index;

/* **********************************************************************
   * MSGTOIDX.BBS File structure                                        *
   *                                                                    *
   * Since the data structure indicates a Pascal convention of storage  *
   * of the string length prior to the actual string, we allocate a     *
   * single byte called 'string_length' and use it to insert a NULL     *
   * into the element 'to_record[]' to make it conform to the C         *
   * convention of a NULL terminated string. We do this for each string *
   * element that happens to occur in the Remote Access message         *
   * subsystem.                                                         *
   *                                                                    *
   ********************************************************************** */

    struct Message_To_Index {
	unsigned char string_length;    /* Length of next field         */
        char to_record[35];             /* Null padded                  */
    } msg_to;

/* **********************************************************************
   * MSGHDR.BBS File strucrure                                          *
   *                                                                    *
   *  message_number is somewhat redundant yet offers some validation of*
   *     the Remote Access data files.                                  *
   *                                                                    *
   * start_block indicates an index into the message text file:         *
   *    MSGTXT.BBS. Each block in the text file is 255 bytes long and   *
   *    there is some additional overhead for the length of the string  *
   *    that describes the messages. To find the starting point of the  *
   *    text of this message, then, you would multiply the size of the  *
   *    message text structure by the starting block number offered     *
   *    here, and you yield a byte offset that may be used to seek into *
   *    the text file.                                                  *
   *                                                                    *
   * message_attribute is defined in the defines.                       *
   *                                                                    *
   * network_attribute is also defined with some defines.               *
   *                                                                    *
   * board is somewhat redundant also yet could be used to validate the *
   *    Remote Access data files when used with the message number and  *
   *    the message index file.                                         *
   *                                                                    *
   * date, time, who_to, who_from, subject - These are all not          *
   *    specifically NULL terminated though they may be. Reguardless,   *
   *    the bytes prior to them indicate the strings length.            *
   *                                                                    *
   ********************************************************************** */

    struct Message_Header {
        unsigned int message_number;
        unsigned int previous_reply;
        unsigned int next_reply;
        unsigned int times_read;
        unsigned int start_block;
        unsigned int number_blocks;
        unsigned int destination_network;
        unsigned int destination_node;
        unsigned int originating_network;
        unsigned int originating_node;
        unsigned char destination_zone;
        unsigned char origination_zone;
        unsigned int cost;
        unsigned char message_attribute;
        unsigned char network_attribute;
        unsigned char board;
	unsigned char ptlength;         /* Hard-coded to 5              */
	char post_time[5];              /* hh:mm                        */
	unsigned char pdlength;         /* Hard-coded to 8              */
	char post_date[8];              /* mm-dd-yy                     */
	unsigned char wtlength;         /* Length of next field         */
        char who_to[35];                /* Null padded                  */
	unsigned char wflength;         /* Length of next field         */
        char who_from[35];              /* Null padded                  */
	unsigned char slength;          /* Length of next field         */
        char subject[72];               /* Null padded                  */
    } msg_hdr;

/* **********************************************************************
   * Message Attribute defines                                          *
   *                                                                    *
   ********************************************************************** */

#define MA_Deleted                              0x01
#define MA_Unmoved_Outbound_Net_Message         0x02
#define MA_Netmail_Message                      0x04
#define MA_Private                              0x08
#define MA_Received                             0x10
#define MA_Unmoved_Outbound_Echo_Message        0x20
#define MA_Local                                0x40
#define MA_Reserved                             0x80

/* **********************************************************************
   * Network Attribute defines                                          *
   *                                                                    *
   ********************************************************************** */

#define NA_Kill_Sent            0x01
#define NA_Sent_OK              0x02
#define NA_File_Attach          0x04
#define NA_Crash_Mail           0x08
#define NA_Request_Receipt      0x10
#define NA_Audit_Request        0x20
#define NA_Is_Return_Receipt    0x40
#define NA_Reserved             0x80

/* **********************************************************************
   * MSGTXT.BBS File structure                                          *
   *                                                                    *
   * The text of the messages is offered with the first byte indicating *
   * the length of the block that's actually used. It could be that all *
   * of the 255 byte block is used for the message and that the next    *
   * blocks will likewise also be fully used. Good going, Remote Access *
   * guys, this saves a LOT of unused disk space.                       *
   *                                                                    *
   * Remote access places the ^A Kludge lines at the top of the text    *
   * file here. There is a product ID (Kludge PID:), and a message ID,  *
   * (Kludge MSGID:). Both of these Kludge lines are terminated with a  *
   * carriage return.                                                   *
   *                                                                    *
   * The lines of the text occures next. Each line is terminated with a *
   * carriage return.                                                   *
   *                                                                    *
   * A 'tear line' comes after all of the text. This is the ---         *
   * characters which indicates that what follows is a human-readable   *
   * identification which usually offers the origination text line of   *
   * the originating system. The tear line can also be used to indicate *
   * that anything which follows may be discarded as unimportant. For   *
   * most, if not all, of the FidoNet world, information after the tear *
   * line is never discarded. This tear line is terminated with both a  *
   * carriage return _AND_ a line feed.                                 *
   *                                                                    *
   * The originating systems origin line comes next (if there is to be  *
   * an origin line. Network and local mail will probably not have an   *
   * origin line). It is terminated with a carriage return.             *
   *                                                                    *
   * The rest of the 255 byte block (if there is more) is all NULLs.    *
   *                                                                    *
   * If the person who entered the message allowed the entry to         *
   * automatically word wrap, then rather than there being a carriage   *
   * return, there will be a soft carriage return. This is, instead of  *
   * a 0x0d, a 0x8d.                                                    *
   *                                                                    *
   ********************************************************************** */

    struct Message_Text {
	unsigned char trlength;         /* Length of next field         */
        unsigned char text_record[255]; /* CR delimited, NULL padded    */
    } msg_text;

/* **********************************************************************
   * Define data storage needed                                         *
   *                                                                    *
   ********************************************************************** */

    static char directory_count;
    static struct ffblk file_block;
    static char *ra_directory;
    static FILE *MSGINFO, *MSGIDX, *MSGTOIDX, *MSGHDR, *MSGTXT;
    static unsigned int ra_lowest, ra_highest, ra_total;
    static unsigned int block_count;
    static BOOL want_delete, want_kill;
    static BOOL skip_inbound, skip_outbound;
    static BOOL rescan_person;
    static BOOL want_diag;
    static char rescan_name[201];
    static short msg_tossed_count, qbbs_tossed_count;

    static char *num_to_month[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    } ;

    static char *num_to_day[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    } ;

/* **********************************************************************
   * Set the offered string to uppercase.                               *
   *                                                                    *
   ********************************************************************** */

static void ucase(char *this_record)
{
   while (*this_record) {
      if (*this_record > 0x60 && *this_record < 0x7b) {
         *this_record = *this_record - 32;
      }

      this_record++;
   }
}

/* **********************************************************************
   * day from 1-31, month from 1-12, year from 80                       *
   * Returns 0 for Sunday, etc.                                         *
   *                                                                    *
   *    This function was not written by Fredric Rice. It was taken     *
   *    from the MSGQ150S.LSH archive which is an on-line full          *
   *    screen editor.                                                  *
   *                                                                    *
   ********************************************************************** */

static int zeller(int day, int month, int year)
{
    int age;

    age = (year < 80) ? 20 : 19;

    if ((month -= 2) <= 0) {
        month += 12;
        year--;
    }

    return(((26 * month-2) / 10 +day +year +year / 4 + age / 4 - 2 * age) % 7);
}

/* **********************************************************************
   * Define a data structure for the area information.                  *
   *                                                                    *
   ********************************************************************** */

    static struct Area_Information {
        char *directory;
        unsigned char tag;
    } Areas[Max_Areas];

/* **********************************************************************
   * Find the highest message number and return it.                     *
   *                                                                    *
   ********************************************************************** */

static short find_highest_message_number(char *directory)
{
    char result;
    short highest_message_number = 0;
    char directory_search[100];
    struct ffblk file_block;

    (void)strcpy(directory_search, directory);

    if (directory_search[strlen(directory_search) - 1] != '\\')
	(void)strcat(directory_search, "\\");

    (void)strcat(directory_search, "*.msg");

    result = findfirst(directory_search, &file_block, 0x16);

/*
    If there is at least one, we'll find it
*/

    if (! result) {
        if (atoi(file_block.ff_name) > highest_message_number) {
            highest_message_number = atoi(file_block.ff_name);
        }
    }

/*
    Keep searching through it all and find the highest
*/

    while (! result) {
        result = findnext(&file_block);
        if (! result) {
            if (atoi(file_block.ff_name) > highest_message_number) {
                highest_message_number = atoi(file_block.ff_name);
            }
        }
    }

/*
    Return what was found. It is up to the calling function
    to add one to this value for the new message if one if
    to be created.
*/

    return(highest_message_number);
}

/* **********************************************************************
   * The month is offered as text. Return it as the month number.       *
   *                                                                    *
   ********************************************************************** */

static char to_month(char *this_one)
{
    if (! strncmp(this_one, "Jan", 3)) return 1;
    if (! strncmp(this_one, "Feb", 3)) return 2;
    if (! strncmp(this_one, "Mar", 3)) return 3;
    if (! strncmp(this_one, "Apr", 3)) return 4;
    if (! strncmp(this_one, "May", 3)) return 5;
    if (! strncmp(this_one, "Jun", 3)) return 6;
    if (! strncmp(this_one, "Jul", 3)) return 7;
    if (! strncmp(this_one, "Aug", 3)) return 8;
    if (! strncmp(this_one, "Sep", 3)) return 9;
    if (! strncmp(this_one, "Oct", 3)) return 10;
    if (! strncmp(this_one, "Nov", 3)) return 11;
    if (! strncmp(this_one, "Dec", 3)) return 12;

    (void)printf("Warning: Unable to determine what month this is: %s\n",
        this_one);

    return(0);
}

/* **********************************************************************
   * o Modify the FidoNet message.                                      *
   *                                                                    *
   * o Append information to the Remote Access message data base.       *
   *                                                                    *
   ********************************************************************** */

static void toss_message(unsigned char this_directory,
    FILE *msg_file,
    unsigned long check_address)
{
    unsigned int result;
    unsigned char char_count;
    unsigned char byte;
    char *point;
    char fido_time[20], fido_date[20];
    unsigned int message_block_count;

    message.cost = 43;        /* Mark as tossed */
    rewind(msg_file);

    if (fwrite(&message, sizeof(struct fido_msg), 1, msg_file) != 1) {
        (void)printf("I was unable to write message file!\n");
        exit(Error_Fail_Write);
    }

/*
    Seek to the start of the message text and Kludge lines,
    skipping over the message header information what we already have.
*/

    if (fseek(msg_file, (long)check_address, SEEK_SET) != 0) {
        (void)printf("I was unable to reseek a FidoNet message file!\n");
        exit(Error_Fail_Seek);
    }

/*
    Update the Remote Access File MSGINFO.BBS
*/

    msg_info.highest_message++;
    msg_info.total_messages++;

    if ((Areas[this_directory].tag - 1) >= Max_Areas) {
        (void)printf("SYSTEM EXCEPTION at point 1 occured!\n");
        exit(Error_Too_Many_Areas);
    }

    msg_info.total_on_board[Areas[this_directory].tag - 1]++;
    rewind(MSGINFO);

    (void)printf("   tossing to board %d\r", Areas[this_directory].tag);
    msg_tossed_count++;

    result =
        fwrite(&msg_info, sizeof(struct Message_Information), 1, MSGINFO);

    if (result != 1) {
        (void)printf("I was unable to write file: MSGINFO.BBS!\n");
        (void)fcloseall();
        exit(Error_Fail_Write);
    }

/*
    Append to the Remote Access file MSGIDX
*/

    msg_index.message_number = msg_info.highest_message;
    msg_index.board_number = Areas[this_directory].tag;
    
    result =
        fwrite(&msg_index, sizeof(struct Message_Index), 1, MSGIDX);

    if (result != 1) {
        (void)printf("I was unable to write file: MSGIDX.BBS!\n");
        (void)fcloseall();
        exit(Error_Fail_Write);
    }

/*
    Append to the Remote Access file MSGTOIDX
*/

    msg_to.string_length = (unsigned char)strlen(message.to);
    (void)strncpy(msg_to.to_record, message.to, 35);

    result =
        fwrite(&msg_to, sizeof(struct Message_To_Index), 1, MSGTOIDX);

    if (result != 1) {
        (void)printf("I was unable to write file: MSGTOIDX.BBS!\n");
        (void)fcloseall();
        exit(Error_Fail_Write);
    }

/*
    Append to the Remote Access file MSGTXT.

    Here we compute the number of 255 byte blocks in the message
    and the information is retained to append information to the
    header file.
*/

    char_count = 0;
    message_block_count = 0;

    while (! feof(msg_file)) {
        byte = (unsigned char)fgetc(msg_file);
        if (! feof(msg_file)) {
	    msg_text.text_record[char_count] = byte;
            if (char_count == 254) {
                msg_text.trlength = 255;
                char_count = 0;

                result =
                    fwrite(&msg_text, sizeof(struct Message_Text), 1, MSGTXT);

                if (result != 1) {
                    (void)printf("I was unable to write file: MSGTXT.BBS!\n");
                    (void)fcloseall();
                    exit(Error_Fail_Write);
                }

                message_block_count++;
            }
            else {
                char_count++;
            }
        }
    }

/*
    Find out what the length of the last message block is and store it
    away. Then append the remaining text field with NULLs.

    If the character count is 0, then it could be an empty message or
    it could have ended exactly on the 255'th byte boundry.
*/

    if (char_count != 0) {
        msg_text.trlength = char_count + 1;

        for (; char_count < 255; char_count++)
            msg_text.text_record[char_count] = 0x00;

        result =
            fwrite(&msg_text, sizeof(struct Message_Text), 1, MSGTXT);

        if (result != 1) {
            (void)printf("I was unable to write file: MSGTXT.BBS!\n");
            (void)fcloseall();
            exit(Error_Fail_Write);
        }

        message_block_count++;
    }

/*
    Append to the Remote Access file MSGHDR
*/

    point = message.date;
    point += 11;
    (void)strncpy(fido_time, point, 5);

    point = message.date;
    (void)sprintf(fido_date, "%02d-%02d-%02d",
        to_month(point + 3), atoi(point), atoi(point + 7));

    msg_hdr.message_number = msg_info.highest_message;
    msg_hdr.previous_reply = message.reply;
    msg_hdr.next_reply = message.upwards_reply;
    msg_hdr.times_read = 0;
    msg_hdr.start_block = block_count;
    msg_hdr.number_blocks = message_block_count;
    msg_hdr.destination_network = (unsigned int)message.destination_net;
    msg_hdr.destination_node = (unsigned int)message.destination_node;
    msg_hdr.originating_network = (unsigned int)message.originate_net;
    msg_hdr.originating_node = (unsigned int)message.originate_node;
    msg_hdr.destination_zone = (unsigned char)message.destination_zone;
    msg_hdr.origination_zone = (unsigned char)message.originate_zone;
    msg_hdr.cost = 0;

/*
    Rework the differences in the bit patterns for the
    message attribute and the network attribute. Take some
    care here to make sure that they are correct!
    message.attribute;
*/

    msg_hdr.message_attribute = (unsigned char)0;
    msg_hdr.network_attribute = (unsigned char)0;

    if ((message.attribute & Fido_Private) > 0)
	msg_hdr.message_attribute += MA_Private;

    if ((message.attribute & Fido_Crash) > 0)
        msg_hdr.network_attribute += NA_Crash_Mail;

    if ((message.attribute & Fido_Read) > 0)
        msg_hdr.message_attribute += MA_Received;

    if ((message.attribute & Fido_Sent) > 0)
        msg_hdr.network_attribute += NA_Sent_OK;

    if ((message.attribute & Fido_File_Attach) > 0)
        msg_hdr.network_attribute += NA_File_Attach;

/* Fido_Forward   Fido_Orphan */

    if ((message.attribute & Fido_Kill) > 0)
        msg_hdr.network_attribute += NA_Kill_Sent;

    if ((message.attribute & Fido_Local) > 0)
        msg_hdr.message_attribute += MA_Local;

/* Fido_Hold   Fido_Reserved1   Fido_Req */

    if ((message.attribute & Fido_Ret_Rec_Req) > 0)
        msg_hdr.network_attribute += NA_Request_Receipt;

    if ((message.attribute & Fido_Ret_Rec) > 0)
        msg_hdr.network_attribute += NA_Is_Return_Receipt;

    if ((message.attribute & Fido_Req_Audit_Trail) > 0)
        msg_hdr.network_attribute += NA_Audit_Request;

/* Fido_Update_Req */

/*
    We leave the following bits cleared because they're being moved
    from *.MSG format over to the RA/QBBS format

    MA_Deleted
    MA_Unmoved_Outbound_Net_Message
    MA_Netmail_Message
    MA_Unmoved_Outbound_Echo_Message
    MA_Reserved
    NA_Reserved
*/

    msg_hdr.board = Areas[this_directory].tag;
    msg_hdr.ptlength = 5;
    (void)strncpy(msg_hdr.post_time, fido_time, 5);
    msg_hdr.pdlength = 8;
    (void)strncpy(msg_hdr.post_date, fido_date, 8);
    msg_hdr.wtlength = (unsigned char)strlen(message.to);
    (void)strncpy(msg_hdr.who_to, message.to, 35);
    msg_hdr.wflength = (unsigned char)strlen(message.from);
    (void)strncpy(msg_hdr.who_from, message.from, 35);
    msg_hdr.slength = (unsigned char)strlen(message.subject);
    (void)strncpy(msg_hdr.subject, message.subject, 72);

    result =
        fwrite(&msg_hdr, sizeof(struct Message_Header), 1, MSGHDR);

    if (result != 1) {
        (void)printf("I was unable to write file: MSGHDR.BBS!\n");
        (void)fcloseall();
        exit(Error_Fail_Write);
    }

/*
    Make sure that we keep track of the block count!
*/

    block_count += msg_hdr.number_blocks;
}

/* **********************************************************************
   * Open up the message file being pointed to by the file control      *
   * block and see if it has already been marked as having been tossed  *
   * into the Remote Access data base.                                  *
   *                                                                    *
   ********************************************************************** */

static void examine_message(unsigned char this_directory)
{
    char file_name[81];
    FILE *msg_file;
    unsigned long check_address;

/*
    The first thing to do is validate the value that was
    passed to us. If there is a problem it indicates that
    a programming error exists in the code!
*/

    if (this_directory >= Max_Areas) {
        (void)printf("SYSTEM EXCEPTION at point 2 occured!\n");
        exit(Error_Directory_Bad);
    }

    if (Areas[this_directory].directory == (char *)NULL) {
        (void)printf("SYSTEM EXCEPTION at point 3 occured!\n");
        exit(Error_Directory_Bad);
    }

/*
    Build a file name out of the path and the name that's
    being passed in the file block data structure.
*/

    (void)sprintf(file_name, "%s%s",
        Areas[this_directory].directory,
        file_block.ff_name);

/*
    Open the message file and read the header information into
    the data structure set aside for that.
*/

    if ((msg_file = fopen(file_name, "r+b")) == (FILE *)NULL) {
        (void)printf("I was unable to open message file: %s!\n", file_name);
        exit(Error_Cant_Open_Message);
    }

    if (fread(&message, sizeof(struct fido_msg), 1, msg_file) != 1) {
        (void)printf("I was unable to read message file: %s!\n", file_name);
        exit(Error_Cant_Read_Message);
    }

/*
    Keep track of where in the file the text starts.
*/

    check_address = (unsigned long)ftell(msg_file);
    if (check_address == (unsigned long) -1) {
        (void)printf("I was unable to process file: %s!\n", file_name);
        exit(Error_Seek_Failed);
    }

    (void)printf("   Looking in %s", file_name);
    clreol();

/*
    If the message appears to have not been tossed already,
    toss it. Else we ignore it. If the message is from Qmail,
    then we also ignore.
*/

    if (message.cost < 42 && strnicmp(message.from, "qmail", 5)) {
        toss_message(this_directory, msg_file, check_address);
    }
    else {
	(void)printf("\r");
    }

    (void)fclose(msg_file);

/*
    If the /delete option was offered, erase the message file
*/

    if (want_delete)
        (void)unlink(file_name);
}

/* **********************************************************************
   * Now move the RA/QBBS unmoved echo mail messages to the *.MSG       *
   * format of the correct directory.                                   *
   *                                                                    *
   * Even messages that have been marked as deleted get moved! So you   *
   * may want to add another test in this function if you dont like!    *
   *                                                                    *
   ********************************************************************** */

static void process_outbound(BOOL rescan_person, char *who)
{
    int high_count, block_count, loop;
    int last_high_directory;
    FILE *msg_out;
    char file_name[80];
    long text_seek, seek_update, msg_count;
    char hold[5];
    int the_day, the_month, the_year;
    BOOL do_process, from_rescan;
    char hold_name[40];
    BOOL was_network = FALSE;

    high_count = NIL;
    last_high_directory = NIL;
    msg_count = 0L;
    from_rescan = FALSE;

    if (rescan_person) {
        (void)printf("\nRe-scanning for mail from %s\n\n", who);
    }

    while (! feof(MSGHDR)) {
        if (fread(&msg_hdr, sizeof(struct Message_Header), 1, MSGHDR) == 1) {

            do_process =
                ((msg_hdr.message_attribute &
                    MA_Unmoved_Outbound_Echo_Message) > 0);

            if (! do_process) {
                do_process =
                    ((msg_hdr.message_attribute &
                        MA_Netmail_Message) > 0);

                if (do_process) {
                    was_network = TRUE;
                }
            }

            if (Areas[msg_hdr.board].tag != 0) {
                if (want_diag) {
                    (void)printf("Looking at folder %d %s %s %x\n",
                        msg_hdr.board,
                        do_process ? "YES" : "NO",
                        was_network ? "NET" : "ECHO",
                        msg_hdr.message_attribute);
                }
            }

            from_rescan = FALSE;

	    if (! do_process) {
		(void)strcpy(hold_name, msg_hdr.who_from);
		ucase(hold_name);

		if (! strcmp(who, hold_name)) {
		    do_process = TRUE;
		    from_rescan = TRUE;
		}
	    }

            if (do_process) {
                if (Areas[msg_hdr.board].tag == 0) {
                    do_process = FALSE;

                    if (want_diag) {
                        (void)printf("DIAG: Message for folder %d ignored\n",
                            msg_hdr.board);
                    }
                }
            }

            if (do_process) {

                seek_update =
                    (long)msg_count * (long)sizeof(struct Message_Header);

/*
    If the directory we are to look at is different than
    any previous directory we looked at, set the high_count
    value to NIL so that we will ask for the new directorys
    highest message number.

    If the directories are the same, then we will skip
    the asking for the high_count so that we save time by
    not doing something that doesn't need doing.
*/

                if (last_high_directory != Areas[msg_hdr.board].tag) {
                    high_count = NIL;
                }

                if (high_count == NIL) {
                    high_count = find_highest_message_number
                        (Areas[msg_hdr.board].directory);

                    last_high_directory = Areas[msg_hdr.board].tag;
                }

/*
    Make sure that we incriment the high message number so
    that the newly created message will not overwrite an
    existing message. Then create a message file name out
    of the number.
*/

                high_count++;

                (void)sprintf(file_name, "%s%d.MSG",
                    Areas[msg_hdr.board].directory,
                    high_count);

                (void)printf("Folder %2d [%s] %s -> %s",
                    msg_hdr.board, Areas[msg_hdr.board].directory,
                    was_network ? "net " : "echo",
                    file_name);

                if (from_rescan) {
                    (void)printf(" Rescan");
                }

                (void)printf("\n");

                (void)strncpy(message.from, msg_hdr.who_from, msg_hdr.wflength);
                message.from[msg_hdr.wflength] = (char)NULL;
                (void)strncpy(message.to, msg_hdr.who_to, msg_hdr.wtlength);
                message.to[msg_hdr.wtlength] = (char)NULL;
                (void)strncpy(message.subject, msg_hdr.subject, msg_hdr.slength);
                message.subject[msg_hdr.slength] = (char)NULL;

/*
    Convert the date and time strings.
*/

                hold[0] = msg_hdr.post_date[3];
                hold[1] = msg_hdr.post_date[4];
                hold[2] = (char)NULL;
                the_day = atoi(hold);

                hold[0] = msg_hdr.post_date[0];
                hold[1] = msg_hdr.post_date[1];
                the_month = atoi(hold);

                hold[0] = msg_hdr.post_date[6];
                hold[1] = msg_hdr.post_date[7];
                the_year = atoi(hold);

                (void)sprintf(message.date, "%s %02d %s %02d %c%c%c%c%c",
                    num_to_day[zeller(the_day, the_month, the_year)],
                    the_day,
                    num_to_month[the_month - 1],
                    the_year,
                    msg_hdr.post_time[0],
                    msg_hdr.post_time[1],
                    msg_hdr.post_time[2],
                    msg_hdr.post_time[3],
                    msg_hdr.post_time[4]);

                message.times = 0;
                message.destination_node = 0;
                message.originate_node = 0;
                message.cost = 43;        /* Mark as tossed! */
                message.originate_net = 0;
                message.destination_net = 0;
                message.destination_zone = 0;
                message.originate_zone = 0;
                message.destination_point = 0;
                message.originate_point = 0;
                message.reply = 0;
                message.attribute = Fido_Local;
                message.upwards_reply = 0;

/*
    Make sure that we can create the new message now and
    write the message header into the file.
*/

                if ((msg_out = fopen(file_name, "wb")) == (FILE *)NULL) {
                    (void)printf("I was unable to create message file!\n");
                    return;
                }

                if (fwrite(&message, sizeof(struct fido_msg), 1, msg_out) != 1) {
                    (void)printf("I was unable to write message file!\n");
                    (void)fclose(msg_out);
                    (void)printf("Message not tossed to *.MSG format!\n");
                    return;
                }

/*
    Copy the message text into the msg file. No evaluation
    is performed and nothing is extrected.
*/

                text_seek =
                    (long)((long)(msg_hdr.start_block) * (long)sizeof(struct Message_Text));

                if (fseek(MSGTXT, (long)text_seek, SEEK_SET) > 0) {
                    (void)printf("Unable to seek to: %ld!\n", text_seek);
                    (void)fclose(msg_out);
                    (void)printf("Message not tossed to *.MSG format!\n");
                    return;
                }

                for (block_count = 0;
                    block_count < msg_hdr.number_blocks;
                        block_count++) {

		    if (fread(&msg_text, sizeof(struct Message_Text), 1, MSGTXT) == 1) {
                        for (loop = 0; loop < msg_text.trlength; loop++) {
                            (void)fputc(msg_text.text_record[loop], msg_out);
                        }
                    }
                    else {
                        (void)printf("Copy of message incompleate!\n");
                        (void)printf("Unable to read all message blocks!\n");
                        (void)fclose(msg_out);
                        (void)printf("Part of the message has been tossed.\n");
                        return;
                    }
                }

/*
    Tossed without a problem. Close the output file and then
    mark the header file element to indicate that it's been moved.
*/

                (void)fclose(msg_out);
		qbbs_tossed_count++;

                if (! was_network) {
                    msg_hdr.message_attribute -= MA_Unmoved_Outbound_Echo_Message;
                }
                else {
                    msg_hdr.message_attribute -= MA_Netmail_Message;
                }

/*
    If the operator wants to kill the message, then allow it!
*/

                if (want_kill) {
                    msg_hdr.message_attribute += MA_Deleted;
                }

                if (fseek(MSGHDR, (long)seek_update, SEEK_SET) != 0) {
                    (void)printf("Unable to reseek to msgheader!\n");
                }
                else {
                    if (fwrite(&msg_hdr, sizeof(struct Message_Header), 1, MSGHDR) != 1) {
                        (void)printf("I was unable to update the RA/QBBS message header!\n");
                    }

/*
    The write was ok. Now, when we leave here, we will read the
    next message header information. But before we do, we must
    seek to the very spot we are currently located so that the
    read will be performed. Norton Guides says it's needed and
    testing shows that the seek between reads and writes are needed.
*/

                    seek_update = (long)ftell(MSGHDR);
                    (void)fseek(MSGHDR, (long)seek_update, SEEK_SET);
                }
            }

            msg_count++;
	}
        else {
            return;
        }
    }
}

/* **********************************************************************
   * Find the files in the offered directory and call the routine that  *
   * checks to see if they should be appended to the Remote Access      *
   * message base.                                                      *
   *                                                                    *
   ********************************************************************** */

static void process_messages(unsigned char this_directory)
{
    unsigned int result;
    char record[81];

/*
    Validate the value that was passed to us. If it fails, then
    it means that there is a programming error in the code!
*/

    if (this_directory >= Max_Areas) {
        (void)printf("SYSTEM EXCEPTION at point 4 occured!\n");
        exit(Error_Directory_Bad);
    }

    if (Areas[this_directory].tag == 0) {
        return;
    }

    (void)sprintf(record, "%s*.msg", Areas[this_directory].directory);

    result =
        (unsigned int)findfirst(record, &file_block, FA_RDONLY | FA_ARCH);

    if (result != 0) {
        (void)printf("There are no messages in: '%s'              \n", record);
        return;
    }

    (void)printf("Looking at area %d ---> %s                            \n",
        this_directory,
        Areas[this_directory].directory);

/*
    Step through the directories one by one, passing control
    to the function which will examine the messages.
*/

    examine_message(this_directory);

    while (result == 0) {
        result = (unsigned int)findnext(&file_block);

        if (result == 0) {
            examine_message(this_directory);
        }
    }
}

/* **********************************************************************
   * The main entry point.                                              *
   *                                                                    *
   ********************************************************************** */

void main(int argc, char **argv)
{
    FILE *config;
    unsigned char loop, upto;
    char record[201], *point;
    unsigned int result;
    unsigned long hold_bc;
    char config_file_path[81];
    char hold_directory[81];

    want_delete = want_kill = FALSE;
    skip_inbound = skip_outbound = FALSE;
    rescan_person = want_diag = FALSE;
    msg_tossed_count = qbbs_tossed_count = 0;
    (void)strcpy(rescan_name, "-{None}-");

/*
    Scan the command line for arguments. Any arguments
    that we don't know anything about are simply ignored.
*/

    for (loop = 0; loop < argc; loop++) {
	if (*argv[loop] == '/') {
            point = argv[loop];
            point++;
            skipspace(point);

            if (! strnicmp(point, "delete", 6)) {
                want_delete = TRUE;
            }
            else if (! strnicmp(point, "kill", 4)) {
                want_kill = TRUE;
            }
            else if (! strnicmp(point, "sin", 3)) {
                skip_inbound = TRUE;
            }
            else if (! strnicmp(point, "sout", 4)) {
                skip_outbound = TRUE;
            }
            else if (! strnicmp(point, "scan", 4)) {
                rescan_person = TRUE;
                (void)printf("Enter rescan name: ");
                (void)fgets(rescan_name, 200, stdin);
                rescan_name[strlen(rescan_name) - 1] = (char)NULL;
                ucase(rescan_name);
            }
            else if (! strnicmp(point, "diag", 4)) {
                want_diag = TRUE;
            }
        }
    }

/*
    Find out where the configuration file should be. We'll get
    a NULL if there is no environment variable configured yet
    this is fine with us!  If there is no backslash at the end
    of the path then we append one.
*/

    if (getenv("FDTORA") != (char *)NULL) {
        (void)strcpy(config_file_path, getenv("FDTORA"));
        if (config_file_path[strlen(config_file_path) - 1] != '\\') {
            (void)strcat(config_file_path, "\\");
        }
    }
    else {
	(void)strcpy(config_file_path, "");
    }

    (void)strcat(config_file_path, "FDTORA.CFG");

    if ((config = fopen(config_file_path, "rt")) == (FILE *)NULL) {
        (void)printf("I am unable to find file: %s\n", config_file_path);
        exit(Error_Cant_Find_Config);
    }

/*
    Initialize both local and global variables
*/

    ra_directory = (char *)NULL;
    block_count = 0;

    for (loop = 0; loop < Max_Areas; loop++) {
        Areas[loop].tag = 0;
        Areas[loop].directory = (char *)NULL;
    }

    clrscr();

/*
    Find out where the Remote Access files are stored
*/

    while (! feof(config) && ra_directory == (char *)NULL) {
        (void)fgets(record, 200, config);

        if (! feof(config)) {
            point = record;
            skipspace(point);

            if (strlen(point) > 2 && *point != ';') {
                ra_directory = farmalloc(strlen(point) + 1);

                if (ra_directory == (char *)NULL) {
                    (void)printf("I ran out of memory!\n");
                    exit(Error_Out_Of_Memory);
                }

		(void)strcpy(ra_directory, point);
		ra_directory[strlen(ra_directory) - 1] = (char)NULL;

                (void)printf("Remote Access files directory: %s\n\n",
                    ra_directory);
            }
        }
    }

/*
    We know where the Remote Access files are kept, now make sure
    that they can be opened! Do so now, taking care with the mode
    that's used. Some should be appended while others need to be
    read at their start point.
*/

    (void)sprintf(record, "%smsginfo.bbs", ra_directory);
    if ((MSGINFO = fopen(record, "r+b")) == (FILE *)NULL) {     /* W and R */
        (void)printf("File: %s could not be opened!\n", record);
        exit(Error_Cant_Open_Message_Base);
    }

    (void)sprintf(record, "%smsgidx.bbs", ra_directory);
    if ((MSGIDX = fopen(record, "a+b")) == (FILE *)NULL) {      /* Append */
        (void)printf("File: %s could not be opened!\n", record);
        exit(Error_Cant_Open_Message_Base);
    }

    (void)sprintf(record, "%smsgtoidx.bbs", ra_directory);
    if ((MSGTOIDX = fopen(record, "a+b")) == (FILE *)NULL) {    /* Append */
        (void)printf("File: %s could not be opened!\n", record);
        exit(Error_Cant_Open_Message_Base);
    }

    (void)sprintf(record, "%smsghdr.bbs", ra_directory);
    if ((MSGHDR = fopen(record, "a+b")) == (FILE *)NULL) {      /* Append */
        (void)printf("File: %s could not be opened!\n", record);
        exit(Error_Cant_Open_Message_Base);
    }

    (void)sprintf(record, "%smsgtxt.bbs", ra_directory);
    if ((MSGTXT = fopen(record, "a+b")) == (FILE *)NULL) {      /* Append */
        (void)printf("File: %s could not be opened!\n", record);
        exit(Error_Cant_Open_Message_Base);
    }

/*
    Count the number of blocks in the text file and then
    keep it at its end to get ready for appending.
*/

    hold_bc = (unsigned long)filelength(fileno(MSGTXT)) / 256L;
    block_count = (unsigned short)hold_bc;

/*
    Now get the rest of the configuration information into memory.
    Dynamically allocate the memory we need along the way.
*/

    while (! feof(config)) {
        (void)fgets(record, 200, config);

        if (! feof(config)) {
            point = record;
            skipspace(point);

            if (strlen(point) > 2 && *point != ';') {
                upto = 0;

                while (point[upto] && point[upto] != ' ' && point[upto] != 0x09)
                    upto++;

                if (point[upto]) {
                    (void)strncpy(hold_directory, point, upto);
                    hold_directory[upto] = (char)NULL;

                    point += upto;
                    skipspace(point);
                    directory_count = (unsigned char)atoi(point);

                    if (directory_count == 0) {
                        (void)printf
                            ("Area for directory '%s' may not have an area tag of 0!\n",
                            hold_directory);
                    }

                    Areas[directory_count].directory =
                        farmalloc(strlen(hold_directory) + 1);

                    if (Areas[directory_count].directory == (char *)NULL) {
                        (void)printf("I ran out of memory!\n");
                        exit(Error_Out_Of_Memory);
                    }

                    (void)strcpy(Areas[directory_count].directory,
                        hold_directory);

                    Areas[directory_count].tag = directory_count++;
                }
            }
        }
    }

/*
    How many messages are in the Remote Access message system?
    This information is for display only. When the information
    is updated in the file, the structure elements are updated.
*/

    result =
        fread(&msg_info, sizeof(struct Message_Information), 1, MSGINFO);

    if (result != 1) {
        (void)printf("I was unable to read file: MSGINFO.BBS!\n");
        (void)fcloseall();
        exit(Error_Cant_Read_Message);
    }

    ra_lowest = msg_info.lowest_message;
    ra_highest = msg_info.highest_message;
    ra_total = msg_info.total_messages;

/*
    We have all of the information we need so close up the
    configuration file.
*/

    (void)fclose(config);
    (void)printf("--------------------------------------------------------------\n");

    (void)printf("FDTORA Version %s\n", The_Version);

    (void)printf("Remote Access has message %d to %d, total: %d, Text Blocks %d\n",
        ra_lowest, ra_highest, ra_total, block_count);

    (void)printf("--------------------------------------------------------------\n");

/*
    For each directory, scan for messages and toss if needed
*/

    if (! skip_inbound) {
        for (loop = 0; loop < directory_count; loop++) {
            process_messages(loop);
        }
    }
    else {
        (void)printf("Skipping scanning for inbound mail\n");
    }

/*
    Now turn around and see if there are outbound messages to be
    sent to the *.MSG directories.
*/

    (void)fclose(MSGHDR);

    if (! skip_outbound) {
        (void)sprintf(record, "%smsghdr.bbs", ra_directory);

        if ((MSGHDR = fopen(record, "r+b")) == (FILE *)NULL) {   /* Update */
            (void)printf("File: %s could not be reopened!\n", record);
            exit(Error_Cant_Open_Message_Base);
        }

        (void)printf("Checking for RA/QBBS messages to send to *.MSG format.\n");
        process_outbound(rescan_person, rescan_name);
    }
    else {
        (void)printf("Skipping scanning for outbound mail\n");
    }

    (void)printf("\nFinished processing all areas\n\n");

    (void)printf("There were %d messages tossed from *.MSG to RA/QBBS format\n",
	msg_tossed_count);

    (void)printf("There were %d messages tossed from RA/QBBS to *.MSG format\n",
	qbbs_tossed_count);

/*
    Make sure that everything is closed
*/

    (void)fcloseall();

/*
    Determine what our errorlevel upon exit is to be
*/

    if (msg_tossed_count + qbbs_tossed_count == 0)
        exit(Exit_No_Mail_Tossed);

    exit(Exit_Mail_Tossed);
}


