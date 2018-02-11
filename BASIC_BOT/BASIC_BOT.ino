// TinyBASIC.cpp : An implementation of TinyBASIC in C
//
// Author : Mike Field - hamster@snap.net.nz
//
// Based on TinyBasic for 68000, by Gordon Brandly
// (see http://members.shaw.ca/gbrandly/68ktinyb.html)
//
// which itself was Derived from Palo Alto Tiny BASIC as 
// published in the May 1976 issue of Dr. Dobb's Journal.  
// 
// 0.03 21/01/2011 : Added INPUT routine 
//                 : Reorganised memory layout
//                 : Expanded all error messages
//                 : Break key added
//                 : Removed the calls to printf (left by debugging)
#include <glcd.h>
#include <fonts/allFonts.h>
#include <EEPROM.h>

//inlcuir bitmaps
#include "bitmaps/thonka1.h"
#include "bitmaps/andres.h"
#include "bitmaps/familia.h"
#include "bitmaps/hermanos.h"

int eepos = 0;
int val;
boolean inhibitOutput = false;
static boolean runAfterLoad = false;
static boolean triggerRun = false;
enum {
  kStreamSerial = 0,
  kStreamEEProm,
  kStreamFile
};
static unsigned char inStream = kStreamSerial;
static unsigned char outStream = kStreamSerial;


#include <PS2Keyboard.h>

const int DataPin = 12;
const int IRQpin =  3;

PS2Keyboard keyboard;

//#include <LiquidCrystal.h>						// include the library code
//LiquidCrystal lcd(7, 8, 9, 10, 11, 12);			// initialize the library with the numbers of the interface pins
//#include <Servo.h> 
//Servo myservo[7];  								// create servo object to control a servo 

//int screenMem[72];
//int cursorX = 0;
int checkChar = 0;

byte pinDef[] = {0,1,2,13,19};                       //pin 0,1,2,3,4.
byte adcDef[] = {A0, A1, A2, A3, A4, A5};
byte pinDir[7]; 					//0=input 1=output 9=servo
byte pinStates[7];					//0 or 1.

#ifndef ARDUINO
#include "stdafx.h"
#include <conio.h>

// include the library code:
//#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins


#endif 

// ASCII Characters
#define CR	'\r'
#define NL	'\n'
#define LF      0x0a
#define TAB	'\t'
#define BELL	'\b'
#define DEL	'\177'
#define SPACE   ' '
#define CTRLC	0xBB // Decimal ASCII 187 //This is Chatpad's CONTROL-C. Old version was 0x03
#define CTRLH	0x08
#define CTRLS	0x13
#define CTRLX	0x18

typedef short unsigned LINENUM;


/***********************************************************/
// Keyword table and constants - the last character has 0x80 added to it
static unsigned char keywords[] = {
	'L','I','S','T'+0x80,
	'L','O','A','D'+0x80,
	'N','E','W'+0x80,
	'R','U','N'+0x80,
	'S','A','V','E'+0x80,
	'N','E','X','T'+0x80,
	'L','E','T'+0x80,
	'I','F'+0x80,
	'G','O','T','O'+0x80,
	'G','O','S','U','B'+0x80,
	'R','E','T','U','R','N'+0x80,
	'R','E','M'+0x80,
	'F','O','R'+0x80,
	'I','N','P','U','T'+0x80,
	'P','R','I','N','T'+0x80,
	'P','O','K','E'+0x80,
	'S','T','O','P'+0x80,
	'B','Y','E'+0x80,
	'P','I','N','O','U','T'+0x80,
	'D','E','L','A','Y'+0x80,
	'P','I','N','M','O','D','E'+0x80,
	'P','W','M'+0x80,
    'E','C','H','A','I','N'+0x80,
    'E','L','I','S','T'+0x80,
    'E','L','O','A','D'+0x80,
    'E','F','O','R','M','A','T'+0x80,
    'E','S','A','V','E'+0x80,
    'P','R','E','N','D','E'+0x80,
    'A','P','A','G','A'+0x80,
    'E','S','P','E','R', 'A'+0x80,
    'F','O','T','O','S'+0x80,
	0
};

#define KW_LIST		0
#define KW_LOAD		1
#define KW_NEW		2
#define KW_RUN		3
#define KW_SAVE		4
#define KW_NEXT		5
#define KW_LET		6
#define KW_IF		7
#define KW_GOTO		8
#define KW_GOSUB	9
#define KW_RETURN	10
#define KW_REM		11
#define KW_FOR		12
#define KW_INPUT	13
#define KW_PRINT	14
#define KW_POKE		15
#define KW_STOP		16
#define KW_BYE		17
#define KW_PINOUT	18
#define KW_DELAY	19
#define KW_PINMODE	20
#define KW_PWM		21
#define KW_ECHAIN   22
#define KW_ELIST    23
#define KW_ELOAD    24
#define KW_EFORMAT  25
#define KW_ESAVE    26
#define KW_PRENDE   27
#define KW_APAGA    28
#define KW_ESPERA   29
#define KW_FOTOS    30
#define KW_DEFAULT	31


