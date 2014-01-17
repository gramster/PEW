/************************************************/
/*						*/
/*     Epretty.c - Pretty printer for E-Code	*/
/*						*/
/* written by Graham Wheeler, December 1988	*/
/*						*/
/************************************************/

#include "misc.h"

#define NEED_OP_ARGS
#define NEED_OP_NAMES
#include "ecode.h"

int 
main(int argc, char *argv[])
{
    long offset = 0;		/* File offset					 */
    short Word = 0;		/* Current Word read from file			 */
    char fname[64], sname[64];
    short indent_lev = 0;	/* Indentation level				 */
    FILE *code, *source = NULL;
    char sourcebuff[128];
    short line = 0;

    fname[0] = 0;
    if (argc != 2)
    {
	fprintf(stderr, "Usage: epretty <file base name>\n");
	exit(0);
    } else
	strcpy(fname, argv[1]);
    strcpy(sname, fname);
    strcat(fname, ".cod");
    strcat(sname, ".est");
    if (fname[0] != 0)
    {
	if ((code = fopen(fname, "rb")) == NULL)
	{
	    fprintf(stderr, "epretty: cannot open file!\n");
	    exit(1);
	}
    } else
    {
	fprintf(stderr, "epretty: invalid or missing filename!\n");
	exit(2);
    }

    source = fopen(sname, "r");

    while (!feof(code))
    {
	if (fread(&Word, 2, 1, code) != 1)
	    break;		/* read an E-code instruction */
	if (Word < 0 || Word > NUM_OPS)
	{
	    printf("Illegal opcode : %d at offset %ld\n", Word, offset);
	    offset++;
	} else
	{
	    short i = Op_NArgs[Word];
	    short j;
	    printf("\n%6ld : ", offset);
	    if (Word != OP_NEWLINE)
	    {
		for (j = 0; j < indent_lev; j++)
		    printf("    ");
		printf("%-16s%c", Op_Names[Word], (i ? '(' : ' '));
		if (Word == OP_MODULE)
		    indent_lev++;
		offset += i + 1;
		while (i--)
		{
		    fread(&Word, 2, 1, code);
		    printf("%5d%c", Word, i ? ',' : ')');
		}
	    } else
	    {
		fread(&Word, 2, 1, code);
		printf("Line %d", Word);
		if (source)
		{
		    short len;
		    do
		    {
			fgets(sourcebuff, 127, source);
			len = strlen(sourcebuff) - 1;
			if (sourcebuff[len] == '\n')
			    sourcebuff[len] = 0;
			printf("\n%s", sourcebuff);
		    } while (++line < Word);
		}
		offset += 2;
	    }
	}
    }
    printf("\n");
    fclose(code);
    if (source)
	fclose(source);
    return 0;
}
