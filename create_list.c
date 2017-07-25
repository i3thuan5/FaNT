/*
********************************************************************************
*
*      COPYRIGHT (C) 2004 Niederrhein University of Applied Sciences
*      47805 Krefeld, GERMANY, Tel Int + 49 172 8353212
*
********************************************************************************
*
*      File             : create_list.c
*      Author           : G. Hirsch
*      Tested Platforms : Linux-OS
*      Description      : The program can be used to create a list of sample
*                         indices for selecting the noise segments without
*                         needing a random generator in the noise adding program.
*
*      Revision history
*
*      Rev  Date       Name            Description
*      -------------------------------------------------------------------
*      p1a  19-11-04   G. Hirsch       basic version 
*
********************************************************************************
*/

/*
********************************************************************************
*                         MODULE INCLUDE FILE AND VERSION ID
********************************************************************************
*/
const char module_id[] = "@(#)$Id: $";

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*=====================================================================*/

typedef struct	{
		char  *input_list;
		char  *output_file;
		char  *noise_file;
		int    seed;
		} PARAMETER;



void anal_comline(PARAMETER*, int, char**);
void print_usage(char*);
long flen(FILE*);

/*=====================================================================*/

int  main(int argc, char *argv[])
{
	PARAMETER	pars;
	FILE       *fp_noise=NULL, *fp_list, *fp_speech, *fp_outlist;
	long        no_noise_samples=0, no_speech_samples, start;
	char        filename[300];
	
	anal_comline(&pars, argc, argv);

	if ( (fp_noise = fopen(pars.noise_file, "r")) == NULL)
	{
		fprintf(stderr, "\ncannot open noise file %s\n\n", pars.noise_file);
		exit(-1);
	}
	no_noise_samples = flen(fp_noise)/2;

	if (pars.seed == -1)
	{
		srand((unsigned int) time(NULL));
		fprintf(stdout, " Random seed (actual time) for the extraction of the noise segment\n");
	}
	else
	{
		srand(pars.seed);
		fprintf(stdout, " Seed for the extraction of the noise segment: %d\n", pars.seed);
	}

	if ( (fp_list = fopen(pars.input_list, "r")) == NULL)
	{
		fprintf(stderr, "\ncannot open list file %s\n", pars.input_list);
		exit(-1);
	}
	if ( (fp_outlist = fopen(pars.output_file, "w")) == NULL)
	{
		fprintf(stderr, "\ncannot open output file %s\n", pars.output_file);
		exit(-1);
	}

	fprintf(stdout, "Creating list of sample indices ...\n");
	while ( fscanf(fp_list, "%s", filename) != EOF)
	{
		if ( (fp_speech = fopen(filename, "r")) == NULL)
		{
			fprintf(stderr, "\ncannot open speech file %s\n", filename);
			exit(-1);
		}

		no_speech_samples = flen(fp_speech)/2;

		if (no_noise_samples > no_speech_samples)  /* noise signal longer than speech signal */
		{
			/* select segment randomly out of noise signal */
			start = (long) ( (double)(rand())/(RAND_MAX) * (double)(no_noise_samples - no_speech_samples));
			fprintf(fp_outlist, "%ld\n", start);
		}
		else /* speech signal longer than noise signal */
		{
			fprintf(fp_outlist, "1\n");
		}

		fclose(fp_speech);
	}
	
	fclose(fp_list);
	fclose(fp_noise);
	fclose(fp_outlist);
	return 0;
}


/*=====================================================================*/
					
void	anal_comline(PARAMETER *pars, int argc, char** argv)
{
	int c;
	extern	int optind;
	extern	char *optarg;

	pars->input_list = NULL;
	pars->noise_file = NULL;
	pars->output_file = NULL;
	pars->seed = -1;

	while( (c = getopt(argc, argv, "hi:o:n:r:")) != -1)
	{
	  switch(c)
	  {
		case 'i':
			pars->input_list = optarg;
			if (access(pars->input_list, F_OK) == -1)
			{
				fprintf(stderr, "\nunable to access list file %s\n", pars->input_list);
				print_usage(argv[0]);
			}
			break;
		case 'n':
			pars->noise_file = optarg;
			if (access(pars->noise_file, F_OK) == -1)
			{
				fprintf(stderr, "\nunable to access noise file %s\n", pars->noise_file);
				print_usage(argv[0]);
			}
			break;
		case 'o':
			pars->output_file = optarg;
			if (access(pars->output_file, F_OK) != -1)
			{
				fprintf(stderr, "\nATTENTION: Output file %s does already exist!\n", pars->output_file);
				print_usage(argv[0]);
			}
			break;
		case 'r':
			pars->seed = atoi(optarg);
			break;
		case 'h':
			print_usage(argv[0]);
		default:
			fprintf(stderr, "\n\nERROR: unknown option!\n");
			print_usage(argv[0]);
	   }
	}
	if (pars->input_list == NULL)
	{
		fprintf(stderr, "\n\n Input list is not defined.");
		print_usage(argv[0]);
	}
	if (pars->output_file == NULL)
	{
		fprintf(stderr, "\n\n Output file is not defined.");
		print_usage(argv[0]);
	}
	if (pars->noise_file == NULL)
	{
		fprintf(stderr, "\n\n Noise file is not defined.");
		print_usage(argv[0]);
	}		
}

/*=====================================================================*/

void print_usage(char *name)
{
	fprintf(stderr,"\nUsage:\t%s [Options]",name);
	fprintf(stderr,"\n\nOptions:");
	fprintf(stderr,"\n\t-i\t<filename> containing a list of speech files");
	fprintf(stderr,"\n\t-o\t<filename> of the output file");
	fprintf(stderr,"\n\t-n\t<filename> referencing a noise file");
	fprintf(stderr,"\n\t-r\t<value> of the random seed");
	fprintf(stderr,"\n\t\t(NOT applying this option the seed is calculated from the actual time)");
	fprintf(stderr,"\n");
	exit(-1);
}

	
long  flen (FILE  *file)
{
        long    pos;
        long     len;

        pos = ftell( file );      
        fseek( file, 0, 2 );     
        len = ftell(file);   
		fseek( file, pos, 0 );    
        return( len );
}