struct stack_for_frame {
	char frame_type;
	char for_var;
	short int terminal;
	short int step;
	unsigned char *current_line;
	unsigned char *txtpos;
};

struct stack_gosub_frame {
	char frame_type;
	unsigned char *current_line;
	unsigned char *txtpos;
};

static unsigned char func_tab[] = {
	'P','E','E','K'+0x80,
	'P','I','N','I','N'+0x80,
	
	0
};
#define FUNC_PEEK    0
#define FUNC_PININ	 1
#define FUNC_UNKNOWN 2

static unsigned char to_tab[] = {
	'T','O'+0x80,
	0
};

static unsigned char step_tab[] = {
	'S','T','E','P'+0x80,
	0
};

static unsigned char relop_tab[] = {
	'>','='+0x80,
	'<','>'+0x80,
	'>'+0x80,
	'='+0x80,
	'<','='+0x80,
	'<'+0x80,
	0
};

#define RELOP_GE		0
#define RELOP_NE		1
#define RELOP_GT		2
#define RELOP_EQ		3
#define RELOP_LE		4
#define RELOP_LT		5
#define RELOP_UNKNOWN	6

#define VAR_SIZE sizeof(short int) // Size of variables in bytes

static unsigned char memory[1200];
static unsigned char *txtpos,*list_line;
static unsigned char expression_error;
static unsigned char *tempsp;
static unsigned char *stack_limit;
static unsigned char *program_start;
static unsigned char *program_end;
static unsigned char *stack; // Software stack for things that should go on the CPU stack
static unsigned char *variables_table;
static unsigned char *current_line;
static unsigned char *sp;
#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG 'F'
static unsigned char table_index;
static LINENUM linenum;

static const unsigned char okmsg[]		= "OK";
static const unsigned char badlinemsg[]		= "Invalid line number";
static const unsigned char invalidexprmsg[] = "Invalid expression";
static const unsigned char syntaxmsg[] = "Syntax Error";
static const unsigned char badinputmsg[] = "\nBad number";
static const unsigned char nomemmsg[]	= "Not enough memory!";
static const unsigned char initmsg[]	= "TinyBasic";
static const unsigned char memorymsg[]	= " Free.";
static const unsigned char breakmsg[]	= "[BREAK]";
static const unsigned char stackstuffedmsg[] = "Stack is stuffed!\n";
static const unsigned char unimplimentedmsg[]	= "Unimplemented";
static const unsigned char backspacemsg[]		= "\b \b";

static int inchar(void);
static void outchar(unsigned char c);
static void line_terminator(void);
static short int expression(void);
static unsigned char breakcheck(void);
/***************************************************************************/
static void ignore_blanks(void)
{
	while(*txtpos == SPACE || *txtpos == TAB)
		txtpos++;
}

/***************************************************************************/
static void scantable(unsigned char *table)
{
	int i = 0;
	ignore_blanks();
	table_index = 0;
	while(1)
	{
		// Run out of table entries?
		if(table[0] == 0)
            return;

		// Do we match this character?
		if(txtpos[i] == table[0])
		{
			i++;
			table++;
		}
		else
		{
			// do we match the last character of keywork (with 0x80 added)? If so, return
			if(txtpos[i]+0x80 == table[0])
			{
				txtpos += i+1;  // Advance the pointer to following the keyword
				ignore_blanks();
				return;
			}

			// Forward to the end of this keyword
			while((table[0] & 0x80) == 0)
				table++;

			// Now move on to the first character of the next word, and reset the position index
			table++;
			table_index++;
			i = 0;
		}
	}
}

/***************************************************************************/
static void pushb(unsigned char b)
{
	sp--;
	*sp = b;
}

/***************************************************************************/
static unsigned char popb()
{
	unsigned char b;
	b = *sp;
	sp++;
	return b;
}

/***************************************************************************/
static void printnum(int num)
{
	int digits = 0;

	if(num < 0)
	{
		num = -num;
		outchar('-');
	}

	do {
		pushb(num%10+'0');
		num = num/10;
		digits++;
	}
	while (num > 0);

	while(digits > 0)
	{
		outchar(popb());
		digits--;
	}
}
/***************************************************************************/
static unsigned short testnum(void)
{
	unsigned short num = 0;
	ignore_blanks();
	
	while(*txtpos>= '0' && *txtpos <= '9' )
	{
		// Trap overflows
		if(num >= 0xFFFF/10)
		{
			num = 0xFFFF;
			break;
		}

		num = num *10 + *txtpos - '0';
		txtpos++;
	}
	return	num;
}

/***************************************************************************/
unsigned char check_statement_end(void)
{
	ignore_blanks();
	return (*txtpos == NL) || (*txtpos == ':');
}

/***************************************************************************/
static void printmsgNoNL(const unsigned char *msg)
{
	while(*msg)
	{
		outchar(*msg);
		msg++;
	}
}

/***************************************************************************/
static unsigned char print_quoted_string(void)
{
	int i=0;
	unsigned char delim = *txtpos;
	if(delim != '"' && delim != '\'')
		return 0;
	txtpos++;

	// Check we have a closing delimiter
	while(txtpos[i] != delim)
	{
		if(txtpos[i] == NL)
			return 0;
		i++;
	}

	// Print the characters
	while(*txtpos != delim)
	{
		outchar(*txtpos);
		txtpos++;
	}
	txtpos++; // Skip over the last delimiter
	ignore_blanks();

	return 1;
}

/***************************************************************************/
static void printmsg(const unsigned char *msg)
{
	printmsgNoNL(msg);
    line_terminator();
}

/***************************************************************************/
unsigned char getln(char prompt)
{
	outchar(prompt);
	txtpos = program_end+sizeof(LINENUM);

	while(1)
	{
		char c = inchar();
		switch(c)
		{
			case CR:
			case NL:
                line_terminator();
				// Terminate all strings with a NL
				txtpos[0] = NL;
				return 1;
			case CTRLC:
				return 0;
			case CTRLH:
				if(txtpos == program_end)
					break;
				txtpos--;
				printmsgNoNL(backspacemsg);
				break;
			default:
				// We need to leave at least one space to allow us to shuffle the line into order
				if(txtpos == sp-2)
					outchar(BELL);
				else
				{
					txtpos[0] = c;
					txtpos++;
					outchar(c);
				}
		}
	}
}

/***************************************************************************/
static unsigned char *findline(void)
{
	unsigned char *line = program_start;
	while(1)
	{
		if(line == program_end)
			return line;

		if(((LINENUM *)line)[0] >= linenum)
			return line;

		// Add the line length onto the current address, to get to the next line;
		line += line[sizeof(LINENUM)];
	}
}

/***************************************************************************/
static void toUppercaseBuffer(void)
{
	unsigned char *c = program_end+sizeof(LINENUM);
	unsigned char quote = 0;

	while(*c != NL)
	{
		// Are we in a quoted string?
		if(*c == quote)
			quote = 0;
		else if(*c == '"' || *c == '\'')
			quote = *c;
		else if(quote == 0 && *c >= 'a' && *c <= 'z')
			*c = *c + 'A' - 'a';
		c++;
	}
}

/***************************************************************************/
void printline()
{
	LINENUM line_num;
	
	line_num = *((LINENUM *)(list_line));
    list_line += sizeof(LINENUM) + sizeof(char);

	// Output the line */
	printnum(line_num);
	outchar(' ');
	while(*list_line != NL)
    {
		outchar(*list_line);
		list_line++;
	}
	list_line++;
	line_terminator();
}

/***************************************************************************/
static short int expr4(void)
{
	short int a = 0;

	if(*txtpos == '0')
	{
		txtpos++;
		a = 0;
		goto success;
	}

	if(*txtpos >= '1' && *txtpos <= '9')
	{
		do 	{
			a = a*10 + *txtpos - '0';
			txtpos++;
		} while(*txtpos >= '0' && *txtpos <= '9');
			goto success;
	}

	// Is it a function or variable reference?
	if(txtpos[0] >= 'A' && txtpos[0] <= 'Z')
	{
		// Is it a variable reference (single alpha)
		if(txtpos[1] < 'A' || txtpos[1] > 'Z')
		{
			a = ((short int *)variables_table)[*txtpos - 'A'];
			txtpos++;
			goto success;
		}

		// Is it a function with a single parameter
		scantable(func_tab);
		if(table_index == FUNC_UNKNOWN)
			goto expr4_error;

		unsigned char f = table_index;

		if(*txtpos != '(')
			goto expr4_error;

		txtpos++;
		a = expression();
		if(*txtpos != ')')
				goto expr4_error;
		txtpos++;
		switch(f)
		{
			case FUNC_PEEK:
				a =  EEPROM.read(a);
				goto success;
			case FUNC_PININ:
				if(a > -1 and a < 7 and pinDir[a] == 0) {	//Within pin range and pin is set to input?
					a = digitalRead(pinDef[a]);
				}
				goto success;
		}
	}

	if(*txtpos == '(')
	{
		txtpos++;
		a = expression();
		if(*txtpos != ')')
			goto expr4_error;

		txtpos++;
		goto success;
	}

expr4_error:
	expression_error = 1;
success:
	ignore_blanks();
	return a;
}

/***************************************************************************/
static short int expr3(void)
{
	short int a,b;

	a = expr4();
	while(1)
	{
		if(*txtpos == '*')
		{
			txtpos++;
			b = expr4();
			a *= b;
		}
		else if(*txtpos == '/')
		{
			txtpos++;
			b = expr4();
			if(b != 0)
				a /= b;
			else
				expression_error = 1;
		}
		else
			return a;
	}
}

/***************************************************************************/
static short int expr2(void)
{
	short int a,b;

	if(*txtpos == '-' || *txtpos == '+')
		a = 0;
	else
		a = expr3();

	while(1)
	{
		if(*txtpos == '-')
		{
			txtpos++;
			b = expr3();
			a -= b;
		}
		else if(*txtpos == '+')
		{
			txtpos++;
			b = expr3();
			a += b;
		}
		else
			return a;
	}
}
/***************************************************************************/
static short int expression(void)
{
	short int a,b;

	a = expr2();
	// Check if we have an error
	if(expression_error)	return a;

	scantable(relop_tab);
	if(table_index == RELOP_UNKNOWN)
		return a;
	
	switch(table_index)
	{
	case RELOP_GE:
		b = expr2();
		if(a >= b) return 1;
		break;
	case RELOP_NE:
		b = expr2();
		if(a != b) return 1;
		break;
	case RELOP_GT:
		b = expr2();
		if(a > b) return 1;
		break;
	case RELOP_EQ:
		b = expr2();
		if(a == b) return 1;
		break;
	case RELOP_LE:
		b = expr2();
		if(a <= b) return 1;
		break;
	case RELOP_LT:
		b = expr2();
		if(a < b) return 1;
		break;
	}
	return 0;
}

/***************************************************************************/
void loop()
{
	unsigned char *start;
	unsigned char *newEnd;
	unsigned char linelen;
	
	variables_table = memory;
	program_start = memory + 27*VAR_SIZE;
	program_end = program_start;
	sp = memory+sizeof(memory);  // Needed for printnum
	printmsg(initmsg);
	printnum(sp-program_end);
	printmsg(memorymsg);

warmstart:
	// this signifies that it is running in 'direct' mode.
	current_line = 0;
	sp = memory+sizeof(memory);  
	//printmsg(okmsg);

prompt:
        if( triggerRun ){
    triggerRun = false;
    current_line = program_start;
    goto execline;
     }
 
	while(!getln('>'))
		line_terminator();
	toUppercaseBuffer();

	txtpos = program_end+sizeof(unsigned short);

	// Find the end of the freshly entered line
	while(*txtpos != NL)
		txtpos++;

	// Move it to the end of program_memory
	{
		unsigned char *dest;
		dest = sp-1;
		while(1)
		{
			*dest = *txtpos;
			if(txtpos == program_end+sizeof(unsigned short))
				break;
			dest--;
			txtpos--;
		}
		txtpos = dest;
	}

	// Now see if we have a line number
	linenum = testnum();
	ignore_blanks();
	if(linenum == 0)
		goto direct;

	if(linenum == 0xFFFF)
		goto badline;

	// Find the length of what is left, including the (yet-to-be-populated) line header
	linelen = 0;
	while(txtpos[linelen] != NL)
		linelen++;
	linelen++; // Include the NL in the line length
	linelen += sizeof(unsigned short)+sizeof(char); // Add space for the line number and line length

	// Now we have the number, add the line header.
	txtpos -= 3;
	*((unsigned short *)txtpos) = linenum;
	txtpos[sizeof(LINENUM)] = linelen;


	// Merge it into the rest of the program
	start = findline();

	// If a line with that number exists, then remove it
	if(start != program_end && *((LINENUM *)start) == linenum)
	{
		unsigned char *dest, *from;
		unsigned tomove;

		from = start + start[sizeof(LINENUM)];
		dest = start;

		tomove = program_end - from;
		while( tomove > 0)
		{
			*dest = *from;
			from++;
			dest++;
			tomove--;
		}	
		program_end = dest;
	}

	if(txtpos[sizeof(LINENUM)+sizeof(char)] == NL) // If the line has no txt, it was just a delete
		goto prompt;



	// Make room for the new line, either all in one hit or lots of little shuffles
	while(linelen > 0)
	{	
		unsigned int tomove;
		unsigned char *from,*dest;
		unsigned int space_to_make;
	
		space_to_make = txtpos - program_end;

		if(space_to_make > linelen)
			space_to_make = linelen;
		newEnd = program_end+space_to_make;
		tomove = program_end - start;


		// Source and destination - as these areas may overlap we need to move bottom up
		from = program_end;
		dest = newEnd;
		while(tomove > 0)
		{
			from--;
			dest--;
			*dest = *from;
			tomove--;
		}

		// Copy over the bytes into the new space
		for(tomove = 0; tomove < space_to_make; tomove++)
		{
			*start = *txtpos;
			txtpos++;
			start++;
			linelen--;
		}
		program_end = newEnd;
	}
	goto prompt;

unimplemented:
	printmsg(unimplimentedmsg);
	goto prompt;

badline:	
	printmsg(badlinemsg);
	goto prompt;
invalidexpr:
	printmsg(invalidexprmsg);
	goto prompt;
syntaxerror:
	printmsg(syntaxmsg);
	if(current_line != (void *)0)
	{
           unsigned char tmp = *txtpos;
		   if(*txtpos != NL)
				*txtpos = '^';
           list_line = current_line;
           printline();
           *txtpos = tmp;
	}
    //line_terminator();
	goto prompt;

stackstuffed:	
	printmsg(stackstuffedmsg);
	goto warmstart;
nomem:	
	printmsg(nomemmsg);
	goto warmstart;

run_next_statement:
	while(*txtpos == ':')
		txtpos++;
	ignore_blanks();
	if(*txtpos == NL)
		goto execnextline;
	goto interperateAtTxtpos;

direct: 
	txtpos = program_end+sizeof(LINENUM);
	if(*txtpos == NL)
		goto prompt;

interperateAtTxtpos:
        if(breakcheck())
        {
          printmsg(breakmsg);
          goto warmstart;
        }

	scantable(keywords);
	ignore_blanks();

	switch(table_index)
	{       
        case KW_EFORMAT:
          goto eformat;
        case KW_ESAVE:
          goto esave;
        case KW_ELOAD:
          goto eload;
        case KW_ELIST:
          goto elist;
        case KW_ECHAIN:
          goto echain;
		case KW_LIST:
			goto list;
		case KW_LOAD:
			goto unimplemented; /////////////////
		case KW_NEW:
			if(txtpos[0] != NL)
				goto syntaxerror;
			program_end = program_start;
			goto prompt;
		case KW_RUN:
			current_line = program_start;
			goto execline;
		case KW_SAVE:
			goto unimplemented; //////////////////////
		case KW_NEXT:
			goto next;
		case KW_LET:
			goto assignment;
		case KW_IF:
			{
			short int val;
			expression_error = 0;
			val = expression();
			if(expression_error || *txtpos == NL)
				goto invalidexpr;
			if(val != 0)
				goto interperateAtTxtpos;
			goto execnextline;
			}
		case KW_GOTO:
			expression_error = 0;
			linenum = expression();
			if(expression_error || *txtpos != NL)
				goto invalidexpr;
			current_line = findline();
			goto execline;

		case KW_GOSUB:
			goto gosub;
		case KW_RETURN:
			goto gosub_return; 
		case KW_REM:	
			goto execnextline;	// Ignore line completely
		case KW_FOR:
			goto forloop; 
		case KW_INPUT:
			goto input; 
		case KW_PRINT:
			goto print;
		case KW_POKE:
			goto poke;
		case KW_STOP:
			// This is the easy way to end - set the current line to the end of program attempt to run it
			if(txtpos[0] != NL)
				goto syntaxerror;
			current_line = program_end;
			goto execline;
		case KW_BYE:
			// Leave the basic interperater
			return;
                case KW_PINMODE:
			goto pinmode;
		case KW_PINOUT:
			goto pinout;
		case KW_DELAY:
			goto delayT;
		case KW_PWM:
			goto doPWM;
		case KW_PRENDE:
			goto prende_led;
		case KW_APAGA:
			goto apaga_led;
		case KW_ESPERA:
			goto delayT;
		case KW_FOTOS:
			goto display_photos;
		case KW_DEFAULT:
			goto assignment;
		default:
			break;
	}
elist:
  {
    int i;
    for( i = 0 ; i < (E2END +1) ; i++ )
    {
      val = EEPROM.read( i );

      if( val == '\0' ) {
        goto execnextline;
      }

      if( ((val < ' ') || (val  > '~')) && (val != NL) && (val != CR))  {
        outchar( '?' );
      } 
      else {
        outchar( val );
      }
    }
  }
  goto execnextline;

eformat:
  {
    for( int i = 0 ; i < E2END ; i++ )
    {
      if( (i & 0x03f) == 0x20 ) outchar( '.' );
      EEPROM.write( i, 0 );
    }
    outchar( LF );
  }
  goto execnextline;

esave:
  {
    outStream = kStreamEEProm;
    eepos = 0;

    // copied from "List"
    list_line = findline();
    while(list_line != program_end)
      printline();

    // go back to standard output, close the file
    outStream = kStreamSerial;
    
    goto warmstart;
  }
  
  
echain:
  runAfterLoad = true;

eload:
  // clear the program
  program_end = program_start;

  // load from a file into memory
  eepos = 0;
  inStream = kStreamEEProm;
  inhibitOutput = true;
  goto warmstart;

	
execnextline:
	if(current_line == (void *)0)		// Processing direct commands?
		goto prompt;
	current_line +=	 current_line[sizeof(LINENUM)];

execline:
  	if(current_line == program_end) // Out of lines to run
		goto warmstart;
	txtpos = current_line+sizeof(LINENUM)+sizeof(char);
	goto interperateAtTxtpos;

input:
	{
		unsigned char isneg=0;
		unsigned char *temptxtpos;
		short int *var;
		ignore_blanks();
		if(*txtpos < 'A' || *txtpos > 'Z')
			goto syntaxerror;
		var = ((short int *)variables_table)+*txtpos-'A';
		txtpos++;
		if(!check_statement_end())
			goto syntaxerror;
again:
		temptxtpos = txtpos;
		if(!getln('?'))
			goto warmstart;

		// Go to where the buffer is read
		txtpos = program_end+sizeof(LINENUM);
		if(*txtpos == '-')
		{
			isneg = 1;
			txtpos++;
		}

		*var = 0;
		do 	{
			*var = *var*10 + *txtpos - '0';
			txtpos++;
		} while(*txtpos >= '0' && *txtpos <= '9');
		ignore_blanks();
		if(*txtpos != NL)
		{
			printmsg(badinputmsg);
			goto again;
		}
	
		if(isneg)
			*var = -*var;

		goto run_next_statement;
	}
forloop:
	{
		unsigned char var;
		short int initial, step, terminal;

		if(*txtpos < 'A' || *txtpos > 'Z')
			goto syntaxerror;
		var = *txtpos;
		txtpos++;
		
		scantable(relop_tab);
		if(table_index != RELOP_EQ)
			goto syntaxerror;

		expression_error = 0;
		initial = expression();
		if(expression_error)
			goto invalidexpr;
	
		scantable(to_tab);
		if(table_index != 0)
			goto syntaxerror;
	
		terminal = expression();
		if(expression_error)
			goto invalidexpr;
	
		scantable(step_tab);
		if(table_index == 0)
		{
			step = expression();
			if(expression_error)
				goto invalidexpr;
		}
		else
			step = 1;
		if(!check_statement_end())
			goto syntaxerror;


		if(!expression_error && *txtpos == NL)
		{
			struct stack_for_frame *f;
			if(sp + sizeof(struct stack_for_frame) < stack_limit)
				goto nomem;

			sp -= sizeof(struct stack_for_frame);
			f = (struct stack_for_frame *)sp;
			((short int *)variables_table)[var-'A'] = initial;
			f->frame_type = STACK_FOR_FLAG;
			f->for_var = var;
			f->terminal = terminal;
			f->step     = step;
			f->txtpos   = txtpos;
			f->current_line = current_line;
			goto run_next_statement;
		}
	}
	goto syntaxerror;

gosub:
	expression_error = 0;
	linenum = expression();
	if(expression_error)
		goto invalidexpr;
	if(!expression_error && *txtpos == NL)
	{
		struct stack_gosub_frame *f;
		if(sp + sizeof(struct stack_gosub_frame) < stack_limit)
			goto nomem;

		sp -= sizeof(struct stack_gosub_frame);
		f = (struct stack_gosub_frame *)sp;
		f->frame_type = STACK_GOSUB_FLAG;
		f->txtpos = txtpos;
		f->current_line = current_line;
		current_line = findline();
		goto execline;
	}
	goto syntaxerror;

next:
	// Fnd the variable name
	ignore_blanks();
	if(*txtpos < 'A' || *txtpos > 'Z')
		goto syntaxerror;
	txtpos++;
	if(!check_statement_end())
		goto syntaxerror;
	
gosub_return:
	// Now walk up the stack frames and find the frame we want, if present
	tempsp = sp;
	while(tempsp < memory+sizeof(memory)-1)
	{
		switch(tempsp[0])
		{
			case STACK_GOSUB_FLAG:
				if(table_index == KW_RETURN)
				{
					struct stack_gosub_frame *f = (struct stack_gosub_frame *)tempsp;
					current_line	= f->current_line;
					txtpos			= f->txtpos;
					sp += sizeof(struct stack_gosub_frame);
					goto run_next_statement;
				}
				// This is not the loop you are looking for... so Walk back up the stack
				tempsp += sizeof(struct stack_gosub_frame);
				break;
			case STACK_FOR_FLAG:
				// Flag, Var, Final, Step
				if(table_index == KW_NEXT)
				{
					struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
					// Is the the variable we are looking for?
					if(txtpos[-1] == f->for_var)
					{
						short int *varaddr = ((short int *)variables_table) + txtpos[-1] - 'A'; 
						*varaddr = *varaddr + f->step;
						// Use a different test depending on the sign of the step increment
						if((f->step > 0 && *varaddr <= f->terminal) || (f->step < 0 && *varaddr >= f->terminal))
						{
							// We have to loop so don't pop the stack
							txtpos = f->txtpos;
							current_line = f->current_line;
							goto run_next_statement;
						}
						// We've run to the end of the loop. drop out of the loop, popping the stack
						sp = tempsp + sizeof(struct stack_for_frame);
						goto run_next_statement;
					}
				}
				// This is not the loop you are looking for... so Walk back up the stack
				tempsp += sizeof(struct stack_for_frame);
				break;
			default:
				goto stackstuffed;
		}
	}
	// Didn't find the variable we've been looking for
	goto syntaxerror;

assignment:
	{
		short int value;
		short int *var;

		if(*txtpos < 'A' || *txtpos > 'Z')
			goto syntaxerror;
		var = (short int *)variables_table + *txtpos - 'A';
		txtpos++;

		ignore_blanks();

		if (*txtpos != '=')
			goto syntaxerror;
		txtpos++;
		ignore_blanks();
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
		// Check that we are at the end of the statement
		if(!check_statement_end())
			goto syntaxerror;
		*var = value;
	}
	goto run_next_statement;
poke:
	{
		int value;
		int address;

		// Work out where to put it
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
		address = value;

		// check for a comma
		ignore_blanks();
		if (*txtpos != ',')
			goto syntaxerror;
		txtpos++;
		ignore_blanks();

		// Now get the value to assign
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
		EEPROM.write(address, value);
		// printf("Poke %p value %i\n",address, (unsigned char)value);
		// Check that we are at the end of the statement
		if(!check_statement_end())
			goto syntaxerror;
	}
	goto run_next_statement;

pinout:
	{
		short int value;
		byte whichPin;

		// Work out where to put it
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
		whichPin = value;

		// check for a comma
		ignore_blanks();
		if (*txtpos != ',')
			goto syntaxerror;
		txtpos++;
		ignore_blanks();

		// Now get the value to assign
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
			
		pinMode(pinDef[whichPin], 1);	//Set to output
		pinDir[whichPin] = 1;
		pinStates[whichPin] = value;
		digitalWrite(pinDef[whichPin], pinStates[whichPin]);
		
		if(!check_statement_end())
			goto syntaxerror;
	}
	goto run_next_statement;	

pinmode:
	{
		short int value;
		byte whichPin;

		// Work out where to put it
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
		whichPin = value;

		// check for a comma
		ignore_blanks();
		if (*txtpos != ',')
			goto syntaxerror;
		txtpos++;
		ignore_blanks();

		// Now get the value to assign
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
		
		pinDir[whichPin] = value;						//Set value in array
		
		pinMode(pinDef[whichPin], pinDir[whichPin]);	//Set pin to that mode
		
		if (value == 0) {								//Are we making it an input?
		
			digitalWrite(pinDef[whichPin], 1);			//Write a 1 to enable pull up resistors
		
		}
		
		if(!check_statement_end())
			goto syntaxerror;
	}
	goto run_next_statement;
	
	
delayT:
	{
		int value;

		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
		delay(value);

		if(!check_statement_end())
			goto syntaxerror;
	}
	goto run_next_statement;	


doPWM:
	{
		short int value;
		byte whichPin;

		// Work out where to put it
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
		whichPin = value;
		if (whichPin != 1 and whichPin != 3 and whichPin != 4) 	//Not a PWM enabled pin?
			goto invalidexpr;										//Error		
		
		// check for a comma
		ignore_blanks();
		if (*txtpos != ',')
			goto syntaxerror;
		txtpos++;
		ignore_blanks();

		// Now get the value to assign
		expression_error = 0;
		value = expression();
		if(expression_error)
			goto invalidexpr;
			
		pinMode(pinDef[whichPin], 1);	//Set to output
		pinDir[whichPin] = 1;
		pinStates[whichPin] = value;
		analogWrite(pinDef[whichPin], pinStates[whichPin]);
		
		if(!check_statement_end())
			goto syntaxerror;
	}
	goto run_next_statement;		
	
list:
	linenum = testnum(); // Retuns 0 if no line found.

	// Should be EOL
	if(txtpos[0] != NL)
		goto syntaxerror;

	// Find the line
	list_line = findline();
	
	if (linenum != 0) {						//A specific line indicated?
		printline();						//Then print only it.	
	}
	else {									//Else, print everything up to the end
		while(list_line != program_end)
			printline();
	}
	
	goto warmstart;

print:
	// If we have an empty list then just put out a NL
	if(*txtpos == ':' )
	{
        line_terminator();
		txtpos++;
		goto run_next_statement;
	}
	if(*txtpos == NL)
	{
		goto execnextline;
	}

	while(1)
	{
		ignore_blanks();
		if(print_quoted_string())
		{
			;
		}
		else if(*txtpos == '"' || *txtpos == '\'')
			goto syntaxerror;
		else
		{
			short int e;
			expression_error = 0;
			e = expression();
			if(expression_error)
				goto invalidexpr;
			printnum(e);
		}

		// At this point we have three options, a comma or a new line
		if(*txtpos == ',')
			txtpos++;	// Skip the comma and move onto the next
		else if(txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':'))
		{
			txtpos++; // This has to be the end of the print - no newline
			break;
		}
		else if(check_statement_end())
		{
			line_terminator();	// The end of the print statement
			break;
		}
		else
			goto syntaxerror;	
	}
	goto run_next_statement;

prende_led: 
	digitalWrite(19,HIGH);
	goto run_next_statement;

apaga_led: 
	digitalWrite(19,LOW);
	goto run_next_statement;

display_photos:
	GLCD.ClearScreen();
    GLCD.DrawBitmap(familia,5,0);
    delay(4000);
    GLCD.ClearScreen();
    GLCD.DrawBitmap(thonka1,5,0);
    delay(4000);
    GLCD.ClearScreen();
    GLCD.DrawBitmap(andres,5,0);
    delay(4000);
    GLCD.ClearScreen();
    GLCD.DrawBitmap(hermanos,5,0);
    delay(4000);
    GLCD.ClearScreen();
    goto run_next_statement;
}

/***************************************************************************/
static void line_terminator(void)
{
  	outchar(NL);
	outchar(CR);
}


/***********************************************************/
static unsigned char breakcheck(void)
{
#ifdef ARDUINO
  if(keyboard.available())
    return keyboard.read() == CTRLC;
  return 0;
#else
  if(kbhit())
    return getch() == CTRLC;
   else
     return 0;
#endif
}
/***********************************************************/
static int inchar()
{
  int v;
  if(  inStream == kStreamEEProm ){

    v = EEPROM.read( eepos++ );
    if( v == '\0' ) {
      goto inchar_loadfinish;
    }
    return v;
  }
#ifdef ARDUINO



  while(1)
  {
    if(keyboard.available()) {
	  
	  checkChar = keyboard.read();
	  
	  if (checkChar != 0) {	//Not a null value of some kind?
	  
		return checkChar;
	  
	  }
	  
	}
  }
inchar_loadfinish:
  inStream = kStreamSerial;
  inhibitOutput = false;

  if( runAfterLoad ) {
    runAfterLoad = false;
    triggerRun = true;
  }
  return NL; // trigger a prompt.
#else
	return getch();
#endif
}

/***********************************************************/
static void outchar(unsigned char c)
{
#ifdef ARDUINO
  if( inhibitOutput ) return;
  if( outStream == kStreamEEProm ) {
      EEPROM.write( eepos++, c );
  }
  else{
  	char x=c;
  	GLCD.print(x);
  }
#else
	putch(c);
#endif
}

#ifdef ARDUINO
/***********************************************************/

void setup()
{
        keyboard.begin(DataPin, IRQpin, PS2Keymap_CUSTOM);
        GLCD.Init();
        GLCD.SelectFont(SystemFont5x7);

        // pin for wheels
        pinMode(13,OUTPUT);
        pinMode(2,OUTPUT);
        pinMode(0,OUTPUT);
        pinMode(1,OUTPUT);
        digitalWrite(0,LOW);
        digitalWrite(1,LOW);
        digitalWrite(13,LOW);
        digitalWrite(2,LOW);

        // led pin
        pinMode(19,OUTPUT);
        digitalWrite(19,LOW);
}
#endif

#ifndef ARDUINO
//***********************************************************/
int main()
{
 while(1)
  loop();
}
/***********************************************************/
#endif


